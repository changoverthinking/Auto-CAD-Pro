#pragma once

#include "acp/architecture3d.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace acp::architecture3d {

struct WallJoinMeshes {
    geo3d::Mesh first;
    geo3d::Mesh second;
};

struct WallJunctionMeshes {
    WallJunctionKind kind{WallJunctionKind::Unsupported};
    geo3d::Mesh hub;
    std::vector<geo3d::Mesh> branches;
};

struct StorySpan {
    double base_z{};
    double top_z{};

    [[nodiscard]] double height() const noexcept { return top_z - base_z; }
};

enum class WallJunctionKind {
    Unsupported = 0,
    L,
    T,
    X
};

struct WallJunctionInfo {
    WallJunctionKind kind{WallJunctionKind::Unsupported};
    geo::Vec2 joint{};
    std::size_t wall_count{};
};

struct MultiStoreyOptions {
    double wall_thickness{200.0};
    double slab_thickness{180.0};
    bool include_slabs{true};
    bool include_top_level_slab{true};
};

// Builds a rectangular frame around a hosted opening. Doors omit the bottom
// rail; windows include all four rails. The frame stays centered in the host
// wall and uses the opening sill/head elevations.
[[nodiscard]] std::optional<geo3d::Mesh> make_opening_frame(
    const WallOpeningSpec& opening,
    double profile_width,
    double frame_depth);

// Produces two wall meshes sharing a true 2D miter line at a common endpoint.
// Parallel/collinear walls and walls that do not share an endpoint are
// rejected instead of silently producing self-intersecting geometry.
[[nodiscard]] std::optional<WallJoinMeshes> make_mitered_wall_pair(
    const WallSpec& first,
    const WallSpec& second,
    double endpoint_tolerance = 1e-6);

// Classifies 2-4 walls meeting at one endpoint. All walls must share base Z
// and height. L requires two non-collinear rays, T requires one opposite pair
// plus a branch, and X requires two opposite pairs.
[[nodiscard]] std::optional<WallJunctionInfo> classify_wall_junction(
    const std::vector<WallSpec>& walls,
    double endpoint_tolerance = 1e-6,
    double angular_tolerance = 1e-6);

// Creates actual non-overlapping geometry for orthogonal T/X junctions.
// A central hub prism fills the node and every branch is trimmed to the hub
// boundary. Non-orthogonal T/X junctions are rejected until the general
// offset/intersection solver is available.
[[nodiscard]] std::optional<WallJunctionMeshes> make_orthogonal_wall_junction(
    const std::vector<WallSpec>& walls,
    double endpoint_tolerance = 1e-6,
    double angular_tolerance = 1e-6);

// Resolves the named level and the next higher level into a story range.
[[nodiscard]] std::optional<StorySpan> story_span(
    const std::vector<Level>& levels,
    std::string_view base_level_name) noexcept;

// Converts visible 2D lines into walls whose base/top come from the level
// system. This is the level-aware equivalent of walls_from_lines().
[[nodiscard]] model3d::Scene walls_for_story(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view base_level_name,
    double thickness);

// Converts visible closed polylines into slabs with their top locked to a
// named level elevation.
[[nodiscard]] model3d::Scene slabs_at_level(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view level_name,
    double thickness);

// Repeats the current 2D wall/floor footprint through all valid storeys.
// Levels may be supplied in any order. The result preserves semantic kinds
// and source entity IDs while prefixing object names with the owning level.
[[nodiscard]] model3d::Scene build_multistorey_scene(
    const Document& document,
    const std::vector<Level>& levels,
    const MultiStoreyOptions& options = {});

} // namespace acp::architecture3d
