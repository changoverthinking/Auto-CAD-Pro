#pragma once

#include "acp/document.hpp"

#include <optional>
#include <string>

namespace acp::property_edit {

[[nodiscard]] std::optional<EntityProperties> line_weight_override(
    const EntityProperties& current,
    std::optional<double> line_weight);

[[nodiscard]] std::optional<Entity> text_content(
    const Entity& current,
    std::string utf8_text);

[[nodiscard]] std::optional<Entity> text_height(
    const Entity& current,
    double height);

[[nodiscard]] std::optional<Entity> text_rotation(
    const Entity& current,
    double radians);

[[nodiscard]] std::optional<Entity> dimension_override(
    const Entity& current,
    std::optional<std::string> utf8_text);

[[nodiscard]] std::optional<Entity> hatch_angle(
    const Entity& current,
    double radians);

[[nodiscard]] std::optional<Entity> hatch_spacing(
    const Entity& current,
    double spacing);

[[nodiscard]] std::optional<Entity> block_scale(
    const Entity& current,
    double scale);

[[nodiscard]] std::optional<Entity> block_rotation(
    const Entity& current,
    double radians);

} // namespace acp::property_edit
