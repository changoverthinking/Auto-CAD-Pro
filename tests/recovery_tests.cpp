#include "acp/recovery.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

namespace {
int failures = 0;

void expect(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}
}

int main() {
    using namespace acp;

    std::error_code ec;
    const auto root =
        std::filesystem::temp_directory_path(ec) /
        "autocadpro-recovery-test";
    expect(!ec, "temp directory available");

    std::filesystem::remove_all(root, ec);
    ec.clear();
    std::filesystem::create_directories(root, ec);
    expect(!ec, "create recovery test directory");

    const auto snapshot = root / "recovery.acp";

    Document original;
    BlockLibrary blocks;
    const EntityId lineId =
        original.insert(LineEntity{{{1.0, 2.0}, {30.0, 40.0}}});
    const EntityId textId =
        original.insert(TextEntity{{5.0, 6.0}, "RECOVERY", 4.0, 0.25});

    persistence::ProjectSettings recoverySettings;
    recoverySettings.page_setup.paper = layout::PaperSize::A2;
    recoverySettings.page_setup.orientation = layout::Orientation::Portrait;
    recoverySettings.page_setup.margins = {11.0, 12.0, 13.0, 14.0};
    recoverySettings.print_scale_denominator = 200.0;

    expect(recovery::write_snapshot(
               snapshot, original, blocks, recoverySettings),
           "write first recovery snapshot");
    expect(std::filesystem::exists(snapshot),
           "recovery snapshot exists");
    expect(!std::filesystem::exists(
               std::filesystem::path(snapshot.wstring() + L".tmp")),
           "temporary recovery file cleaned");

    const auto loaded = recovery::load_snapshot(snapshot);
    expect(loaded.has_value(), "load recovery snapshot");
    if (loaded.has_value()) {
        expect(loaded->document.size() == original.size(),
               "recovery entity count");
        expect(loaded->document.find(lineId) != nullptr,
               "recovery line id");
        expect(loaded->document.find(textId) != nullptr,
               "recovery text id");

        const auto& recoveredLine =
            std::get<LineEntity>(*loaded->document.find(lineId)).segment;
        expect(geo::nearly_equal(recoveredLine.a, {1.0, 2.0}) &&
               geo::nearly_equal(recoveredLine.b, {30.0, 40.0}),
               "recovery line geometry");

        const auto& recoveredText =
            std::get<TextEntity>(*loaded->document.find(textId));
        expect(recoveredText.text == "RECOVERY" &&
               recoveredText.height == 4.0,
               "recovery text geometry");
        expect(loaded->settings.page_setup.paper == layout::PaperSize::A2 &&
               loaded->settings.page_setup.orientation ==
                   layout::Orientation::Portrait,
               "recovery restores page paper and orientation");
        expect(loaded->settings.print_scale_denominator.has_value() &&
               geo::nearly_equal(
                   *loaded->settings.print_scale_denominator, 200.0),
               "recovery restores print scale");
    }

    // Replacing an existing recovery snapshot must leave a complete,
    // parseable file rather than a partially written document.
    Document replacement;
    const EntityId circleId =
        replacement.insert(CircleEntity{{{100.0, 100.0}, 25.0}});
    expect(recovery::write_snapshot(snapshot, replacement, blocks),
           "replace recovery snapshot");

    const auto replaced = recovery::load_snapshot(snapshot);
    expect(replaced.has_value(), "load replaced recovery snapshot");
    if (replaced.has_value()) {
        expect(replaced->document.size() == 1,
               "replaced recovery entity count");
        expect(replaced->document.find(circleId) != nullptr,
               "replaced recovery circle");
    }

    // Corruption must fail explicitly.
    {
        std::ofstream corrupt(snapshot, std::ios::binary | std::ios::trunc);
        corrupt << "BROKEN RECOVERY DATA";
    }
    expect(!recovery::load_snapshot(snapshot).has_value(),
           "reject corrupt recovery snapshot");

    expect(recovery::remove_snapshot(snapshot),
           "remove recovery snapshot");
    expect(!std::filesystem::exists(snapshot),
           "recovery snapshot removed");
    expect(recovery::remove_snapshot(snapshot),
           "removing missing snapshot is harmless");

    std::filesystem::remove_all(root, ec);

    if (failures != 0) {
        std::cerr << failures << " recovery regression test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All recovery regression tests passed\n";
    return EXIT_SUCCESS;
}
