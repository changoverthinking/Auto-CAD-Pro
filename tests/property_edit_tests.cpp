#include "acp/history.hpp"
#include "acp/property_edit.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
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
    History history;

    auto add_text = std::make_unique<AddEntityCommand>(
        TextEntity{{10.0, 20.0}, "NOTE", 2.5, 0.0});
    auto* add_text_ptr = add_text.get();
    expect(history.apply(document, std::move(add_text)), "create text");
    const EntityId text_id = add_text_ptr->id();
    const Entity* text_entity = document.find(text_id);
    expect(text_entity != nullptr, "find text");

    if (text_entity != nullptr) {
        const auto content = property_edit::text_content(*text_entity, "EDITED-CAD");
        expect(content.has_value(), "edit text content");
        expect(
            !property_edit::text_content(*text_entity, "").has_value(),
            "reject empty text content");
        expect(
            !property_edit::text_height(*text_entity, 0.0).has_value(),
            "reject zero text height");
        expect(
            !property_edit::text_height(
                *text_entity,
                std::numeric_limits<double>::infinity()).has_value(),
            "reject infinite text height");

        const auto height = property_edit::text_height(*text_entity, 5.0);
        expect(height.has_value(), "edit text height");
        if (height.has_value()) {
            expect(history.apply(
                       document,
                       std::make_unique<UpdateEntityCommand>(
                           text_id, *height)),
                   "apply text height");
            expect(
                std::get<TextEntity>(*document.find(text_id)).height == 5.0,
                "text height applied");
            expect(history.undo(document), "undo text height");
            expect(
                std::get<TextEntity>(*document.find(text_id)).height == 2.5,
                "text height restored");
            expect(history.redo(document), "redo text height");
            expect(
                std::get<TextEntity>(*document.find(text_id)).height == 5.0,
                "text height redone");
        }

        const auto rotation = property_edit::text_rotation(
            *document.find(text_id), std::numbers::pi / 4.0);
        expect(rotation.has_value(), "edit text rotation");
    }

    EntityProperties base_properties{};
    expect(
        property_edit::line_weight_override(base_properties, 0.50).has_value(),
        "valid line weight");
    expect(
        property_edit::line_weight_override(base_properties, std::nullopt)
            .has_value(),
        "ByLayer line weight");
    expect(
        !property_edit::line_weight_override(base_properties, -0.01)
             .has_value(),
        "reject negative line weight");
    expect(
        !property_edit::line_weight_override(
             base_properties,
             std::numeric_limits<double>::quiet_NaN())
             .has_value(),
        "reject NaN line weight");

    Entity dimension = LinearDimensionEntity{
        {0.0, 0.0}, {100.0, 0.0}, {0.0, 15.0}, std::nullopt};
    expect(
        property_edit::dimension_override(
            dimension, std::optional<std::string>{"100 TYP"})
            .has_value(),
        "dimension override");
    expect(
        property_edit::dimension_override(dimension, std::nullopt).has_value(),
        "clear dimension override");
    expect(
        !property_edit::dimension_override(
             dimension, std::optional<std::string>{""})
             .has_value(),
        "reject empty explicit dimension override");

    Entity hatch = HatchEntity{
        {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}},
        "ANSI31", 0.0, 1.0, false};
    expect(
        property_edit::hatch_angle(hatch, std::numbers::pi / 4.0).has_value(),
        "hatch angle");
    expect(
        property_edit::hatch_spacing(hatch, 2.0).has_value(),
        "hatch spacing");
    expect(
        !property_edit::hatch_spacing(hatch, 0.0).has_value(),
        "reject zero hatch spacing");

    Entity block = BlockReferenceEntity{1, {0.0, 0.0}, 0.0, 1.0};
    expect(property_edit::block_scale(block, 2.0).has_value(), "block scale");
    expect(
        !property_edit::block_scale(block, 0.0).has_value(),
        "reject zero block scale");
    expect(
        property_edit::block_rotation(block, std::numbers::pi / 2.0)
            .has_value(),
        "block rotation");

    expect(
        !property_edit::text_height(block, 3.0).has_value(),
        "reject wrong entity type");

    if (failures != 0) {
        std::cerr << failures << " property edit core test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All property edit core tests passed\n";
    return EXIT_SUCCESS;
}
