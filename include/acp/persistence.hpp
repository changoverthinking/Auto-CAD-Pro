#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"
#include "acp/layout.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace acp::persistence {

struct ProjectSettings {
    layout::PageSetup page_setup{};
    std::optional<double> print_scale_denominator;
};

struct ProjectData {
    Document document;
    BlockLibrary blocks;
    ProjectSettings settings{};
};

[[nodiscard]] std::string serialize_project(
    const Document& document,
    const BlockLibrary& blocks,
    const ProjectSettings& settings = {});

[[nodiscard]] std::optional<ProjectData> deserialize_project(
    std::string_view data);

[[nodiscard]] bool save_project_atomic(
    const std::filesystem::path& path,
    const Document& document,
    const BlockLibrary& blocks,
    const ProjectSettings& settings = {});

} // namespace acp::persistence
