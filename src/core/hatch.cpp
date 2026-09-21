#include "acp/hatch.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace acp::hatch {

bool valid(const HatchEntity& value) noexcept {
    if (value.boundary.size() < 3) {
        return false;
    }
    if (!std::isfinite(value.angle) ||
        !std::isfinite(value.spacing) ||
        value.spacing <= geo::kEpsilon) {
        return false;
    }
    return value.solid || !value.pattern.empty();
}

double perimeter(const HatchEntity& value) noexcept {
    return valid(value)
        ? geo::polyline_length(value.boundary, true)
        : 0.0;
}

std::vector<geo::Segment> pattern_segments(
    const HatchEntity& value,
    std::size_t max_segments) {

    std::vector<geo::Segment> segments;
    if (!valid(value) || value.solid || max_segments == 0) {
        return segments;
    }

    const double c = std::cos(value.angle);
    const double s = std::sin(value.angle);

    auto to_pattern = [c, s](geo::Vec2 p) noexcept {
        return geo::Vec2{
            c * p.x + s * p.y,
            -s * p.x + c * p.y
        };
    };
    auto from_pattern = [c, s](geo::Vec2 p) noexcept {
        return geo::Vec2{
            c * p.x - s * p.y,
            s * p.x + c * p.y
        };
    };

    std::vector<geo::Vec2> polygon;
    polygon.reserve(value.boundary.size());

    double min_y = 0.0;
    double max_y = 0.0;
    bool first = true;
    for (const geo::Vec2 point : value.boundary) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return {};
        }
        const geo::Vec2 rotated = to_pattern(point);
        polygon.push_back(rotated);
        if (first) {
            min_y = max_y = rotated.y;
            first = false;
        } else {
            min_y = std::min(min_y, rotated.y);
            max_y = std::max(max_y, rotated.y);
        }
    }

    const double first_y =
        std::ceil((min_y - geo::kEpsilon) / value.spacing) * value.spacing;
    if (!std::isfinite(first_y)) {
        return {};
    }

    const double span = std::max(0.0, max_y - first_y);
    const double estimated_lines = span / value.spacing + 1.0;
    if (!std::isfinite(estimated_lines) ||
        estimated_lines > static_cast<double>(max_segments) * 2.0 + 2.0) {
        return {};
    }

    std::vector<double> intersections;
    intersections.reserve(polygon.size());

    for (double y = first_y;
         y <= max_y + geo::kEpsilon && segments.size() < max_segments;
         y += value.spacing) {

        intersections.clear();

        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const geo::Vec2 a = polygon[i];
            const geo::Vec2 b = polygon[(i + 1) % polygon.size()];

            const bool crosses =
                (a.y <= y && b.y > y) ||
                (b.y <= y && a.y > y);
            if (!crosses) {
                continue;
            }

            const double dy = b.y - a.y;
            if (std::abs(dy) <= geo::kEpsilon) {
                continue;
            }

            const double t = (y - a.y) / dy;
            const double x = a.x + (b.x - a.x) * t;
            if (std::isfinite(x)) {
                intersections.push_back(x);
            }
        }

        std::sort(intersections.begin(), intersections.end());

        for (std::size_t i = 0;
             i + 1 < intersections.size() && segments.size() < max_segments;
             i += 2) {
            const double x0 = intersections[i];
            const double x1 = intersections[i + 1];
            if (x1 - x0 <= geo::kEpsilon) {
                continue;
            }
            segments.push_back({
                from_pattern({x0, y}),
                from_pattern({x1, y})
            });
        }
    }

    return segments;
}

} // namespace acp::hatch
