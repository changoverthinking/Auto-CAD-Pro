#include "acp/architecture3d_roof.hpp"
#include "acp/scene_revision_cache.hpp"

#include <cmath>
#include <iostream>

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

    const arch::GableRoofSpec roof{
        {0.0, 0.0}, {6000.0, 4000.0}, 6500.0, 1500.0, 300.0,
        arch::RoofRidgeAxis::X};
    expect(arch::valid(roof), "gable roof spec valid");

    const auto mesh = arch::make_gable_roof(roof);
    expect(mesh.has_value(), "gable roof mesh builds");
    if (mesh.has_value()) {
        expect(acp::geo3d::valid_mesh(*mesh), "gable roof mesh valid");
        expect(mesh->vertices.size() == 6, "gable roof uses six prism vertices");
        expect(mesh->triangles.size() == 8, "gable roof uses eight triangles");
        const auto bounds = acp::geo3d::bounds(*mesh);
        expect(bounds.initialized, "gable roof bounds initialized");
        expect(close(bounds.min.x, -300.0) && close(bounds.max.x, 6300.0),
               "roof overhang expands X bounds");
        expect(close(bounds.min.y, -300.0) && close(bounds.max.y, 4300.0),
               "roof overhang expands Y bounds");
        expect(close(bounds.min.z, 6500.0) && close(bounds.max.z, 8000.0),
               "roof eave and ridge elevations correct");
    }

    acp::model3d::Scene scene;
    const auto roof_id = arch::add_gable_roof(scene, roof, "MainRoof");
    expect(roof_id != 0, "semantic roof inserted into scene");
    const auto* roof_object = scene.find(roof_id);
    expect(roof_object != nullptr, "roof object retrievable");
    if (roof_object != nullptr) {
        expect(roof_object->kind == acp::model3d::ObjectKind::Roof,
               "roof semantic kind preserved");
    }

    auto invalid = roof;
    invalid.ridge_height = 0.0;
    expect(!arch::valid(invalid), "zero ridge height rejected");
    expect(!arch::make_gable_roof(invalid).has_value(), "invalid roof does not build");

    acp::Document document;
    acp::model3d::SceneRevisionCache cache;
    constexpr std::uint64_t settings_revision = 1;
    expect(cache.stale(document, settings_revision), "new cache starts stale");
    cache.mark_built(document, settings_revision);
    expect(!cache.stale(document, settings_revision), "cache clean after build mark");

    const auto line_id = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {1000.0, 0.0}}});
    expect(line_id != 0, "cache test document mutates");
    expect(cache.stale(document, settings_revision),
           "document mutation invalidates revision cache automatically");

    cache.mark_built(document, settings_revision);
    expect(cache.stale(document, settings_revision + 1),
           "3D settings revision invalidates cache");

    cache.mark_built(document, settings_revision + 1);
    acp::Document snapshot = document;
    document.erase(line_id);
    cache.mark_built(document, settings_revision + 1);
    document = snapshot;
    expect(cache.stale(document, settings_revision + 1),
           "undo-style document replacement invalidates stale 3D cache");

    cache.mark_built(document, settings_revision + 1);
    cache.invalidate();
    expect(cache.stale(document, settings_revision + 1),
           "explicit invalidation remains available for non-document inputs");

    if (failures == 0) {
        std::cout << "Architecture roof/cache tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
