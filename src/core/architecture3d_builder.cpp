#include "acp/architecture3d_builder.hpp"
#include "acp/architecture3d_roof.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace acp::architecture3d {
namespace {

bool finite_positive(double value) noexcept {
    return std::isfinite(value) && value > geo::kEpsilon;
}

bool finite_nonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

void mix(std::uint64_t& seed, std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void mix_double(std::uint64_t& seed, double value) noexcept {
    mix(seed, std::bit_cast<std::uint64_t>(value));
}

void mix_bool(std::uint64_t& seed, bool value) noexcept {
    mix(seed, value ? 1ULL : 0ULL);
}

void append_scene(model3d::Scene& target, const model3d::Scene& source) {
    for (const model3d::ObjectId id : source.ids()) {
        const model3d::Object3D* object = source.find(id);
        if (object == nullptr) {
            continue;
        }
        (void)target.insert(
            object->mesh,
            object->name,
            object->color,
            object->kind,
            object->source_entity_id);
    }
}

double highest_level_elevation(const std::vector<Level>& levels) noexcept {
    double highest = -std::numeric_limits<double>::infinity();
    for (const Level& level : levels) {
        highest = std::max(highest, level.elevation);
    }
    return highest;
}

} // namespace

bool valid(const ArchitecturalSceneOptions& options) noexcept {
    if (!std::isfinite(options.base_z) ||
        !finite_positive(options.wall_height) ||
        !finite_positive(options.wall_thickness) ||
        !finite_positive(options.slab_thickness) ||
        !std::isfinite(options.roof_eave_z) ||
        !finite_positive(options.roof_ridge_height) ||
        !finite_nonnegative(options.roof_overhang) ||
        !std::isfinite(options.roof_eave_offset)) {
        return false;
    }

    if (options.mode == ArchitecturalBuildMode::MultiStorey) {
        if (options.levels.size() < 2 || !valid_levels(options.levels)) {
            return false;
        }
    }

    return options.include_walls || options.include_slabs || options.include_roofs;
}

std::uint64_t settings_signature(
    const ArchitecturalSceneOptions& options) noexcept {

    std::uint64_t seed = 0x415550524f334455ULL;
    mix(seed, static_cast<std::uint64_t>(options.mode));
    mix_double(seed, options.base_z);
    mix_double(seed, options.wall_height);
    mix_double(seed, options.wall_thickness);
    mix_double(seed, options.slab_thickness);
    mix_bool(seed, options.include_walls);
    mix_bool(seed, options.include_slabs);
    mix_bool(seed, options.include_roofs);
    mix_double(seed, options.roof_eave_z);
    mix_double(seed, options.roof_ridge_height);
    mix_double(seed, options.roof_overhang);
    mix_bool(seed, options.include_top_level_slab);
    mix_double(seed, options.roof_eave_offset);
    mix(seed, static_cast<std::uint64_t>(options.levels.size()));
    for (const Level& level : options.levels) {
        mix_double(seed, level.elevation);
        mix(seed, static_cast<std::uint64_t>(level.name.size()));
        for (const unsigned char byte : level.name) {
            mix(seed, static_cast<std::uint64_t>(byte));
        }
    }
    return seed;
}

model3d::Scene build_architectural_scene(
    const Document& document,
    const ArchitecturalSceneOptions& options) {

    model3d::Scene result;
    if (!valid(options)) {
        return result;
    }

    if (options.mode == ArchitecturalBuildMode::SingleStorey) {
        if (options.include_walls) {
            append_scene(
                result,
                walls_from_lines(
                    document,
                    options.wall_thickness,
                    options.wall_height,
                    options.base_z));
        }

        if (options.include_slabs) {
            append_scene(
                result,
                slabs_from_closed_polylines(
                    document,
                    options.slab_thickness,
                    options.base_z));
        }

        if (options.include_roofs) {
            append_scene(
                result,
                gable_roofs_from_rectangular_polylines(
                    document,
                    options.roof_eave_z,
                    options.roof_ridge_height,
                    options.roof_overhang));
        }
        return result;
    }

    MultiStoreyOptions storey_options;
    storey_options.wall_thickness = options.wall_thickness;
    storey_options.slab_thickness = options.slab_thickness;
    storey_options.include_slabs = options.include_slabs;
    storey_options.include_top_level_slab = options.include_top_level_slab;

    if (options.include_walls || options.include_slabs) {
        model3d::Scene multi = build_multistorey_scene(
            document,
            options.levels,
            storey_options);

        if (!options.include_walls) {
            model3d::Scene filtered;
            for (const model3d::ObjectId id : multi.ids()) {
                const model3d::Object3D* object = multi.find(id);
                if (object != nullptr && object->kind == model3d::ObjectKind::Slab) {
                    (void)filtered.insert(
                        object->mesh,
                        object->name,
                        object->color,
                        object->kind,
                        object->source_entity_id);
                }
            }
            multi = std::move(filtered);
        }
        append_scene(result, multi);
    }

    if (options.include_roofs) {
        const double roof_eave =
            highest_level_elevation(options.levels) + options.roof_eave_offset;
        append_scene(
            result,
            gable_roofs_from_rectangular_polylines(
                document,
                roof_eave,
                options.roof_ridge_height,
                options.roof_overhang));
    }

    return result;
}

} // namespace acp::architecture3d
