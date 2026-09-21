#include "acp/recovery.hpp"
#include "acp/persistence.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace acp::recovery {
namespace {

bool write_bytes(const std::filesystem::path& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    return out.good();
}

bool replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {

#ifdef _WIN32
    return MoveFileExW(
        source.c_str(),
        destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(source, destination, ec);
    if (!ec) return true;

    std::filesystem::remove(destination, ec);
    ec.clear();
    std::filesystem::rename(source, destination, ec);
    return !ec;
#endif
}

} // namespace

bool write_snapshot(
    const std::filesystem::path& path,
    const Document& document,
    const BlockLibrary& blocks) {

    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    const auto temp = path.wstring() + L".tmp";
    const std::filesystem::path temp_path{temp};

    const std::string data =
        persistence::serialize_project(document, blocks);

    if (!write_bytes(temp_path, data)) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }

    if (!replace_file(temp_path, path)) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }
    return true;
}

std::optional<ProjectData> load_snapshot(
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
