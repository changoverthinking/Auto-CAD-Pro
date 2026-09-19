#pragma once

#include "acp/geometry2d.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace acp {

using EntityId = std::uint64_t;

struct LineEntity { geo::Segment segment; };
struct CircleEntity { geo::Circle circle; };
struct ArcEntity { geo::Arc arc; };
struct PolylineEntity {
    std::vector<geo::Vec2> points;
    bool closed{false};
};

using Entity = std::variant<LineEntity, CircleEntity, ArcEntity, PolylineEntity>;

class Document {
public:
    [[nodiscard]] EntityId insert(Entity entity);
    bool insert_with_id(EntityId id, Entity entity);
    [[nodiscard]] bool erase(EntityId id);
    [[nodiscard]] const Entity* find(EntityId id) const noexcept;
    [[nodiscard]] Entity* find(EntityId id) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    [[nodiscard]] std::vector<EntityId> ids() const;

private:
    EntityId next_id_{1};
    std::unordered_map<EntityId, Entity> entities_;
};

} // namespace acp
