#pragma once

#include "acp/document.hpp"

#include <optional>
#include <algorithm>

namespace acp {
class BlockLibrary;
}

namespace acp::selection {

// Ordered unique selection; the last item is the primary entity for inspection.
class Set {
public:
    Set& operator=(std::optional<EntityId> id) {
        reset();
        if (id && *id != 0) ids_.push_back(*id);
        return *this;
    }
    [[nodiscard]] bool has_value() const noexcept { return !ids_.empty(); }
    [[nodiscard]] EntityId operator*() const { return ids_.back(); }
    [[nodiscard]] EntityId value_or(EntityId fallback) const noexcept {
        return has_value() ? ids_.back() : fallback;
    }
    [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
    [[nodiscard]] const std::vector<EntityId>& ids() const noexcept { return ids_; }
    void reset() noexcept { ids_.clear(); }
    void toggle(EntityId id) {
        if (id == 0) return;
        const auto it = std::find(ids_.begin(), ids_.end(), id);
        if (it == ids_.end()) ids_.push_back(id);
        else ids_.erase(it);
    }
    void prune(const Document& document) {
        std::erase_if(ids_, [&](EntityId id) { return !document.entity_visible(id); });
    }
    void select_all_editable(const Document& document) {
        ids_ = document.ids();
        std::erase_if(ids_, [&](EntityId id) { return !document.entity_editable(id); });
    }
private:
    std::vector<EntityId> ids_;
};

struct Hit {
    EntityId id{};
    double distance{};
    geo::Vec2 nearest{};
};

[[nodiscard]] double distance_to_entity(const Entity& entity, geo::Vec2 point) noexcept;

[[nodiscard]] std::optional<Hit> hit_test(
    const Document& document,
    geo::Vec2 point,
    double aperture) noexcept;

[[nodiscard]] std::optional<Hit> hit_test(
    const Document& document,
    const BlockLibrary& blocks,
    geo::Vec2 point,
    double aperture) noexcept;

} // namespace acp::selection
