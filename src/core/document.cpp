#include "acp/document.hpp"

#include <algorithm>

namespace acp {

EntityId Document::insert(Entity entity) {
    while (entities_.contains(next_id_)) {
        ++next_id_;
    }
    const EntityId id = next_id_++;
    entities_.emplace(id, std::move(entity));
    return id;
}

bool Document::insert_with_id(EntityId id, Entity entity) {
    if (id == 0 || entities_.contains(id)) {
        return false;
    }
    entities_.emplace(id, std::move(entity));
    if (id >= next_id_) {
        next_id_ = id + 1;
    }
    return true;
}

bool Document::erase(EntityId id) {
    return entities_.erase(id) == 1;
}

const Entity* Document::find(EntityId id) const noexcept {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

Entity* Document::find(EntityId id) noexcept {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

std::vector<EntityId> Document::ids() const {
    std::vector<EntityId> result;
    result.reserve(entities_.size());
    for (const auto& [id, _] : entities_) {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace acp
