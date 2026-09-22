#pragma once

#include "acp/model3d.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace acp::blender {

struct Bundle {
    std::filesystem::path obj_path;
    std::filesystem::path script_path;
};

[[nodiscard]] std::string import_script(
    const std::filesystem::path& obj_path,
    const std::optional<std::filesystem::path>& save_blend_path = std::nullopt);

[[nodiscard]] std::optional<Bundle> write_bundle(
    const std::filesystem::path& directory,
    const model3d::Scene& scene,
    const std::optional<std::filesystem::path>& save_blend_path = std::nullopt);

[[nodiscard]] std::string command_line(
    const std::filesystem::path& blender_executable,
    const std::filesystem::path& script_path);

} // namespace acp::blender
