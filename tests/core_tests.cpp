#include "acp/document.hpp"
#include "acp/geometry2d.hpp"
#include "acp/history.hpp"
#include "acp/snap.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {
int failures = 0;
void expect(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}
}

int main() {
    using namespace acp;

    expect(geo::nearly_equal(geo::distance({0, 0}, {3, 4}), 5.0), "distance 3-4-5");
    expect(geo::nearly_equal(geo::midpoint({{0, 0}, {10, 0}}), {5, 0}), "segment midpoint");
    expect(geo::nearly_equal(geo::nearest_point({{0, 0}, {10, 0}}, {7, 3}), {7, 0}), "nearest point");
    expect(geo::nearly_equal(geo::nearest_point({{2, 2}, {2, 2}}, {9, 9}), {2, 2}), "degenerate segment");

    const auto hit = geo::segment_intersection({{0, 0}, {10, 10}}, {{0, 10}, {10, 0}});
    expect(hit.has_value() && geo::nearly_equal(*hit, {5, 5}), "crossing segment intersection");
    expect(!geo::segment_intersection({{0, 0}, {1, 0}}, {{2, -1}, {2, 1}}).has_value(), "non-crossing");
    expect(!geo::segment_intersection({{0, 0}, {10, 0}}, {{0, 1}, {10, 1}}).has_value(), "parallel");
    expect(geo::nearly_equal(geo::polyline_length({{0, 0}, {3, 4}, {6, 4}}), 8.0), "polyline length");

    const auto endpointSnap = snap::best_for_segment({{0, 0}, {10, 0}}, {0.1, 0.1}, 1.0);
    expect(endpointSnap.has_value() && endpointSnap->kind == snap::Kind::Endpoint, "endpoint snap");

    const auto midpointSnap = snap::best_for_segment({{0, 0}, {10, 0}}, {5.0, 0.2}, 1.0, false);
    expect(midpointSnap.has_value() && midpointSnap->kind == snap::Kind::Midpoint, "midpoint snap");

    Document doc;
    History history;
    auto add = std::make_unique<AddEntityCommand>(LineEntity{{{0, 0}, {10, 0}}});
    auto* addPtr = add.get();
    expect(history.apply(doc, std::move(add)), "history add");
    const EntityId lineId = addPtr->id();
    expect(lineId != 0 && doc.find(lineId) != nullptr && doc.size() == 1, "document add result");
    expect(history.undo(doc) && doc.size() == 0, "undo add");
    expect(history.redo(doc) && doc.find(lineId) != nullptr, "redo add preserves id");

    expect(history.apply(doc, std::make_unique<RemoveEntityCommand>(lineId)), "history remove");
    expect(doc.find(lineId) == nullptr, "remove result");
    expect(history.undo(doc) && doc.find(lineId) != nullptr, "undo remove");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
