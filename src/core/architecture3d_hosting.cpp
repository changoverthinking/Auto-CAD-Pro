#include "acp/architecture3d.hpp"

#include <utility>

namespace acp::architecture3d {

model3d::ObjectId add_opening_object(
    model3d::Scene& scene,
    const WallOpeningSpec& opening,
    double panel_depth,
    std::string name,
    RgbColor color,
    std::optional<EntityId> source_entity_id,
    std::optional<model3d::ObjectId> host_object_id) {

    auto mesh = make_opening_panel(opening, panel_depth);
    if (!mesh.has_value()) {
        return 0;
    }

    const model3d::ObjectKind kind =
        opening.kind == OpeningKind::Door
            ? model3d::ObjectKind::Door
            : model3d::ObjectKind::Window;
    if (name.empty()) {
        name = opening.kind == OpeningKind::Door ? "Door" : "Window";
    }
    return scene.insert(
        std::move(*mesh),
        std::move(name),
        color,
        kind,
        source_entity_id,
        host_object_id);
}

} // namespace acp::architecture3d
