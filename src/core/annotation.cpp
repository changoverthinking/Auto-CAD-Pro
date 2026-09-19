#include "acp/annotation.hpp"

#include <cmath>

namespace acp::annotation {

bool valid_text(const TextEntity& text) noexcept {
    return !text.text.empty() &&
           std::isfinite(text.height) &&
           text.height > geo::kEpsilon &&
           std::isfinite(text.rotation);
}

double estimated_text_width(const TextEntity& text) noexcept {
    if (!valid_text(text)) {
        return 0.0;
    }
    return text.height * 0.6 * static_cast<double>(text.text.size());
}

geo::Segment text_baseline(const TextEntity& text) noexcept {
    if (!valid_text(text)) {
        return {text.position, text.position};
    }

    const double width = estimated_text_width(text);
    const geo::Vec2 direction{std::cos(text.rotation), std::sin(text.rotation)};
    return {text.position, text.position + direction * width};
}

bool valid_linear_dimension(const LinearDimensionEntity& dimension) noexcept {
    return geo::distance(dimension.first, dimension.second) > geo::kEpsilon;
}

double measurement(const LinearDimensionEntity& dimension) noexcept {
    return valid_linear_dimension(dimension)
        ? geo::distance(dimension.first, dimension.second)
        : 0.0;
}

geo::Segment dimension_line(const LinearDimensionEntity& dimension) noexcept {
    if (!valid_linear_dimension(dimension)) {
        return {dimension.first, dimension.first};
    }

    const geo::Vec2 delta = dimension.second - dimension.first;
    const double len = geo::length(delta);
    const geo::Vec2 normal{-delta.y / len, delta.x / len};
    const double offset = geo::dot(dimension.line_point - dimension.first, normal);

    return {
        dimension.first + normal * offset,
        dimension.second + normal * offset
    };
}

geo::Segment first_extension_line(const LinearDimensionEntity& dimension) noexcept {
    const auto dim = dimension_line(dimension);
    return {dimension.first, dim.a};
}

geo::Segment second_extension_line(const LinearDimensionEntity& dimension) noexcept {
    const auto dim = dimension_line(dimension);
    return {dimension.second, dim.b};
}

} // namespace acp::annotation
