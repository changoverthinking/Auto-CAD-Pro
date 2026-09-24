#include "acp/icon_catalog.hpp"

#include <iostream>
#include <set>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace

int main() {
    using namespace acp::icons;

    const auto all = ids();
    expect(all.size() == kIconCount, "compiled icon catalog exposes all 553 uploaded icons");

    std::set<std::string_view> unique;
    for (std::size_t i = 0; i < all.size(); ++i) {
        expect(unique.insert(all[i]).second, "compiled icon IDs are unique");
        const auto index = index_of(all[i]);
        expect(index.has_value(), "every compiled icon ID resolves to an atlas index");
        if (index.has_value()) {
            expect(*index == i, "icon index remains stable");
            const auto cell = cell_for(*index);
            expect(cell.width == kAtlasIconSize && cell.height == kAtlasIconSize,
                "every atlas cell is 32x32");
            expect(cell.x >= 0 && cell.y >= 0, "atlas cell origin is valid");
            expect(cell.x + cell.width <= kAtlasWidth, "atlas cell fits atlas width");
            expect(cell.y + cell.height <= kAtlasHeight, "atlas cell fits atlas height");
        }
    }

    expect(index_of("pack-a.new-drawing") == std::optional<std::size_t>{7},
        "known File icon has stable atlas location");
    expect(index_of("pack-c.line").has_value(), "Line icon resolves");
    expect(index_of("pack-m.wall").has_value(), "Wall icon resolves");
    expect(index_of("missing.icon") == std::nullopt, "unknown icon ID is rejected");

    const auto last = cell_for(kIconCount - 1);
    expect(last.x + last.width <= kAtlasWidth && last.y + last.height <= kAtlasHeight,
        "last uploaded icon fits within atlas bounds");

    if (failures == 0) {
        std::cout << "Full icon atlas catalog tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
