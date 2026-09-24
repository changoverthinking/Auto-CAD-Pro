#include "acp/command_catalog.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <string>

#ifndef ACP_TEST_FULL_ICON_CATALOG_PATH
#error ACP_TEST_FULL_ICON_CATALOG_PATH must be defined
#endif

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace

int main() {
    using namespace acp::commands;

    std::ifstream input(ACP_TEST_FULL_ICON_CATALOG_PATH);
    expect(input.good(), "full icon catalog opens");

    std::set<std::string> icon_ids;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            const bool inserted = icon_ids.insert(line).second;
            expect(inserted, "icon IDs are unique");
        }
    }

    expect(icon_ids.size() == 553, "user icon pack contains exactly 553 unique icon IDs");

    const auto commands = catalog();
    expect(commands.size() == 79, "known command registry cardinality is stable");

    std::set<std::string_view> command_ids;
    for (const auto& command : commands) {
        expect(!command.id.empty(), "command ID is not empty");
        expect(!command.icon_id.empty(), "command icon ID is not empty");
        expect(!command.label.empty(), "command label is not empty");
        expect(command_ids.insert(command.id).second, "command IDs are unique");
        expect(icon_ids.contains(std::string(command.icon_id)), "every registered command icon exists in the 553-icon pack");
        if (command.state == FeatureState::Planned) {
            expect(!command.ui_reachable, "planned commands cannot be exposed as active tools");
        }
        if (command.ui_reachable) {
            expect(can_expose_as_active_tool(command), "reachable non-planned commands can be exposed");
        }
    }

    const auto* line_command = find("draw.line");
    expect(line_command != nullptr, "draw.line command resolves");
    if (line_command) {
        expect(line_command->icon_id == "pack-c.line", "Line uses the uploaded Line icon");
        expect(line_command->workspace == Workspace::Drafting2D, "Line belongs to 2D Drafting");
    }

    const auto* wall_command = find("bim.wall");
    expect(wall_command != nullptr, "bim.wall command resolves");
    if (wall_command) {
        expect(wall_command->icon_id == "pack-m.wall", "Wall uses the uploaded Wall icon");
        expect(!wall_command->ui_reachable, "BIM wall core is not falsely marked GUI reachable yet");
    }

    const auto* planned_room = find("bim.room");
    expect(planned_room != nullptr, "bim.room roadmap command resolves");
    if (planned_room) {
        expect(planned_room->state == FeatureState::Planned, "Room remains planned until implementation exists");
        expect(!can_expose_as_active_tool(*planned_room), "planned Room cannot become an enabled UI button");
    }

    expect(find("does.not.exist") == nullptr, "unknown command is rejected");

    if (failures == 0) {
        std::cout << "Command/icon catalog tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
