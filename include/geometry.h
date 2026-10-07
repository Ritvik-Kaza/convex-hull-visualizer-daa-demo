#pragma once

#include <algorithm>
#include <string>
#include <vector>

// Points use integer coordinates so that every orientation test is exact.
struct Point {
    long long x = 0;
    long long y = 0;
};

inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Point& a, const Point& b) { return !(a == b); }

// Lexicographic order: by x, then by y.
inline bool operator<(const Point& a, const Point& b) {
    return a.x < b.x || (a.x == b.x && a.y < b.y);
}

// Cross product of (a - o) and (b - o).
//   > 0 : o -> a -> b turns left (counterclockwise)
//   < 0 : o -> a -> b turns right (clockwise)
//   = 0 : o, a and b are collinear
inline long long cross(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

inline long long dist2(const Point& a, const Point& b) {
    const long long dx = a.x - b.x;
    const long long dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Global counter of orientation tests, used to compare the algorithms by
// counting work instead of measuring wall-clock time.
inline long long& testCounter() {
    static long long counter = 0;
    return counter;
}

inline void resetTestCounter() { testCounter() = 0; }

// Same as cross(), but counted. The hull algorithms call this one.
inline long long orientation(const Point& o, const Point& a, const Point& b) {
    ++testCounter();
    return cross(o, a, b);
}

// What happened in one recorded step of an algorithm. The meaning of the
// points a, b, c in Step depends on the kind:
//
//   Init, Done : no points used
//   Test       : orientation test on (a, b, c). Jarvis: a = current hull
//                vertex, b = current candidate, c = probe. Graham: a and b are
//                the two top stack points, c = the point being processed.
//   Update     : Jarvis only. Same as Test, and the probe becomes the new
//                candidate.
//   Advance    : Jarvis only. b = the vertex just added to the hull.
//   Pop        : Graham only. b = the popped point, c = the point being processed.
//   Push       : Graham only. c = the point just pushed.
enum class StepKind { Init, Test, Update, Advance, Pop, Push, Done };

struct Step {
    StepKind kind = StepKind::Init;
    std::vector<Point> hull;  // hull (Jarvis) or stack (Graham) after this step
    bool hasTriple = false;   // true for Test and Update
    Point a;
    Point b;
    Point c;
    long long cross = 0;      // value of the orientation test, if any
    long long tests = 0;      // orientation tests performed up to and including this step
    std::string note;         // short description shown by the visualizer
};

struct HullResult {
    std::vector<Point> hull;   // counterclockwise, no collinear points on edges
    std::vector<Point> order;  // Graham only: points in sorted (polar angle) order
    std::vector<Step> steps;   // empty when recording is turned off
    long long tests = 0;       // total orientation tests
    long long sortTests = 0;   // Graham only: tests spent inside the sort
};

// Sorted copy of the input with duplicates removed.
inline std::vector<Point> uniquePoints(std::vector<Point> pts) {
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    return pts;
}

// Rotates a hull so that it starts at its lexicographically smallest point.
// Used to compare hulls produced by different algorithms.
inline std::vector<Point> normalizeHull(std::vector<Point> hull) {
    if (hull.empty()) return hull;
    auto first = std::min_element(hull.begin(), hull.end());
    std::rotate(hull.begin(), first, hull.end());
    return hull;
}
