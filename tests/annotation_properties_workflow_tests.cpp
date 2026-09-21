#include "acp/annotation.hpp"
#include "acp/document.hpp"
#include "acp/hatch.hpp"
#include "acp/history.hpp"
#include "acp/persistence.hpp"

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

    // TEXT property edits must be undoable and persistent.
    auto addText = std::make_unique<AddEntityCommand>(
        TextEntity{{10.0, 20.0}, "NOTE", 2.5, 0.0});
    auto* addTextPtr = addText.get();
    expect(history.apply(document, std::move(addText)), "text create");
    const EntityId textId = addTextPtr->id();

    Entity textReplacement = *document.find(textId);
    auto& text = std::get<TextEntity>(textReplacement);
    text.height = 5.0;
    text.rotation = std::numbers::pi / 12.0;
    expect(annotation::valid_text(text), "edited text valid");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(textId, textReplacement)),
           "text property edit");
    expect(history.undo(document), "text property undo");
    {
        const auto& restored = std::get<TextEntity>(*document.find(textId));
        expect(restored.height == 2.5 && restored.rotation == 0.0,
               "text property undo values");
    }
    expect(history.redo(document), "text property redo");

    // DIMENSION line-position edit.
    auto addDimension = std::make_unique<AddEntityCommand>(
        LinearDimensionEntity{
            {0.0, 0.0}, {100.0, 0.0}, {0.0, 15.0}, std::nullopt});
    auto* addDimensionPtr = addDimension.get();
    expect(history.apply(document, std::move(addDimension)), "dimension create");
    const EntityId dimensionId = addDimensionPtr->id();

    Entity dimensionReplacement = *document.find(dimensionId);
    auto& dimension =
        std::get<LinearDimensionEntity>(dimensionReplacement);
    dimension.line_point = {0.0, 20.0};
    expect(annotation::valid_linear_dimension(dimension),
           "edited dimension valid");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(
                   dimensionId, dimensionReplacement)),
           "dimension property edit");
    expect(history.undo(document), "dimension property undo");
    expect(history.redo(document), "dimension property redo");

    // HATCH display-property edits.
    auto addHatch = std::make_unique<AddEntityCommand>(
        HatchEntity{
            {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}},
            "ANSI31", 0.0, 1.0, false});
    auto* addHatchPtr = addHatch.get();
    expect(history.apply(document, std::move(addHatch)), "hatch create");
    const EntityId hatchId = addHatchPtr->id();

    Entity hatchReplacement = *document.find(hatchId);
    auto& hatchValue = std::get<HatchEntity>(hatchReplacement);
    hatchValue.solid = true;
    hatchValue.angle = std::numbers::pi / 4.0;
    hatchValue.spacing = 2.0;
    expect(hatch::valid(hatchValue), "edited hatch valid");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(
                   hatchId, hatchReplacement)),
           "hatch property edit");
    expect(history.undo(document), "hatch property undo");
    {
        const auto& restored = std::get<HatchEntity>(*document.find(hatchId));
        expect(!restored.solid &&
               restored.angle == 0.0 &&
               restored.spacing == 1.0,
               "hatch property undo values");
    }
    expect(history.redo(document), "hatch property redo");

    // Generic entity properties also remain part of the same history model.
    EntityProperties properties = *document.properties(textId);
    properties.visible = false;
    properties.line_weight_override = 0.50;
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityPropertiesCommand>(
                   textId, properties)),
           "generic property edit");
    expect(history.undo(document), "generic property undo");
    expect(history.redo(document), "generic property redo");

    const std::string serialized =
        persistence::serialize_project(document, blocks);
    const auto loaded = persistence::deserialize_project(serialized);
    expect(loaded.has_value(), "annotation property save/open");
    if (loaded.has_value()) {
        const auto& loadedText =
            std::get<TextEntity>(*loaded->document.find(textId));
        expect(loadedText.height == 5.0, "text height persisted");
        expect(loadedText.rotation == std::numbers::pi / 12.0,
               "text rotation persisted");

        const auto& loadedDimension =
            std::get<LinearDimensionEntity>(
                *loaded->document.find(dimensionId));
        expect(loadedDimension.line_point.y == 20.0,
               "dimension line point persisted");

        const auto& loadedHatch =
            std::get<HatchEntity>(*loaded->document.find(hatchId));
        expect(loadedHatch.solid &&
               loadedHatch.spacing == 2.0,
               "hatch properties persisted");

        const auto* loadedProps = loaded->document.properties(textId);
        expect(loadedProps != nullptr &&
               !loadedProps->visible &&
               loadedProps->line_weight_override.has_value(),
               "generic properties persisted");
    }

    if (failures != 0) {
        std::cerr << failures
                  << " annotation/property regression test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All annotation/property regression tests passed\n";
    return EXIT_SUCCESS;
}
