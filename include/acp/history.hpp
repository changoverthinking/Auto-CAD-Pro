#pragma once

#include "acp/document.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace acp {

class Command {
public:
    virtual ~Command() = default;
    virtual bool execute(Document& document) = 0;
    virtual void undo(Document& document) = 0;
};

class AddEntityCommand final : public Command {
public:
    explicit AddEntityCommand(Entity entity);

    bool execute(Document& document) override;
    void undo(Document& document) override;
    [[nodiscard]] EntityId id() const noexcept { return id_; }

private:
    Entity entity_;
    std::optional<EntityProperties> properties_;
    EntityId id_{0};
    bool executed_{false};
};

class RemoveEntityCommand final : public Command {
public:
    explicit RemoveEntityCommand(EntityId id);

    bool execute(Document& document) override;
    void undo(Document& document) override;

private:
    EntityId id_;
    std::optional<Entity> backup_;
    std::optional<EntityProperties> backup_properties_;
};

class UpdateEntityCommand final : public Command {
public:
    UpdateEntityCommand(EntityId id, Entity replacement);

    bool execute(Document& document) override;
    void undo(Document& document) override;

private:
    EntityId id_{};
    Entity replacement_;
    std::optional<Entity> original_;
};

class History {
public:
    bool apply(Document& document, std::unique_ptr<Command> command);
    bool undo(Document& document);
    bool redo(Document& document);

    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }

private:
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
};

} // namespace acp
