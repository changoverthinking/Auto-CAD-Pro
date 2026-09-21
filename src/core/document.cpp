#include "acp/document.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace acp {
namespace {
bool valid_line_type(LineType value) noexcept {
    return value == LineType::Continuous ||
           value == LineType::Dashed ||
           value == LineType::Center;
}
} // namespace

Document::Document() {
    layers_.emplace(kDefaultLayerId, Layer{kDefaultLayerId, "0", true, false, 0.25});
}

EntityId Document::insert(Entity entity) {
    while (entities_.contains(next_id_)) {
        ++next_id_;
    }
    const EntityId id = next_id_++;
    entities_.emplace(id, std::move(entity));
    properties_.emplace(id, EntityProperties{});
    return id;
}

bool Document::insert_with_id(EntityId id, Entity entity) {
    if (id == 0 || entities_.contains(id)) {
        return false;
    }
    entities_.emplace(id, std::move(entity));
    properties_.emplace(id, EntityProperties{});
    if (id >= next_id_) {
        next_id_ = id + 1;
    }
    return true;
}

bool Document::erase(EntityId id) {
    const bool erased = entities_.erase(id) == 1;
    if (erased) {
        properties_.erase(id);
    }
    return erased;
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
    for (const auto& entry : entities_) {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

const EntityProperties* Document::properties(EntityId id) const noexcept {
    const auto it = properties_.find(id);
    return it == properties_.end() ? nullptr : &it->second;
}

EntityProperties* Document::properties(EntityId id) noexcept {
    const auto it = properties_.find(id);
    return it == properties_.end() ? nullptr : &it->second;
}

bool Document::set_entity_layer(EntityId id, LayerId layer_id) noexcept {
    auto* props = properties(id);
    if (props == nullptr || layer(layer_id) == nullptr) {
        return false;
    }
    props->layer_id = layer_id;
    return true;
}

bool Document::entity_visible(EntityId id) const noexcept {
    const auto* props = properties(id);
    if (props == nullptr || !props->visible) {
        return false;
    }
    const auto* owner = layer(props->layer_id);
    return owner != nullptr && owner->visible;
}

bool Document::entity_locked(EntityId id) const noexcept {
    const auto* props = properties(id);
    if (props == nullptr) {
        return true;
    }
    const auto* owner = layer(props->layer_id);
    return owner == nullptr || owner->locked;
}

double Document::effective_line_weight(EntityId id) const noexcept {
    const auto* props = properties(id);
    if (props == nullptr) {
        return 0.0;
    }
    if (props->line_weight_override.has_value()) {
        return *props->line_weight_override;
    }
    const auto* owner = layer(props->layer_id);
    return owner == nullptr ? 0.0 : owner->line_weight;
}

RgbColor Document::effective_color(EntityId id) const noexcept {
    const auto* props = properties(id);
    if (props == nullptr) {
        return {};
    }
    if (props->color_override.has_value()) {
        return *props->color_override;
    }
    const auto* owner = layer(props->layer_id);
    return owner == nullptr ? RgbColor{} : owner->color;
}

LineType Document::effective_line_type(EntityId id) const noexcept {
    const auto* props = properties(id);
    if (props == nullptr) {
        return LineType::Continuous;
    }
    if (props->line_type_override.has_value()) {
        return *props->line_type_override;
    }
    const auto* owner = layer(props->layer_id);
    return owner == nullptr ? LineType::Continuous : owner->line_type;
}

LayerId Document::create_layer(std::string name) {
    if (name.empty() || layer_name_exists(name)) {
        return 0;
    }
    while (layers_.contains(next_layer_id_)) {
        ++next_layer_id_;
    }
    const LayerId id = next_layer_id_++;
    layers_.emplace(id, Layer{id, std::move(name), true, false, 0.25});
    return id;
}

bool Document::insert_layer_with_id(Layer value) {
    if (value.id == 0 ||
        value.name.empty() ||
        layers_.contains(value.id) ||
        layer_name_exists(value.name) ||
        !std::isfinite(value.line_weight) ||
        value.line_weight < 0.0 ||
        !valid_line_type(value.line_type)) {
        return false;
    }

    const LayerId id = value.id;
    layers_.emplace(id, std::move(value));
    if (id >= next_layer_id_) {
        next_layer_id_ = id + 1;
    }
    return true;
}

const Layer* Document::layer(LayerId id) const noexcept {
    const auto it = layers_.find(id);
    return it == layers_.end() ? nullptr : &it->second;
}

Layer* Document::layer(LayerId id) noexcept {
    const auto it = layers_.find(id);
    return it == layers_.end() ? nullptr : &it->second;
}

std::vector<LayerId> Document::layer_ids() const {
    std::vector<LayerId> result;
    result.reserve(layers_.size());
    for (const auto& entry : layers_) {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool Document::rename_layer(LayerId id, std::string name) {
    auto* target = layer(id);
    if (target == nullptr || name.empty() || layer_name_exists(name, id)) {
        return false;
    }
    target->name = std::move(name);
    return true;
}

bool Document::set_layer_visible(LayerId id, bool visible) noexcept {
    auto* target = layer(id);
    if (target == nullptr) {
        return false;
    }
    target->visible = visible;
    return true;
}

bool Document::set_layer_locked(LayerId id, bool locked) noexcept {
    auto* target = layer(id);
    if (target == nullptr) {
        return false;
    }
    target->locked = locked;
    return true;
}

bool Document::set_layer_line_weight(LayerId id, double line_weight) noexcept {
    auto* target = layer(id);
    if (target == nullptr || !std::isfinite(line_weight) || line_weight < 0.0) {
        return false;
    }
    target->line_weight = line_weight;
    return true;
}

bool Document::set_layer_color(LayerId id, RgbColor color) noexcept {
    auto* target = layer(id);
    if (target == nullptr) {
        return false;
    }
    target->color = color;
    return true;
}

bool Document::set_layer_line_type(LayerId id, LineType line_type) noexcept {
    auto* target = layer(id);
    if (target == nullptr || !valid_line_type(line_type)) {
        return false;
    }
    target->line_type = line_type;
    return true;
}

bool Document::remove_layer(LayerId id) {
    if (id == kDefaultLayerId || !layers_.contains(id)) {
        return false;
    }
    for (const auto& entry : properties_) {
        if (entry.second.layer_id == id) {
            return false;
        }
    }
    return layers_.erase(id) == 1;
}

bool Document::layer_name_exists(const std::string& name, LayerId ignore_id) const {
    for (const auto& entry : layers_) {
        if (entry.first != ignore_id && entry.second.name == name) {
            return true;
        }
    }
    return false;
}

} // namespace acp
