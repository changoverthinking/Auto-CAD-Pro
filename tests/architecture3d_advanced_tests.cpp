#include "acp/architecture3d_advanced.hpp"

#include <cmath>
#include <iostream>
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

    const arch::WallSpec wall{{0.0, 0.0}, {5000.0, 0.0}, 200.0, 3000.0, 0.0};
    const arch::WallOpeningSpec door{
        wall, arch::OpeningKind::Door, 0.5, 900.0, 2100.0, 0.0, 2.0};
    const auto door_frame = arch::make_opening_frame(door, 60.0, 80.0);
    expect(door_frame.has_value(), "door frame builds");
    if (door_frame.has_value()) {
        expect(door_frame->vertices.size() == 24, "door frame contains three rails");
        expect(door_frame->triangles.size() == 36, "door frame rail topology complete");
        const auto b = acp::geo3d::bounds(*door_frame);
        expect(close(b.size().x, 900.0), "door frame preserves opening width");
        expect(close(b.size().y, 80.0), "door frame depth preserved");
        expect(close(b.size().z, 2100.0), "door frame height preserved");
    }

    const arch::WallOpeningSpec window{
        wall, arch::OpeningKind::Window, 0.65, 1200.0, 1200.0, 900.0, 2.0};
    const auto window_frame = arch::make_opening_frame(window, 50.0, 70.0);
    expect(window_frame.has_value(), "window frame builds");
    if (window_frame.has_value()) {
        expect(window_frame->vertices.size() == 32, "window frame contains four rails");
        expect(window_frame->triangles.size() == 48, "window frame topology complete");
        const auto b = acp::geo3d::bounds(*window_frame);
        expect(close(b.min.z, 900.0), "window frame sill follows opening");
        expect(close(b.max.z, 2100.0), "window frame head follows opening");
    }
    expect(
        !arch::make_opening_frame(window, 700.0, 70.0).has_value(),
        "oversized frame profile rejected");

    const arch::WallSpec wall_x{{0.0, 0.0}, {4000.0, 0.0}, 200.0, 3000.0, 0.0};
    const arch::WallSpec wall_y{{0.0, 0.0}, {0.0, 3000.0}, 200.0, 3000.0, 0.0};
    const auto joined = arch::make_mitered_wall_pair(wall_x, wall_y);
    expect(joined.has_value(), "perpendicular walls form a miter pair");
    if (joined.has_value()) {
        expect(acp::geo3d::valid_mesh(joined->first), "first miter wall mesh valid");
        expect(acp::geo3d::valid_mesh(joined->second), "second miter wall mesh valid");
        const auto a = acp::geo3d::bounds(joined->first);
        const auto b = acp::geo3d::bounds(joined->second);
        expect(close(a.min.y, -100.0) && close(a.max.y, 100.0), "first wall keeps thickness");
        expect(close(b.min.x, -100.0) && close(b.max.x, 100.0), "second wall keeps thickness");
    }

    const arch::WallSpec parallel{{0.0, 0.0}, {3000.0, 0.0}, 200.0, 3000.0, 0.0};
    expect(
        !arch::make_mitered_wall_pair(wall_x, parallel).has_value(),
        "collinear walls do not create unstable miter");
    const arch::WallSpec detached{{10.0, 10.0}, {10.0, 3010.0}, 200.0, 3000.0, 0.0};
    expect(
        !arch::make_mitered_wall_pair(wall_x, detached).has_value(),
        "detached walls rejected");

    const std::vector<arch::Level> levels{
        {"Ground", 0.0}, {"Level 2", 3200.0}, {"Roof", 6500.0}};
    const auto ground = arch::story_span(levels, "Ground");
    expect(ground.has_value(), "ground story resolves");
    if (ground.has_value()) {
        expect(close(ground->base_z, 0.0), "ground base elevation correct");
        expect(close(ground->top_z, 3200.0), "ground top elevation correct");
        expect(close(ground->height(), 3200.0), "ground story height correct");
    }
    const auto level2 = arch::story_span(levels, "Level 2");
    expect(level2.has_value() && close(level2->height(), 3300.0), "second story uses next higher level");
    expect(!arch::story_span(levels, "Roof").has_value(), "topmost level has no story above");

    acp::Document document;
    const auto line_id = document.insert(acp::LineEntity{{{0.0, 0.0}, {5000.0, 0.0}}});
    const auto poly_id = document.insert(acp::PolylineEntity{{
        {0.0, 0.0}, {4000.0, 0.0}, {4000.0, 3000.0}, {0.0, 3000.0}}, true});
    expect(line_id != 0 && poly_id != 0, "2D story source geometry inserted");

    const auto story_walls = arch::walls_for_story(document, levels, "Level 2", 200.0);
    expect(story_walls.size() == 1, "line creates one level-aware wall");
    if (story_walls.size() == 1) {
        const auto* object = story_walls.find(story_walls.ids().front());
        expect(object != nullptr, "story wall object exists");
        if (object != nullptr) {
            const auto b = acp::geo3d::bounds(object->mesh);
            expect(close(b.min.z, 3200.0), "story wall base follows level");
            expect(close(b.max.z, 6500.0), "story wall top follows next level");
        }
    }

    const auto roof_slab = arch::slabs_at_level(document, levels, "Roof", 180.0);
    expect(roof_slab.size() == 1, "closed polyline creates slab at named level");
    if (roof_slab.size() == 1) {
        const auto* object = roof_slab.find(roof_slab.ids().front());
        expect(object != nullptr, "roof slab object exists");
        if (object != nullptr) {
            const auto b = acp::geo3d::bounds(object->mesh);
            expect(close(b.max.z, 6500.0), "slab top locks to level elevation");
            expect(close(b.min.z, 6320.0), "slab thickness extends downward");
        }
    }

    if (failures == 0) {
        std::cout << "Advanced architectural 3D tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
