#include "acp/history.hpp"

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

UpdateEntityPropertiesCommand::UpdateEntityPropertiesCommand(
    EntityId id,
    EntityProperties replacement)
    : id_(id), replacement_(std::move(replacement)) {}

bool UpdateEntityPropertiesCommand::execute(Document& document) {
    EntityProperties* existing = document.properties(id_);
    if (existing == nullptr || document.layer(replacement_.layer_id) == nullptr) {
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

UpdateLayerCommand::UpdateLayerCommand(LayerId id, Layer replacement)
    : id_(id), replacement_(std::move(replacement)) {}

bool UpdateLayerCommand::execute(Document& document) {
    Layer* existing = document.layer(id_);
    if (existing == nullptr || replacement_.id != id_) {
        return false;
    }

    if (!original_.has_value()) {
        original_ = *existing;
    }
    *existing = replacement_;
    return true;
}

void UpdateLayerCommand::undo(Document& document) {
    if (!original_.has_value()) {
        return;
    }
    if (Layer* existing = document.layer(id_)) {
        *existing = *original_;
    }
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

} // namespace acp
