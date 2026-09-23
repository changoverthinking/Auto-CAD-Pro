#pragma once

#include "acp/model3d.hpp"

#include <optional>
#include <string>

namespace acp::architecture3d {

enum class RoofRidgeAxis {
    X,
    Y
};

struct GableRoofSpec {
    geo::Vec2 min{};
    geo::Vec2 max{};
    double eave_z{3000.0};
    double ridge_height{1200.0};
    double overhang{300.0};
    RoofRidgeAxis ridge_axis{RoofRidgeAxis::X};
};

[[nodiscard]] bool valid(const GableRoofSpec& roof) noexcept;

// Builds a closed triangular-prism roof volume. This is intentionally a
// simple architectural solid; roof skins/fascia/gutters can layer on later.
[[nodiscard]] std::optional<geo3d::Mesh> make_gable_roof(
    const GableRoofSpec& roof);

[[nodiscard]] model3d::ObjectId add_gable_roof(
    model3d::Scene& scene,
    const GableRoofSpec& roof,
    std::string name = "Roof",
    RgbColor color = {155, 95, 75});

// Converts visible, closed, axis-aligned rectangular polylines into semantic
// gable roofs. Non-rectangular footprints are intentionally skipped until the
// general polygon roof solver is available. Ridge direction follows the longer
// footprint axis and each object preserves its source 2D entity id.
[[nodiscard]] model3d::Scene gable_roofs_from_rectangular_polylines(
    const Document& document,
    double eave_z,
    double ridge_height,
    double overhang = 300.0);

} // namespace acp::architecture3d
