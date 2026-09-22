#include "acp/ui_layout.hpp"

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

void verify(int width, int height) {
    const auto m = acp::ui_layout::metrics_for_client(width, height);

    expect(m.toolbar_height >= 1, "toolbar positive");
    expect(m.right_panel_width >= 1, "right panel positive");
    expect(m.left_rail_width >= 1, "left rail positive");

    expect(m.toolbar_height * 20 <= height,
           "toolbar does not exceed 1/20 height");
    expect(m.right_panel_width * 10 <= width * 2,
           "right panel does not exceed 2/10 width");
    expect(m.left_rail_width * 25 <= width,
           "left rail does not exceed 1/25 width");
}
}

int main() {
    verify(640, 480);
    verify(1024, 768);
    verify(1366, 768);
    verify(1920, 1080);
    verify(2560, 1440);
    verify(3840, 2160);

    expect(acp::ui_layout::compact_right_panel(128),
           "128px right panel uses compact layout");
    expect(!acp::ui_layout::compact_right_panel(204),
           "204px right panel uses full layout");
    expect(acp::ui_layout::property_key_width(128) == 57,
           "compact property key width remains readable");
    expect(acp::ui_layout::property_key_width(384) == 100,
           "wide property key width is capped");

    const auto full_hd = acp::ui_layout::metrics_for_client(1920, 1080);
    expect(full_hd.toolbar_height == 54, "1080p toolbar exact 1/20");
    expect(full_hd.right_panel_width == 384, "1080p right panel exact 2/10");
    expect(full_hd.left_rail_width == 76, "1080p left rail floor 1/25");

    if (failures != 0) {
        std::cerr << failures << " UI layout metric test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All UI layout metric tests passed\n";
    return EXIT_SUCCESS;
}
