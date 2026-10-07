// Collision broad phase with convex hulls.
//
// Two concave shapes can be dragged with the mouse. Each frame the convex hull
// of each shape is computed with Graham's Scan, and the two hulls are tested
// for overlap with the Separating Axis Theorem (SAT). An exact test on the
// concave outlines is shown next to it, so the cases where the hull test
// reports a collision that does not exist (false positives) are visible.
//
// Keys: F false-positive position, R reset, H toggle hulls, A toggle SAT axis,
// ESC quit.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "raylib.h"

#include "geometry.h"
#include "graham.h"

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;

const Color kBackground{16, 20, 26, 255};
const Color kText{225, 230, 235, 255};
const Color kDim{130, 140, 150, 255};
const Color kOk{80, 220, 130, 255};
const Color kHit{240, 90, 90, 255};
const Color kWarn{255, 200, 60, 255};

// ---------------------------------------------------------------------------
// UI font and window scaling

Font gFont;
bool gFontLoaded = false;

// Loads a smooth TTF font if one is installed, otherwise falls back to raylib's bitmap font.
void loadUiFont() {
    const char* candidates[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    };
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        gFont = LoadFontEx(path, 64, nullptr, 0);
        if (gFont.texture.id != 0) {
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            gFontLoaded = true;
            return;
        }
    }
    gFont = GetFontDefault();
}

float fontSpacing(int size) { return gFontLoaded ? static_cast<float>(size) * 0.02f : static_cast<float>(size) / 10.0f; }

void drawText(const char* text, int x, int y, int size, Color color) {
    DrawTextEx(gFont, text, {static_cast<float>(x), static_cast<float>(y)}, static_cast<float>(size),
               fontSpacing(size), color);
}

// The program draws on a fixed 1280x720 canvas. The camera scales it to the real window
// size (letterboxed), so it stays sharp when the window is resized or the monitor is scaled.
Camera2D makeCamera() {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    const float zoom = std::min(w / static_cast<float>(kWidth), h / static_cast<float>(kHeight));
    Camera2D cam{};
    cam.zoom = zoom;
    cam.offset = {(w - static_cast<float>(kWidth) * zoom) / 2.0f, (h - static_cast<float>(kHeight) * zoom) / 2.0f};
    cam.target = {0.0f, 0.0f};
    cam.rotation = 0.0f;
    return cam;
}

// ---------------------------------------------------------------------------
// Shapes

struct Shape {
    std::string name;
    std::vector<Point> local;                // simple (possibly concave) outline
    std::vector<std::array<int, 3>> tris;    // triangulation of the outline, for filling
    Point pos;                               // world position of the local origin
    Color fill;
    Color edge;

    std::vector<Point> world() const {
        std::vector<Point> w;
        w.reserve(local.size());
        for (const Point& p : local) w.push_back({p.x + pos.x, p.y + pos.y});
        return w;
    }
};

bool pointInTriangle(const Point& p, const Point& a, const Point& b, const Point& c) {
    return cross(a, b, p) >= 0 && cross(b, c, p) >= 0 && cross(c, a, p) >= 0;
}

// Ear clipping triangulation of a simple polygon.
std::vector<std::array<int, 3>> triangulate(const std::vector<Point>& poly) {
    const int n = static_cast<int>(poly.size());
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;

    long long area2 = 0;
    for (int i = 0; i < n; ++i) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % n];
        area2 += a.x * b.y - b.x * a.y;
    }
    if (area2 < 0) std::reverse(idx.begin(), idx.end());  // make it counterclockwise

    std::vector<std::array<int, 3>> tris;
    while (idx.size() > 3) {
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m; ++i) {
            const int ia = idx[(i + m - 1) % m];
            const int ib = idx[i];
            const int ic = idx[(i + 1) % m];
            if (cross(poly[ia], poly[ib], poly[ic]) <= 0) continue;  // reflex or flat corner

            bool ear = true;
            for (int k : idx) {
                if (k == ia || k == ib || k == ic) continue;
                if (pointInTriangle(poly[k], poly[ia], poly[ib], poly[ic])) {
                    ear = false;
                    break;
                }
            }
            if (!ear) continue;

            tris.push_back({ia, ib, ic});
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;  // should not happen for a simple polygon
    }
    if (idx.size() == 3) tris.push_back({idx[0], idx[1], idx[2]});
    return tris;
}

Shape makeStar() {
    Shape s;
    s.name = "Star";
    const double pi = 3.14159265358979323846;
    const double outer = 135.0;
    const double inner = 57.0;
    for (int i = 0; i < 10; ++i) {
        const double angle = (-90.0 + 36.0 * i) * pi / 180.0;
        const double r = (i % 2 == 0) ? outer : inner;
        s.local.push_back({std::llround(r * std::cos(angle)), std::llround(r * std::sin(angle))});
    }
    s.tris = triangulate(s.local);
    s.fill = Color{230, 180, 60, 255};
    s.edge = Color{255, 220, 120, 255};
    return s;
}

Shape makeL() {
    Shape s;
    s.name = "L-shape";
    s.local = {{0, 0}, {120, 0}, {120, 240}, {360, 240}, {360, 360}, {0, 360}};
    s.tris = triangulate(s.local);
    s.fill = Color{70, 130, 220, 255};
    s.edge = Color{130, 180, 255, 255};
    return s;
}

// ---------------------------------------------------------------------------
// Exact overlap test for two simple polygons

bool onSegment(const Point& a, const Point& b, const Point& p) {
    return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) &&
           std::min(a.y, b.y) <= p.y && p.y <= std::max(a.y, b.y);
}

bool segmentsIntersect(const Point& p1, const Point& p2, const Point& p3, const Point& p4) {
    const long long d1 = cross(p3, p4, p1);
    const long long d2 = cross(p3, p4, p2);
    const long long d3 = cross(p1, p2, p3);
    const long long d4 = cross(p1, p2, p4);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return true;
    if (d1 == 0 && onSegment(p3, p4, p1)) return true;
    if (d2 == 0 && onSegment(p3, p4, p2)) return true;
    if (d3 == 0 && onSegment(p1, p2, p3)) return true;
    if (d4 == 0 && onSegment(p1, p2, p4)) return true;
    return false;
}

bool pointInPolygon(const std::vector<Point>& poly, double x, double y) {
    bool inside = false;
    const int n = static_cast<int>(poly.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const double xi = static_cast<double>(poly[i].x), yi = static_cast<double>(poly[i].y);
        const double xj = static_cast<double>(poly[j].x), yj = static_cast<double>(poly[j].y);
        if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) inside = !inside;
    }
    return inside;
}

bool polygonsOverlap(const std::vector<Point>& a, const std::vector<Point>& b) {
    const int na = static_cast<int>(a.size());
    const int nb = static_cast<int>(b.size());
    for (int i = 0; i < na; ++i)
        for (int j = 0; j < nb; ++j)
            if (segmentsIntersect(a[i], a[(i + 1) % na], b[j], b[(j + 1) % nb])) return true;
    // No edges cross: either disjoint or one contains the other.
    return pointInPolygon(a, static_cast<double>(b[0].x), static_cast<double>(b[0].y)) ||
           pointInPolygon(b, static_cast<double>(a[0].x), static_cast<double>(a[0].y));
}

// ---------------------------------------------------------------------------
// Separating Axis Theorem for two convex polygons

struct Vec2 {
    double x;
    double y;
};

struct SatResult {
    bool overlap = true;
    Vec2 axis{0, 0};  // unit separating axis, valid when overlap is false
};

void project(const std::vector<Point>& poly, const Vec2& axis, double& lo, double& hi) {
    lo = 1e300;
    hi = -1e300;
    for (const Point& p : poly) {
        const double d = static_cast<double>(p.x) * axis.x + static_cast<double>(p.y) * axis.y;
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
}

// Two convex polygons are disjoint exactly when some edge normal of either one
// separates their projections.
SatResult satTest(const std::vector<Point>& a, const std::vector<Point>& b) {
    SatResult result;
    const std::vector<Point>* polys[2] = {&a, &b};
    for (const std::vector<Point>* poly : polys) {
        const int n = static_cast<int>(poly->size());
        for (int i = 0; i < n; ++i) {
            const Point& p = (*poly)[i];
            const Point& q = (*poly)[(i + 1) % n];
            Vec2 axis{static_cast<double>(-(q.y - p.y)), static_cast<double>(q.x - p.x)};
            const double len = std::hypot(axis.x, axis.y);
            if (len == 0.0) continue;
            axis.x /= len;
            axis.y /= len;

            double minA, maxA, minB, maxB;
            project(a, axis, minA, maxA);
            project(b, axis, minB, maxB);
            if (maxA < minB || maxB < minA) {
                result.overlap = false;
                result.axis = axis;
                return result;
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Drawing helpers

Vector2 toV(const Point& p) { return {static_cast<float>(p.x), static_cast<float>(p.y)}; }

// Both winding orders are submitted; the back-facing one is culled.
void fillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

void drawOutline(const std::vector<Point>& poly, float thickness, Color color) {
    const int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; ++i) DrawLineEx(toV(poly[i]), toV(poly[(i + 1) % n]), thickness, color);
}

void drawShape(const Shape& s) {
    const std::vector<Point> w = s.world();
    for (const auto& t : s.tris) fillTriangle(toV(w[t[0]]), toV(w[t[1]]), toV(w[t[2]]), Fade(s.fill, 0.75f));
    drawOutline(w, 2.0f, s.edge);
}

void drawHull(const std::vector<Point>& hull, Color color) {
    if (hull.size() < 3) return;
    for (size_t i = 1; i + 1 < hull.size(); ++i) {
        fillTriangle(toV(hull[0]), toV(hull[i]), toV(hull[i + 1]), Fade(color, 0.12f));
    }
    drawOutline(hull, 2.0f, color);
    for (const Point& p : hull) DrawCircleV(toV(p), 4.0f, color);
}

Vec2 centroid(const std::vector<Point>& poly) {
    Vec2 c{0, 0};
    for (const Point& p : poly) {
        c.x += static_cast<double>(p.x);
        c.y += static_cast<double>(p.y);
    }
    c.x /= static_cast<double>(poly.size());
    c.y /= static_cast<double>(poly.size());
    return c;
}

// Draws the separating axis and the projection of each hull onto it.
void drawSatAxis(const std::vector<Point>& hullA, const std::vector<Point>& hullB, const Vec2& axis, Color colorA,
                 Color colorB) {
    const Vec2 ca = centroid(hullA);
    const Vec2 cb = centroid(hullB);
    const Vec2 m{(ca.x + cb.x) / 2.0, (ca.y + cb.y) / 2.0};
    const double base = m.x * axis.x + m.y * axis.y;

    auto onAxis = [&](double d) {
        return Vector2{static_cast<float>(m.x + axis.x * (d - base)), static_cast<float>(m.y + axis.y * (d - base))};
    };

    DrawLineEx(onAxis(base - 450.0), onAxis(base + 450.0), 1.0f, Fade(kDim, 0.6f));

    const std::vector<Point>* hulls[2] = {&hullA, &hullB};
    const Color colors[2] = {colorA, colorB};
    for (int h = 0; h < 2; ++h) {
        for (const Point& p : *hulls[h]) {
            const double d = static_cast<double>(p.x) * axis.x + static_cast<double>(p.y) * axis.y;
            DrawLineEx(toV(p), onAxis(d), 1.0f, Fade(colors[h], 0.25f));
        }
        double lo, hi;
        project(*hulls[h], axis, lo, hi);
        DrawLineEx(onAxis(lo), onAxis(hi), 7.0f, colors[h]);
    }
}

// ---------------------------------------------------------------------------

struct Drag {
    int shape = -1;  // index into z-order, -1 when nothing is dragged
    Vector2 grab{0, 0};
};

}  // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kWidth, kHeight, "Hull collision demo: Graham's Scan and SAT");
    SetWindowMinSize(640, 360);
    SetTargetFPS(60);
    loadUiFont();

    Shape star = makeStar();
    Shape ell = makeL();

    auto reset = [&]() {
        star.pos = {250, 420};
        ell.pos = {700, 220};
    };
    auto falsePositive = [&]() {
        // One tip of the star sits in the notch of the L: inside the hull of the L, outside the L.
        ell.pos = {620, 150};
        star.pos = {620 + 270, 150 + 60};
    };
    reset();

    std::array<Shape*, 2> z = {&star, &ell};  // draw order, last is on top
    Drag drag;
    bool showHulls = true;
    bool showAxis = true;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) reset();
        if (IsKeyPressed(KEY_F)) falsePositive();
        if (IsKeyPressed(KEY_H)) showHulls = !showHulls;
        if (IsKeyPressed(KEY_A)) showAxis = !showAxis;

        // Dragging: pick the topmost shape under the cursor.
        const Camera2D cam = makeCamera();
        const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), cam);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 1; i >= 0; --i) {
                if (pointInPolygon(z[i]->world(), mouse.x, mouse.y)) {
                    std::swap(z[i], z[1]);
                    drag.shape = 1;
                    drag.grab = {mouse.x - static_cast<float>(z[1]->pos.x), mouse.y - static_cast<float>(z[1]->pos.y)};
                    break;
                }
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) drag.shape = -1;
        if (drag.shape >= 0) {
            z[drag.shape]->pos = {std::llround(mouse.x - drag.grab.x), std::llround(mouse.y - drag.grab.y)};
        }

        // Collision pipeline.
        const std::vector<Point> worldStar = star.world();
        const std::vector<Point> worldL = ell.world();
        const HullResult hullStar = graham(worldStar, false);
        const HullResult hullL = graham(worldL, false);
        const SatResult sat = satTest(hullStar.hull, hullL.hull);
        const bool exact = polygonsOverlap(worldStar, worldL);

        BeginDrawing();
        ClearBackground(kBackground);

        BeginMode2D(cam);

        for (Shape* s : z) drawShape(*s);

        if (showHulls) {
            const Color hullColor = sat.overlap ? kHit : kOk;
            drawHull(hullStar.hull, hullColor);
            drawHull(hullL.hull, hullColor);
            if (!sat.overlap && showAxis) drawSatAxis(hullStar.hull, hullL.hull, sat.axis, star.edge, ell.edge);
        }

        int y = 20;
        drawText("Broad phase: SAT on convex hulls", 20, y, 22, kText);
        y += 30;
        drawText(sat.overlap ? "Hulls: OVERLAP" : "Hulls: separated", 20, y, 22, sat.overlap ? kHit : kOk);
        y += 28;
        drawText(exact ? "Exact shapes: OVERLAP" : "Exact shapes: separated", 20, y, 22, exact ? kHit : kOk);
        y += 34;
        if (sat.overlap && !exact) {
            drawText("False positive: the hulls overlap, the shapes do not.", 20, y, 20, kWarn);
        } else if (sat.overlap && exact) {
            drawText("Real collision. An exact test would confirm it.", 20, y, 20, kWarn);
        } else {
            drawText("A separating axis exists. The exact test is skipped.", 20, y, 20, kDim);
        }
        y += 40;
        drawText(TextFormat("Star hull: %d vertices, %lld orientation tests", static_cast<int>(hullStar.hull.size()),
                            hullStar.tests),
                 20, y, 18, kDim);
        y += 24;
        drawText(TextFormat("L hull: %d vertices, %lld orientation tests", static_cast<int>(hullL.hull.size()),
                            hullL.tests),
                 20, y, 18, kDim);

        drawText("Drag shapes with the mouse   F: false-positive position   R: reset   H: hulls   A: SAT axis   ESC: quit",
                 20, kHeight - 28, 16, kDim);

        EndMode2D();
        EndDrawing();
    }

    if (gFontLoaded) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
