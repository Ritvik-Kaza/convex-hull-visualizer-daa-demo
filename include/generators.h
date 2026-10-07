#pragma once

#include <cmath>
#include <random>
#include <vector>

#include "geometry.h"
#include "graham.h"
#include "jarvis.h"

// n points drawn uniformly from the integer square [0, side] x [0, side].
// For random points h is small (about log n), which is Jarvis' best case.
inline std::vector<Point> randomSquare(int n, long long side, std::mt19937& rng) {
    std::uniform_int_distribution<long long> dist(0, side);
    std::vector<Point> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) pts.push_back({dist(rng), dist(rng)});
    return pts;
}

// n points on a circle, rounded to integers. With a large radius all n points
// stay on the hull (h = n), which is Jarvis' worst case.
inline std::vector<Point> circlePoints(int n, long long cx, long long cy, long long radius) {
    const double pi = 3.14159265358979323846;
    std::vector<Point> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double angle = 2.0 * pi * i / n;
        pts.push_back({cx + std::llround(radius * std::cos(angle)),
                       cy + std::llround(radius * std::sin(angle))});
    }
    return pts;
}

struct BenchRow {
    int n = 0;
    long long jarvisCircle = 0;
    long long grahamCircle = 0;
    long long jarvisSquare = 0;
    long long grahamSquare = 0;
    int hCircle = 0;
    int hSquare = 0;
};

// Counts orientation tests for both algorithms on the two input families.
// The radius is large enough that rounding never pushes a circle point off the hull.
inline std::vector<BenchRow> runBenchmark(const std::vector<int>& sizes, unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::vector<BenchRow> rows;
    for (int n : sizes) {
        BenchRow row;
        row.n = n;

        const std::vector<Point> circle = circlePoints(n, 0, 0, 100000000LL);
        const HullResult jc = jarvis(circle, false);
        const HullResult gc = graham(circle, false);
        row.jarvisCircle = jc.tests;
        row.grahamCircle = gc.tests;
        row.hCircle = static_cast<int>(jc.hull.size());

        const std::vector<Point> square = randomSquare(n, 1000000, rng);
        const HullResult js = jarvis(square, false);
        const HullResult gs = graham(square, false);
        row.jarvisSquare = js.tests;
        row.grahamSquare = gs.tests;
        row.hSquare = static_cast<int>(js.hull.size());

        rows.push_back(row);
    }
    return rows;
}
