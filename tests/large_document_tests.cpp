#include "acp/bounds.hpp"
#include "acp/document.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/snap.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

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
    using clock = std::chrono::steady_clock;

    constexpr int kLineCount = 4096;
    constexpr int kCircleCount = 1024;
    constexpr std::size_t kExpectedCount =
        static_cast<std::size_t>(kLineCount + kCircleCount);

    const auto start = clock::now();

    Document document;
    BlockLibrary blocks;

    EntityId targetLineId = 0;
    for (int i = 0; i < kLineCount; ++i) {
        const double y = static_cast<double>(i);
        const EntityId id = document.insert(
            LineEntity{{{0.0, y}, {1000.0, y}}});
        if (i == 2048) {
            targetLineId = id;
        }
    }

    for (int i = 0; i < kCircleCount; ++i) {
        const double x = 1200.0 + static_cast<double>(i % 128) * 20.0;
        const double y = static_cast<double>(i / 128) * 20.0;
        (void)document.insert(CircleEntity{{{x, y}, 5.0}});
    }

    expect(document.size() == kExpectedCount,
           "large document entity count");

    const auto drawing = bounds::drawing_bounds(document, &blocks);
    expect(drawing.has_value(), "large document bounds");
    if (drawing.has_value()) {
        expect(drawing->min.x <= 0.0 &&
               drawing->max.x >= 1200.0,
               "large document bounds range");
    }

    const auto hit = selection::hit_test(
        document,
        {500.0, 2048.1},
        0.25);
    expect(hit.has_value() && hit->id == targetLineId,
           "large document selection");

    const auto snapCandidate = snap::best_for_document(
        document,
        &blocks,
        {0.05, 2048.05},
        0.20,
        false);
    expect(snapCandidate.has_value() &&
           snapCandidate->kind == snap::Kind::Endpoint,
           "large document endpoint snap");

    const std::string serialized =
        persistence::serialize_project(document, blocks);
    expect(!serialized.empty(),
           "large document serialization");

    const auto loaded =
        persistence::deserialize_project(serialized);
    expect(loaded.has_value(),
           "large document deserialization");
    if (loaded.has_value()) {
        expect(loaded->document.size() == kExpectedCount,
               "large document roundtrip count");

        const auto reloadedHit = selection::hit_test(
            loaded->document,
            {500.0, 2048.1},
            0.25);
        expect(reloadedHit.has_value() &&
               reloadedHit->id == targetLineId,
               "large document roundtrip selection");
    }

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - start);

    // This is deliberately generous: the gate is intended to catch
    // catastrophic O(n^2)-style regressions, not micro-benchmark noise.
    expect(elapsed < std::chrono::seconds(10),
           "large document workflow stays within stability budget");

    if (failures != 0) {
        std::cerr << failures
                  << " large-document regression test(s) failed in "
                  << elapsed.count() << " ms\n";
        return EXIT_FAILURE;
    }

    std::cout << "Large-document workflow passed in "
              << elapsed.count() << " ms\n";
    return EXIT_SUCCESS;
}
