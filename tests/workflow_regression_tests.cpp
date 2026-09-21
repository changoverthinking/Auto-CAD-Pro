#include "acp/document.hpp"
#include "acp/history.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
#include <variant>

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

    Document document;
    BlockLibrary blocks;
    History history;

    // LINE — Create -> Select -> Modify -> Undo -> Redo.
    auto addLine = std::make_unique<AddEntityCommand>(
        LineEntity{{{0.0, 0.0}, {100.0, 0.0}}});
    auto* addLinePtr = addLine.get();
    expect(history.apply(document, std::move(addLine)), "line create");
    const EntityId lineId = addLinePtr->id();

    const auto lineHit = selection::hit_test(document, {50.0, 0.2}, 1.0);
    expect(lineHit.has_value() && lineHit->id == lineId, "line select");

    Entity movedLine = *document.find(lineId);
    transform::translate(movedLine, {25.0, 10.0});
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(lineId, movedLine)),
           "line modify");
    {
        const auto& line = std::get<LineEntity>(*document.find(lineId)).segment;
        expect(geo::nearly_equal(line.a, {25.0, 10.0}) &&
               geo::nearly_equal(line.b, {125.0, 10.0}),
               "line modified geometry");
    }
    expect(history.undo(document), "line undo");
    {
        const auto& line = std::get<LineEntity>(*document.find(lineId)).segment;
        expect(geo::nearly_equal(line.a, {0.0, 0.0}) &&
               geo::nearly_equal(line.b, {100.0, 0.0}),
               "line undo geometry");
    }
    expect(history.redo(document), "line redo");

    // CIRCLE — Create -> Select -> Scale -> Undo -> Redo.
    auto addCircle = std::make_unique<AddEntityCommand>(
        CircleEntity{{{200.0, 50.0}, 20.0}});
    auto* addCirclePtr = addCircle.get();
    expect(history.apply(document, std::move(addCircle)), "circle create");
    const EntityId circleId = addCirclePtr->id();

    const auto circleHit = selection::hit_test(document, {220.2, 50.0}, 1.0);
    expect(circleHit.has_value() && circleHit->id == circleId, "circle select");

    Entity scaledCircle = *document.find(circleId);
    expect(transform::scale_uniform(scaledCircle, {200.0, 50.0}, 2.0),
           "circle scale transform");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(circleId, scaledCircle)),
           "circle modify");
    expect(geo::nearly_equal(
               std::get<CircleEntity>(*document.find(circleId)).circle.radius,
               40.0),
           "circle modified geometry");
    expect(history.undo(document), "circle undo");
    expect(geo::nearly_equal(
               std::get<CircleEntity>(*document.find(circleId)).circle.radius,
               20.0),
           "circle undo geometry");
    expect(history.redo(document), "circle redo");

    // RECTANGLE representation — closed Polyline.
    auto addRectangle = std::make_unique<AddEntityCommand>(
        PolylineEntity{{
            {300.0, 0.0},
            {360.0, 0.0},
            {360.0, 40.0},
            {300.0, 40.0}}, true});
    auto* addRectanglePtr = addRectangle.get();
    expect(history.apply(document, std::move(addRectangle)), "rectangle create");
    const EntityId rectangleId = addRectanglePtr->id();

    const auto rectangleHit = selection::hit_test(document, {330.0, 0.3}, 1.0);
    expect(rectangleHit.has_value() && rectangleHit->id == rectangleId,
           "rectangle select");

    Entity mirroredRectangle = *document.find(rectangleId);
    expect(transform::mirror(
               mirroredRectangle,
               {{300.0, -100.0}, {300.0, 100.0}}),
           "rectangle mirror transform");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(
                   rectangleId, mirroredRectangle)),
           "rectangle modify");
    {
        const auto& poly = std::get<PolylineEntity>(*document.find(rectangleId));
        expect(poly.closed && poly.points.size() == 4 &&
               geo::nearly_equal(poly.points[1], {240.0, 0.0}),
               "rectangle modified geometry");
    }
    expect(history.undo(document), "rectangle undo");
    expect(history.redo(document), "rectangle redo");

    // ARC — Create -> Select -> Rotate -> Undo -> Redo.
    auto addArc = std::make_unique<AddEntityCommand>(
        ArcEntity{{{450.0, 50.0}, 30.0, 0.0, std::numbers::pi / 2.0, true}});
    auto* addArcPtr = addArc.get();
    expect(history.apply(document, std::move(addArc)), "arc create");
    const EntityId arcId = addArcPtr->id();

    const auto arcHit = selection::hit_test(document, {480.2, 50.0}, 1.0);
    expect(arcHit.has_value() && arcHit->id == arcId, "arc select");

    Entity rotatedArc = *document.find(arcId);
    transform::rotate(rotatedArc, {450.0, 50.0}, std::numbers::pi / 2.0);
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(arcId, rotatedArc)),
           "arc modify");
    expect(history.undo(document), "arc undo");
    expect(history.redo(document), "arc redo");

    // DELETE — remove one entity, then verify Undo/Redo does not affect others.
    expect(history.apply(
               document,
               std::make_unique<RemoveEntityCommand>(lineId)),
           "delete selected line");
    expect(document.find(lineId) == nullptr &&
           document.find(circleId) != nullptr &&
           document.find(rectangleId) != nullptr &&
           document.find(arcId) != nullptr,
           "delete isolates selected entity");
    expect(history.undo(document), "delete undo");
    expect(document.find(lineId) != nullptr, "delete undo restores line");
    expect(history.redo(document), "delete redo");
    expect(document.find(lineId) == nullptr, "delete redo removes line");
    expect(history.undo(document), "restore line before save");

    // BLOCK — create from selected geometry as one project-history transaction,
    // then insert a second reference and prove Undo/Redo + persistence.
    Document blockDocument;
    BlockLibrary blockLibrary;
    History blockHistory;

    const EntityId blockSourceId =
        blockDocument.insert(LineEntity{{{10.0, 10.0}, {30.0, 10.0}}});
    const LayerId blockLayer =
        blockDocument.create_layer("Blocks");
    expect(blockLayer != 0 &&
           blockDocument.set_entity_layer(blockSourceId, blockLayer),
           "block source layer setup");
    blockDocument.properties(blockSourceId)->line_weight_override = 0.50;

    auto createBlock =
        std::make_unique<CreateBlockFromEntityCommand>(
            blockSourceId, "Symbol A");
    auto* createBlockPtr = createBlock.get();
    expect(blockHistory.apply(
               blockDocument, blockLibrary, std::move(createBlock)),
           "block create project transaction");
    const BlockId createdBlockId = createBlockPtr->block_id();
    expect(createdBlockId != 0 &&
           blockLibrary.find(createdBlockId) != nullptr,
           "block definition created");
    expect(std::holds_alternative<BlockReferenceEntity>(
               *blockDocument.find(blockSourceId)),
           "source geometry replaced by block reference");
    if (const auto* props = blockDocument.properties(blockSourceId)) {
        expect(props->layer_id == blockLayer &&
               props->line_weight_override.has_value() &&
               geo::nearly_equal(*props->line_weight_override, 0.50),
               "block conversion preserves entity properties");
    } else {
        expect(false, "block conversion properties exist");
    }

    expect(blockHistory.undo(blockDocument, blockLibrary),
           "block create undo");
    expect(blockLibrary.find(createdBlockId) == nullptr &&
           std::holds_alternative<LineEntity>(
               *blockDocument.find(blockSourceId)),
           "block undo restores primitive and removes definition");

    expect(blockHistory.redo(blockDocument, blockLibrary),
           "block create redo");
    expect(blockLibrary.find(createdBlockId) != nullptr &&
           std::get<BlockReferenceEntity>(
               *blockDocument.find(blockSourceId)).block_id ==
               createdBlockId,
           "block redo restores same definition id");

    auto insertBlock = std::make_unique<AddEntityCommand>(
        BlockReferenceEntity{
            createdBlockId, {100.0, 50.0}, std::numbers::pi / 4.0, 2.0});
    auto* insertBlockPtr = insertBlock.get();
    expect(blockHistory.apply(
               blockDocument, blockLibrary, std::move(insertBlock)),
           "insert second block reference");
    const EntityId secondBlockRefId = insertBlockPtr->id();
    expect(secondBlockRefId != 0 &&
           std::holds_alternative<BlockReferenceEntity>(
               *blockDocument.find(secondBlockRefId)),
           "second block reference exists");

    const std::string blockSerialized =
        persistence::serialize_project(blockDocument, blockLibrary);
    const auto blockLoaded =
        persistence::deserialize_project(blockSerialized);
    expect(blockLoaded.has_value(),
           "block workflow save/open parses");
    if (blockLoaded.has_value()) {
        expect(blockLoaded->blocks.find(createdBlockId) != nullptr,
               "block definition persisted");
        expect(blockLoaded->document.find(blockSourceId) != nullptr &&
               blockLoaded->document.find(secondBlockRefId) != nullptr &&
               std::holds_alternative<BlockReferenceEntity>(
                   *blockLoaded->document.find(blockSourceId)) &&
               std::holds_alternative<BlockReferenceEntity>(
                   *blockLoaded->document.find(secondBlockRefId)),
               "block references persisted");
        const auto instantiated = blockLoaded->blocks.instantiate(
            std::get<BlockReferenceEntity>(
                *blockLoaded->document.find(secondBlockRefId)));
        expect(instantiated.size() == 1 &&
               std::holds_alternative<LineEntity>(instantiated.front()),
               "loaded block instantiates geometry");
    }

    // SAVE -> OPEN — complete drawing round trip after all modifications.
    const std::string serialized =
        persistence::serialize_project(document, blocks);
    const auto loaded = persistence::deserialize_project(serialized);
    expect(loaded.has_value(), "workflow save/open parse");
    if (loaded.has_value()) {
        expect(loaded->document.size() == document.size(),
               "workflow save/open entity count");
        expect(loaded->document.find(lineId) != nullptr,
               "workflow save/open line id");
        expect(loaded->document.find(circleId) != nullptr,
               "workflow save/open circle id");
        expect(loaded->document.find(rectangleId) != nullptr,
               "workflow save/open rectangle id");
        expect(loaded->document.find(arcId) != nullptr,
               "workflow save/open arc id");

        const auto& loadedLine =
            std::get<LineEntity>(*loaded->document.find(lineId)).segment;
        expect(geo::nearly_equal(loadedLine.a, {25.0, 10.0}) &&
               geo::nearly_equal(loadedLine.b, {125.0, 10.0}),
               "workflow save/open modified line geometry");

        const auto& loadedCircle =
            std::get<CircleEntity>(*loaded->document.find(circleId)).circle;
        expect(geo::nearly_equal(loadedCircle.radius, 40.0),
               "workflow save/open modified circle geometry");

        const auto& loadedRectangle =
            std::get<PolylineEntity>(*loaded->document.find(rectangleId));
        expect(loadedRectangle.closed &&
               loadedRectangle.points.size() == 4 &&
               geo::nearly_equal(loadedRectangle.points[1], {240.0, 0.0}),
               "workflow save/open modified rectangle geometry");

        const auto& loadedArc =
            std::get<ArcEntity>(*loaded->document.find(arcId)).arc;
        expect(geo::nearly_equal(
                   loadedArc.start_angle,
                   std::numbers::pi / 2.0,
                   1e-8),
               "workflow save/open modified arc geometry");
    }

    if (failures != 0) {
        std::cerr << failures << " workflow regression test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All 2D workflow regression tests passed\n";
    return EXIT_SUCCESS;
}
