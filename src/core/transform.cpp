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

void translate(Entity& entity, geo::Vec2 delta) noexcept {
    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = value.segment.a + delta;
            value.segment.b = value.segment.b + delta;
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = value.circle.center + delta;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) {
                point = point + delta;
            }
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
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) {
                point = rotate_point(point, origin, radians);
            }
        }
    }, entity);
}

bool scale_uniform(Entity& entity, geo::Vec2 origin, double factor) noexcept {
    if (!std::isfinite(factor) || factor <= geo::kEpsilon) {
        return false;
    }

    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = scale_point(value.segment.a, origin, factor);
            value.segment.b = scale_point(value.segment.b, origin, factor);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = scale_point(value.circle.center, origin, factor);
            value.circle.radius *= factor;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) {
                point = scale_point(point, origin, factor);
            }
        }
    }, entity);

    return true;
}

} // namespace acp::transform
