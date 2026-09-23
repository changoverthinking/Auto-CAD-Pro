#pragma once

#include "acp/document.hpp"
#include "acp/model3d.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace acp::architecture3d {

struct Level {
    std::string name;
    double elevation{0.0};
};

struct WallSpec {
    geo::Vec2 start{};
    geo::Vec2 end{};
    double thickness{200.0};
    double height{3000.0};
    double base_z{0.0};
};

struct SlabSpec {
    std::vector<geo::Vec2> boundary;
    double thickness{200.0};
    double top_z{0.0};
};

struct ColumnSpec {
    geo::Vec2 center{};
    double width{400.0};
    double depth{400.0};
    double height{3000.0};
    double base_z{0.0};
    double rotation{0.0};
};

struct BeamSpec {
    geo::Vec2 start{};
    geo::Vec2 end{};
    double width{300.0};
    double depth{500.0};
    double top_z{3000.0};
};

enum class OpeningKind {
    Door,
    Window
};

struct WallOpeningSpec {
    WallSpec wall;
    OpeningKind kind{OpeningKind::Door};
    double center_offset{0.5}; // Normalized distance along the wall: [0, 1].
    double width{900.0};
    double height{2100.0};
    double sill_height{0.0};
    double cut_clearance{2.0};
};

[[nodiscard]] bool valid(const Level& level) noexcept;
[[nodiscard]] bool valid_levels(const std::vector<Level>& levels) noexcept;
[[nodiscard]] std::optional<double> level_elevation(
    const std::vector<Level>& levels,
    std::string_view name) noexcept;

[[nodiscard]] bool valid(const WallSpec& wall) noexcept;
[[nodiscard]] bool valid(const SlabSpec& slab) noexcept;
[[nodiscard]] bool valid(const ColumnSpec& column) noexcept;
[[nodiscard]] bool valid(const BeamSpec& beam) noexcept;
[[nodiscard]] bool valid(const WallOpeningSpec& opening) noexcept;

[[nodiscard]] std::optional<geo3d::Mesh> make_wall(const WallSpec& wall);
[[nodiscard]] std::optional<geo3d::Mesh> make_slab(const SlabSpec& slab);
[[nodiscard]] std::optional<geo3d::Mesh> make_column(const ColumnSpec& column);
[[nodiscard]] std::optional<geo3d::Mesh> make_beam(const BeamSpec& beam);

// Returns a watertight cutter volume aligned to the host wall. This remains
// useful for a future general CSG kernel and for external authoring tools.
[[nodiscard]] std::optional<geo3d::Mesh> make_wall_opening_volume(
    const WallOpeningSpec& opening);

// Builds the host wall with rectangular door/window holes already removed.
// Openings must not overlap along the wall. The resulting mesh can contain
// several disconnected watertight pieces, but contains no geometry inside
// the requested opening volumes.
[[nodiscard]] std::optional<geo3d::Mesh> make_wall_with_openings(
    const WallSpec& wall,
    const std::vector<WallOpeningSpec>& openings);

[[nodiscard]] model3d::Scene walls_from_lines(
    const Document& document,
    double thickness,
    double height,
    double base_z = 0.0);

[[nodiscard]] model3d::Scene beams_from_lines(
    const Document& document,
    double width,
    double depth,
    double top_z);

[[nodiscard]] model3d::Scene slabs_from_closed_polylines(
    const Document& document,
    double thickness,
    double top_z = 0.0);

} // namespace acp::architecture3d
