// Verifies jarvis() and graham() against a brute-force reference.
//
// Usage:
//   checker                 run all tests (default 5000 random rounds)
//   checker --rounds N      number of random rounds
//   checker --seed S        seed for the random generator
//   checker --bench         print orientation-test counts as CSV and exit

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "generators.h"
#include "geometry.h"
#include "graham.h"
#include "jarvis.h"

namespace {

bool onSegment(const Point& a, const Point& b, const Point& p) {
    return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) &&
           std::min(a.y, b.y) <= p.y && p.y <= std::max(a.y, b.y);
}

// O(n^3) reference. (i, j) is a hull edge when every other point is strictly
// left of it, or collinear and lying on the segment between i and j. Chaining
// the edges gives the strict hull in counterclockwise order.
std::vector<Point> bruteForceHull(const std::vector<Point>& input) {
    const std::vector<Point> pts = uniquePoints(input);
    const int n = static_cast<int>(pts.size());
    if (n < 3) return pts;

    std::vector<int> next(n, -1);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            bool ok = true;
            for (int k = 0; k < n && ok; ++k) {
                if (k == i || k == j) continue;
                const long long c = cross(pts[i], pts[j], pts[k]);
                if (c < 0) ok = false;
                else if (c == 0 && !onSegment(pts[i], pts[j], pts[k])) ok = false;
            }
            if (ok) next[i] = j;
        }
    }

    std::vector<Point> hull;
    int i = 0;  // pts is sorted, so index 0 is the lexicographically smallest point
    do {
        hull.push_back(pts[i]);
        i = next[i];
    } while (i > 0 && static_cast<int>(hull.size()) <= n);
    return hull;
}

// Independent sanity check used on large inputs where brute force is too slow:
// every point lies left of or on every hull edge, and the hull is strictly convex.
bool isValidHull(const std::vector<Point>& hull, const std::vector<Point>& input) {
    const std::vector<Point> pts = uniquePoints(input);
    const int h = static_cast<int>(hull.size());
    if (pts.size() < 3 || h < 3) return true;  // degenerate sizes are covered by brute force
    for (int i = 0; i < h; ++i) {
        const Point& a = hull[i];
        const Point& b = hull[(i + 1) % h];
        const Point& c = hull[(i + 2) % h];
        if (cross(a, b, c) <= 0) return false;
        for (const Point& p : pts) {
            if (cross(a, b, p) < 0) return false;
        }
    }
    return true;
}

void printPoints(const char* label, const std::vector<Point>& pts) {
    std::printf("  %s (%zu):", label, pts.size());
    for (const Point& p : pts) std::printf(" (%lld,%lld)", p.x, p.y);
    std::printf("\n");
}

struct Stats {
    long long cases = 0;
    long long failures = 0;
};

void runCase(const std::string& name, const std::vector<Point>& pts, bool useBruteForce, Stats& stats) {
    ++stats.cases;
    const std::vector<Point> jh = normalizeHull(jarvis(pts, false).hull);
    const std::vector<Point> gh = normalizeHull(graham(pts, false).hull);

    bool ok = true;
    std::vector<Point> expected;
    if (useBruteForce) {
        expected = normalizeHull(bruteForceHull(pts));
        if (jh != expected || gh != expected) ok = false;
    } else {
        if (jh != gh || !isValidHull(jh, pts) || !isValidHull(gh, pts)) ok = false;
    }

    if (!ok) {
        ++stats.failures;
        std::printf("FAIL: %s\n", name.c_str());
        printPoints("input", pts);
        if (useBruteForce) printPoints("expected", expected);
        printPoints("jarvis", jh);
        printPoints("graham", gh);
    }
}

void handMadeCases(Stats& stats) {
    runCase("empty", {}, true, stats);
    runCase("single point", {{3, 4}}, true, stats);
    runCase("two equal points", {{3, 4}, {3, 4}}, true, stats);
    runCase("two distinct points", {{0, 0}, {5, 5}}, true, stats);
    runCase("three collinear", {{0, 0}, {1, 1}, {2, 2}}, true, stats);
    runCase("triangle", {{0, 0}, {4, 0}, {0, 3}}, true, stats);
    runCase("triangle with duplicate vertices", {{0, 0}, {4, 0}, {0, 3}, {0, 0}, {4, 0}}, true, stats);
    runCase("square with centre", {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {2, 2}}, true, stats);
    runCase("square with edge midpoints",
            {{0, 0}, {2, 0}, {4, 0}, {4, 2}, {4, 4}, {2, 4}, {0, 4}, {0, 2}}, true, stats);
    runCase("horizontal line", {{0, 5}, {3, 5}, {1, 5}, {9, 5}, {-4, 5}}, true, stats);
    runCase("vertical line", {{2, 0}, {2, 3}, {2, -7}, {2, 1}}, true, stats);
    runCase("diagonal line", {{0, 0}, {5, 5}, {2, 2}, {-3, -3}}, true, stats);

    std::vector<Point> grid;
    for (int x = 0; x < 6; ++x)
        for (int y = 0; y < 6; ++y) grid.push_back({x, y});
    runCase("6x6 grid", grid, true, stats);

    runCase("circle of 24 points", circlePoints(24, 0, 0, 1000000), true, stats);
}

void randomCases(int rounds, unsigned seed, Stats& stats) {
    std::mt19937 rng(seed);
    const long long ranges[] = {1, 2, 3, 5, 10, 50, 1000, 1000000};
    for (int round = 0; round < rounds; ++round) {
        const long long range = ranges[rng() % (sizeof(ranges) / sizeof(ranges[0]))];
        const int n = static_cast<int>(rng() % 61);  // 0..60
        std::vector<Point> pts = randomSquare(n, range, rng);
        runCase("random, range " + std::to_string(range) + ", n " + std::to_string(n), pts, true, stats);
    }

    // Larger inputs: compare the two algorithms with each other and validate the hull.
    for (int round = 0; round < 50; ++round) {
        const int n = 200 + static_cast<int>(rng() % 1800);
        const long long range = (round % 2 == 0) ? 100 : 1000000;
        runCase("large random, range " + std::to_string(range) + ", n " + std::to_string(n),
                randomSquare(n, range, rng), false, stats);
    }
}

void printBenchmark() {
    const std::vector<int> sizes = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    const std::vector<BenchRow> rows = runBenchmark(sizes);
    std::printf("n,jarvis_circle,graham_circle,jarvis_square,graham_square,h_circle,h_square\n");
    for (const BenchRow& r : rows) {
        std::printf("%d,%lld,%lld,%lld,%lld,%d,%d\n", r.n, r.jarvisCircle, r.grahamCircle,
                    r.jarvisSquare, r.grahamSquare, r.hCircle, r.hSquare);
    }
}

}  // namespace

int main(int argc, char** argv) {
    int rounds = 5000;
    unsigned seed = 12345;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bench") == 0) {
            printBenchmark();
            return 0;
        } else if (std::strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else {
            std::fprintf(stderr, "Usage: %s [--rounds N] [--seed S] [--bench]\n", argv[0]);
            return 2;
        }
    }

    Stats stats;
    handMadeCases(stats);
    randomCases(rounds, seed, stats);

    std::printf("%lld cases checked, %lld failures\n", stats.cases, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
