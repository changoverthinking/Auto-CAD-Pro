#include "acp/history.hpp"

#include <cmath>
#include <type_traits>
#include <utility>

namespace acp {

AddEntityCommand::AddEntityCommand(Entity entity)
    : entity_(std::move(entity)) {}

bool AddEntityCommand::execute(Document& document) {
    if (executed_ && id_ != 0) {
        if (!document.insert_with_id(id_, entity_)) {
            return false;
        }
        if (properties_.has_value()) {
            if (auto* props = document.properties(id_)) {
                *props = *properties_;
            }
        }
        return true;
    }

    id_ = document.insert(entity_);
    executed_ = true;
    return id_ != 0;
}

void AddEntityCommand::undo(Document& document) {
    if (id_ == 0) {
        return;
    }

    if (const auto* props = document.properties(id_)) {
        properties_ = *props;
    }
    (void)document.erase(id_);
}

RemoveEntityCommand::RemoveEntityCommand(EntityId id)
    : id_(id) {}

bool RemoveEntityCommand::execute(Document& document) {
    const Entity* existing = document.find(id_);
    const EntityProperties* props = document.properties(id_);
    if (existing == nullptr || props == nullptr) {
        return false;
    }

    backup_ = *existing;
    backup_properties_ = *props;
    return document.erase(id_);
}

void RemoveEntityCommand::undo(Document& document) {
    if (!backup_.has_value() || !backup_properties_.has_value()) {
        return;
    }

    if (document.insert_with_id(id_, *backup_)) {
        if (auto* props = document.properties(id_)) {
            *props = *backup_properties_;
        }
    }
}

UpdateEntityCommand::UpdateEntityCommand(EntityId id, Entity replacement)
    : id_(id), replacement_(std::move(replacement)) {}

bool UpdateEntityCommand::execute(Document& document) {
    Entity* existing = document.find(id_);
    if (existing == nullptr) {
        return false;
    }

    if (!original_.has_value()) {
        original_ = *existing;
    }
    *existing = replacement_;
    return true;
}

void UpdateEntityCommand::undo(Document& document) {
    if (!original_.has_value()) {
        return;
    }
    if (Entity* existing = document.find(id_)) {
        *existing = *original_;
    }
}

CreateLayerCommand::CreateLayerCommand(std::string name)
    : name_(std::move(name)) {}

bool CreateLayerCommand::execute(Document& document) {
    if (executed_ && layer_.has_value()) {
        return document.insert_layer_with_id(*layer_);
    }

    id_ = document.create_layer(name_);
    if (id_ == 0) {
        return false;
    }

    const Layer* created = document.layer(id_);
    if (created == nullptr) {
        return false;
    }

    layer_ = *created;
    executed_ = true;
    return true;
}

void CreateLayerCommand::undo(Document& document) {
    if (!executed_ || id_ == 0) {
        return;
    }

    if (const Layer* current = document.layer(id_)) {
        layer_ = *current;
    }
    (void)document.remove_layer(id_);
}

UpdateEntityPropertiesCommand::UpdateEntityPropertiesCommand(
    EntityId id,
    EntityProperties replacement)
    : id_(id), replacement_(std::move(replacement)) {}

bool UpdateEntityPropertiesCommand::execute(Document& document) {
    EntityProperties* existing = document.properties(id_);
    if (existing == nullptr || document.layer(replacement_.layer_id) == nullptr) {
        return false;
    }
    if (replacement_.line_weight_override.has_value() &&
        (!std::isfinite(*replacement_.line_weight_override) ||
         *replacement_.line_weight_override < 0.0)) {
        return false;
    }

    if (!original_.has_value()) {
        original_ = *existing;
    }
    *existing = replacement_;
    return true;
}

void UpdateEntityPropertiesCommand::undo(Document& document) {
    if (!original_.has_value()) {
        return;
    }
    if (EntityProperties* existing = document.properties(id_)) {
        *existing = *original_;
    }
}

CreateBlockFromEntityCommand::CreateBlockFromEntityCommand(
    EntityId source_id,
    std::string name)
    : source_id_(source_id), name_(std::move(name)) {}

namespace {

std::optional<BlockPrimitive> block_primitive_from_entity(const Entity& entity) {
    return std::visit([](const auto& value) -> std::optional<BlockPrimitive> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (
            std::is_same_v<T, LineEntity> ||
            std::is_same_v<T, CircleEntity> ||
            std::is_same_v<T, ArcEntity> ||
            std::is_same_v<T, PolylineEntity>) {
            return BlockPrimitive{value};
        }
        return std::nullopt;
    }, entity);
}

geo::Vec2 block_base_point(const BlockPrimitive& primitive) {
    return std::visit([](const auto& value) -> geo::Vec2 {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            return value.segment.a;
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            return value.circle.center;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            return value.arc.center;
        } else {
            return value.points.empty() ? geo::Vec2{} : value.points.front();
        }
    }, primitive);
}

} // namespace

bool CreateBlockFromEntityCommand::execute(Document&) {
    return false;
}

void CreateBlockFromEntityCommand::undo(Document&) {}

bool CreateBlockFromEntityCommand::execute_project(
    Document& document,
    BlockLibrary& blocks) {

    Entity* source = document.find(source_id_);
    if (source == nullptr) {
        return false;
    }

    if (!executed_) {
        const auto primitive = block_primitive_from_entity(*source);
        if (!primitive.has_value()) {
            return false;
        }

        const geo::Vec2 base = block_base_point(*primitive);
        const BlockId created =
            blocks.create(name_, base, std::vector<BlockPrimitive>{*primitive});
        if (created == 0) {
            return false;
        }

        const BlockDefinition* created_definition = blocks.find(created);
        if (created_definition == nullptr) {
            (void)blocks.remove(created);
            return false;
        }

        original_ = *source;
        definition_ = *created_definition;
        block_id_ = created;
        *source = BlockReferenceEntity{block_id_, base, 0.0, 1.0};
        executed_ = true;
        return true;
    }

    if (!definition_.has_value() || block_id_ == 0) {
        return false;
    }
    if (!blocks.insert_with_id(*definition_)) {
        return false;
    }

    *source = BlockReferenceEntity{
        block_id_, definition_->base_point, 0.0, 1.0};
    return true;
}

void CreateBlockFromEntityCommand::undo_project(
    Document& document,
    BlockLibrary& blocks) {

    if (!executed_ || !original_.has_value() || block_id_ == 0) {
        return;
    }

    Entity* source = document.find(source_id_);
    if (source == nullptr) {
        return;
    }

    if (const BlockDefinition* current = blocks.find(block_id_)) {
        definition_ = *current;
    }

    *source = *original_;
    (void)blocks.remove(block_id_);
}

UpdateLayerCommand::UpdateLayerCommand(LayerId id, Layer replacement)
    : id_(id), replacement_(std::move(replacement)) {}

namespace {
bool apply_layer_state(Document& document, LayerId id, const Layer& state) {
    Layer* existing = document.layer(id);
    if (existing == nullptr || state.id != id || state.name.empty() ||
        !std::isfinite(state.line_weight) || state.line_weight < 0.0) {
        return false;
    }

    if (existing->name != state.name &&
        !document.rename_layer(id, state.name)) {
        return false;
    }
    return document.set_layer_visible(id, state.visible) &&
           document.set_layer_locked(id, state.locked) &&
           document.set_layer_line_weight(id, state.line_weight);
}
} // namespace

bool UpdateLayerCommand::execute(Document& document) {
    Layer* existing = document.layer(id_);
    if (existing == nullptr) {
        return false;
    }

    if (!original_.has_value()) {
        original_ = *existing;
    }
    return apply_layer_state(document, id_, replacement_);
}

void UpdateLayerCommand::undo(Document& document) {
    if (!original_.has_value()) {
        return;
    }
    (void)apply_layer_state(document, id_, *original_);
}

bool History::apply(Document& document, std::unique_ptr<Command> command) {
    if (!command || !command->execute(document)) {
        return false;
    }

    undo_.push_back(std::move(command));
    redo_.clear();
    return true;
}

bool History::undo(Document& document) {
    if (undo_.empty()) {
        return false;
    }

    auto command = std::move(undo_.back());
    undo_.pop_back();
    command->undo(document);
    redo_.push_back(std::move(command));
    return true;
}

bool History::redo(Document& document) {
    if (redo_.empty()) {
        return false;
    }

    auto command = std::move(redo_.back());
    redo_.pop_back();
    if (!command->execute(document)) {
        redo_.push_back(std::move(command));
        return false;
    }

    undo_.push_back(std::move(command));
    return true;
}

bool History::apply(
    Document& document,
    BlockLibrary& blocks,
    std::unique_ptr<Command> command) {

    if (!command || !command->execute_project(document, blocks)) {
        return false;
    }

    undo_.push_back(std::move(command));
    redo_.clear();
    return true;
}

bool History::undo(Document& document, BlockLibrary& blocks) {
    if (undo_.empty()) {
        return false;
    }

    auto command = std::move(undo_.back());
    undo_.pop_back();
    command->undo_project(document, blocks);
    redo_.push_back(std::move(command));
    return true;
}

bool History::redo(Document& document, BlockLibrary& blocks) {
    if (redo_.empty()) {
        return false;
    }

    auto command = std::move(redo_.back());
    redo_.pop_back();
    if (!command->execute_project(document, blocks)) {
        redo_.push_back(std::move(command));
        return false;
    }

    undo_.push_back(std::move(command));
    return true;
}

} // namespace acp
