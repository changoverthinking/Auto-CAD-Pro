#pragma once

#include "acp/geometry2d.hpp"

#include <optional>
#include <string>

namespace acp {

struct TextEntity {
    geo::Vec2 position{};
    std::string text;
    double height{2.5};
    double rotation{};
};

struct LinearDimensionEntity {
    geo::Vec2 first{};
    geo::Vec2 second{};
    geo::Vec2 line_point{};
    std::optional<std::string> text_override;
};

namespace annotation {

[[nodiscard]] bool valid_text(const TextEntity& text) noexcept;
[[nodiscard]] double estimated_text_width(const TextEntity& text) noexcept;
[[nodiscard]] geo::Segment text_baseline(const TextEntity& text) noexcept;

[[nodiscard]] bool valid_linear_dimension(const LinearDimensionEntity& dimension) noexcept;
[[nodiscard]] double measurement(const LinearDimensionEntity& dimension) noexcept;
[[nodiscard]] geo::Segment dimension_line(const LinearDimensionEntity& dimension) noexcept;
[[nodiscard]] geo::Segment first_extension_line(const LinearDimensionEntity& dimension) noexcept;
[[nodiscard]] geo::Segment second_extension_line(const LinearDimensionEntity& dimension) noexcept;

} // namespace annotation
} // namespace acp
