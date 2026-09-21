#include "acp/document.hpp"
#include "acp/dxf.hpp"
#include "acp/history.hpp"
#include "acp/pdf.hpp"
#include "acp/persistence.hpp"
#include "acp/property_edit.hpp"
#include "acp/svg.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

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

    const LayerId styled = document.create_layer("Styled");
    expect(styled != 0, "create styled layer");
    expect(document.set_layer_color(styled, RgbColor{120, 80, 40}),
           "set layer color");
    expect(document.set_layer_line_type(styled, LineType::Dashed),
           "set layer linetype");

    auto add = std::make_unique<AddEntityCommand>(
        LineEntity{{{0.0, 0.0}, {100.0, 0.0}}});
    auto* add_ptr = add.get();
    expect(history.apply(document, std::move(add)), "create styled line");
    const EntityId id = add_ptr->id();
    expect(document.set_entity_layer(id, styled), "assign styled layer");

    expect(document.effective_color(id) == RgbColor{120, 80, 40},
           "ByLayer color resolves");
    expect(document.effective_line_type(id) == LineType::Dashed,
           "ByLayer linetype resolves");

    const EntityProperties* initial = document.properties(id);
    expect(initial != nullptr, "line properties exist");
    if (initial != nullptr) {
        auto color = property_edit::color_override(
            *initial, RgbColor{255, 0, 0});
        auto type = property_edit::line_type_override(
            *initial, LineType::Center);
        expect(color.has_value(), "valid color override");
        expect(type.has_value(), "valid linetype override");

        if (color.has_value()) {
            color->line_type_override = LineType::Center;
            expect(history.apply(
                       document,
                       std::make_unique<UpdateEntityPropertiesCommand>(
                           id, *color)),
                   "apply entity style override");
            expect(document.effective_color(id) == RgbColor{255, 0, 0},
                   "entity color override resolves");
            expect(document.effective_line_type(id) == LineType::Center,
                   "entity linetype override resolves");
            expect(history.undo(document), "undo entity style override");
            expect(document.effective_color(id) == RgbColor{120, 80, 40},
                   "undo restores ByLayer color");
            expect(history.redo(document), "redo entity style override");
        }

        expect(
            !property_edit::line_type_override(
                 *initial, static_cast<LineType>(255)).has_value(),
            "reject invalid linetype enum");
        expect(
            property_edit::color_override(*initial, std::nullopt).has_value(),
            "clear color override");
        expect(
            property_edit::line_type_override(*initial, std::nullopt).has_value(),
            "clear linetype override");
    }

    const std::string saved =
        persistence::serialize_project(document, blocks);
    const auto loaded = persistence::deserialize_project(saved);
    expect(loaded.has_value(), "Color/Linetype project roundtrip");
    if (loaded.has_value()) {
        const Layer* loaded_layer = loaded->document.layer(styled);
        expect(
            loaded_layer != nullptr &&
            loaded_layer->color == RgbColor{120, 80, 40} &&
            loaded_layer->line_type == LineType::Dashed,
            "layer style persists");
        expect(
            loaded->document.effective_color(id) == RgbColor{255, 0, 0} &&
            loaded->document.effective_line_type(id) == LineType::Center,
            "entity style persists");
    }

    const std::string dxf_text = dxf::export_ascii(document);
    expect(!dxf_text.empty(), "DXF Color/Linetype export");
    expect(dxf_text.find("420\n16711680\n") != std::string::npos,
           "DXF writes truecolor");
    expect(dxf_text.find("6\nCENTER\n") != std::string::npos,
           "DXF writes linetype");

    const auto svg_text = svg::export_document(document, &blocks);
    expect(svg_text.has_value(), "SVG Color/Linetype export");
    if (svg_text.has_value()) {
        expect(svg_text->find("#FF0000") != std::string::npos,
               "SVG writes color");
        expect(svg_text->find("18 6 3 6") != std::string::npos,
               "SVG writes center dash pattern");
    }

    const auto pdf_text = pdf::export_document(document, &blocks);
    expect(pdf_text.has_value(), "PDF Color/Linetype export");
    if (pdf_text.has_value()) {
        expect(pdf_text->find("1 0 0 RG 1 0 0 rg") != std::string::npos,
               "PDF writes color");
        expect(pdf_text->find("[14 5 3 5] 0 d") != std::string::npos,
               "PDF writes center dash pattern");
    }

    expect(
        persistence::deserialize_project(
            "ACP2D 1\n"
            "L 1 \"0\" 1 0 0.25\n"
            "LX 1 300 0 0 0\n"
            "END\n") == std::nullopt,
        "reject invalid persisted RGB");

    if (failures != 0) {
        std::cerr << failures << " Color/Linetype test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Color/Linetype tests passed\n";
    return EXIT_SUCCESS;
}
