#include "acp/architecture3d_roof.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <variant>

namespace acp::architecture3d {
namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

bool close(double a, double b, double tolerance = 1e-6) noexcept {
    return std::abs(a - b) <= tolerance;
}

std::optional<std::pair<geo::Vec2, geo::Vec2>> axis_aligned_rectangle_bounds(
    const PolylineEntity& polyline) noexcept {

    if (!polyline.closed || polyline.points.size() != 4) {
        return std::nullopt;
    }

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    for (const geo::Vec2 point : polyline.points) {
        if (!finite(point.x) || !finite(point.y)) {
            return std::nullopt;
        }
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
        max_x = std::max(max_x, point.x);
        max_y = std::max(max_y, point.y);
    }

    if (max_x - min_x <= geo::kEpsilon ||
        max_y - min_y <= geo::kEpsilon) {
        return std::nullopt;
    }

    const std::array<geo::Vec2, 4> corners{{
        {min_x, min_y},
        {max_x, min_y},
        {max_x, max_y},
        {min_x, max_y}
    }};

    std::array<bool, 4> seen{};
    for (const geo::Vec2 point : polyline.points) {
        bool matched = false;
        for (std::size_t i = 0; i < corners.size(); ++i) {
            if (!seen[i] &&
                close(point.x, corners[i].x) &&
                close(point.y, corners[i].y)) {
                seen[i] = true;
                matched = true;
                break;
            }
        }
        if (!matched) {
            return std::nullopt;
        }
    }

    return std::pair<geo::Vec2, geo::Vec2>{{min_x, min_y}, {max_x, max_y}};
}

} // namespace

bool valid(const GableRoofSpec& roof) noexcept {
    return finite(roof.min.x) && finite(roof.min.y) &&
           finite(roof.max.x) && finite(roof.max.y) &&
           finite(roof.eave_z) && finite(roof.ridge_height) &&
           finite(roof.overhang) &&
           roof.max.x - roof.min.x > geo::kEpsilon &&
           roof.max.y - roof.min.y > geo::kEpsilon &&
           roof.ridge_height > geo::kEpsilon &&
           roof.overhang >= 0.0;
}

std::optional<geo3d::Mesh> make_gable_roof(const GableRoofSpec& roof) {
    if (!valid(roof)) {
        return std::nullopt;
    }

    const double min_x = roof.min.x - roof.overhang;
    const double max_x = roof.max.x + roof.overhang;
    const double min_y = roof.min.y - roof.overhang;
    const double max_y = roof.max.y + roof.overhang;
    const double ridge_z = roof.eave_z + roof.ridge_height;

    geo3d::Mesh mesh;
    mesh.vertices.reserve(6);
    mesh.triangles.reserve(8);

    if (roof.ridge_axis == RoofRidgeAxis::X) {
        const double center_y = (min_y + max_y) * 0.5;
        mesh.vertices = {
            {min_x, min_y, roof.eave_z},
            {min_x, max_y, roof.eave_z},
            {min_x, center_y, ridge_z},
            {max_x, min_y, roof.eave_z},
            {max_x, max_y, roof.eave_z},
            {max_x, center_y, ridge_z}
        };
    } else {
        const double center_x = (min_x + max_x) * 0.5;
        mesh.vertices = {
            {min_x, min_y, roof.eave_z},
            {max_x, min_y, roof.eave_z},
            {center_x, min_y, ridge_z},
            {min_x, max_y, roof.eave_z},
            {max_x, max_y, roof.eave_z},
            {center_x, max_y, ridge_z}
        };
    }

    mesh.triangles = {
        {0, 2, 1},
        {3, 4, 5},
        {0, 1, 4}, {0, 4, 3},
        {0, 3, 5}, {0, 5, 2},
        {2, 5, 4}, {2, 4, 1}
    };

    return geo3d::valid_mesh(mesh)
        ? std::optional<geo3d::Mesh>{std::move(mesh)}
        : std::nullopt;
}

model3d::ObjectId add_gable_roof(
    model3d::Scene& scene,
    const GableRoofSpec& roof,
    std::string name,
    RgbColor color) {

    auto mesh = make_gable_roof(roof);
    if (!mesh.has_value()) {
        return 0;
    }
    return scene.insert(
        std::move(*mesh),
        std::move(name),
        color,
        model3d::ObjectKind::Roof);
}

model3d::Scene gable_roofs_from_rectangular_polylines(
    const Document& document,
    double eave_z,
    double ridge_height,
    double overhang) {

    model3d::Scene result;
    if (!finite(eave_z) ||
        !finite(ridge_height) || ridge_height <= geo::kEpsilon ||
        !finite(overhang) || overhang < 0.0) {
        return result;
    }

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        const auto* polyline =
            entity == nullptr ? nullptr : std::get_if<PolylineEntity>(entity);
        if (polyline == nullptr) {
            continue;
        }

        const auto bounds = axis_aligned_rectangle_bounds(*polyline);
        if (!bounds.has_value()) {
            continue;
        }
        const double width = bounds->second.x - bounds->first.x;
        const double depth = bounds->second.y - bounds->first.y;
        GableRoofSpec roof{
            bounds->first,
            bounds->second,
            eave_z,
            ridge_height,
            overhang,
            width >= depth ? RoofRidgeAxis::X : RoofRidgeAxis::Y};
        auto mesh = make_gable_roof(roof);
        if (!mesh.has_value()) {
            continue;
        }
        (void)result.insert(
            std::move(*mesh),
            "Roof_" + std::to_string(id),
            document.effective_color(id),
            model3d::ObjectKind::Roof,
            id);
    }
    return result;
}

} // namespace acp::architecture3d
