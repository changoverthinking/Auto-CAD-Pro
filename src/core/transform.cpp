#include "acp/transform.hpp"

#include <cmath>
#include <type_traits>
#include <variant>

namespace acp::transform {

geo::Vec2 rotate_point(geo::Vec2 point, geo::Vec2 origin, double radians) noexcept {
    const auto local = point - origin;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {
        origin.x + local.x * c - local.y * s,
        origin.y + local.x * s + local.y * c
    };
}

geo::Vec2 scale_point(geo::Vec2 point, geo::Vec2 origin, double factor) noexcept {
    return origin + (point - origin) * factor;
}

geo::Vec2 mirror_point(geo::Vec2 point, geo::Segment axis) noexcept {
    const geo::Vec2 direction = axis.b - axis.a;
    const double denom = geo::dot(direction, direction);
    if (denom <= geo::kEpsilon) return point;
    const double t = geo::dot(point - axis.a, direction) / denom;
    const geo::Vec2 projection = axis.a + direction * t;
    return projection * 2.0 - point;
}

void translate(Entity& entity, geo::Vec2 delta) noexcept {
    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = value.segment.a + delta;
            value.segment.b = value.segment.b + delta;
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = value.circle.center + delta;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            value.arc.center = value.arc.center + delta;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) point = point + delta;
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            value.insertion_point = value.insertion_point + delta;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            value.position = value.position + delta;
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            value.first = value.first + delta;
            value.second = value.second + delta;
            value.line_point = value.line_point + delta;
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            for (auto& point : value.boundary) point = point + delta;
        }
    }, entity);
}

void rotate(Entity& entity, geo::Vec2 origin, double radians) noexcept {
    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = rotate_point(value.segment.a, origin, radians);
            value.segment.b = rotate_point(value.segment.b, origin, radians);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = rotate_point(value.circle.center, origin, radians);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            value.arc.center = rotate_point(value.arc.center, origin, radians);
            value.arc.start_angle += radians;
            value.arc.end_angle += radians;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) point = rotate_point(point, origin, radians);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            value.insertion_point = rotate_point(value.insertion_point, origin, radians);
            value.rotation += radians;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            value.position = rotate_point(value.position, origin, radians);
            value.rotation += radians;
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            value.first = rotate_point(value.first, origin, radians);
            value.second = rotate_point(value.second, origin, radians);
            value.line_point = rotate_point(value.line_point, origin, radians);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            for (auto& point : value.boundary) point = rotate_point(point, origin, radians);
            value.angle += radians;
        }
    }, entity);
}

bool scale_uniform(Entity& entity, geo::Vec2 origin, double factor) noexcept {
    if (!std::isfinite(factor) || factor <= geo::kEpsilon) return false;

    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = scale_point(value.segment.a, origin, factor);
            value.segment.b = scale_point(value.segment.b, origin, factor);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = scale_point(value.circle.center, origin, factor);
            value.circle.radius *= factor;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            value.arc.center = scale_point(value.arc.center, origin, factor);
            value.arc.radius *= factor;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) point = scale_point(point, origin, factor);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            value.insertion_point = scale_point(value.insertion_point, origin, factor);
            value.scale *= factor;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            value.position = scale_point(value.position, origin, factor);
            value.height *= factor;
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            value.first = scale_point(value.first, origin, factor);
            value.second = scale_point(value.second, origin, factor);
            value.line_point = scale_point(value.line_point, origin, factor);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            for (auto& point : value.boundary) point = scale_point(point, origin, factor);
            value.spacing *= factor;
        }
    }, entity);

    return true;
}

bool mirror(Entity& entity, geo::Segment axis) noexcept {
    const geo::Vec2 direction = axis.b - axis.a;
    if (geo::dot(direction, direction) <= geo::kEpsilon) return false;

    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = mirror_point(value.segment.a, axis);
            value.segment.b = mirror_point(value.segment.b, axis);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = mirror_point(value.circle.center, axis);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            const auto oldStart = geo::arc_start_point(value.arc);
            const auto oldEnd = geo::arc_end_point(value.arc);
            value.arc.center = mirror_point(value.arc.center, axis);
            const auto newStart = mirror_point(oldStart, axis);
            const auto newEnd = mirror_point(oldEnd, axis);
            value.arc.start_angle = std::atan2(newStart.y - value.arc.center.y, newStart.x - value.arc.center.x);
            value.arc.end_angle = std::atan2(newEnd.y - value.arc.center.y, newEnd.x - value.arc.center.x);
            value.arc.counter_clockwise = !value.arc.counter_clockwise;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) point = mirror_point(point, axis);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            const geo::Vec2 local_axis_direction = axis.b - axis.a;
            const double axis_angle = std::atan2(local_axis_direction.y, local_axis_direction.x);
            value.insertion_point = mirror_point(value.insertion_point, axis);
            value.rotation = 2.0 * axis_angle - value.rotation;
            value.mirrored = !value.mirrored;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            const geo::Vec2 local_axis_direction = axis.b - axis.a;
            const double axis_angle = std::atan2(local_axis_direction.y, local_axis_direction.x);
            value.position = mirror_point(value.position, axis);
            value.rotation = 2.0 * axis_angle - value.rotation;
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            value.first = mirror_point(value.first, axis);
            value.second = mirror_point(value.second, axis);
            value.line_point = mirror_point(value.line_point, axis);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            const double axis_angle = std::atan2(direction.y, direction.x);
            for (auto& point : value.boundary) point = mirror_point(point, axis);
            value.angle = 2.0 * axis_angle - value.angle;
        }
    }, entity);

    return true;
}

Entity translated_copy(const Entity& entity, geo::Vec2 delta) noexcept {
    Entity copy = entity;
    translate(copy, delta);
    return copy;
}

} // namespace acp::transform
