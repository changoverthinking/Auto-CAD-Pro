#pragma once

#include "acp/architecture3d.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace acp::architecture3d {

enum class WallJunctionKind {
    Unsupported = 0,
    L,
    T,
    X
};

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

[[nodiscard]] std::optional<geo3d::Mesh> make_opening_frame(
    const WallOpeningSpec& opening,
    double profile_width,
    double frame_depth);

[[nodiscard]] std::optional<WallJoinMeshes> make_mitered_wall_pair(
    const WallSpec& first,
    const WallSpec& second,
    double endpoint_tolerance = 1e-6);

[[nodiscard]] std::optional<WallJunctionInfo> classify_wall_junction(
    const std::vector<WallSpec>& walls,
    double endpoint_tolerance = 1e-6,
    double angular_tolerance = 1e-6);

[[nodiscard]] std::optional<WallJunctionMeshes> make_orthogonal_wall_junction(
    const std::vector<WallSpec>& walls,
    double endpoint_tolerance = 1e-6,
    double angular_tolerance = 1e-6);

[[nodiscard]] std::optional<StorySpan> story_span(
    const std::vector<Level>& levels,
    std::string_view base_level_name) noexcept;

[[nodiscard]] model3d::Scene walls_for_story(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view base_level_name,
    double thickness);

[[nodiscard]] model3d::Scene slabs_at_level(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view level_name,
    double thickness);

[[nodiscard]] model3d::Scene build_multistorey_scene(
    const Document& document,
    const std::vector<Level>& levels,
    const MultiStoreyOptions& options = {});

} // namespace acp::architecture3d
