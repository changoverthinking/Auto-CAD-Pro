#include "acp/document.hpp"
#include "acp/id_allocation.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

namespace acp {
namespace {
bool valid_line_type(LineType value) noexcept {
    return value == LineType::Continuous ||
           value == LineType::Dashed ||
           value == LineType::Center;
}

std::atomic<DocumentRevision> g_next_document_revision{1};

DocumentRevision next_document_revision() noexcept {
    return g_next_document_revision.fetch_add(1, std::memory_order_relaxed);
}
} // namespace

Document::Document() {
    layers_.emplace(kDefaultLayerId, Layer{kDefaultLayerId, "0", true, false, 0.25});
    revision_ = next_document_revision();
}

Document::Document(const Document& other)
    : next_id_(other.next_id_),
      next_layer_id_(other.next_layer_id_),
      entities_(other.entities_),
      properties_(other.properties_),
      layers_(other.layers_),
      revision_(next_document_revision()) {}

Document::Document(Document&& other) noexcept
    : next_id_(other.next_id_),
      next_layer_id_(other.next_layer_id_),
      entities_(std::move(other.entities_)),
      properties_(std::move(other.properties_)),
      layers_(std::move(other.layers_)),
      revision_(next_document_revision()) {}

Document& Document::operator=(const Document& other) {
    if (this == &other) {
        return *this;
    }
    next_id_ = other.next_id_;
    next_layer_id_ = other.next_layer_id_;
    entities_ = other.entities_;
    properties_ = other.properties_;
    layers_ = other.layers_;
    revision_ = next_document_revision();
    return *this;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    next_id_ = other.next_id_;
    next_layer_id_ = other.next_layer_id_;
    entities_ = std::move(other.entities_);
    properties_ = std::move(other.properties_);
    layers_ = std::move(other.layers_);
    revision_ = next_document_revision();
    return *this;
}

void Document::mark_changed() noexcept {
    revision_ = next_document_revision();
}

EntityId Document::insert(Entity entity) {
    const EntityId id = detail::allocate_id(entities_, next_id_);
    if (id == 0) return 0;
    entities_.emplace(id, std::move(entity));
    properties_.emplace(id, EntityProperties{});
    mark_changed();
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
    mark_changed();
    return true;
}

bool Document::erase(EntityId id) {
    const bool erased = entities_.erase(id) == 1;
    if (erased) {
        properties_.erase(id);
        mark_changed();
    }
    return erased;
}

const Entity* Document::find(EntityId id) const noexcept {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

Entity* Document::find(EntityId id) noexcept {
    const auto it = entities_.find(id);
    if (it == entities_.end()) {
        return nullptr;
    }
    // Returning mutable access means the caller may change geometry without
    // going through a setter. Invalidate derived caches conservatively now.
    mark_changed();
    return &it->second;
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
    if (it == properties_.end()) {
        return nullptr;
    }
    mark_changed();
    return &it->second;
}

bool Document::set_entity_layer(EntityId id, LayerId layer_id) noexcept {
    const auto entity_it = properties_.find(id);
    if (entity_it == properties_.end() || !layers_.contains(layer_id)) {
        return false;
    }
    if (entity_it->second.layer_id == layer_id) {
        return true;
    }
    entity_it->second.layer_id = layer_id;
    mark_changed();
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

bool Document::entity_editable(EntityId id) const noexcept {
    return find(id) != nullptr &&
           entity_visible(id) &&
           !entity_locked(id);
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
    const LayerId id = detail::allocate_id(layers_, next_layer_id_);
    if (id == 0) return 0;
    layers_.emplace(id, Layer{id, std::move(name), true, false, 0.25});
    mark_changed();
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
    mark_changed();
    return true;
}

const Layer* Document::layer(LayerId id) const noexcept {
    const auto it = layers_.find(id);
    return it == layers_.end() ? nullptr : &it->second;
}

Layer* Document::layer(LayerId id) noexcept {
    const auto it = layers_.find(id);
    if (it == layers_.end()) {
        return nullptr;
    }
    mark_changed();
    return &it->second;
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
    auto it = layers_.find(id);
    if (it == layers_.end() || name.empty() || layer_name_exists(name, id)) {
        return false;
    }
    if (it->second.name == name) {
        return true;
    }
    it->second.name = std::move(name);
    mark_changed();
    return true;
}

bool Document::set_layer_visible(LayerId id, bool visible) noexcept {
    auto it = layers_.find(id);
    if (it == layers_.end()) {
        return false;
    }
    if (it->second.visible == visible) {
        return true;
    }
    it->second.visible = visible;
    mark_changed();
    return true;
}

bool Document::set_layer_locked(LayerId id, bool locked) noexcept {
    auto it = layers_.find(id);
    if (it == layers_.end()) {
        return false;
    }
    if (it->second.locked == locked) {
        return true;
    }
    it->second.locked = locked;
    mark_changed();
    return true;
}

bool Document::set_layer_line_weight(LayerId id, double line_weight) noexcept {
    auto it = layers_.find(id);
    if (it == layers_.end() || !std::isfinite(line_weight) || line_weight < 0.0) {
        return false;
    }
    if (it->second.line_weight == line_weight) {
        return true;
    }
    it->second.line_weight = line_weight;
    mark_changed();
    return true;
}

bool Document::set_layer_color(LayerId id, RgbColor color) noexcept {
    auto it = layers_.find(id);
    if (it == layers_.end()) {
        return false;
    }
    if (it->second.color == color) {
        return true;
    }
    it->second.color = color;
    mark_changed();
    return true;
}

bool Document::set_layer_line_type(LayerId id, LineType line_type) noexcept {
    auto it = layers_.find(id);
    if (it == layers_.end() || !valid_line_type(line_type)) {
        return false;
    }
    if (it->second.line_type == line_type) {
        return true;
    }
    it->second.line_type = line_type;
    mark_changed();
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
    const bool removed = layers_.erase(id) == 1;
    if (removed) {
        mark_changed();
    }
    return removed;
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
