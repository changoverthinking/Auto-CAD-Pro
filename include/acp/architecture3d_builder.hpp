#pragma once

#include "acp/architecture3d.hpp"
#include "acp/architecture3d_advanced.hpp"
#include "acp/model3d.hpp"

#include <cstdint>
#include <vector>

namespace acp::architecture3d {

enum class ArchitecturalBuildMode : std::uint8_t {
    SingleStorey = 0,
    MultiStorey
};

struct ArchitecturalSceneOptions {
    ArchitecturalBuildMode mode{ArchitecturalBuildMode::SingleStorey};

    // Single-storey parameters.
    double base_z{0.0};
    double wall_height{3000.0};

    // Shared architectural dimensions.
    double wall_thickness{200.0};
    double slab_thickness{180.0};

    bool include_walls{true};
    bool include_slabs{true};
    bool include_roofs{false};

    // Roof parameters. Roof footprints are visible closed rectangular
    // polylines from the source document.
    double roof_eave_z{3000.0};
    double roof_ridge_height{1200.0};
    double roof_overhang{300.0};

    // Multi-storey parameters. In MultiStorey mode these elevations drive
    // wall story spans and slabs. Roofs, when enabled, use the highest level
    // as their eave datum plus roof_eave_offset.
    std::vector<Level> levels;
    bool include_top_level_slab{true};
    double roof_eave_offset{0.0};
};

[[nodiscard]] bool valid(const ArchitecturalSceneOptions& options) noexcept;

// Stable signature for all settings that materially affect generated scene
// geometry. It is intended to be paired with Document::revision() by the GUI
// scene cache so changes to either source geometry or build settings rebuild.
[[nodiscard]] std::uint64_t settings_signature(
    const ArchitecturalSceneOptions& options) noexcept;

// Builds one semantic architectural scene from the 2D document. Lines become
// walls, closed polylines become slabs, and rectangular closed polylines can
// additionally become gable roofs. Multi-storey mode repeats walls/slabs using
// the supplied level elevations.
[[nodiscard]] model3d::Scene build_architectural_scene(
    const Document& document,
    const ArchitecturalSceneOptions& options);

} // namespace acp::architecture3d
