// Visualizer for Jarvis' March and Graham's Scan.
//
// Modes (keys 1-4):
//   1  Jarvis' March, step by step
//   2  Graham's Scan, step by step
//   3  Race: both algorithms on the same points, advanced by the same number
//      of orientation tests
//   4  Chart: orientation tests against n for circle (h = n) and random points
//
// The algorithms run first and record snapshots. This file only replays them.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "raylib.h"

#include "generators.h"
#include "geometry.h"
#include "graham.h"
#include "jarvis.h"

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr double kWorldMin = -50.0;
constexpr double kWorldSize = 1100.0;  // world square is [-50, 1050] on both axes

const Color kBackground{16, 20, 26, 255};
const Color kPanel{26, 32, 40, 255};
const Color kText{225, 230, 235, 255};
const Color kDim{130, 140, 150, 255};
const Color kGrid{50, 58, 68, 255};
const Color kPointColor{170, 180, 190, 255};
const Color kHullColor{80, 220, 130, 255};
const Color kCandidate{255, 200, 60, 255};
const Color kProbe{80, 200, 255, 255};
const Color kBad{240, 90, 90, 255};

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

int measureText(const char* text, int size) {
    return static_cast<int>(MeasureTextEx(gFont, text, static_cast<float>(size), fontSpacing(size)).x);
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
// Viewport: maps the fixed world square to a screen rectangle (y axis up).

struct Viewport {
    Rectangle area{};
    float scale = 1.0f;
    float ox = 0.0f;
    float oy = 0.0f;
};

Viewport makeViewport(Rectangle area) {
    Viewport v;
    v.area = area;
    v.scale = std::min(area.width, area.height) / static_cast<float>(kWorldSize);
    const float side = static_cast<float>(kWorldSize) * v.scale;
    v.ox = area.x + (area.width - side) / 2.0f;
    v.oy = area.y + (area.height - side) / 2.0f;
    return v;
}

float viewportSide(const Viewport& v) { return static_cast<float>(kWorldSize) * v.scale; }

Vector2 toScreen(const Viewport& v, const Point& p) {
    return {v.ox + static_cast<float>(p.x - kWorldMin) * v.scale,
            v.oy + static_cast<float>(kWorldMin + kWorldSize - p.y) * v.scale};
}

Point toWorld(const Viewport& v, Vector2 s) {
    const double wx = (s.x - v.ox) / v.scale + kWorldMin;
    const double wy = kWorldMin + kWorldSize - (s.y - v.oy) / v.scale;
    return {std::llround(wx), std::llround(wy)};
}

bool insideViewport(const Viewport& v, Vector2 s) {
    const float side = viewportSide(v);
    return s.x >= v.ox && s.x <= v.ox + side && s.y >= v.oy && s.y <= v.oy + side;
}

// ---------------------------------------------------------------------------
// Scenes (point sets). All of them fit in the world square [0, 1000]^2.

enum Scene { SceneTextbook, SceneSquare, SceneCircle, SceneGrid, SceneCustom, SceneCount };

const char* sceneName(int scene) {
    static const char* names[] = {"Textbook example (8 points)", "Random points in a square (40)",
                                  "Points on a circle (24)", "5x5 grid (collinear points)",
                                  "Custom (click to add points)"};
    return names[scene];
}

std::vector<Point> buildScene(int scene, unsigned seed, const std::vector<Point>& custom) {
    switch (scene) {
        case SceneTextbook: {
            // Hull: (1,1) (7,0) (10,4) (6,9) (0,6). Interior: (4,4) (6,5) (5,7).
            const long long raw[][2] = {{1, 1}, {7, 0}, {10, 4}, {6, 9}, {0, 6}, {4, 4}, {6, 5}, {5, 7}};
            std::vector<Point> pts;
            for (const auto& r : raw) pts.push_back({r[0] * 90 + 50, r[1] * 90 + 100});
            return pts;
        }
        case SceneSquare: {
            std::mt19937 rng(seed);
            std::vector<Point> pts = randomSquare(40, 900, rng);
            for (Point& p : pts) {
                p.x += 50;
                p.y += 50;
            }
            return pts;
        }
        case SceneCircle:
            return circlePoints(24, 500, 500, 450);
        case SceneGrid: {
            std::vector<Point> pts;
            for (int i = 0; i < 5; ++i)
                for (int j = 0; j < 5; ++j) pts.push_back({100 + 200 * i, 100 + 200 * j});
            return pts;
        }
        default:
            return custom;
    }
}

// ---------------------------------------------------------------------------
// Text helpers

int drawWrapped(const std::string& text, int x, int y, int maxWidth, int size, Color color) {
    std::vector<std::string> words;
    {
        std::string w;
        for (char ch : text) {
            if (ch == ' ') {
                if (!w.empty()) words.push_back(w);
                w.clear();
            } else {
                w += ch;
            }
        }
        if (!w.empty()) words.push_back(w);
    }
    std::string line;
    for (const std::string& w : words) {
        const std::string trial = line.empty() ? w : line + " " + w;
        if (!line.empty() && measureText(trial.c_str(), size) > maxWidth) {
            drawText(line.c_str(), x, y, size, color);
            y += size + 4;
            line = w;
        } else {
            line = trial;
        }
    }
    if (!line.empty()) {
        drawText(line.c_str(), x, y, size, color);
        y += size + 4;
    }
    return y;
}

int drawLine(const std::string& text, int x, int y, int size, Color color) {
    drawText(text.c_str(), x, y, size, color);
    return y + size + 6;
}

// ---------------------------------------------------------------------------
// Scene drawing

void drawScene(const Viewport& vp, const std::vector<Point>& pts, const HullResult& res, int stepIdx,
               bool isGraham, const char* title) {
    const float side = viewportSide(vp);
    DrawRectangleLinesEx({vp.ox, vp.oy, side, side}, 1.0f, kGrid);
    if (title != nullptr) drawText(title, static_cast<int>(vp.ox), static_cast<int>(vp.oy) - 26, 20, kText);

    const bool haveStep = !res.steps.empty();
    const Step* step = haveStep ? &res.steps[stepIdx] : nullptr;

    // Graham: faint rays from the pivot show the angular order.
    if (isGraham && !res.order.empty()) {
        const Vector2 pv = toScreen(vp, res.order[0]);
        for (size_t i = 1; i < res.order.size(); ++i) {
            DrawLineEx(pv, toScreen(vp, res.order[i]), 1.0f, Fade(kDim, 0.35f));
        }
    }

    // Hull (Jarvis) or stack (Graham) built so far.
    if (step != nullptr) {
        const std::vector<Point>& h = step->hull;
        for (size_t i = 1; i < h.size(); ++i) {
            DrawLineEx(toScreen(vp, h[i - 1]), toScreen(vp, h[i]), 3.0f, kHullColor);
        }
        if (step->kind == StepKind::Done && h.size() >= 3) {
            DrawLineEx(toScreen(vp, h.back()), toScreen(vp, h.front()), 3.0f, kHullColor);
        }

        // Step specific overlays.
        if (step->hasTriple) {
            const Vector2 a = toScreen(vp, step->a);
            const Vector2 b = toScreen(vp, step->b);
            const Vector2 c = toScreen(vp, step->c);
            if (!isGraham) {
                DrawLineEx(a, b, 3.0f, kCandidate);
                DrawLineEx(a, c, 2.0f, kProbe);
                const Color probeColor = (step->kind == StepKind::Update) ? kHullColor : kBad;
                DrawRing(b, 8.0f, 11.0f, 0.0f, 360.0f, 24, kCandidate);
                DrawCircleV(c, 7.0f, probeColor);
            } else {
                DrawLineEx(a, b, 3.0f, kCandidate);
                DrawLineEx(b, c, 3.0f, step->cross > 0 ? kHullColor : kBad);
                DrawRing(c, 8.0f, 11.0f, 0.0f, 360.0f, 24, kProbe);
            }
        } else if (step->kind == StepKind::Pop) {
            DrawRing(toScreen(vp, step->b), 8.0f, 12.0f, 0.0f, 360.0f, 24, kBad);
            DrawRing(toScreen(vp, step->c), 8.0f, 11.0f, 0.0f, 360.0f, 24, kProbe);
        } else if (step->kind == StepKind::Push) {
            DrawRing(toScreen(vp, step->c), 8.0f, 11.0f, 0.0f, 360.0f, 24, kHullColor);
        }
    }

    // Points.
    for (const Point& p : pts) DrawCircleV(toScreen(vp, p), 4.0f, kPointColor);
    if (step != nullptr) {
        for (const Point& p : step->hull) DrawCircleV(toScreen(vp, p), 6.0f, kHullColor);
        if (!step->hull.empty() && step->kind != StepKind::Done) {
            DrawRing(toScreen(vp, step->hull.back()), 8.0f, 10.0f, 0.0f, 360.0f, 24, kHullColor);
        }
    }

    // Graham: show the sorted rank next to each point.
    if (isGraham && !res.order.empty()) {
        for (size_t i = 0; i < res.order.size(); ++i) {
            const Vector2 s = toScreen(vp, res.order[i]);
            drawText(TextFormat("%d", static_cast<int>(i)), static_cast<int>(s.x) + 8, static_cast<int>(s.y) - 18,
                     16, kDim);
        }
    }
}

// ---------------------------------------------------------------------------
// Application state

enum class Mode { Jarvis, Graham, Race, Chart };

const double kStepSpeeds[] = {1, 2, 4, 8, 16, 32, 64};
const double kRaceSpeeds[] = {2, 5, 10, 20, 50, 100, 200, 400};
constexpr int kStepSpeedCount = sizeof(kStepSpeeds) / sizeof(kStepSpeeds[0]);
constexpr int kRaceSpeedCount = sizeof(kRaceSpeeds) / sizeof(kRaceSpeeds[0]);

struct App {
    Mode mode = Mode::Jarvis;
    int scene = SceneTextbook;
    unsigned seed = 7;
    std::vector<Point> custom;

    std::vector<Point> pts;
    HullResult jarvisRes;
    HullResult grahamRes;

    int stepIdx = 0;      // modes 1 and 2
    double budget = 0.0;  // mode 3: orientation tests available to each algorithm
    bool playing = false;
    double timer = 0.0;
    int stepSpeed = 2;
    int raceSpeed = 3;
    bool showHelp = true;

    std::vector<BenchRow> bench;  // mode 4, computed on first use

    float leftHold = 0.0f;
    float rightHold = 0.0f;
};

void rebuild(App& app) {
    app.pts = buildScene(app.scene, app.seed, app.custom);
    app.jarvisRes = jarvis(app.pts, true);
    app.grahamRes = graham(app.pts, true);
    app.stepIdx = 0;
    app.budget = 0.0;
    app.playing = false;
    app.timer = 0.0;
}

const HullResult& activeResult(const App& app) {
    return app.mode == Mode::Graham ? app.grahamRes : app.jarvisRes;
}

int lastStep(const HullResult& r) { return static_cast<int>(r.steps.size()) - 1; }

// First step that has used at least t orientation tests (the last step if none).
int stepAtTests(const HullResult& r, long long t) {
    auto it = std::lower_bound(r.steps.begin(), r.steps.end(), t,
                               [](const Step& s, long long v) { return s.tests < v; });
    if (it == r.steps.end()) return lastStep(r);
    return static_cast<int>(it - r.steps.begin());
}

bool repeatKey(int key, float& hold, float dt) {
    if (IsKeyPressed(key)) {
        hold = 0.35f;
        return true;
    }
    if (IsKeyDown(key)) {
        hold -= dt;
        if (hold <= 0.0f) {
            hold = 0.05f;
            return true;
        }
    }
    return false;
}

void setMode(App& app, Mode mode) {
    app.mode = mode;
    app.stepIdx = 0;
    app.budget = 0.0;
    app.playing = false;
    app.timer = 0.0;
    if (mode == Mode::Chart && app.bench.empty()) {
        app.bench = runBenchmark({10, 20, 50, 100, 200, 500, 1000, 2000});
    }
}

void handleInput(App& app, float dt, Vector2 mouse) {
    if (IsKeyPressed(KEY_ONE)) setMode(app, Mode::Jarvis);
    if (IsKeyPressed(KEY_TWO)) setMode(app, Mode::Graham);
    if (IsKeyPressed(KEY_THREE)) setMode(app, Mode::Race);
    if (IsKeyPressed(KEY_FOUR)) setMode(app, Mode::Chart);
    if (IsKeyPressed(KEY_H)) app.showHelp = !app.showHelp;

    if (app.mode == Mode::Chart) return;

    // Scene selection.
    if (IsKeyPressed(KEY_TAB)) {
        const bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        app.scene = (app.scene + (back ? SceneCount - 1 : 1)) % SceneCount;
        rebuild(app);
    }
    if (IsKeyPressed(KEY_N) && app.scene == SceneSquare) {
        ++app.seed;
        rebuild(app);
    }
    if (IsKeyPressed(KEY_C)) {
        app.scene = SceneCustom;
        app.custom.clear();
        rebuild(app);
    }

    // Custom scene: left click adds a point, right click removes the last one.
    if (app.scene == SceneCustom && app.mode != Mode::Race) {
        const Viewport vp = makeViewport({20, 20, 880, 680});
        const Vector2 m = mouse;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && insideViewport(vp, m) && app.custom.size() < 80) {
            app.custom.push_back(toWorld(vp, m));
            rebuild(app);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !app.custom.empty()) {
            app.custom.pop_back();
            rebuild(app);
        }
    }

    // Playback.
    const HullResult& res = activeResult(app);
    const long long raceEnd = std::max(app.jarvisRes.tests, app.grahamRes.tests);

    if (IsKeyPressed(KEY_SPACE)) {
        if (!app.playing) {
            const bool atEnd = (app.mode == Mode::Race) ? (app.budget > static_cast<double>(raceEnd))
                                                        : (app.stepIdx >= lastStep(res));
            if (atEnd) {
                app.stepIdx = 0;
                app.budget = 0.0;
            }
        }
        app.playing = !app.playing;
        app.timer = 0.0;
    }
    if (IsKeyPressed(KEY_HOME)) {
        app.stepIdx = 0;
        app.budget = 0.0;
        app.playing = false;
    }
    if (IsKeyPressed(KEY_END)) {
        app.stepIdx = std::max(0, lastStep(res));
        app.budget = static_cast<double>(raceEnd) + 1.0;
        app.playing = false;
    }
    if (repeatKey(KEY_RIGHT, app.rightHold, dt)) {
        app.playing = false;
        if (app.mode == Mode::Race) {
            app.budget = std::min(std::floor(app.budget) + 1.0, static_cast<double>(raceEnd) + 1.0);
        } else {
            app.stepIdx = std::min(app.stepIdx + 1, std::max(0, lastStep(res)));
        }
    }
    if (repeatKey(KEY_LEFT, app.leftHold, dt)) {
        app.playing = false;
        if (app.mode == Mode::Race) {
            app.budget = std::max(std::ceil(app.budget) - 1.0, 0.0);
        } else {
            app.stepIdx = std::max(app.stepIdx - 1, 0);
        }
    }
    if (IsKeyPressed(KEY_UP)) {
        if (app.mode == Mode::Race) app.raceSpeed = std::min(app.raceSpeed + 1, kRaceSpeedCount - 1);
        else app.stepSpeed = std::min(app.stepSpeed + 1, kStepSpeedCount - 1);
    }
    if (IsKeyPressed(KEY_DOWN)) {
        if (app.mode == Mode::Race) app.raceSpeed = std::max(app.raceSpeed - 1, 0);
        else app.stepSpeed = std::max(app.stepSpeed - 1, 0);
    }
}

void update(App& app, float dt) {
    if (!app.playing) return;
    if (app.mode == Mode::Race) {
        app.budget += kRaceSpeeds[app.raceSpeed] * dt;
        const double end = static_cast<double>(std::max(app.jarvisRes.tests, app.grahamRes.tests));
        if (app.budget > end) {
            app.budget = end + 1.0;
            app.playing = false;
        }
    } else if (app.mode == Mode::Jarvis || app.mode == Mode::Graham) {
        const HullResult& res = activeResult(app);
        const double interval = 1.0 / kStepSpeeds[app.stepSpeed];
        app.timer += dt;
        while (app.timer >= interval) {
            app.timer -= interval;
            if (app.stepIdx < lastStep(res)) ++app.stepIdx;
            if (app.stepIdx >= lastStep(res)) {
                app.playing = false;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Panels

const char* turnName(long long c) { return c > 0 ? "left turn" : (c < 0 ? "right turn" : "collinear"); }

int drawControls(int x, int y, bool race, bool chart, bool showHelp) {
    if (!showHelp) return drawLine("H: show controls", x, y, 16, kDim);
    y = drawLine("Controls", x, y, 18, kText);
    y = drawLine("1 Jarvis  2 Graham  3 Race  4 Chart", x, y, 16, kDim);
    if (!chart) {
        y = drawLine("SPACE play / pause", x, y, 16, kDim);
        y = drawLine("LEFT / RIGHT step back / forward", x, y, 16, kDim);
        y = drawLine(race ? "UP / DOWN tests per second" : "UP / DOWN steps per second", x, y, 16, kDim);
        y = drawLine("HOME / END first / last", x, y, 16, kDim);
        y = drawLine("TAB next point set (SHIFT: previous)", x, y, 16, kDim);
        y = drawLine("N new random set   C custom points", x, y, 16, kDim);
    }
    y = drawLine("H hide controls   ESC quit", x, y, 16, kDim);
    return y;
}

void drawSinglePanel(const App& app) {
    const int x = 930;
    const int w = 320;
    DrawRectangle(910, 0, kWidth - 910, kHeight, kPanel);

    const bool graham = app.mode == Mode::Graham;
    const HullResult& res = activeResult(app);
    int y = 22;
    y = drawLine(graham ? "Graham's Scan" : "Jarvis' March", x, y, 28, kText);
    y = drawLine(graham ? "Time: O(n log n)" : "Time: O(n h)", x, y, 20, kCandidate);
    y = drawWrapped(sceneName(app.scene), x, y, w, 18, kDim);
    y += 6;

    const int n = static_cast<int>(uniquePoints(app.pts).size());
    y = drawLine(TextFormat("n = %d    h = %d", n, static_cast<int>(res.hull.size())), x, y, 20, kText);

    if (!res.steps.empty()) {
        const Step& s = res.steps[app.stepIdx];
        y = drawLine(TextFormat("Step %d / %d", app.stepIdx + 1, static_cast<int>(res.steps.size())), x, y, 20, kText);
        y = drawLine(TextFormat("Orientation tests: %lld", s.tests), x, y, 20, kText);
        if (graham) {
            const long long scan = std::max(0LL, s.tests - res.sortTests);
            y = drawLine(TextFormat("  sort: %lld   scan: %lld", res.sortTests, scan), x, y, 16, kDim);
        }
        y = drawLine(TextFormat("Hull vertices so far: %d", static_cast<int>(s.hull.size())), x, y, 18, kDim);
        y += 6;
        if (s.hasTriple) {
            y = drawLine(TextFormat("cross = %lld  (%s)", s.cross, turnName(s.cross)), x, y, 20, kProbe);
        }
        y = drawWrapped(s.note, x, y, w, 18, kText);
        if (app.stepIdx == lastStep(res)) {
            y += 6;
            y = drawLine(TextFormat("Done: h = %d, %lld tests", static_cast<int>(res.hull.size()), res.tests), x, y,
                         20, kHullColor);
        }
    }

    y += 10;
    if (!graham) {
        y = drawLine("yellow: candidate edge", x, y, 16, kCandidate);
        y = drawLine("cyan: probe point", x, y, 16, kProbe);
        y = drawLine("green dot: probe becomes candidate", x, y, 16, kHullColor);
        y = drawLine("red dot: probe rejected", x, y, 16, kBad);
    } else {
        y = drawLine("numbers: order after the sort", x, y, 16, kDim);
        y = drawLine("yellow: top two stack points", x, y, 16, kCandidate);
        y = drawLine("green edge: left turn, keep", x, y, 16, kHullColor);
        y = drawLine("red edge: not a left turn, pop", x, y, 16, kBad);
    }
    y += 10;
    drawControls(x, y, false, false, app.showHelp);
}

void drawRacePanel(const App& app) {
    const int x = 930;
    DrawRectangle(910, 0, kWidth - 910, kHeight, kPanel);

    const long long budget = static_cast<long long>(std::ceil(app.budget));
    const HullResult& j = app.jarvisRes;
    const HullResult& g = app.grahamRes;
    const long long jUsed = std::min(budget, j.tests);
    const long long gUsed = std::min(budget, g.tests);

    int y = 22;
    y = drawLine("Race", x, y, 28, kText);
    y = drawWrapped("Same points. Both algorithms get the same number of orientation tests.", x, y, 320, 18, kDim);
    y += 6;
    const int n = static_cast<int>(uniquePoints(app.pts).size());
    y = drawLine(TextFormat("n = %d    h = %d", n, static_cast<int>(j.hull.size())), x, y, 20, kText);
    y = drawLine(TextFormat("Budget: %lld tests", budget), x, y, 20, kProbe);
    y += 8;

    y = drawLine("Jarvis' March", x, y, 22, kText);
    y = drawLine(TextFormat("%lld tests used", jUsed), x, y, 20, kText);
    if (budget >= j.tests) y = drawLine(TextFormat("finished (%lld tests)", j.tests), x, y, 18, kHullColor);
    y += 8;

    y = drawLine("Graham's Scan", x, y, 22, kText);
    y = drawLine(TextFormat("%lld tests used", gUsed), x, y, 20, kText);
    if (budget < g.sortTests) {
        y = drawLine(TextFormat("sorting by angle: %lld / %lld", gUsed, g.sortTests), x, y, 18, kCandidate);
    } else if (budget >= g.tests) {
        y = drawLine(TextFormat("finished (%lld tests)", g.tests), x, y, 18, kHullColor);
    } else {
        y = drawLine(TextFormat("scanning (sort cost %lld)", g.sortTests), x, y, 18, kDim);
    }

    y += 14;
    drawControls(x, y, true, false, app.showHelp);
}

// ---------------------------------------------------------------------------
// Chart (mode 4)

struct Series {
    const char* name;
    Color color;
    std::vector<double> values;
};

void drawChart(const App& app) {
    const Rectangle area{20, 20, 880, 680};
    const float left = area.x + 90;
    const float right = area.x + area.width - 20;
    const float top = area.y + 40;
    const float bottom = area.y + area.height - 60;

    const std::vector<BenchRow>& rows = app.bench;
    std::vector<Series> series = {
        {"Jarvis, circle (h = n)", kBad, {}},
        {"Graham, circle (h = n)", kCandidate, {}},
        {"Jarvis, random square", kProbe, {}},
        {"Graham, random square", kHullColor, {}},
    };
    double maxV = 1.0;
    double minV = 1e18;
    for (const BenchRow& r : rows) {
        series[0].values.push_back(static_cast<double>(r.jarvisCircle));
        series[1].values.push_back(static_cast<double>(r.grahamCircle));
        series[2].values.push_back(static_cast<double>(r.jarvisSquare));
        series[3].values.push_back(static_cast<double>(r.grahamSquare));
    }
    for (const Series& s : series) {
        for (double v : s.values) {
            maxV = std::max(maxV, v);
            minV = std::min(minV, v);
        }
    }

    const double yLo = std::floor(std::log10(minV));
    const double yHi = std::ceil(std::log10(maxV));
    const double xLo = std::log10(static_cast<double>(rows.front().n));
    const double xHi = std::log10(static_cast<double>(rows.back().n));

    auto px = [&](double n) { return left + static_cast<float>((std::log10(n) - xLo) / (xHi - xLo)) * (right - left); };
    auto py = [&](double v) {
        return bottom - static_cast<float>((std::log10(v) - yLo) / (yHi - yLo)) * (bottom - top);
    };

    drawText("Orientation tests (log scale)", static_cast<int>(area.x), static_cast<int>(area.y) + 4, 20, kText);

    for (int k = static_cast<int>(yLo); k <= static_cast<int>(yHi); ++k) {
        const float yy = py(std::pow(10.0, k));
        DrawLineEx({left, yy}, {right, yy}, 1.0f, kGrid);
        drawText(TextFormat("10^%d", k), static_cast<int>(area.x) + 20, static_cast<int>(yy) - 8, 16, kDim);
    }
    for (const BenchRow& r : rows) {
        const float xx = px(r.n);
        DrawLineEx({xx, top}, {xx, bottom}, 1.0f, kGrid);
        drawText(TextFormat("%d", r.n), static_cast<int>(xx) - 14, static_cast<int>(bottom) + 8, 16, kDim);
    }
    drawText("number of points n (log scale)", static_cast<int>(left) + 220, static_cast<int>(bottom) + 34, 18, kDim);

    for (const Series& s : series) {
        for (size_t i = 0; i < rows.size(); ++i) {
            const Vector2 p{px(rows[i].n), py(s.values[i])};
            if (i > 0) DrawLineEx({px(rows[i - 1].n), py(s.values[i - 1])}, p, 3.0f, s.color);
            DrawCircleV(p, 5.0f, s.color);
        }
    }

    // Panel
    DrawRectangle(910, 0, kWidth - 910, kHeight, kPanel);
    const int x = 930;
    int y = 22;
    y = drawLine("Complexity", x, y, 28, kText);
    y = drawWrapped("Counted orientation tests, not time. On a log-log chart a slope of 2 means n^2 and a slope "
                    "close to 1 means n log n.",
                    x, y, 320, 18, kDim);
    y += 8;
    const BenchRow& last = rows.back();
    for (size_t i = 0; i < series.size(); ++i) {
        y = drawLine(series[i].name, x, y, 18, series[i].color);
        y = drawLine(TextFormat("  n = %d: %lld tests", last.n, static_cast<long long>(series[i].values.back())), x, y,
                     16, kDim);
    }
    y += 6;
    y = drawLine(TextFormat("h at n = %d: circle %d, square %d", last.n, last.hCircle, last.hSquare), x, y, 16, kText);
    y += 10;
    drawControls(x, y, false, true, app.showHelp);
}

// ---------------------------------------------------------------------------

void draw(const App& app) {
    switch (app.mode) {
        case Mode::Jarvis:
        case Mode::Graham: {
            const bool graham = app.mode == Mode::Graham;
            const Viewport vp = makeViewport({20, 20, 880, 680});
            const HullResult& res = activeResult(app);
            drawScene(vp, app.pts, res, app.stepIdx, graham, nullptr);
            drawSinglePanel(app);
            if (app.scene == SceneCustom && app.custom.empty()) {
                drawText("Left click: add a point    Right click: remove the last point", 60, 350, 22, kDim);
            }
            break;
        }
        case Mode::Race: {
            const Viewport left = makeViewport({20, 50, 435, 640});
            const Viewport right = makeViewport({465, 50, 435, 640});
            const long long t = static_cast<long long>(std::ceil(app.budget));
            drawScene(left, app.pts, app.jarvisRes, stepAtTests(app.jarvisRes, t), false, "Jarvis' March");
            drawScene(right, app.pts, app.grahamRes, stepAtTests(app.grahamRes, t), true, "Graham's Scan");
            drawRacePanel(app);
            break;
        }
        case Mode::Chart:
            drawChart(app);
            break;
    }
}

}  // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kWidth, kHeight, "Convex hull: Jarvis' March and Graham's Scan");
    SetWindowMinSize(640, 360);
    SetTargetFPS(60);
    loadUiFont();

    App app;
    rebuild(app);

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const Camera2D cam = makeCamera();
        handleInput(app, dt, GetScreenToWorld2D(GetMousePosition(), cam));
        update(app, dt);

        BeginDrawing();
        ClearBackground(kBackground);
        BeginMode2D(cam);
        draw(app);
        EndMode2D();
        EndDrawing();
    }

    if (gFontLoaded) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
