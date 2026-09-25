#include "acp/recovery.hpp"
#include "acp/persistence.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace acp::recovery {

bool write_snapshot(
    const std::filesystem::path& path,
    const Document& document,
    const BlockLibrary& blocks,
    const persistence::ProjectSettings& settings) {

    if (path.empty()) return false;

    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    // Autosave must use the same validation and read-back verification as Save.
    // Never replace the last recoverable snapshot with an unreadable project.
    return persistence::save_project_atomic(path, document, blocks, settings);
}

std::optional<persistence::ProjectData> load_snapshot(
    const std::filesystem::path& path) {

    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    const std::string data{
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()};

    if (!in.good() && !in.eof()) {
        return std::nullopt;
    }

    return persistence::deserialize_project(data);
}

bool remove_snapshot(
    const std::filesystem::path& path) noexcept {

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return !ec;
    }
    return std::filesystem::remove(path, ec) && !ec;
}

} // namespace acp::recovery
