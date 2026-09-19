#include "acp/geometry2d.hpp"
#include "acp/snap.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

} // namespace

int main() {
    using namespace acp;

    expect(geo::nearly_equal(geo::distance({0, 0}, {3, 4}), 5.0), "distance 3-4-5");
    expect(geo::nearly_equal(geo::midpoint({{0, 0}, {10, 0}}), {5, 0}), "segment midpoint");
    expect(geo::nearly_equal(geo::nearest_point({{0, 0}, {10, 0}}, {7, 3}), {7, 0}), "nearest point");
    expect(geo::nearly_equal(geo::nearest_point({{2, 2}, {2, 2}}, {9, 9}), {2, 2}), "degenerate segment");

    const auto hit = geo::segment_intersection({{0, 0}, {10, 10}}, {{0, 10}, {10, 0}});
    expect(hit.has_value() && geo::nearly_equal(*hit, {5, 5}), "crossing segment intersection");

    const auto miss = geo::segment_intersection({{0, 0}, {1, 0}}, {{2, -1}, {2, 1}});
    expect(!miss.has_value(), "non-crossing segment intersection");

    const auto parallel = geo::segment_intersection({{0, 0}, {10, 0}}, {{0, 1}, {10, 1}});
    expect(!parallel.has_value(), "parallel segments");

    expect(geo::nearly_equal(geo::polyline_length({{0, 0}, {3, 4}, {6, 4}}), 8.0), "polyline length");

    const auto endpointSnap = snap::best_for_segment({{0, 0}, {10, 0}}, {0.1, 0.1}, 1.0);
    expect(endpointSnap.has_value() && endpointSnap->kind == snap::Kind::Endpoint, "endpoint snap");

    const auto midpointSnap = snap::best_for_segment({{0, 0}, {10, 0}}, {5.0, 0.2}, 1.0, false);
    expect(midpointSnap.has_value() && midpointSnap->kind == snap::Kind::Midpoint, "midpoint snap");

    const auto noSnap = snap::best_for_circle({{100, 100}, 20}, {0, 0}, 5.0);
    expect(!noSnap.has_value(), "snap aperture");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
