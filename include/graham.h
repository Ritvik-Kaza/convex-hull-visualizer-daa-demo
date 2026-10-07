#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "geometry.h"

// Graham's Scan. Time O(n log n): the sort dominates, the scan is O(n).
//
// Conventions (same as jarvis()):
//   - duplicate points are removed first
//   - collinear points on a hull edge are not hull vertices
//   - fewer than 3 distinct points: the distinct points are returned as they are
//   - the hull is returned counterclockwise, starting at the pivot
//     (lowest point, leftmost on ties)
//
// The comparisons made while sorting are counted as orientation tests, so the
// counts are comparable with Jarvis' March. res.sortTests holds the share
// spent in the sort.
inline HullResult graham(const std::vector<Point>& input, bool record = true) {
    HullResult res;
    resetTestCounter();

    std::vector<Point> pts = uniquePoints(input);
    const int n = static_cast<int>(pts.size());

    auto snap = [&](StepKind kind, const std::vector<Point>& stack, bool hasTriple,
                    const Point& a, const Point& b, const Point& c, long long cr,
                    const std::string& note) {
        Step s;
        s.kind = kind;
        s.hull = stack;
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
        res.order = pts;
        if (record) snap(StepKind::Done, pts, false, {}, {}, {}, 0, "Fewer than 3 distinct points: nothing to scan.");
        return res;
    }

    // Pivot: lowest y, leftmost on ties. Every other point then has a polar
    // angle in [0, pi) around it, so comparing angles with a cross product works.
    auto pivotIt = std::min_element(pts.begin(), pts.end(), [](const Point& a, const Point& b) {
        return a.y < b.y || (a.y == b.y && a.x < b.x);
    });
    std::iter_swap(pts.begin(), pivotIt);
    const Point pivot = pts[0];

    // Sort by polar angle around the pivot. Points with the same angle are
    // ordered by distance, nearest first.
    std::sort(pts.begin() + 1, pts.end(), [&](const Point& a, const Point& b) {
        const long long c = orientation(pivot, a, b);
        if (c != 0) return c > 0;
        return dist2(pivot, a) < dist2(pivot, b);
    });
    res.sortTests = testCounter();
    res.order = pts;

    std::vector<Point> stack;
    stack.push_back(pts[0]);
    if (record) snap(StepKind::Init, stack, false, {}, {}, {}, 0,
                     "Pivot is the lowest point. The other points are sorted by polar angle around it.");

    for (int i = 1; i < n; ++i) {
        const Point c = pts[i];

        // Pop while the last two stack points and c do not make a left turn.
        while (stack.size() >= 2) {
            const Point a = stack[stack.size() - 2];
            const Point b = stack.back();
            const long long cr = orientation(a, b, c);

            if (record) {
                std::string note;
                if (cr > 0) {
                    note = "Left turn. The top of the stack stays.";
                } else if (cr < 0) {
                    note = "Right turn. The top of the stack is not on the hull: pop it.";
                } else {
                    note = "Collinear. The top of the stack is not a vertex: pop it.";
                }
                snap(StepKind::Test, stack, true, a, b, c, cr, note);
            }

            if (cr > 0) break;

            stack.pop_back();
            if (record) snap(StepKind::Pop, stack, false, {}, b, c, 0, "Popped.");
        }

        stack.push_back(c);
        if (record) snap(StepKind::Push, stack, false, {}, {}, c, 0, "Push the point onto the stack.");
    }

    if (record) snap(StepKind::Done, stack, false, {}, {}, {}, 0, "All points processed. The stack is the hull.");

    res.hull = stack;
    res.tests = testCounter();
    return res;
}
