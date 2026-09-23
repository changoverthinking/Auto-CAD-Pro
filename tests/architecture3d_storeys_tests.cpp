#include "acp/architecture3d_advanced.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
bool close(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}
} // namespace

int main() {
    namespace arch = acp::architecture3d;

    const arch::WallSpec left{{0.0, 0.0}, {-4000.0, 0.0}, 200.0, 3000.0, 0.0};
    const arch::WallSpec right{{0.0, 0.0}, {4000.0, 0.0}, 200.0, 3000.0, 0.0};
    const arch::WallSpec up{{0.0, 0.0}, {0.0, 3000.0}, 300.0, 3000.0, 0.0};
    const arch::WallSpec down{{0.0, 0.0}, {0.0, -3000.0}, 300.0, 3000.0, 0.0};

    const auto t = arch::make_orthogonal_wall_junction({left, right, up});
    expect(t.has_value(), "orthogonal T junction builds");
    if (t.has_value()) {
        expect(t->kind == arch::WallJunctionKind::T, "T junction semantic kind preserved");
        expect(acp::geo3d::valid_mesh(t->hub), "T junction hub mesh valid");
        expect(t->branches.size() == 3, "T junction has three trimmed branches");
        const auto hub = acp::geo3d::bounds(t->hub);
        expect(close(hub.min.x, -150.0) && close(hub.max.x, 150.0),
               "T hub x extent follows vertical wall thickness");
        expect(close(hub.min.y, -100.0) && close(hub.max.y, 100.0),
               "T hub y extent follows horizontal wall thickness");
        for (const auto& branch : t->branches) {
            expect(acp::geo3d::valid_mesh(branch), "T branch mesh valid");
        }
    }

    const auto x = arch::make_orthogonal_wall_junction({left, right, up, down});
    expect(x.has_value(), "orthogonal X junction builds");
    if (x.has_value()) {
        expect(x->kind == arch::WallJunctionKind::X, "X junction semantic kind preserved");
        expect(x->branches.size() == 4, "X junction has four trimmed branches");
    }

    const arch::WallSpec diagonal{{0.0, 0.0}, {2000.0, 2000.0}, 200.0, 3000.0, 0.0};
    expect(
        !arch::make_orthogonal_wall_junction({left, right, diagonal}).has_value(),
        "oblique T junction rejected until general solver exists");

    acp::Document document;
    const auto line_id = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {5000.0, 0.0}}});
    const auto poly_id = document.insert(acp::PolylineEntity{{
        {0.0, 0.0}, {4000.0, 0.0}, {4000.0, 3000.0}, {0.0, 3000.0}}, true});
    expect(line_id != 0 && poly_id != 0, "multi-storey source entities inserted");

    const std::vector<arch::Level> levels{
        {"Roof", 6500.0},
        {"Ground", 0.0},
        {"Level 2", 3200.0}};

    arch::MultiStoreyOptions options;
    options.wall_thickness = 200.0;
    options.slab_thickness = 180.0;
    const auto scene = arch::build_multistorey_scene(document, levels, options);
    expect(scene.size() == 5, "two storeys plus three slabs build from unsorted levels");

    std::size_t wall_count = 0;
    std::size_t slab_count = 0;
    bool ground_prefix = false;
    bool level2_prefix = false;
    bool roof_prefix = false;
    for (const auto id : scene.ids()) {
        const auto* object = scene.find(id);
        expect(object != nullptr, "multi-storey object exists");
        if (object == nullptr) {
            continue;
        }
        if (object->kind == acp::model3d::ObjectKind::Wall) {
            ++wall_count;
            expect(object->source_entity_id == line_id, "storey wall preserves source entity");
        }
        if (object->kind == acp::model3d::ObjectKind::Slab) {
            ++slab_count;
            expect(object->source_entity_id == poly_id, "storey slab preserves source entity");
        }
        ground_prefix = ground_prefix || object->name.rfind("Ground::", 0) == 0;
        level2_prefix = level2_prefix || object->name.rfind("Level 2::", 0) == 0;
        roof_prefix = roof_prefix || object->name.rfind("Roof::", 0) == 0;
    }
    expect(wall_count == 2, "multi-storey scene contains one wall per story");
    expect(slab_count == 3, "multi-storey scene contains one slab per level");
    expect(ground_prefix && level2_prefix && roof_prefix,
           "multi-storey names carry owning level prefix");

    const auto scene_bounds = scene.bounds();
    expect(scene_bounds.initialized, "multi-storey bounds valid");
    if (scene_bounds.initialized) {
        expect(close(scene_bounds.min.z, -180.0), "ground slab extends below zero");
        expect(close(scene_bounds.max.z, 6500.0), "scene reaches roof level");
    }

    options.include_top_level_slab = false;
    const auto no_roof_slab = arch::build_multistorey_scene(document, levels, options);
    expect(no_roof_slab.size() == 4, "top slab can be excluded explicitly");

    const std::vector<arch::Level> duplicate_elevation{
        {"Ground", 0.0}, {"Alternate", 0.0}, {"Roof", 3000.0}};
    expect(
        arch::build_multistorey_scene(document, duplicate_elevation, {}).size() == 0,
        "duplicate level elevations rejected");

    options.wall_thickness = 0.0;
    expect(
        arch::build_multistorey_scene(document, levels, options).size() == 0,
        "invalid storey wall thickness rejected");

    if (failures == 0) {
        std::cout << "Architecture storey/junction tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
