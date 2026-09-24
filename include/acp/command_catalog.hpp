#pragma once

#include <span>
#include <string_view>

namespace acp::commands {

enum class Workspace {
    Global,
    Drafting2D,
    Modeling3D,
    Architecture,
    Structure,
    Documentation,
};

enum class FeatureState {
    Production,
    Verified,
    Prototype,
    Planned,
};

struct CommandDescriptor {
    std::string_view id;
    std::string_view icon_id;
    std::string_view label;
    Workspace workspace{Workspace::Global};
    FeatureState state{FeatureState::Planned};
    bool ui_reachable{false};
};

[[nodiscard]] std::span<const CommandDescriptor> catalog() noexcept;
[[nodiscard]] const CommandDescriptor* find(std::string_view id) noexcept;
[[nodiscard]] const CommandDescriptor* find_by_icon(std::string_view icon_id) noexcept;
[[nodiscard]] bool can_expose_as_active_tool(const CommandDescriptor& command) noexcept;

} // namespace acp::commands
