#include "acp/property_edit.hpp"

#include "acp/annotation.hpp"
#include "acp/hatch.hpp"

#include <cmath>
#include <utility>
#include <variant>

namespace acp::property_edit {

std::optional<EntityProperties> line_weight_override(
    const EntityProperties& current,
    std::optional<double> line_weight) {

    if (line_weight.has_value() &&
        (!std::isfinite(*line_weight) || *line_weight < 0.0)) {
        return std::nullopt;
    }

    EntityProperties replacement = current;
    replacement.line_weight_override = line_weight;
    return replacement;
}

std::optional<Entity> text_content(
    const Entity& current,
    std::string utf8_text) {

    if (utf8_text.empty() ||
        !std::holds_alternative<TextEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& text = std::get<TextEntity>(replacement);
    text.text = std::move(utf8_text);
    if (!annotation::valid_text(text)) {
        return std::nullopt;
    }
    return replacement;
}

std::optional<Entity> text_height(
    const Entity& current,
    double height) {

    if (!std::isfinite(height) || height <= geo::kEpsilon ||
        !std::holds_alternative<TextEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& text = std::get<TextEntity>(replacement);
    text.height = height;
    return annotation::valid_text(text)
        ? std::optional<Entity>{std::move(replacement)}
        : std::nullopt;
}

std::optional<Entity> text_rotation(
    const Entity& current,
    double radians) {

    if (!std::isfinite(radians) ||
        !std::holds_alternative<TextEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& text = std::get<TextEntity>(replacement);
    text.rotation = radians;
    return annotation::valid_text(text)
        ? std::optional<Entity>{std::move(replacement)}
        : std::nullopt;
}

std::optional<Entity> dimension_override(
    const Entity& current,
    std::optional<std::string> utf8_text) {

    if (!std::holds_alternative<LinearDimensionEntity>(current)) {
        return std::nullopt;
    }
    if (utf8_text.has_value() && utf8_text->empty()) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& dimension = std::get<LinearDimensionEntity>(replacement);
    dimension.text_override = std::move(utf8_text);
    return annotation::valid_linear_dimension(dimension)
        ? std::optional<Entity>{std::move(replacement)}
        : std::nullopt;
}

std::optional<Entity> hatch_angle(
    const Entity& current,
    double radians) {

    if (!std::isfinite(radians) ||
        !std::holds_alternative<HatchEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& hatch = std::get<HatchEntity>(replacement);
    hatch.angle = radians;
    return hatch::valid(hatch)
        ? std::optional<Entity>{std::move(replacement)}
        : std::nullopt;
}

std::optional<Entity> hatch_spacing(
    const Entity& current,
    double spacing) {

    if (!std::isfinite(spacing) || spacing <= geo::kEpsilon ||
        !std::holds_alternative<HatchEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    auto& hatch = std::get<HatchEntity>(replacement);
    hatch.spacing = spacing;
    return hatch::valid(hatch)
        ? std::optional<Entity>{std::move(replacement)}
        : std::nullopt;
}

std::optional<Entity> block_scale(
    const Entity& current,
    double scale) {

    if (!std::isfinite(scale) || scale <= geo::kEpsilon ||
        !std::holds_alternative<BlockReferenceEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    std::get<BlockReferenceEntity>(replacement).scale = scale;
    return replacement;
}

std::optional<Entity> block_rotation(
    const Entity& current,
    double radians) {

    if (!std::isfinite(radians) ||
        !std::holds_alternative<BlockReferenceEntity>(current)) {
        return std::nullopt;
    }

    Entity replacement = current;
    std::get<BlockReferenceEntity>(replacement).rotation = radians;
    return replacement;
}

} // namespace acp::property_edit
