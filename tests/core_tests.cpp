#include "acp/annotation.hpp"
#include "acp/block.hpp"
#include "acp/document.hpp"
#include "acp/edit2d.hpp"
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


    const auto infiniteHit = edit2d::infinite_line_intersection(
        {{0, 0}, {5, 0}}, {{10, -5}, {10, 5}});
    expect(infiniteHit.has_value() &&
           geo::nearly_equal(infiniteHit->point, {10, 0}) &&
           geo::nearly_equal(infiniteHit->lhs_parameter, 2.0),
           "infinite line intersection parameters");

    const auto offsetUp = edit2d::offset_segment({{0, 0}, {10, 0}}, 2.0);
    expect(offsetUp.has_value() &&
           geo::nearly_equal(offsetUp->a, {0, 2}) &&
           geo::nearly_equal(offsetUp->b, {10, 2}),
           "offset horizontal segment left");

    const auto offsetDown = edit2d::offset_segment({{0, 0}, {10, 0}}, -3.0);
    expect(offsetDown.has_value() &&
           geo::nearly_equal(offsetDown->a, {0, -3}) &&
           geo::nearly_equal(offsetDown->b, {10, -3}),
           "offset horizontal segment right");

    expect(!edit2d::offset_segment({{1, 1}, {1, 1}}, 2.0).has_value(),
           "reject offset degenerate segment");

    geo::Segment trimLeft{{0, 0}, {10, 0}};
    expect(edit2d::trim_segment(trimLeft, {{4, -5}, {4, 5}}, {1, 0}),
           "trim removes picked left side");
    expect(geo::nearly_equal(trimLeft.a, {4, 0}) &&
           geo::nearly_equal(trimLeft.b, {10, 0}),
           "trim left result");

    geo::Segment trimRight{{0, 0}, {10, 0}};
    expect(edit2d::trim_segment(trimRight, {{6, -5}, {6, 5}}, {9, 0}),
           "trim removes picked right side");
    expect(geo::nearly_equal(trimRight.a, {0, 0}) &&
           geo::nearly_equal(trimRight.b, {6, 0}),
           "trim right result");

    geo::Segment noTrim{{0, 0}, {10, 0}};
    expect(!edit2d::trim_segment(noTrim, {{20, -1}, {20, 1}}, {5, 0}),
           "trim rejects cutter outside target");
    expect(geo::nearly_equal(noTrim.a, {0, 0}) &&
           geo::nearly_equal(noTrim.b, {10, 0}),
           "failed trim leaves target unchanged");

    geo::Segment extendEnd{{0, 0}, {5, 0}};
    expect(edit2d::extend_segment(extendEnd, {{10, -5}, {10, 5}}),
           "extend end to boundary");
    expect(geo::nearly_equal(extendEnd.a, {0, 0}) &&
           geo::nearly_equal(extendEnd.b, {10, 0}),
           "extend end result");

    geo::Segment extendStart{{5, 0}, {10, 0}};
    expect(edit2d::extend_segment(extendStart, {{0, -5}, {0, 5}}),
           "extend start to boundary");
    expect(geo::nearly_equal(extendStart.a, {0, 0}) &&
           geo::nearly_equal(extendStart.b, {10, 0}),
           "extend start result");

    geo::Segment noExtend{{0, 0}, {10, 0}};
    expect(!edit2d::extend_segment(noExtend, {{5, -5}, {5, 5}}),
           "extend rejects existing interior intersection");


    const geo::Arc quarterArc{{0, 0}, 10.0, 0.0, std::numbers::pi / 2.0, true};
    expect(geo::valid_arc(quarterArc), "valid quarter arc");
    expect(geo::nearly_equal(geo::arc_sweep(quarterArc), std::numbers::pi / 2.0),
           "quarter arc sweep");
    expect(geo::nearly_equal(geo::arc_length(quarterArc), 5.0 * std::numbers::pi),
           "quarter arc length");
    expect(geo::nearly_equal(geo::arc_start_point(quarterArc), {10, 0}, 1e-8) &&
           geo::nearly_equal(geo::arc_end_point(quarterArc), {0, 10}, 1e-8),
           "quarter arc endpoints");

    const geo::Arc clockwiseArc{{0, 0}, 2.0, 0.0, -std::numbers::pi / 2.0, false};
    expect(geo::nearly_equal(geo::arc_sweep(clockwiseArc), std::numbers::pi / 2.0),
           "clockwise arc sweep");

    const geo::Arc fullArc{{1, 2}, 3.0, 0.0, 0.0, true};
    expect(geo::nearly_equal(geo::arc_sweep(fullArc), 2.0 * std::numbers::pi),
           "equal arc angles represent full circle");

    const geo::Arc invalidArc{{0, 0}, 0.0, 0.0, 1.0, true};
    expect(!geo::valid_arc(invalidArc) && geo::nearly_equal(geo::arc_length(invalidArc), 0.0),
           "reject invalid arc radius");

    const auto rectangle = geo::rectangle_from_corners({1, 2}, {5, 8});
    expect(rectangle.size() == 4 &&
           geo::nearly_equal(rectangle[0], {1, 2}) &&
           geo::nearly_equal(rectangle[1], {5, 2}) &&
           geo::nearly_equal(rectangle[2], {5, 8}) &&
           geo::nearly_equal(rectangle[3], {1, 8}),
           "rectangle from opposite corners");
    expect(geo::nearly_equal(geo::polyline_length(rectangle, true), 20.0),
           "closed rectangle perimeter");


    Document arcDoc;
    const EntityId arcId = arcDoc.insert(ArcEntity{{{50, 50}, 10.0, 0.0, std::numbers::pi / 2.0, true}});
    const auto arcHit = selection::hit_test(arcDoc, {57.1, 57.1}, 0.5);
    expect(arcHit.has_value() && arcHit->id == arcId, "select arc on valid sweep");
    expect(!selection::hit_test(arcDoc, {42.9, 57.1}, 0.5).has_value(),
           "arc selection ignores circle outside sweep");

    const auto arcEndpointSnap = snap::best_for_arc(
        {{0, 0}, 10.0, 0.0, std::numbers::pi / 2.0, true},
        {9.8, 0.1}, 0.5);
    expect(arcEndpointSnap.has_value() && arcEndpointSnap->kind == snap::Kind::Endpoint,
           "arc endpoint snap");

    const auto arcNearestSnap = snap::best_for_arc(
        {{0, 0}, 10.0, 0.0, std::numbers::pi / 2.0, true},
        {7.0, 7.2}, 0.5);
    expect(arcNearestSnap.has_value() && arcNearestSnap->kind == snap::Kind::Nearest,
           "arc nearest snap");

    const auto intersectionSnap = snap::intersection_for_segments(
        {{0, 0}, {10, 10}}, {{0, 10}, {10, 0}}, {5.1, 5.1}, 0.5);
    expect(intersectionSnap.has_value() && intersectionSnap->kind == snap::Kind::Intersection &&
           geo::nearly_equal(intersectionSnap->point, {5, 5}),
           "segment intersection snap");

    Entity arcTransform = ArcEntity{{{2, 0}, 3.0, 0.0, std::numbers::pi / 2.0, true}};
    transform::rotate(arcTransform, {0, 0}, std::numbers::pi / 2.0);
    const auto& rotatedArcEntity = std::get<ArcEntity>(arcTransform);
    expect(geo::nearly_equal(rotatedArcEntity.arc.center, {0, 2}, 1e-8) &&
           geo::nearly_equal(rotatedArcEntity.arc.start_angle, std::numbers::pi / 2.0, 1e-8),
           "rotate arc entity");

    expect(transform::scale_uniform(arcTransform, {0, 0}, 2.0), "scale arc accepted");
    const auto& scaledArcEntity = std::get<ArcEntity>(arcTransform);
    expect(geo::nearly_equal(scaledArcEntity.arc.center, {0, 4}, 1e-8) &&
           geo::nearly_equal(scaledArcEntity.arc.radius, 6.0),
           "scale arc entity");

    Entity mirroredArc = ArcEntity{{{2, 0}, 3.0, 0.0, std::numbers::pi / 2.0, true}};
    expect(transform::mirror(mirroredArc, {{0, -10}, {0, 10}}), "mirror arc accepted");
    const auto& mirroredArcEntity = std::get<ArcEntity>(mirroredArc);
    expect(geo::nearly_equal(mirroredArcEntity.arc.center, {-2, 0}, 1e-8) &&
           !mirroredArcEntity.arc.counter_clockwise,
           "mirror arc center and orientation");


    Document layerDoc;
    expect(layerDoc.layer(kDefaultLayerId) != nullptr &&
           layerDoc.layer(kDefaultLayerId)->name == "0",
           "default layer exists");

    const LayerId wallsLayer = layerDoc.create_layer("Walls");
    const LayerId dimsLayer = layerDoc.create_layer("Dimensions");
    expect(wallsLayer != 0 && dimsLayer != 0 && wallsLayer != dimsLayer,
           "create unique layers");
    expect(layerDoc.create_layer("Walls") == 0, "reject duplicate layer name");
    expect(layerDoc.rename_layer(dimsLayer, "Dims"), "rename layer");
    expect(!layerDoc.rename_layer(dimsLayer, "Walls"), "reject duplicate renamed layer");

    const EntityId wallEntity = layerDoc.insert(LineEntity{{{0, 0}, {10, 0}}});
    expect(layerDoc.set_entity_layer(wallEntity, wallsLayer), "assign entity layer");
    expect(layerDoc.properties(wallEntity) != nullptr &&
           layerDoc.properties(wallEntity)->layer_id == wallsLayer,
           "entity layer persisted");

    expect(layerDoc.set_layer_line_weight(wallsLayer, 0.50), "set layer line weight");
    expect(geo::nearly_equal(layerDoc.effective_line_weight(wallEntity), 0.50),
           "entity inherits layer line weight");
    layerDoc.properties(wallEntity)->line_weight_override = 0.80;
    expect(geo::nearly_equal(layerDoc.effective_line_weight(wallEntity), 0.80),
           "entity line weight override");

    expect(layerDoc.set_layer_visible(wallsLayer, false), "hide layer");
    expect(!layerDoc.entity_visible(wallEntity), "hidden layer hides entity");
    expect(!selection::hit_test(layerDoc, {5, 0}, 0.5).has_value(),
           "selection skips hidden layer");

    expect(layerDoc.set_layer_visible(wallsLayer, true), "show layer");
    layerDoc.properties(wallEntity)->visible = false;
    expect(!layerDoc.entity_visible(wallEntity), "entity visibility override");
    layerDoc.properties(wallEntity)->visible = true;

    expect(layerDoc.set_layer_locked(wallsLayer, true), "lock layer");
    expect(layerDoc.entity_locked(wallEntity), "entity inherits locked layer");
    expect(layerDoc.set_layer_locked(wallsLayer, false), "unlock layer");
    expect(!layerDoc.entity_locked(wallEntity), "entity unlocked with layer");

    expect(!layerDoc.remove_layer(wallsLayer), "cannot remove layer in use");
    expect(layerDoc.remove_layer(dimsLayer), "remove unused layer");
    expect(!layerDoc.remove_layer(kDefaultLayerId), "cannot remove default layer");


    BlockLibrary blocks;
    std::vector<BlockPrimitive> doorGeometry{
        LineEntity{{{0, 0}, {10, 0}}},
        ArcEntity{{{0, 0}, 10.0, 0.0, std::numbers::pi / 2.0, true}}
    };
    const BlockId doorBlock = blocks.create("Door", {0, 0}, doorGeometry);
    expect(doorBlock != 0 && blocks.find(doorBlock) != nullptr, "create block definition");
    expect(blocks.create("Door", {0, 0}, doorGeometry) == 0, "reject duplicate block name");
    expect(blocks.rename(doorBlock, "Door-900"), "rename block definition");

    const auto placedDoor = blocks.instantiate(
        BlockReferenceEntity{doorBlock, {100, 200}, std::numbers::pi / 2.0, 2.0});
    expect(placedDoor.size() == 2, "instantiate block geometry");
    const auto& placedDoorLine = std::get<LineEntity>(placedDoor[0]);
    expect(geo::nearly_equal(placedDoorLine.segment.a, {100, 200}, 1e-8) &&
           geo::nearly_equal(placedDoorLine.segment.b, {100, 220}, 1e-8),
           "block instance transforms line");

    const auto& placedDoorArc = std::get<ArcEntity>(placedDoor[1]);
    expect(geo::nearly_equal(placedDoorArc.arc.center, {100, 200}, 1e-8) &&
           geo::nearly_equal(placedDoorArc.arc.radius, 20.0) &&
           geo::nearly_equal(placedDoorArc.arc.start_angle, std::numbers::pi / 2.0, 1e-8),
           "block instance transforms arc");

    const auto* sourceDoor = blocks.find(doorBlock);
    expect(sourceDoor != nullptr &&
           geo::nearly_equal(std::get<LineEntity>(sourceDoor->geometry[0]).segment.b, {10, 0}),
           "block instancing preserves definition");

    expect(blocks.instantiate(BlockReferenceEntity{doorBlock, {0, 0}, 0.0, 0.0}).empty(),
           "reject invalid block scale");
    expect(blocks.remove(doorBlock) && blocks.find(doorBlock) == nullptr,
           "remove block definition");


    Document historyPropsDoc;
    const LayerId historyLayer = historyPropsDoc.create_layer("HistoryLayer");
    History propertyHistory;
    auto addStyled = std::make_unique<AddEntityCommand>(LineEntity{{{1, 1}, {2, 2}}});
    auto* addStyledPtr = addStyled.get();
    expect(propertyHistory.apply(historyPropsDoc, std::move(addStyled)), "add styled entity");
    const EntityId styledId = addStyledPtr->id();
    expect(historyPropsDoc.set_entity_layer(styledId, historyLayer), "style history layer assignment");
    historyPropsDoc.properties(styledId)->line_weight_override = 0.90;
    historyPropsDoc.properties(styledId)->visible = false;

    expect(propertyHistory.undo(historyPropsDoc), "undo styled add");
    expect(propertyHistory.redo(historyPropsDoc), "redo styled add");
    expect(historyPropsDoc.properties(styledId) != nullptr &&
           historyPropsDoc.properties(styledId)->layer_id == historyLayer &&
           historyPropsDoc.properties(styledId)->line_weight_override.has_value() &&
           geo::nearly_equal(*historyPropsDoc.properties(styledId)->line_weight_override, 0.90) &&
           !historyPropsDoc.properties(styledId)->visible,
           "redo add preserves entity properties");

    expect(propertyHistory.apply(historyPropsDoc, std::make_unique<RemoveEntityCommand>(styledId)),
           "remove styled entity");
    expect(propertyHistory.undo(historyPropsDoc), "undo styled remove");
    expect(historyPropsDoc.properties(styledId) != nullptr &&
           historyPropsDoc.properties(styledId)->layer_id == historyLayer &&
           historyPropsDoc.properties(styledId)->line_weight_override.has_value() &&
           geo::nearly_equal(*historyPropsDoc.properties(styledId)->line_weight_override, 0.90) &&
           !historyPropsDoc.properties(styledId)->visible,
           "undo remove preserves entity properties");


    BlockLibrary integrationBlocks;
    const BlockId chairBlock = integrationBlocks.create(
        "Chair",
        {0, 0},
        std::vector<BlockPrimitive>{
            LineEntity{{{0, 0}, {10, 0}}},
            LineEntity{{{10, 0}, {10, 10}}}
        });
    expect(chairBlock != 0, "create integration block");

    Document blockDoc;
    const EntityId blockRefId = blockDoc.insert(
        BlockReferenceEntity{chairBlock, {100, 50}, 0.0, 1.0});
    const auto blockHit = selection::hit_test(
        blockDoc, integrationBlocks, {105, 50.2}, 0.5);
    expect(blockHit.has_value() && blockHit->id == blockRefId &&
           geo::nearly_equal(blockHit->nearest, {105, 50}),
           "select block reference by instantiated geometry");

    expect(!selection::hit_test(blockDoc, {105, 50.2}, 0.5).has_value(),
           "block reference requires library-aware selection");

    Entity blockMove = BlockReferenceEntity{chairBlock, {10, 20}, 0.0, 2.0};
    transform::translate(blockMove, {5, -5});
    const auto& movedBlock = std::get<BlockReferenceEntity>(blockMove);
    expect(geo::nearly_equal(movedBlock.insertion_point, {15, 15}) &&
           geo::nearly_equal(movedBlock.scale, 2.0),
           "translate block reference");

    transform::rotate(blockMove, {0, 0}, std::numbers::pi / 2.0);
    const auto& rotatedBlock = std::get<BlockReferenceEntity>(blockMove);
    expect(geo::nearly_equal(rotatedBlock.insertion_point, {-15, 15}, 1e-8) &&
           geo::nearly_equal(rotatedBlock.rotation, std::numbers::pi / 2.0, 1e-8),
           "rotate block reference");

    expect(transform::scale_uniform(blockMove, {0, 0}, 0.5),
           "scale block reference");
    const auto& scaledBlock = std::get<BlockReferenceEntity>(blockMove);
    expect(geo::nearly_equal(scaledBlock.insertion_point, {-7.5, 7.5}, 1e-8) &&
           geo::nearly_equal(scaledBlock.scale, 1.0),
           "scale block reference geometry scale");

    Entity mirroredBlock = BlockReferenceEntity{chairBlock, {5, 2}, 0.25, 1.0};
    expect(transform::mirror(mirroredBlock, {{0, -10}, {0, 10}}),
           "mirror block reference");
    const auto& mirroredBlockRef = std::get<BlockReferenceEntity>(mirroredBlock);
    expect(geo::nearly_equal(mirroredBlockRef.insertion_point, {-5, 2}, 1e-8),
           "mirror block insertion point");


    const TextEntity note{{10, 20}, "ROOM", 5.0, 0.0};
    expect(annotation::valid_text(note), "valid text entity");
    expect(geo::nearly_equal(annotation::estimated_text_width(note), 12.0),
           "estimated text width");
    const auto noteBaseline = annotation::text_baseline(note);
    expect(geo::nearly_equal(noteBaseline.a, {10, 20}) &&
           geo::nearly_equal(noteBaseline.b, {22, 20}),
           "text baseline geometry");

    const LinearDimensionEntity dim{{0, 0}, {10, 0}, {0, 5}, std::nullopt};
    expect(annotation::valid_linear_dimension(dim), "valid linear dimension");
    expect(geo::nearly_equal(annotation::measurement(dim), 10.0),
           "linear dimension measurement");
    const auto dimLine = annotation::dimension_line(dim);
    expect(geo::nearly_equal(dimLine.a, {0, 5}) &&
           geo::nearly_equal(dimLine.b, {10, 5}),
           "linear dimension line placement");

    Document annotationDoc;
    const EntityId textId = annotationDoc.insert(note);
    const EntityId dimId = annotationDoc.insert(dim);

    const auto textHit = selection::hit_test(annotationDoc, {16, 20.2}, 0.5);
    expect(textHit.has_value() && textHit->id == textId,
           "select text baseline");

    const auto dimHit = selection::hit_test(annotationDoc, {6, 5.2}, 0.5);
    expect(dimHit.has_value() && dimHit->id == dimId,
           "select dimension line");

    Entity textTransform = note;
    transform::translate(textTransform, {5, -5});
    transform::rotate(textTransform, {0, 0}, std::numbers::pi / 2.0);
    const auto& transformedText = std::get<TextEntity>(textTransform);
    expect(geo::nearly_equal(transformedText.position, {-15, 15}, 1e-8) &&
           geo::nearly_equal(transformedText.rotation, std::numbers::pi / 2.0, 1e-8),
           "transform text entity");

    expect(transform::scale_uniform(textTransform, {0, 0}, 2.0),
           "scale text entity");
    const auto& scaledText = std::get<TextEntity>(textTransform);
    expect(geo::nearly_equal(scaledText.height, 10.0),
           "scale text height");

    Entity dimensionTransform = dim;
    transform::translate(dimensionTransform, {2, 3});
    const auto& movedDim = std::get<LinearDimensionEntity>(dimensionTransform);
    expect(geo::nearly_equal(movedDim.first, {2, 3}) &&
           geo::nearly_equal(movedDim.second, {12, 3}) &&
           geo::nearly_equal(movedDim.line_point, {2, 8}),
           "translate dimension entity");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
