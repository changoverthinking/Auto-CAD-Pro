#pragma once

#include "acp/block.hpp"
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

    virtual bool execute_project(Document& document, BlockLibrary& blocks) {
        (void)blocks;
        return execute(document);
    }
    virtual void undo_project(Document& document, BlockLibrary& blocks) {
        (void)blocks;
        undo(document);
    }
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

class UpdateEntityPropertiesCommand final : public Command {
public:
    UpdateEntityPropertiesCommand(EntityId id, EntityProperties replacement);

    bool execute(Document& document) override;
    void undo(Document& document) override;

private:
    EntityId id_{};
    EntityProperties replacement_;
    std::optional<EntityProperties> original_;
};

class CreateLayerCommand final : public Command {
public:
    explicit CreateLayerCommand(std::string name);

    bool execute(Document& document) override;
    void undo(Document& document) override;
    [[nodiscard]] LayerId id() const noexcept { return id_; }

private:
    std::string name_;
    std::optional<Layer> layer_;
    LayerId id_{0};
    bool executed_{false};
};

class CreateBlockFromEntityCommand final : public Command {
public:
    CreateBlockFromEntityCommand(EntityId source_id, std::string name);

    bool execute(Document& document) override;
    void undo(Document& document) override;
    bool execute_project(Document& document, BlockLibrary& blocks) override;
    void undo_project(Document& document, BlockLibrary& blocks) override;

    [[nodiscard]] BlockId block_id() const noexcept { return block_id_; }

private:
    EntityId source_id_{};
    std::string name_;
    std::optional<Entity> original_;
    std::optional<BlockDefinition> definition_;
    BlockId block_id_{0};
    bool executed_{false};
};

class UpdateLayerCommand final : public Command {
public:
    UpdateLayerCommand(LayerId id, Layer replacement);

    bool execute(Document& document) override;
    void undo(Document& document) override;

private:
    LayerId id_{};
    Layer replacement_;
    std::optional<Layer> original_;
};

class History {
public:
    bool apply(Document& document, std::unique_ptr<Command> command);
    bool apply(
        Document& document,
        BlockLibrary& blocks,
        std::unique_ptr<Command> command);
    bool undo(Document& document);
    bool undo(Document& document, BlockLibrary& blocks);
    bool redo(Document& document);
    bool redo(Document& document, BlockLibrary& blocks);

    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }

private:
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
};

} // namespace acp
