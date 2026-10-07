# Convex Hull Visualizer

C++17 implementations of Jarvis' March and Graham's Scan, with a step-by-step visualizer built on raylib and a collision broad-phase demo that uses the computed hulls.

Developed for the course Design and Analysis of Algorithms (BCSE204L), Module 5.

## Contents

| Path | Description |
| --- | --- |
| `include/geometry.h` | Point type, cross product, orientation-test counter, step snapshot types |
| `include/jarvis.h` | Jarvis' March, O(nh) |
| `include/graham.h` | Graham's Scan, O(n log n) |
| `include/generators.h` | Point set generators and the shared benchmark |
| `src/checker.cpp` | Verification against a brute-force reference, benchmark CSV output |
| `src/viz.cpp` | Step-by-step replay, side-by-side race, complexity chart |
| `src/demo.cpp` | Draggable concave shapes, hull computation, SAT overlap test |

## Requirements

- A C++17 compiler (g++ or clang++)
- make
- [raylib](https://www.raylib.com/) 5.x (only for `viz` and `demo`)
- pkg-config (optional, used by the Makefile to locate raylib)
- A TTF font such as DejaVu Sans (optional, for smooth text; raylib's built-in bitmap font is used otherwise)

## Build

```
make            # builds checker, viz and demo into bin/
make checker    # builds only the checker (no raylib needed)
make clean
```

## Usage

### Checker

```
./bin/checker                 # run all tests
./bin/checker --rounds 20000  # more random rounds
./bin/checker --seed 42       # different random seed
./bin/checker --bench         # orientation-test counts as CSV
```

Both algorithms are compared against an O(n^3) brute-force hull on hand-made degenerate cases and random inputs, and against each other on larger inputs. The process exits with a non-zero status on any mismatch.

### Visualizer

```
./bin/viz
```

The window is resizable. The drawing is scaled to fit the window size, so it stays sharp at any size.

| Key | Action |
| --- | --- |
| 1 / 2 | Jarvis' March / Graham's Scan, step by step |
| 3 | Race: both algorithms advance by the same number of orientation tests |
| 4 | Chart of orientation tests against n |
| Space | Play or pause |
| Left / Right | Step back or forward |
| Up / Down | Change playback speed |
| Home / End | Jump to first or last step |
| Tab | Next point set (Shift+Tab: previous) |
| N | New random point set |
| C | Custom point set: left click adds a point, right click removes the last one |
| H | Show or hide the controls |
| Esc | Quit |

### Collision demo

```
./bin/demo
```

Drag the two shapes with the mouse. Each frame the convex hull of both shapes is computed with Graham's Scan, and the hulls are tested for overlap with the Separating Axis Theorem. An exact test on the concave outlines is shown for comparison.

| Key | Action |
| --- | --- |
| F | Move the shapes to a hull false-positive position |
| R | Reset positions |
| H | Show or hide the hulls |
| A | Show or hide the separating axis |
| Esc | Quit |

## Conventions

- Coordinates are integers, so every orientation test is exact.
- Duplicate points are removed before processing.
- Collinear points on a hull edge are not hull vertices.
- With fewer than 3 distinct points the distinct points are returned unchanged.
- Hulls are returned in counterclockwise order.
- In Graham's Scan the comparisons made by the sort are counted as orientation tests, so the counts are comparable with Jarvis' March.

## References

- T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009. Chapter 33.
- R. L. Graham, "An efficient algorithm for determining the convex hull of a finite planar set", *Information Processing Letters* 1(4), 1972.
- R. A. Jarvis, "On the identification of the convex hull of a finite set of points in the plane", *Information Processing Letters* 2(1), 1973.
- raylib, https://www.raylib.com/ (zlib license).
