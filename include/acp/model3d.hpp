#pragma once

#include "acp/document.hpp"
#include "acp/geometry3d.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace acp::model3d {

using ObjectId = std::uint64_t;

enum class ObjectKind : std::uint8_t {
    Generic = 0,
    Wall,
    Slab,
    Column,
    Beam,
    Door,
    Window,
    Roof,
    Stair
};

struct Object3D {
    ObjectId id{};
    std::string name;
    geo3d::Mesh mesh;
    RgbColor color{200, 205, 214};
    bool visible{true};
    ObjectKind kind{ObjectKind::Generic};
    std::optional<EntityId> source_entity_id;
};

class Scene {
public:
    [[nodiscard]] ObjectId insert(
        geo3d::Mesh mesh,
        std::string name = {},
        RgbColor color = {200, 205, 214},
        ObjectKind kind = ObjectKind::Generic,
        std::optional<EntityId> source_entity_id = std::nullopt);

    [[nodiscard]] const Object3D* find(ObjectId id) const noexcept;
    [[nodiscard]] Object3D* find(ObjectId id) noexcept;
    [[nodiscard]] std::vector<ObjectId> ids() const;
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }
    [[nodiscard]] bool erase(ObjectId id) noexcept;
    void clear() noexcept;
    [[nodiscard]] geo3d::Aabb3 bounds() const noexcept;

private:
    ObjectId next_id_{1};
    std::unordered_map<ObjectId, Object3D> objects_;
};

[[nodiscard]] std::optional<geo3d::Mesh> extrude_polygon(
    const std::vector<geo::Vec2>& polygon,
    double height,
    double base_z = 0.0);

[[nodiscard]] Scene extrude_closed_polylines(
    const Document& document,
    double height,
    double base_z = 0.0);

} // namespace acp::model3d
