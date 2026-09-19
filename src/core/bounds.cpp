#include "acp/bounds.hpp"

#include "acp/annotation.hpp"
#include "acp/hatch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>
#include <variant>

namespace acp::bounds {

namespace {

Bounds2 from_points(const std::vector<geo::Vec2>& points) noexcept {
    Bounds2 result;
    for (const auto point : points) {
        result.include(point);
    }
    return result;
}

Bounds2 primitive_bounds(const BlockPrimitive& primitive) noexcept {
    return std::visit([](const auto& value) -> Bounds2 {
        using T = std::decay_t<decltype(value)>;
        Bounds2 result;

        if constexpr (std::is_same_v<T, LineEntity>) {
            result.include(value.segment.a);
            result.include(value.segment.b);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            if (std::isfinite(value.circle.radius) && value.circle.radius >= 0.0) {
                result.include(geo::Vec2{
                    value.circle.center.x - value.circle.radius,
                    value.circle.center.y - value.circle.radius
                });
                result.include(geo::Vec2{
                    value.circle.center.x + value.circle.radius,
                    value.circle.center.y + value.circle.radius
                });
            }
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            if (!geo::valid_arc(value.arc)) {
                return result;
            }

            result.include(geo::arc_start_point(value.arc));
            result.include(geo::arc_end_point(value.arc));

            constexpr std::array<double, 4> quadrants{
                0.0,
                std::numbers::pi / 2.0,
                std::numbers::pi,
                3.0 * std::numbers::pi / 2.0
            };

            for (const double angle : quadrants) {
                if (!geo::angle_on_arc(value.arc, angle)) {
                    continue;
                }
                result.include(geo::Vec2{
                    value.arc.center.x + value.arc.radius * std::cos(angle),
                    value.arc.center.y + value.arc.radius * std::sin(angle)
                });
            }
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            result = from_points(value.points);
        }

        return result;
    }, primitive);
}

Bounds2 text_bounds(const TextEntity& text) noexcept {
    Bounds2 result;
    if (!annotation::valid_text(text)) {
        return result;
    }

    const double width = annotation::estimated_text_width(text);
    const double c = std::cos(text.rotation);
    const double s = std::sin(text.rotation);
    const geo::Vec2 along{c * width, s * width};
    const geo::Vec2 up{-s * text.height, c * text.height};

    result.include(text.position);
    result.include(text.position + along);
    result.include(text.position + up);
    result.include(text.position + along + up);
    return result;
}

Bounds2 dimension_bounds(const LinearDimensionEntity& dimension) noexcept {
    Bounds2 result;
    if (!annotation::valid_linear_dimension(dimension)) {
        return result;
    }

    const std::array<geo::Segment, 3> segments{
        annotation::dimension_line(dimension),
        annotation::first_extension_line(dimension),
        annotation::second_extension_line(dimension)
    };

    for (const auto& segment : segments) {
        result.include(segment.a);
        result.include(segment.b);
    }
    return result;
}

} // namespace

void Bounds2::include(geo::Vec2 point) noexcept {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return;
    }

    if (!valid) {
        min = point;
        max = point;
        valid = true;
        return;
    }

    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
}

void Bounds2::include(const Bounds2& other) noexcept {
    if (!other.valid) {
        return;
    }
    include(other.min);
    include(other.max);
}

double Bounds2::width() const noexcept {
    return valid ? max.x - min.x : 0.0;
}

double Bounds2::height() const noexcept {
    return valid ? max.y - min.y : 0.0;
}

geo::Vec2 Bounds2::center() const noexcept {
    return valid ? geo::Vec2{(min.x + max.x) * 0.5, (min.y + max.y) * 0.5} : geo::Vec2{};
}

std::optional<Bounds2> entity_bounds(
    const Entity& entity,
    const BlockLibrary* blocks) noexcept {

    Bounds2 result = std::visit([&](const auto& value) -> Bounds2 {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, LineEntity> ||
                      std::is_same_v<T, CircleEntity> ||
                      std::is_same_v<T, ArcEntity> ||
                      std::is_same_v<T, PolylineEntity>) {
            return primitive_bounds(BlockPrimitive{value});
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            Bounds2 block_bounds;
            if (blocks == nullptr) {
                return block_bounds;
            }

            const auto geometry = blocks->instantiate(value);
            for (const auto& primitive : geometry) {
                block_bounds.include(primitive_bounds(primitive));
            }
            return block_bounds;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            return text_bounds(value);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            return dimension_bounds(value);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            return from_points(value.boundary);
        } else {
            return {};
        }
    }, entity);

    return result.valid ? std::optional<Bounds2>{result} : std::nullopt;
}

std::optional<Bounds2> drawing_bounds(
    const Document& document,
    const BlockLibrary* blocks) noexcept {

    Bounds2 result;

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }

        const Entity* entity = document.find(id);
        if (entity == nullptr) {
            continue;
        }

        const auto current = entity_bounds(*entity, blocks);
        if (current.has_value()) {
            result.include(*current);
        }
    }

    return result.valid ? std::optional<Bounds2>{result} : std::nullopt;
}

std::optional<ViewportFit> fit_to_aspect(
    const Bounds2& source,
    double viewport_aspect,
    double margin_fraction) noexcept {

    if (!source.valid ||
        !std::isfinite(viewport_aspect) ||
        viewport_aspect <= geo::kEpsilon ||
        !std::isfinite(margin_fraction) ||
        margin_fraction < 0.0) {
        return std::nullopt;
    }

    const double margin_scale = 1.0 + 2.0 * margin_fraction;
    double width = std::max(source.width(), geo::kEpsilon) * margin_scale;
    double height = std::max(source.height(), geo::kEpsilon) * margin_scale;

    if (width / height < viewport_aspect) {
        width = height * viewport_aspect;
    } else {
        height = width / viewport_aspect;
    }

    return ViewportFit{source.center(), width, height};
}

} // namespace acp::bounds
