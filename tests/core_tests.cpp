#include "acp/document.hpp"
#include "acp/geometry2d.hpp"
#include "acp/history.hpp"
#include "acp/snap.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
#include <variant>
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

    Entity moving = LineEntity{{{1, 2}, {3, 4}}};
    transform::translate(moving, {10, -2});
    const auto& movedLine = std::get<LineEntity>(moving);
    expect(geo::nearly_equal(movedLine.segment.a, {11, 0}) &&
           geo::nearly_equal(movedLine.segment.b, {13, 2}), "translate line");

    Entity rotating = LineEntity{{{1, 0}, {2, 0}}};
    transform::rotate(rotating, {0, 0}, std::numbers::pi / 2.0);
    const auto& rotatedLine = std::get<LineEntity>(rotating);
    expect(geo::nearly_equal(rotatedLine.segment.a, {0, 1}, 1e-8) &&
           geo::nearly_equal(rotatedLine.segment.b, {0, 2}, 1e-8), "rotate line 90 degrees");

    Entity circle = CircleEntity{{{2, 2}, 4}};
    expect(transform::scale_uniform(circle, {0, 0}, 2.0), "scale circle accepted");
    const auto& scaledCircle = std::get<CircleEntity>(circle);
    expect(geo::nearly_equal(scaledCircle.circle.center, {4, 4}) &&
           geo::nearly_equal(scaledCircle.circle.radius, 8.0), "scale circle geometry");

    const Entity circleBeforeInvalidScale = circle;
    expect(!transform::scale_uniform(circle, {0, 0}, 0.0), "reject zero scale");
    const auto& circleAfterInvalidScale = std::get<CircleEntity>(circle);
    const auto& circleBefore = std::get<CircleEntity>(circleBeforeInvalidScale);
    expect(geo::nearly_equal(circleAfterInvalidScale.circle.center, circleBefore.circle.center) &&
           geo::nearly_equal(circleAfterInvalidScale.circle.radius, circleBefore.circle.radius),
           "invalid scale leaves entity unchanged");

    Entity polyline = PolylineEntity{{{0, 0}, {1, 0}, {1, 1}}, false};
    transform::translate(polyline, {-1, 2});
    const auto& movedPolyline = std::get<PolylineEntity>(polyline);
    expect(movedPolyline.points.size() == 3 &&
           geo::nearly_equal(movedPolyline.points[0], {-1, 2}) &&
           geo::nearly_equal(movedPolyline.points[2], {0, 3}), "translate polyline");

    Document selectionDoc;
    const EntityId selectionLine = selectionDoc.insert(LineEntity{{{0, 0}, {10, 0}}});
    const EntityId selectionCircle = selectionDoc.insert(CircleEntity{{{20, 0}, 5}});
    const EntityId selectionPolyline = selectionDoc.insert(PolylineEntity{{{30, 0}, {35, 5}, {40, 0}}, false});

    const auto lineHit = selection::hit_test(selectionDoc, {4, 0.25}, 0.5);
    expect(lineHit.has_value() && lineHit->id == selectionLine &&
           geo::nearly_equal(lineHit->nearest, {4, 0}), "select line by aperture");

    const auto circleHit = selection::hit_test(selectionDoc, {25.2, 0}, 0.5);
    expect(circleHit.has_value() && circleHit->id == selectionCircle &&
           geo::nearly_equal(circleHit->nearest, {25, 0}), "select circle perimeter");

    const auto polylineHit = selection::hit_test(selectionDoc, {35, 4.8}, 0.5);
    expect(polylineHit.has_value() && polylineHit->id == selectionPolyline, "select polyline segment");

    expect(!selection::hit_test(selectionDoc, {100, 100}, 1.0).has_value(), "selection miss outside aperture");
    expect(!selection::hit_test(selectionDoc, {0, 0}, -1.0).has_value(), "reject negative selection aperture");


    const Entity sourceCopy = LineEntity{{{1, 1}, {4, 1}}};
    const Entity offsetCopy = transform::translated_copy(sourceCopy, {10, 5});
    const auto& originalCopyLine = std::get<LineEntity>(sourceCopy);
    const auto& offsetCopyLine = std::get<LineEntity>(offsetCopy);
    expect(geo::nearly_equal(originalCopyLine.segment.a, {1, 1}) &&
           geo::nearly_equal(originalCopyLine.segment.b, {4, 1}),
           "copy preserves source");
    expect(geo::nearly_equal(offsetCopyLine.segment.a, {11, 6}) &&
           geo::nearly_equal(offsetCopyLine.segment.b, {14, 6}),
           "copy applies offset");

    Entity mirroredLine = LineEntity{{{2, 1}, {4, 3}}};
    expect(transform::mirror(mirroredLine, {{0, 0}, {0, 10}}), "mirror line accepted");
    const auto& mirroredLineValue = std::get<LineEntity>(mirroredLine);
    expect(geo::nearly_equal(mirroredLineValue.segment.a, {-2, 1}) &&
           geo::nearly_equal(mirroredLineValue.segment.b, {-4, 3}),
           "mirror line across y axis");

    Entity mirroredCircle = CircleEntity{{{3, 4}, 2}};
    expect(transform::mirror(mirroredCircle, {{0, 0}, {10, 0}}), "mirror circle accepted");
    const auto& mirroredCircleValue = std::get<CircleEntity>(mirroredCircle);
    expect(geo::nearly_equal(mirroredCircleValue.circle.center, {3, -4}) &&
           geo::nearly_equal(mirroredCircleValue.circle.radius, 2.0),
           "mirror circle preserves radius");

    Entity invalidMirror = LineEntity{{{1, 2}, {3, 4}}};
    const Entity invalidMirrorBefore = invalidMirror;
    expect(!transform::mirror(invalidMirror, {{5, 5}, {5, 5}}), "reject degenerate mirror axis");
    const auto& invalidMirrorLine = std::get<LineEntity>(invalidMirror);
    const auto& invalidMirrorLineBefore = std::get<LineEntity>(invalidMirrorBefore);
    expect(geo::nearly_equal(invalidMirrorLine.segment.a, invalidMirrorLineBefore.segment.a) &&
           geo::nearly_equal(invalidMirrorLine.segment.b, invalidMirrorLineBefore.segment.b),
           "degenerate mirror leaves entity unchanged");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
