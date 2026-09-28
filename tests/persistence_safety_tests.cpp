#include "acp/persistence.hpp"
#include "acp/recovery.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void rejects(const std::string& record, const char* message) {
    try {
        expect(!acp::persistence::deserialize_project(
            "ACP2D 1\nL 1 \"0\" 1 0 0.25\n" + record + "\nEND\n"), message);
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: " << message << " threw " << error.what() << '\n';
    }
}
}

int main() {
    using namespace acp;
    const auto huge = std::to_string(std::numeric_limits<std::size_t>::max());
    rejects("E 1 1 1 0 0 POLY 0 " + huge, "oversized polyline count is rejected without throwing");
    rejects("E 1 1 1 0 0 HATCH \"ANSI31\" 0 1 0 " + huge, "oversized hatch count is rejected without throwing");
    rejects("B 1 \"block\" 0 0 " + huge, "oversized block count is rejected without throwing");
    rejects("B 1 \"block\" 0 0 1\nP POLY 0 " + huge, "oversized block polyline count is rejected without throwing");
    rejects("E 1 1 1 0 0 POLY 0 -1", "negative point count is rejected without throwing");
    rejects("END\nE 1 1 1 0 0 LINE 0 0 1 1", "records after END are not silently lost");
    expect(persistence::deserialize_project("ACP2D 1\nL 1 \"0\" 1 0 0.25\nEND\n \t\r\n").has_value(),
           "trailing whitespace remains compatible");

    const auto root = std::filesystem::temp_directory_path() / "acp-persistence-safety-test";
    std::filesystem::remove_all(root);
    const auto target = root / "nested" / "recovery.acp";
    Document document;
    BlockLibrary blocks;
    (void)document.insert(LineEntity{{{1, 2}, {3, 4}}});
    expect(recovery::write_snapshot(target, document, blocks), "snapshot creates parent directories");
    const auto original = bytes(target);
    Document invalid;
    (void)invalid.insert(BlockReferenceEntity{999, {0, 0}, 0, 1});
    expect(!recovery::write_snapshot(target, invalid, blocks), "invalid snapshot is refused");
    expect(bytes(target) == original, "invalid autosave preserves last recoverable bytes");
    expect(recovery::load_snapshot(target).has_value(), "last valid autosave still opens");
    persistence::ProjectSettings invalid_settings;
    invalid_settings.print_scale_denominator = -1;
    expect(!recovery::write_snapshot(target, document, blocks, invalid_settings), "invalid settings are refused");
    expect(bytes(target) == original, "invalid settings preserve previous snapshot");

    // A failed replacement must not delete an existing destination directory.
    const auto directory_target = root / "existing-directory";
    std::filesystem::create_directory(directory_target);
    expect(!persistence::save_project_atomic(directory_target, document, blocks), "save rejects directory destination");
    expect(std::filesystem::is_directory(directory_target), "failed save preserves destination directory");
    const auto recovery_directory = root / "recovery-directory";
    std::filesystem::create_directory(recovery_directory);
    expect(!recovery::write_snapshot(recovery_directory, document, blocks), "autosave rejects directory destination");
    expect(std::filesystem::is_directory(recovery_directory), "failed autosave preserves destination directory");
    expect(!std::filesystem::exists(target.string() + ".tmp"), "no stale snapshot temporary file");
    std::filesystem::remove_all(root);
    if (failures == 0) std::cout << "Persistence safety tests passed\n";
    return failures == 0 ? 0 : 1;
}
