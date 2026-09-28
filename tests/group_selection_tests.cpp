#include "acp/history.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void expect(bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::string snapshot(const acp::Document& document) {
    return acp::persistence::serialize_project(document, {});
}
}

int main() {
    using namespace acp;
    Document document;
    const auto a = document.insert(LineEntity{{{0,0},{10,0}}});
    const auto b = document.insert(CircleEntity{{{20,30},5}});
    const auto layer = document.create_layer("Detail");
    document.set_entity_layer(b, layer);
    document.properties(b)->color_override = RgbColor{12,34,56};
    document.properties(b)->line_type_override = LineType::Dashed;
    document.properties(b)->line_weight_override = 0.7;
    const auto baseline = snapshot(document);

    selection::Set selected;
    selected.toggle(a); selected.toggle(b); selected.toggle(a);
    expect(selected.size() == 1 && *selected == b, "Ctrl toggle removes only hit item");
    selected = a;
    expect(selected.size() == 1 && *selected == a, "ordinary selection replaces set");
    selected.select_all_editable(document);
    expect(selected.size() == 2, "select all includes both writable layers");
    document.set_layer_locked(layer, true);
    selected.select_all_editable(document);
    expect(selected.size() == 1 && *selected == a, "select all excludes locked layer");
    document.set_layer_locked(layer, false);
    document.set_layer_visible(layer, false);
    selected.select_all_editable(document);
    expect(selected.size() == 1, "select all excludes hidden layer");
    document.set_layer_visible(layer, true);
    selected.select_all_editable(document);

    History history;
    std::vector<std::unique_ptr<Command>> updates;
    for (const auto id : selected.ids()) {
        Entity next = *document.find(id);
        transform::translate(next, {7,-3});
        updates.push_back(std::make_unique<UpdateEntityCommand>(id, next));
    }
    expect(history.apply(document, std::make_unique<EntityBatchCommand>(selected.ids(), std::move(updates))), "move group");
    expect(history.undo_size() == 1, "group consumes one undo entry");
    expect(geo::nearly_equal(std::get<LineEntity>(*document.find(a)).segment.a, {7,-3}), "line moved exactly");
    expect(geo::nearly_equal(std::get<CircleEntity>(*document.find(b)).circle.center, {27,27}), "circle moved exactly");
    const auto moved = snapshot(document);
    expect(history.undo(document) && snapshot(document) == baseline, "single undo restores entire group and styles");
    expect(history.redo(document) && snapshot(document) == moved, "single redo replays entire group");
    expect(history.undo(document), "return to baseline");

    // The second child fails after the first has already modified its staged copy.
    std::vector<std::unique_ptr<Command>> bad;
    bad.push_back(std::make_unique<RemoveEntityCommand>(a));
    bad.push_back(std::make_unique<RemoveEntityCommand>(99999));
    const auto revision = document.revision();
    expect(!history.apply(document, std::make_unique<EntityBatchCommand>(selected.ids(), std::move(bad))), "failing second child rejected");
    expect(snapshot(document) == baseline && document.revision() == revision, "failure preserves bytes and revision");
    expect(history.redo_size() == 1 && history.undo_size() == 0, "failure preserves history including redo");

    document.set_layer_locked(layer, true);
    std::vector<std::unique_ptr<Command>> removals;
    removals.push_back(std::make_unique<RemoveEntityCommand>(a));
    removals.push_back(std::make_unique<RemoveEntityCommand>(b));
    const auto locked = snapshot(document);
    expect(!history.apply(document, std::make_unique<EntityBatchCommand>(selected.ids(), std::move(removals))), "mixed locked group rejected");
    expect(snapshot(document) == locked, "locked group leaves writable member intact");
    document.set_layer_locked(layer, false);

    std::vector<std::unique_ptr<Command>> copies;
    for (const auto id : selected.ids()) {
        const auto* props = document.properties(id);
        copies.push_back(std::make_unique<AddEntityCommand>(
            transform::translated_copy(*document.find(id), {100,0}), *props));
    }
    expect(history.apply(document, std::make_unique<EntityBatchCommand>(selected.ids(), std::move(copies))), "copy group");
    expect(document.size() == 4, "copy inserts every member");
    const auto ids = document.ids();
    const auto* copied = document.properties(ids.back());
    expect(copied->layer_id == layer && copied->color_override == RgbColor{12,34,56} &&
           copied->line_type_override == LineType::Dashed && copied->line_weight_override == 0.7,
           "copy preserves independent layer and style");
    const auto duplicated = snapshot(document);
    expect(history.undo(document) && snapshot(document) == baseline, "undo copy removes all new members");
    expect(history.redo(document) && snapshot(document) == duplicated, "redo copy keeps IDs and properties");
    const auto loaded = persistence::deserialize_project(duplicated);
    expect(loaded && snapshot(loaded->document) == duplicated, "group result round trips project persistence");

    selected.select_all_editable(document);
    std::vector<std::unique_ptr<Command>> deletes;
    for (const auto id : selected.ids()) deletes.push_back(std::make_unique<RemoveEntityCommand>(id));
    expect(history.apply(document, std::make_unique<EntityBatchCommand>(selected.ids(), std::move(deletes))), "delete entire selection");
    selected.prune(document);
    expect(document.size() == 0 && !selected.has_value(), "prune all deleted IDs");
    expect(history.undo(document) && snapshot(document) == duplicated, "undo deletion restores styles and IDs");
    expect(history.redo(document) && document.size() == 0, "redo deletes restored group");

    Entity huge = LineEntity{{{1e308,0},{1e308,1}}};
    transform::translate(huge, {1e308,0});
    expect(!transform::has_finite_geometry(huge), "overflowed transform rejected before commit");
    Entity invalid = TextEntity{{0,0},"text",2.5,std::numeric_limits<double>::quiet_NaN()};
    expect(!transform::has_finite_geometry(invalid), "nonfinite scalar detected");
    return failures == 0 ? 0 : 1;
}
