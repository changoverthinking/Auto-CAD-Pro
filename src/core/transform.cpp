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
