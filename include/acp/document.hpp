#pragma once

#include "acp/annotation.hpp"
#include "acp/block_types.hpp"
#include "acp/geometry2d.hpp"
#include "acp/hatch.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace acp {

using EntityId = std::uint64_t;
using LayerId = std::uint32_t;

inline constexpr LayerId kDefaultLayerId = 1;

struct LineEntity { geo::Segment segment; };
struct CircleEntity { geo::Circle circle; };
struct ArcEntity { geo::Arc arc; };
struct PolylineEntity {
    std::vector<geo::Vec2> points;
    bool closed{false};
};

using Entity = std::variant<
    LineEntity,
    CircleEntity,
    ArcEntity,
    PolylineEntity,
    BlockReferenceEntity,
    TextEntity,
    LinearDimensionEntity,
    HatchEntity>;

struct Layer {
    LayerId id{};
    std::string name;
    bool visible{true};
    bool locked{false};
    double line_weight{0.25};
};

struct EntityProperties {
    LayerId layer_id{kDefaultLayerId};
    bool visible{true};
    std::optional<double> line_weight_override;
};

class Document {
public:
    Document();

    [[nodiscard]] EntityId insert(Entity entity);
    bool insert_with_id(EntityId id, Entity entity);
    [[nodiscard]] bool erase(EntityId id);
    [[nodiscard]] const Entity* find(EntityId id) const noexcept;
    [[nodiscard]] Entity* find(EntityId id) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    [[nodiscard]] std::vector<EntityId> ids() const;

    [[nodiscard]] const EntityProperties* properties(EntityId id) const noexcept;
    [[nodiscard]] EntityProperties* properties(EntityId id) noexcept;
    bool set_entity_layer(EntityId id, LayerId layer_id) noexcept;
    [[nodiscard]] bool entity_visible(EntityId id) const noexcept;
    [[nodiscard]] bool entity_locked(EntityId id) const noexcept;
    [[nodiscard]] double effective_line_weight(EntityId id) const noexcept;

    [[nodiscard]] LayerId create_layer(std::string name);
    bool insert_layer_with_id(Layer layer);
    [[nodiscard]] const Layer* layer(LayerId id) const noexcept;
    [[nodiscard]] Layer* layer(LayerId id) noexcept;
    [[nodiscard]] std::vector<LayerId> layer_ids() const;
    bool rename_layer(LayerId id, std::string name);
    bool set_layer_visible(LayerId id, bool visible) noexcept;
    bool set_layer_locked(LayerId id, bool locked) noexcept;
    bool set_layer_line_weight(LayerId id, double line_weight) noexcept;
    [[nodiscard]] bool remove_layer(LayerId id);

private:
    [[nodiscard]] bool layer_name_exists(const std::string& name, LayerId ignore_id = 0) const;

    EntityId next_id_{1};
    LayerId next_layer_id_{kDefaultLayerId + 1};
    std::unordered_map<EntityId, Entity> entities_;
    std::unordered_map<EntityId, EntityProperties> properties_;
    std::unordered_map<LayerId, Layer> layers_;
};

} // namespace acp
