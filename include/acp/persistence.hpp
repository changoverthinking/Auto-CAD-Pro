#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace acp::persistence {

struct ProjectData {
    Document document;
    BlockLibrary blocks;
};

[[nodiscard]] std::string serialize_project(
    const Document& document,
    const BlockLibrary& blocks);

[[nodiscard]] std::optional<ProjectData> deserialize_project(
    std::string_view data);

} // namespace acp::persistence
