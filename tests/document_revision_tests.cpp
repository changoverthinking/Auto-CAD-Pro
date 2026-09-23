#include "acp/document.hpp"

#include <iostream>
#include <utility>

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
    acp::Document document;
    const auto initial = document.revision();
    expect(initial != 0, "new document has a non-zero revision");

    const auto line_id = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {1000.0, 0.0}}});
    const auto after_insert = document.revision();
    expect(line_id != 0, "line inserted");
    expect(after_insert != initial, "insert changes revision");

    const acp::Document& read_only = document;
    (void)read_only.find(line_id);
    expect(document.revision() == after_insert, "const entity lookup does not invalidate revision");

    auto* line = document.find(line_id);
    const auto after_mutable_access = document.revision();
    expect(line != nullptr, "mutable entity lookup succeeds");
    expect(after_mutable_access != after_insert, "mutable entity access invalidates revision");
    if (line != nullptr) {
        auto* entity = std::get_if<acp::LineEntity>(line);
        expect(entity != nullptr, "mutable entity retains line type");
        if (entity != nullptr) {
            entity->segment.b.x = 2000.0;
        }
    }

    acp::Document snapshot = document;
    expect(snapshot.revision() != document.revision(), "copy snapshot gets a distinct revision");

    const auto before_erase = document.revision();
    expect(document.erase(line_id), "line erased");
    expect(document.revision() != before_erase, "erase changes revision");

    const auto revision_before_restore = document.revision();
    document = snapshot;
    expect(document.revision() != revision_before_restore, "copy assignment replacement changes revision");
    expect(document.revision() != snapshot.revision(), "restored document does not reuse snapshot revision");
    expect(document.find(line_id) != nullptr, "snapshot content restored");

    acp::Document moved = std::move(snapshot);
    expect(moved.revision() != document.revision(), "move construction gets a distinct revision");

    const auto before_layer = document.revision();
    const auto layer_id = document.create_layer("Walls");
    expect(layer_id != 0, "layer created");
    expect(document.revision() != before_layer, "layer creation changes revision");

    const auto before_visibility = document.revision();
    expect(document.set_layer_visible(layer_id, false), "layer visibility changed");
    expect(document.revision() != before_visibility, "layer visibility changes revision");

    const auto before_noop_visibility = document.revision();
    expect(document.set_layer_visible(layer_id, false), "reapplying layer visibility succeeds");
    expect(document.revision() == before_noop_visibility, "no-op setter preserves revision");

    if (failures == 0) {
        std::cout << "Document revision tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
