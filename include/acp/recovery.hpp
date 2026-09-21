#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"

#include <filesystem>
#include <optional>

namespace acp::recovery {

[[nodiscard]] bool write_snapshot(
    const std::filesystem::path& path,
    const Document& document,
    const BlockLibrary& blocks);

[[nodiscard]] std::optional<ProjectData> load_snapshot(
    const std::filesystem::path& path);

[[nodiscard]] bool remove_snapshot(
    const std::filesystem::path& path) noexcept;

} // namespace acp::recovery
