#pragma once

#include "acp/model3d.hpp"

#include <filesystem>
#include <string>

namespace acp::obj {

[[nodiscard]] std::string serialize(const model3d::Scene& scene);

[[nodiscard]] bool save(
    const std::filesystem::path& path,
    const model3d::Scene& scene);

} // namespace acp::obj
