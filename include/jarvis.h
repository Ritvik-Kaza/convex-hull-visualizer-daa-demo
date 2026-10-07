#pragma once

#include <string>
#include <vector>

#include "geometry.h"

// Jarvis' March (gift wrapping). Time O(n h) for n points and h hull vertices.
//
// Conventions:
//   - duplicate points are removed first
//   - collinear points on a hull edge are not hull vertices
//   - fewer than 3 distinct points: the distinct points are returned as they are
//   - the hull is returned counterclockwise, starting at the leftmost point
//
// With record == false no snapshots are stored, which keeps the large
// benchmark runs fast.
inline HullResult jarvis(const std::vector<Point>& input, bool record = true) {
    HullResult res;
    resetTestCounter();

    const std::vector<Point> pts = uniquePoints(input);
    const int n = static_cast<int>(pts.size());

    auto snap = [&](StepKind kind, const std::vector<Point>& hull, bool hasTriple,
                    const Point& a, const Point& b, const Point& c, long long cr,
                    const std::string& note) {
        Step s;
        s.kind = kind;
        s.hull = hull;
        s.hasTriple = hasTriple;
        s.a = a;
        s.b = b;
        s.c = c;
        s.cross = cr;
        s.tests = testCounter();
        s.note = note;
        res.steps.push_back(std::move(s));
    };

    if (n < 3) {
        res.hull = pts;
        if (record) snap(StepKind::Done, pts, false, {}, {}, {}, 0, "Fewer than 3 distinct points: nothing to wrap.");
        return res;
    }

    // pts is sorted lexicographically, so index 0 is the leftmost point
    // (lowest on ties). It is always a hull vertex.
    const int start = 0;
    int p = start;

    std::vector<Point> hull;
    hull.push_back(pts[p]);
    if (record) snap(StepKind::Init, hull, false, {}, {}, {}, 0, "Start at the leftmost point (lowest on ties). It is always on the hull.");

    for (int iter = 0; iter < n; ++iter) {
        int q = (p + 1) % n;  // initial candidate for the next hull vertex

        for (int r = 0; r < n; ++r) {
            if (r == p || r == q) continue;

            const long long c = orientation(pts[p], pts[q], pts[r]);
            bool update = false;
            if (c < 0) {
                update = true;  // probe is right of p->q: it is more clockwise
            } else if (c == 0 && dist2(pts[p], pts[r]) > dist2(pts[p], pts[q])) {
                update = true;  // collinear: keep the farthest point
            }

            if (record) {
                std::string note;
                if (c < 0) {
                    note = "Probe is right of the candidate edge. It becomes the new candidate.";
                } else if (c > 0) {
                    note = "Probe is left of the candidate edge. Keep the candidate.";
                } else if (update) {
                    note = "Collinear and farther away. Probe becomes the new candidate.";
                } else {
                    note = "Collinear and closer. Keep the candidate.";
                }
                snap(update ? StepKind::Update : StepKind::Test, hull, true,
                     pts[p], pts[q], pts[r], c, note);
            }

            if (update) q = r;
        }

        if (q == start) break;  // wrapped all the way around

        hull.push_back(pts[q]);
        if (record) snap(StepKind::Advance, hull, false, pts[p], pts[q], {}, 0,
                         "No point is right of the candidate edge. Add it to the hull.");
        p = q;
    }

    if (record) snap(StepKind::Done, hull, false, {}, {}, {}, 0, "Back at the start. The hull is complete.");

    res.hull = hull;
    res.tests = testCounter();
    return res;
}
