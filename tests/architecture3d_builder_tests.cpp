#include "acp/architecture3d_builder.hpp"

#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::size_t count_kind(
    const acp::model3d::Scene& scene,
    acp::model3d::ObjectKind kind) {

    std::size_t count = 0;
    for (const auto id : scene.ids()) {
        const auto* object = scene.find(id);
        if (object != nullptr && object->kind == kind) {
            ++count;
        }
    }
    return count;
}
} // namespace

int main() {
    namespace arch = acp::architecture3d;

    acp::Document document;
    const auto wall_line = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {6000.0, 0.0}}});
    const auto footprint = document.insert(
        acp::PolylineEntity{{
            {0.0, 0.0},
            {6000.0, 0.0},
            {6000.0, 4000.0},
            {0.0, 4000.0}}, true});
    expect(wall_line != 0 && footprint != 0, "source document created");

    arch::ArchitecturalSceneOptions single;
    single.mode = arch::ArchitecturalBuildMode::SingleStorey;
    single.base_z = 0.0;
    single.wall_height = 3000.0;
    single.wall_thickness = 200.0;
    single.slab_thickness = 180.0;
    single.include_walls = true;
    single.include_slabs = true;
    single.include_roofs = true;
    single.roof_eave_z = 3000.0;
    single.roof_ridge_height = 1200.0;
    single.roof_overhang = 300.0;

    expect(arch::valid(single), "single-storey options valid");
    const auto signature_a = arch::settings_signature(single);
    const auto signature_b = arch::settings_signature(single);
    expect(signature_a == signature_b, "settings signature deterministic");

    auto changed = single;
    changed.wall_thickness = 250.0;
    expect(arch::settings_signature(changed) != signature_a,
           "geometry setting changes signature");

    changed = single;
    changed.include_roofs = false;
    expect(arch::settings_signature(changed) != signature_a,
           "feature toggle changes signature");

    const auto single_scene = arch::build_architectural_scene(document, single);
    expect(count_kind(single_scene, acp::model3d::ObjectKind::Wall) == 1,
           "single-storey builder creates one wall from line");
    expect(count_kind(single_scene, acp::model3d::ObjectKind::Slab) == 1,
           "single-storey builder creates one slab from footprint");
    expect(count_kind(single_scene, acp::model3d::ObjectKind::Roof) == 1,
           "single-storey builder creates one roof from rectangular footprint");

    bool wall_linked = false;
    bool slab_linked = false;
    bool roof_linked = false;
    for (const auto id : single_scene.ids()) {
        const auto* object = single_scene.find(id);
        if (object == nullptr) continue;
        if (object->kind == acp::model3d::ObjectKind::Wall) {
            wall_linked = object->source_entity_id == wall_line;
        } else if (object->kind == acp::model3d::ObjectKind::Slab) {
            slab_linked = object->source_entity_id == footprint;
        } else if (object->kind == acp::model3d::ObjectKind::Roof) {
            roof_linked = object->source_entity_id == footprint;
        }
    }
    expect(wall_linked && slab_linked && roof_linked,
           "builder preserves 2D source linkage across semantics");

    arch::ArchitecturalSceneOptions multi = single;
    multi.mode = arch::ArchitecturalBuildMode::MultiStorey;
    multi.levels = {
        {"Roof", 6500.0},
        {"Ground", 0.0},
        {"Level 2", 3200.0}
    };
    multi.roof_eave_offset = 0.0;
    multi.include_top_level_slab = false;

    expect(arch::valid(multi), "multi-storey options valid when levels unsorted");
    expect(arch::settings_signature(multi) != signature_a,
           "build mode and levels affect signature");

    const auto multi_scene = arch::build_architectural_scene(document, multi);
    expect(count_kind(multi_scene, acp::model3d::ObjectKind::Wall) == 2,
           "multi-storey builder creates one wall per story span");
    expect(count_kind(multi_scene, acp::model3d::ObjectKind::Slab) == 2,
           "multi-storey builder omits requested top-level slab");
    expect(count_kind(multi_scene, acp::model3d::ObjectKind::Roof) == 1,
           "multi-storey builder adds roof at highest level");

    const auto bounds = multi_scene.bounds();
    expect(bounds.initialized, "multi-storey scene has bounds");
    if (bounds.initialized) {
        expect(bounds.max.z > 6500.0,
               "multi-storey roof ridge rises above highest level");
    }

    auto invalid = single;
    invalid.wall_thickness = 0.0;
    expect(!arch::valid(invalid), "zero wall thickness rejected");
    expect(arch::build_architectural_scene(document, invalid).size() == 0,
           "invalid builder options return empty scene");

    invalid = multi;
    invalid.levels = {{"Only", 0.0}};
    expect(!arch::valid(invalid), "multi-storey requires at least two levels");

    if (failures == 0) {
        std::cout << "Architecture scene builder tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
