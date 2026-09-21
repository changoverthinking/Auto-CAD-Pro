#include "acp/annotation.hpp"
#include "acp/block.hpp"
#include "acp/bounds.hpp"
#include "acp/document.hpp"
#include "acp/edit2d.hpp"
#include "acp/dxf.hpp"
#include "acp/geometry2d.hpp"
#include "acp/history.hpp"
#include "acp/layout.hpp"
#include "acp/hatch.hpp"
#include "acp/persistence.hpp"
#include "acp/snap.hpp"
#include "acp/svg.hpp"
#include "acp/pdf.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iterator>
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


    HatchEntity hatchEntity{
        {{0, 0}, {10, 0}, {10, 5}, {0, 5}},
        "ANSI31",
        0.0,
        1.0,
        false
    };
    expect(hatch::valid(hatchEntity), "valid hatch entity");
    expect(geo::nearly_equal(hatch::perimeter(hatchEntity), 30.0),
           "hatch boundary perimeter");

    const auto horizontalHatch =
        hatch::pattern_segments(hatchEntity);
    expect(!horizontalHatch.empty(),
           "hatch pattern generates clipped segments");
    if (!horizontalHatch.empty()) {
        expect(geo::nearly_equal(
                   horizontalHatch.front().a.y,
                   horizontalHatch.front().b.y,
                   1e-8),
               "zero-angle hatch produces horizontal pattern");
    }

    HatchEntity verticalHatch = hatchEntity;
    verticalHatch.angle = std::numbers::pi / 2.0;
    const auto verticalSegments =
        hatch::pattern_segments(verticalHatch);
    expect(!verticalSegments.empty(),
           "rotated hatch pattern generates segments");
    if (!verticalSegments.empty()) {
        expect(geo::nearly_equal(
                   verticalSegments.front().a.x,
                   verticalSegments.front().b.x,
                   1e-8),
               "90-degree hatch produces vertical pattern");
    }

    HatchEntity sparseHatch = hatchEntity;
    sparseHatch.spacing = 2.0;
    const auto sparseSegments =
        hatch::pattern_segments(sparseHatch);
    expect(sparseSegments.size() < horizontalHatch.size(),
           "larger hatch spacing produces fewer lines");

    HatchEntity solidPattern = hatchEntity;
    solidPattern.solid = true;
    expect(hatch::pattern_segments(solidPattern).empty(),
           "solid hatch does not generate pattern segments");

    Document hatchDoc;
    const EntityId hatchId = hatchDoc.insert(hatchEntity);
    const auto hatchHit = selection::hit_test(hatchDoc, {5, 0.2}, 0.5);
    expect(hatchHit.has_value() && hatchHit->id == hatchId,
           "select hatch boundary");

    Entity hatchTransform = hatchEntity;
    transform::translate(hatchTransform, {2, 3});
    const auto& movedHatch = std::get<HatchEntity>(hatchTransform);
    expect(geo::nearly_equal(movedHatch.boundary[0], {2, 3}) &&
           geo::nearly_equal(movedHatch.boundary[2], {12, 8}),
           "translate hatch");

    transform::rotate(hatchTransform, {0, 0}, std::numbers::pi / 2.0);
    const auto& rotatedHatch = std::get<HatchEntity>(hatchTransform);
    expect(geo::nearly_equal(rotatedHatch.angle, std::numbers::pi / 2.0, 1e-8),
           "rotate hatch pattern angle");

    expect(transform::scale_uniform(hatchTransform, {0, 0}, 2.0),
           "scale hatch");
    const auto& scaledHatch = std::get<HatchEntity>(hatchTransform);
    expect(geo::nearly_equal(scaledHatch.spacing, 2.0),
           "scale hatch spacing");

    HatchEntity invalidHatch{{{0, 0}, {1, 0}}, "ANSI31", 0.0, 1.0, false};
    expect(!hatch::valid(invalidHatch), "reject hatch with open undersized boundary");


    Document persistDoc;
    const LayerId temporaryLayer = persistDoc.create_layer("Temporary");
    expect(temporaryLayer != 0 && persistDoc.remove_layer(temporaryLayer),
           "create and remove layer to create id gap");
    const LayerId savedLayer = persistDoc.create_layer("Saved Layer");
    expect(savedLayer != 0, "create persisted layer");
    expect(persistDoc.set_layer_locked(savedLayer, true) &&
           persistDoc.set_layer_line_weight(savedLayer, 0.65),
           "configure persisted layer");

    BlockLibrary persistBlocks;
    const BlockId temporaryBlock = persistBlocks.create(
        "TemporaryBlock", {0, 0},
        std::vector<BlockPrimitive>{LineEntity{{{0, 0}, {1, 0}}}});
    expect(temporaryBlock != 0 && persistBlocks.remove(temporaryBlock),
           "create and remove block to create id gap");
    const BlockId savedBlock = persistBlocks.create(
        "SavedBlock", {0, 0},
        std::vector<BlockPrimitive>{
            LineEntity{{{0, 0}, {4, 0}}},
            CircleEntity{{{2, 2}, 1.5}}
        });
    expect(savedBlock != 0, "create persisted block");

    const EntityId persistedLineId = persistDoc.insert(LineEntity{{{1, 2}, {9, 2}}});
    expect(persistDoc.set_entity_layer(persistedLineId, savedLayer), "assign persisted line layer");
    persistDoc.properties(persistedLineId)->visible = false;
    persistDoc.properties(persistedLineId)->line_weight_override = 0.95;

    const EntityId persistedBlockId = persistDoc.insert(
        BlockReferenceEntity{savedBlock, {20, 30}, 0.25, 1.5});
    const EntityId persistedTextId = persistDoc.insert(
        TextEntity{{5, 6}, "Room A", 3.5, 0.15});
    const EntityId persistedDimId = persistDoc.insert(
        LinearDimensionEntity{{0, 0}, {12, 0}, {0, 4}, std::string{"1200"}});
    const EntityId persistedHatchId = persistDoc.insert(
        HatchEntity{{{0, 0}, {8, 0}, {8, 4}, {0, 4}}, "ANSI31", 0.4, 2.0, false});

    const std::string savedProject = persistence::serialize_project(persistDoc, persistBlocks);
    const auto loadedProject = persistence::deserialize_project(savedProject);
    expect(loadedProject.has_value(), "project save open roundtrip parses");

    if (loadedProject.has_value()) {
        const auto& loadedDoc = loadedProject->document;
        const auto& loadedBlocks = loadedProject->blocks;

        expect(loadedDoc.layer(savedLayer) != nullptr &&
               loadedDoc.layer(savedLayer)->name == "Saved Layer" &&
               loadedDoc.layer(savedLayer)->locked &&
               geo::nearly_equal(loadedDoc.layer(savedLayer)->line_weight, 0.65),
               "roundtrip preserves layer id and properties");

        expect(loadedBlocks.find(savedBlock) != nullptr &&
               loadedBlocks.find(savedBlock)->name == "SavedBlock" &&
               loadedBlocks.find(savedBlock)->geometry.size() == 2,
               "roundtrip preserves block id and definition");

        expect(loadedDoc.find(persistedLineId) != nullptr &&
               loadedDoc.properties(persistedLineId) != nullptr &&
               loadedDoc.properties(persistedLineId)->layer_id == savedLayer &&
               !loadedDoc.properties(persistedLineId)->visible &&
               loadedDoc.properties(persistedLineId)->line_weight_override.has_value() &&
               geo::nearly_equal(*loadedDoc.properties(persistedLineId)->line_weight_override, 0.95),
               "roundtrip preserves entity id and properties");

        const auto* loadedBlockRefEntity = loadedDoc.find(persistedBlockId);
        expect(loadedBlockRefEntity != nullptr &&
               std::holds_alternative<BlockReferenceEntity>(*loadedBlockRefEntity) &&
               std::get<BlockReferenceEntity>(*loadedBlockRefEntity).block_id == savedBlock,
               "roundtrip preserves block reference");

        const geo::Vec2 loadedBlockProbe{
            20.0 + 3.0 * std::cos(0.25),
            30.0 + 3.0 * std::sin(0.25) + 0.1
        };
        const auto blockRoundtripHit = selection::hit_test(
            loadedDoc, loadedBlocks, loadedBlockProbe, 0.5);
        expect(blockRoundtripHit.has_value() && blockRoundtripHit->id == persistedBlockId,
               "loaded block reference remains selectable");

        const auto* loadedTextEntity = loadedDoc.find(persistedTextId);
        expect(loadedTextEntity != nullptr &&
               std::holds_alternative<TextEntity>(*loadedTextEntity) &&
               std::get<TextEntity>(*loadedTextEntity).text == "Room A",
               "roundtrip preserves text");

        const auto* loadedDimEntity = loadedDoc.find(persistedDimId);
        expect(loadedDimEntity != nullptr &&
               std::holds_alternative<LinearDimensionEntity>(*loadedDimEntity) &&
               std::get<LinearDimensionEntity>(*loadedDimEntity).text_override.has_value() &&
               *std::get<LinearDimensionEntity>(*loadedDimEntity).text_override == "1200",
               "roundtrip preserves dimension override");

        const auto* loadedHatchEntity = loadedDoc.find(persistedHatchId);
        expect(loadedHatchEntity != nullptr &&
               std::holds_alternative<HatchEntity>(*loadedHatchEntity) &&
               std::get<HatchEntity>(*loadedHatchEntity).pattern == "ANSI31" &&
               geo::nearly_equal(std::get<HatchEntity>(*loadedHatchEntity).spacing, 2.0),
               "roundtrip preserves hatch");
    }

    expect(!persistence::deserialize_project("ACP2D 99\nEND\n").has_value(),
           "reject unsupported project version");
    expect(!persistence::deserialize_project(
        "ACP2D 1\n"
        "L 1 \"0\" 1 0 0.25\n"
        "E 1 1 1 0 0 BLOCKREF 999 0 0 0 1\n"
        "END\n").has_value(),
        "reject missing block reference on load");


    Document dxfDoc;
    const LayerId dxfWalls = dxfDoc.create_layer("Walls");
    const EntityId dxfLineId = dxfDoc.insert(LineEntity{{{1, 2}, {11, 2}}});
    expect(dxfDoc.set_entity_layer(dxfLineId, dxfWalls), "assign dxf line layer");
    (void)dxfDoc.insert(CircleEntity{{{5, 5}, 2.5}});
    (void)dxfDoc.insert(ArcEntity{{{10, 10}, 4.0, 0.0, std::numbers::pi / 2.0, true}});
    (void)dxfDoc.insert(PolylineEntity{{{0, 0}, {3, 0}, {3, 4}}, true});
    (void)dxfDoc.insert(TextEntity{{2, 8}, "DXF NOTE", 2.0, 0.25});

    const std::string dxfText = dxf::export_ascii(dxfDoc);
    expect(dxfText.find("LWPOLYLINE") != std::string::npos &&
           dxfText.find("DXF NOTE") != std::string::npos,
           "export dxf ascii entity records");

    const auto dxfLoaded = dxf::import_ascii(dxfText);
    expect(dxfLoaded.has_value() && dxfLoaded->imported == 5 && dxfLoaded->skipped == 0,
           "import exported dxf ascii");
    if (dxfLoaded.has_value()) {
        expect(dxfLoaded->document.size() == 5, "dxf roundtrip entity count");
        bool foundWallsLayer = false;
        for (const LayerId layerId : dxfLoaded->document.layer_ids()) {
            const Layer* layerValue = dxfLoaded->document.layer(layerId);
            if (layerValue != nullptr && layerValue->name == "Walls") {
                foundWallsLayer = true;
                break;
            }
        }
        expect(foundWallsLayer, "dxf import creates source layer");

        bool foundText = false;
        bool foundArc = false;
        for (const EntityId entityId : dxfLoaded->document.ids()) {
            const Entity* loadedEntity = dxfLoaded->document.find(entityId);
            if (loadedEntity == nullptr) continue;
            if (std::holds_alternative<TextEntity>(*loadedEntity) &&
                std::get<TextEntity>(*loadedEntity).text == "DXF NOTE") {
                foundText = true;
            }
            if (std::holds_alternative<ArcEntity>(*loadedEntity)) {
                const auto& loadedArc = std::get<ArcEntity>(*loadedEntity).arc;
                foundArc = geo::nearly_equal(loadedArc.radius, 4.0) &&
                           geo::nearly_equal(loadedArc.end_angle, std::numbers::pi / 2.0, 1e-8);
            }
        }
        expect(foundText, "dxf roundtrip preserves text");
        expect(foundArc, "dxf roundtrip preserves arc angles");
    }

    const auto dxfUnsupported = dxf::import_ascii(
        "0\nSECTION\n2\nENTITIES\n"
        "0\nSPLINE\n8\n0\n"
        "0\nENDSEC\n0\nEOF\n");
    expect(dxfUnsupported.has_value() &&
           dxfUnsupported->imported == 0 &&
           dxfUnsupported->skipped == 1,
           "dxf import skips unsupported entity");

    expect(!dxf::import_ascii("0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n1\n").has_value(),
           "dxf import rejects truncated pair stream");


    const auto circleBounds = bounds::entity_bounds(
        Entity{CircleEntity{{{5, 6}, 2.0}}});
    expect(circleBounds.has_value() &&
           geo::nearly_equal(circleBounds->min, {3, 4}) &&
           geo::nearly_equal(circleBounds->max, {7, 8}),
           "circle bounds exact");

    const auto arcBounds = bounds::entity_bounds(
        Entity{ArcEntity{{{0, 0}, 10.0, 0.0, std::numbers::pi, true}}});
    expect(arcBounds.has_value() &&
           geo::nearly_equal(arcBounds->min, {-10, 0}, 1e-8) &&
           geo::nearly_equal(arcBounds->max, {10, 10}, 1e-8),
           "arc bounds include quadrants");

    const auto rotatedTextBounds = bounds::entity_bounds(
        Entity{TextEntity{{0, 0}, "AB", 5.0, std::numbers::pi / 2.0}});
    expect(rotatedTextBounds.has_value() &&
           geo::nearly_equal(rotatedTextBounds->min.x, -5.0, 1e-8) &&
           geo::nearly_equal(rotatedTextBounds->max.x, 0.0, 1e-8) &&
           rotatedTextBounds->max.y > 0.0,
           "rotated text bounds");

    BlockLibrary boundsBlocks;
    const BlockId boundsBlockId = boundsBlocks.create(
        "BoundsBlock",
        {0, 0},
        std::vector<BlockPrimitive>{
            LineEntity{{{0, 0}, {10, 0}}},
            CircleEntity{{{5, 5}, 2}}
        });
    const auto blockBounds = bounds::entity_bounds(
        Entity{BlockReferenceEntity{
            boundsBlockId, {100, 50}, std::numbers::pi / 2.0, 2.0}},
        &boundsBlocks);
    expect(blockBounds.has_value() &&
           geo::nearly_equal(blockBounds->min, {86, 50}, 1e-8) &&
           geo::nearly_equal(blockBounds->max, {100, 70}, 1e-8),
           "transformed block bounds");

    Document boundsDoc;
    const EntityId visibleBoundsId = boundsDoc.insert(
        LineEntity{{{-5, -2}, {15, 8}}});
    const EntityId hiddenBoundsId = boundsDoc.insert(
        CircleEntity{{{1000, 1000}, 100}});
    boundsDoc.properties(hiddenBoundsId)->visible = false;
    const auto drawingBounds = bounds::drawing_bounds(boundsDoc);
    expect(drawingBounds.has_value() &&
           geo::nearly_equal(drawingBounds->min, {-5, -2}) &&
           geo::nearly_equal(drawingBounds->max, {15, 8}) &&
           boundsDoc.find(visibleBoundsId) != nullptr,
           "drawing bounds ignore hidden entities");

    const auto fittedView = bounds::fit_to_aspect(*drawingBounds, 16.0 / 9.0, 0.10);
    expect(fittedView.has_value() &&
           geo::nearly_equal(fittedView->center, {5, 3}) &&
           geo::nearly_equal(
               fittedView->world_width / fittedView->world_height,
               16.0 / 9.0,
               1e-8) &&
           fittedView->world_width >= drawingBounds->width() &&
           fittedView->world_height >= drawingBounds->height(),
           "fit bounds preserves viewport aspect and margin");

    bounds::Bounds2 invalidBounds;
    expect(!bounds::fit_to_aspect(invalidBounds, 1.0).has_value(),
           "reject invalid viewport bounds");


    const auto a3Landscape = layout::paper_size_mm(
        layout::PaperSize::A3, layout::Orientation::Landscape);
    expect(geo::nearly_equal(a3Landscape.width, 420.0) &&
           geo::nearly_equal(a3Landscape.height, 297.0),
           "A3 landscape dimensions");

    layout::PageSetup printPage{
        layout::PaperSize::A3,
        layout::Orientation::Landscape,
        {10.0, 10.0, 10.0, 10.0}
    };
    const auto printable = layout::printable_size_mm(printPage);
    expect(printable.has_value() &&
           geo::nearly_equal(printable->width, 400.0) &&
           geo::nearly_equal(printable->height, 277.0),
           "page printable area");

    bounds::Bounds2 printDrawing;
    printDrawing.include(geo::Vec2{0, 0});
    printDrawing.include(geo::Vec2{20000, 10000});
    const auto printFit = layout::fit_to_page(printDrawing, printPage);
    expect(printFit.has_value() &&
           geo::nearly_equal(printFit->center, {10000, 5000}) &&
           geo::nearly_equal(printFit->scale_denominator, 50.0) &&
           printFit->world_width >= printDrawing.width() &&
           printFit->world_height >= printDrawing.height(),
           "fit drawing to A3 page");

    const auto scale100 = layout::viewport_at_scale(
        {5000, 3000}, printPage, 100.0);
    expect(scale100.has_value() &&
           geo::nearly_equal(scale100->scale_denominator, 100.0) &&
           geo::nearly_equal(scale100->world_width, 40000.0) &&
           geo::nearly_equal(scale100->world_height, 27700.0),
           "fixed 1 to 100 print viewport");

    layout::PageSetup invalidPage = printPage;
    invalidPage.margins.left = 500.0;
    expect(!layout::printable_size_mm(invalidPage).has_value(),
           "reject page margins larger than paper");

    Document updateDoc;
    const EntityId updateId = updateDoc.insert(LineEntity{{{0, 0}, {10, 0}}});
    History updateHistory;
    Entity updatedLine = *updateDoc.find(updateId);
    transform::translate(updatedLine, {5, 7});
    expect(updateHistory.apply(
               updateDoc,
               std::make_unique<UpdateEntityCommand>(updateId, updatedLine)),
           "update entity command apply");
    {
        const auto& segment = std::get<LineEntity>(*updateDoc.find(updateId)).segment;
        expect(geo::nearly_equal(segment.a, {5, 7}) &&
               geo::nearly_equal(segment.b, {15, 7}),
               "update entity command changes geometry");
    }
    expect(updateHistory.undo(updateDoc), "update entity undo");
    {
        const auto& segment = std::get<LineEntity>(*updateDoc.find(updateId)).segment;
        expect(geo::nearly_equal(segment.a, {0, 0}) &&
               geo::nearly_equal(segment.b, {10, 0}),
               "update entity undo restores geometry");
    }
    expect(updateHistory.redo(updateDoc), "update entity redo");
    {
        const auto& segment = std::get<LineEntity>(*updateDoc.find(updateId)).segment;
        expect(geo::nearly_equal(segment.a, {5, 7}) &&
               geo::nearly_equal(segment.b, {15, 7}),
               "update entity redo reapplies geometry");
    }

    Document propertyDoc;
    const EntityId propertyId =
        propertyDoc.insert(LineEntity{{{0, 0}, {5, 0}}});
    const LayerId propertyLayer = propertyDoc.create_layer("PropertyLayer");
    History propertyMutationHistory;

    EntityProperties propertyReplacement = *propertyDoc.properties(propertyId);
    propertyReplacement.layer_id = propertyLayer;
    propertyReplacement.visible = false;
    propertyReplacement.line_weight_override = 0.50;
    expect(propertyMutationHistory.apply(
               propertyDoc,
               std::make_unique<UpdateEntityPropertiesCommand>(
                   propertyId, propertyReplacement)),
           "entity properties command apply");
    expect(propertyDoc.properties(propertyId)->layer_id == propertyLayer &&
           !propertyDoc.properties(propertyId)->visible &&
           propertyDoc.properties(propertyId)->line_weight_override.has_value(),
           "entity properties command changes properties");
    expect(propertyMutationHistory.undo(propertyDoc),
           "entity properties command undo");
    expect(propertyDoc.properties(propertyId)->layer_id == kDefaultLayerId &&
           propertyDoc.properties(propertyId)->visible &&
           !propertyDoc.properties(propertyId)->line_weight_override.has_value(),
           "entity properties undo restores properties");
    expect(propertyMutationHistory.redo(propertyDoc),
           "entity properties command redo");
    expect(propertyDoc.properties(propertyId)->layer_id == propertyLayer &&
           !propertyDoc.properties(propertyId)->visible,
           "entity properties redo reapplies properties");

    Layer layerReplacement = *propertyDoc.layer(propertyLayer);
    layerReplacement.visible = false;
    layerReplacement.locked = true;
    layerReplacement.line_weight = 0.70;
    expect(propertyMutationHistory.apply(
               propertyDoc,
               std::make_unique<UpdateLayerCommand>(
                   propertyLayer, layerReplacement)),
           "layer update command apply");
    expect(!propertyDoc.layer(propertyLayer)->visible &&
           propertyDoc.layer(propertyLayer)->locked &&
           geo::nearly_equal(propertyDoc.layer(propertyLayer)->line_weight, 0.70),
           "layer update command changes layer");
    expect(propertyMutationHistory.undo(propertyDoc), "layer update command undo");
    expect(propertyDoc.layer(propertyLayer)->visible &&
           !propertyDoc.layer(propertyLayer)->locked,
           "layer update undo restores layer");
    expect(propertyMutationHistory.redo(propertyDoc), "layer update command redo");
    expect(!propertyDoc.layer(propertyLayer)->visible &&
           propertyDoc.layer(propertyLayer)->locked,
           "layer update redo reapplies layer");

    EntityProperties invalidPropertyReplacement =
        *propertyDoc.properties(propertyId);
    invalidPropertyReplacement.line_weight_override = -1.0;
    expect(!propertyMutationHistory.apply(
               propertyDoc,
               std::make_unique<UpdateEntityPropertiesCommand>(
                   propertyId, invalidPropertyReplacement)),
           "reject invalid entity line weight command");

    const LayerId duplicateLayer =
        propertyDoc.create_layer("DuplicateLayer");
    Layer invalidLayerReplacement = *propertyDoc.layer(propertyLayer);
    invalidLayerReplacement.name = propertyDoc.layer(duplicateLayer)->name;
    expect(!propertyMutationHistory.apply(
               propertyDoc,
               std::make_unique<UpdateLayerCommand>(
                   propertyLayer, invalidLayerReplacement)),
           "reject duplicate layer name command");
    expect(propertyDoc.layer(propertyLayer)->name == "PropertyLayer",
           "failed layer update keeps original name");

    Document layerHistoryDoc;
    History layerHistory;
    auto createLayerCommand =
        std::make_unique<CreateLayerCommand>("Undoable Layer");
    auto* createLayerPtr = createLayerCommand.get();
    expect(layerHistory.apply(layerHistoryDoc, std::move(createLayerCommand)),
           "create layer history apply");
    const LayerId undoableLayerId = createLayerPtr->id();
    expect(undoableLayerId != 0 &&
           layerHistoryDoc.layer(undoableLayerId) != nullptr,
           "create layer history creates layer");
    expect(layerHistory.undo(layerHistoryDoc),
           "create layer history undo");
    expect(layerHistoryDoc.layer(undoableLayerId) == nullptr,
           "create layer undo removes layer");
    expect(layerHistory.redo(layerHistoryDoc),
           "create layer history redo");
    expect(layerHistoryDoc.layer(undoableLayerId) != nullptr &&
           layerHistoryDoc.layer(undoableLayerId)->name == "Undoable Layer",
           "create layer redo restores same layer");

    Document documentSnapDoc;
    (void)documentSnapDoc.insert(
        LineEntity{{{0, 0}, {10, 10}}});
    (void)documentSnapDoc.insert(
        LineEntity{{{0, 10}, {10, 0}}});
    const auto documentIntersectionSnap =
        snap::best_for_document(
            documentSnapDoc, nullptr, {5.1, 5.1}, 0.5, true);
    expect(documentIntersectionSnap.has_value() &&
           documentIntersectionSnap->kind == snap::Kind::Intersection &&
           geo::nearly_equal(
               documentIntersectionSnap->point, {5, 5}, 1e-8),
           "document snap finds nearby segment intersection");

    const EntityId hiddenSnapId = documentSnapDoc.insert(
        LineEntity{{{100, 100}, {110, 100}}});
    documentSnapDoc.properties(hiddenSnapId)->visible = false;
    const auto hiddenSnap =
        snap::best_for_document(
            documentSnapDoc, nullptr, {100, 100}, 0.25, false);
    expect(!hiddenSnap.has_value(),
           "document snap ignores hidden entities");

    (void)documentSnapDoc.insert(CircleEntity{{{20, 20}, 5.0}});
    const auto centerSnap =
        snap::best_for_document(
            documentSnapDoc, nullptr, {20.1, 20.1}, 0.5, false);
    expect(centerSnap.has_value() &&
           centerSnap->kind == snap::Kind::Center &&
           geo::nearly_equal(centerSnap->point, {20, 20}, 1e-8),
           "document snap finds circle center");

    BlockLibrary documentSnapBlocks;
    const BlockId snapBlockId = documentSnapBlocks.create(
        "SnapBlock",
        {0, 0},
        std::vector<BlockPrimitive>{
            LineEntity{{{0, 0}, {2, 0}}}
        });
    (void)documentSnapDoc.insert(BlockReferenceEntity{
        snapBlockId, {30, 30}, 0.0, 1.0});
    const auto blockEndpointSnap =
        snap::best_for_document(
            documentSnapDoc, &documentSnapBlocks,
            {30.1, 30.0}, 0.25, false);
    expect(blockEndpointSnap.has_value() &&
           blockEndpointSnap->kind == snap::Kind::Endpoint &&
           geo::nearly_equal(
               blockEndpointSnap->point, {30, 30}, 1e-8),
           "document snap supports instantiated block geometry");

    expect(!snap::best_for_document(
               documentSnapDoc, &documentSnapBlocks,
               {0, 0}, -1.0, true).has_value(),
           "document snap rejects invalid aperture");

    Document svgDoc;
    const EntityId svgLineId =
        svgDoc.insert(LineEntity{{{0, 0}, {100, 0}}});
    expect(svgDoc.properties(svgLineId) != nullptr, "svg line properties exist");
    svgDoc.properties(svgLineId)->line_weight_override = 0.50;
    (void)svgDoc.insert(CircleEntity{{{50, 25}, 10.0}});
    (void)svgDoc.insert(ArcEntity{{
        {50, 25}, 20.0, 0.0, std::numbers::pi / 2.0, true}});
    (void)svgDoc.insert(PolylineEntity{{
        {0, 0}, {0, 50}, {40, 50}}, false});
    (void)svgDoc.insert(TextEntity{{5, 15}, "A&B<1>", 3.0, 0.0});
    (void)svgDoc.insert(LinearDimensionEntity{
        {0, 0}, {100, 0}, {0, -10}, std::nullopt});
    (void)svgDoc.insert(HatchEntity{
        {{10, 10}, {30, 10}, {30, 30}, {10, 30}},
        "SOLID", 0.0, 1.0, true});

    BlockLibrary svgBlocks;
    const BlockId svgBlockId = svgBlocks.create(
        "SVG_BLOCK", {0, 0},
        std::vector<BlockPrimitive>{
            LineEntity{{{0, 0}, {5, 5}}},
            CircleEntity{{{5, 5}, 2.0}}
        });
    (void)svgDoc.insert(BlockReferenceEntity{
        svgBlockId, {80, 40}, 0.0, 1.0});

    const EntityId hiddenSvgId =
        svgDoc.insert(TextEntity{{999, 999}, "HIDDEN_SENTINEL", 2.5, 0.0});
    svgDoc.properties(hiddenSvgId)->visible = false;

    const auto svgText = svg::export_document(svgDoc, &svgBlocks, 5.0);
    expect(svgText.has_value(), "svg export succeeds");
    if (svgText.has_value()) {
        expect(svgText->find("<svg") != std::string::npos,
               "svg root emitted");
        expect(svgText->find("<line") != std::string::npos &&
               svgText->find("<circle") != std::string::npos &&
               svgText->find("<path") != std::string::npos &&
               svgText->find("<polyline") != std::string::npos &&
               svgText->find("<polygon") != std::string::npos &&
               svgText->find("<text") != std::string::npos,
               "svg emits supported primitives");
        expect(svgText->find("A&amp;B&lt;1&gt;") != std::string::npos,
               "svg escapes text");
        expect(svgText->find("stroke-width=\"0.5\"") != std::string::npos,
               "svg preserves effective line weight");
        expect(svgText->find("HIDDEN_SENTINEL") == std::string::npos,
               "svg excludes hidden entities");
    }
    expect(!svg::export_document(Document{}).has_value(),
           "svg rejects empty drawing");
    expect(!svg::export_document(svgDoc, &svgBlocks, -1.0).has_value(),
           "svg rejects invalid margin");


    const auto pdfText = pdf::export_document(svgDoc, &svgBlocks);
    expect(pdfText.has_value(), "pdf export succeeds");
    if (pdfText.has_value()) {
        expect(pdfText->starts_with("%PDF-1.4"),
               "pdf header emitted");
        expect(pdfText->find("/Type /Page") != std::string::npos &&
               pdfText->find("xref") != std::string::npos &&
               pdfText->find("startxref") != std::string::npos,
               "pdf document structure emitted");
        expect(pdfText->find(" m ") != std::string::npos &&
               pdfText->find(" l ") != std::string::npos &&
               pdfText->find("BT /F1") != std::string::npos,
               "pdf emits vector geometry and text");
        expect(pdfText->find("HIDDEN_SENTINEL") == std::string::npos,
               "pdf excludes hidden entities");
    }
    expect(!pdf::export_document(Document{}).has_value(),
           "pdf rejects empty drawing");

    Document unicodePdfDoc;
    unicodePdfDoc.insert(TextEntity{{0, 0}, "日本語 Tiếng Việt: Đường kính", 2.5, 0.0});
    unicodePdfDoc.insert(LinearDimensionEntity{
        {0, 0}, {100, 0}, {0, -10}, std::string{"寸法 100"}});
    expect(!pdf::export_document(unicodePdfDoc).has_value(),
           "pdf still rejects Unicode when no bundled font is supplied");

#ifdef ACP_TEST_PDF_FONT_PATH
    std::ifstream pdfFontInput(ACP_TEST_PDF_FONT_PATH, std::ios::binary);
    const std::string pdfFontBytes{
        std::istreambuf_iterator<char>(pdfFontInput),
        std::istreambuf_iterator<char>()};
    expect(!pdfFontBytes.empty(), "bundled PDF font test asset loads");

    const pdf::FontData pdfFont{pdfFontBytes, "NotoSansJP"};
    const auto unicodePdf =
        pdf::export_document(
            unicodePdfDoc, nullptr, {}, std::nullopt, &pdfFont);
    expect(unicodePdf.has_value(),
           "pdf exports Japanese and Vietnamese with embedded Unicode font");
    if (unicodePdf.has_value()) {
        expect(unicodePdf->find("/Subtype /Type0") != std::string::npos &&
               unicodePdf->find("/Subtype /CIDFontType2") != std::string::npos &&
               unicodePdf->find("/FontFile2") != std::string::npos &&
               unicodePdf->find("/ToUnicode") != std::string::npos,
               "pdf embeds Type0 TrueType Unicode font resources");
        expect(unicodePdf->find("<65E5>") != std::string::npos &&
               unicodePdf->find("<672C>") != std::string::npos &&
               unicodePdf->find("<8A9E>") != std::string::npos,
               "pdf ToUnicode map contains Japanese codepoints");
        expect(unicodePdf->find("<0110>") != std::string::npos &&
               unicodePdf->find("<01B0>") != std::string::npos,
               "pdf ToUnicode map contains Vietnamese codepoints");
        expect(unicodePdf->size() > pdfFontBytes.size(),
               "pdf contains embedded font payload");
    }

    const std::string brokenFont{"not a font"};
    const pdf::FontData invalidPdfFont{brokenFont, "Broken"};
    expect(!pdf::export_document(
                unicodePdfDoc, nullptr, {}, std::nullopt, &invalidPdfFont).has_value(),
           "pdf rejects corrupt embedded font input");
#endif

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
