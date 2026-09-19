#include "acp/selection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <variant>

namespace acp::selection {

namespace {

struct DistanceResult {
    double distance{std::numeric_limits<double>::infinity()};
    geo::Vec2 nearest{};
};

DistanceResult nearest_on_entity(const Entity& entity, geo::Vec2 point) noexcept {
    return std::visit([&](const auto& value) -> DistanceResult {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, LineEntity>) {
            const auto nearest = geo::nearest_point(value.segment, point);
            return {geo::distance(nearest, point), nearest};
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            const auto delta = point - value.circle.center;
            const double centerDistance = geo::length(delta);
            const double radius = std::max(0.0, value.circle.radius);
            if (centerDistance <= geo::kEpsilon) {
                return {radius, {value.circle.center.x + radius, value.circle.center.y}};
            }
            const double scale = radius / centerDistance;
            const geo::Vec2 nearest = value.circle.center + delta * scale;
            return {std::abs(centerDistance - radius), nearest};
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            if (!geo::valid_arc(value.arc)) return {};
            const auto nearest = geo::nearest_point(value.arc, point);
            return {geo::distance(nearest, point), nearest};
        } else {
            if (value.points.empty()) return {};
            if (value.points.size() == 1) {
                return {geo::distance(value.points.front(), point), value.points.front()};
            }
            DistanceResult best;
            const auto consider = [&](geo::Vec2 a, geo::Vec2 b, DistanceResult& current) {
                const auto nearest = geo::nearest_point({a, b}, point);
                const double d = geo::distance(nearest, point);
                if (d < current.distance) current = {d, nearest};
            };
            for (std::size_t i = 1; i < value.points.size(); ++i) {
                consider(value.points[i - 1], value.points[i], best);
            }
            if (value.closed && value.points.size() > 2) {
                consider(value.points.back(), value.points.front(), best);
            }
            return best;
        }
    }, entity);
}

} // namespace

double distance_to_entity(const Entity& entity, geo::Vec2 point) noexcept {
    return nearest_on_entity(entity, point).distance;
}

std::optional<Hit> hit_test(const Document& document, geo::Vec2 point, double aperture) noexcept {
    if (!std::isfinite(aperture) || aperture < 0.0) return std::nullopt;
    std::optional<Hit> best;
    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        if (entity == nullptr) continue;
        const auto result = nearest_on_entity(*entity, point);
        if (result.distance > aperture) continue;
        if (!best.has_value() || result.distance < best->distance ||
            (geo::nearly_equal(result.distance, best->distance) && id < best->id)) {
            best = Hit{id, result.distance, result.nearest};
        }
    }
    return best;
}

} // namespace acp::selection
