#include "acp/architecture3d_roof.hpp"

#include <cmath>
#include <utility>

namespace acp::architecture3d {
namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
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

} // namespace acp::architecture3d
