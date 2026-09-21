#include "acp/document.hpp"
#include "acp/edit2d.hpp"
#include "acp/history.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
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

    Document document;
    BlockLibrary blocks;
    History history;

    auto addBase = std::make_unique<AddEntityCommand>(
        LineEntity{{{0.0, 0.0}, {10.0, 0.0}}});
    auto* addBasePtr = addBase.get();
    expect(history.apply(document, std::move(addBase)), "base line create");
    const EntityId baseId = addBasePtr->id();

    // MOVE
    Entity moved = *document.find(baseId);
    transform::translate(moved, {5.0, 5.0});
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(baseId, moved)),
           "move apply");
    expect(history.undo(document), "move undo");
    expect(history.redo(document), "move redo");

    // COPY
    const Entity copiedEntity =
        transform::translated_copy(*document.find(baseId), {20.0, 0.0});
    auto addCopy = std::make_unique<AddEntityCommand>(copiedEntity);
    auto* addCopyPtr = addCopy.get();
    expect(history.apply(document, std::move(addCopy)), "copy apply");
    const EntityId copyId = addCopyPtr->id();
    expect(copyId != baseId && document.find(copyId) != nullptr, "copy distinct id");
    expect(history.undo(document), "copy undo");
    expect(document.find(copyId) == nullptr, "copy undo removes copy");
    expect(history.redo(document), "copy redo");
    expect(document.find(copyId) != nullptr, "copy redo restores copy");

    // ROTATE
    Entity rotated = *document.find(copyId);
    transform::rotate(rotated, {25.0, 5.0}, std::numbers::pi / 2.0);
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(copyId, rotated)),
           "rotate apply");
    expect(history.undo(document), "rotate undo");
    expect(history.redo(document), "rotate redo");

    // SCALE
    auto addCircle = std::make_unique<AddEntityCommand>(
        CircleEntity{{{60.0, 10.0}, 5.0}});
    auto* addCirclePtr = addCircle.get();
    expect(history.apply(document, std::move(addCircle)), "scale circle create");
    const EntityId circleId = addCirclePtr->id();
    Entity scaled = *document.find(circleId);
    expect(transform::scale_uniform(scaled, {60.0, 10.0}, 3.0),
           "scale transform accepted");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(circleId, scaled)),
           "scale apply");
    expect(geo::nearly_equal(
               std::get<CircleEntity>(*document.find(circleId)).circle.radius,
               15.0),
           "scale geometry");
    expect(history.undo(document), "scale undo");
    expect(history.redo(document), "scale redo");

    // MIRROR
    auto addMirror = std::make_unique<AddEntityCommand>(
        LineEntity{{{80.0, 2.0}, {90.0, 4.0}}});
    auto* addMirrorPtr = addMirror.get();
    expect(history.apply(document, std::move(addMirror)), "mirror line create");
    const EntityId mirrorId = addMirrorPtr->id();
    Entity mirrored = *document.find(mirrorId);
    expect(transform::mirror(mirrored, {{75.0, -10.0}, {75.0, 10.0}}),
           "mirror transform accepted");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(mirrorId, mirrored)),
           "mirror apply");
    {
        const auto& segment =
            std::get<LineEntity>(*document.find(mirrorId)).segment;
        expect(geo::nearly_equal(segment.a, {70.0, 2.0}) &&
               geo::nearly_equal(segment.b, {60.0, 4.0}),
               "mirror geometry");
    }
    expect(history.undo(document), "mirror undo");
    expect(history.redo(document), "mirror redo");

    // TRIM
    auto addTrim = std::make_unique<AddEntityCommand>(
        LineEntity{{{100.0, 0.0}, {120.0, 0.0}}});
    auto* addTrimPtr = addTrim.get();
    expect(history.apply(document, std::move(addTrim)), "trim line create");
    const EntityId trimId = addTrimPtr->id();
    Entity trimmedEntity = *document.find(trimId);
    auto& trimSegment = std::get<LineEntity>(trimmedEntity).segment;
    expect(edit2d::trim_segment(
               trimSegment,
               {{108.0, -10.0}, {108.0, 10.0}},
               {101.0, 0.0}),
           "trim core operation");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(trimId, trimmedEntity)),
           "trim apply");
    expect(geo::nearly_equal(
               std::get<LineEntity>(*document.find(trimId)).segment.a,
               {108.0, 0.0}),
           "trim geometry");
    expect(history.undo(document), "trim undo");
    expect(history.redo(document), "trim redo");

    // EXTEND
    auto addExtend = std::make_unique<AddEntityCommand>(
        LineEntity{{{130.0, 0.0}, {135.0, 0.0}}});
    auto* addExtendPtr = addExtend.get();
    expect(history.apply(document, std::move(addExtend)), "extend line create");
    const EntityId extendId = addExtendPtr->id();
    Entity extendedEntity = *document.find(extendId);
    auto& extendSegment = std::get<LineEntity>(extendedEntity).segment;
    expect(edit2d::extend_segment(
               extendSegment,
               {{140.0, -10.0}, {140.0, 10.0}}),
           "extend core operation");
    expect(history.apply(
               document,
               std::make_unique<UpdateEntityCommand>(extendId, extendedEntity)),
           "extend apply");
    expect(geo::nearly_equal(
               std::get<LineEntity>(*document.find(extendId)).segment.b,
               {140.0, 0.0}),
           "extend geometry");
    expect(history.undo(document), "extend undo");
    expect(history.redo(document), "extend redo");

    // OFFSET creates a new line and therefore must have independent history.
    const auto sourceSegment =
        std::get<LineEntity>(*document.find(extendId)).segment;
    const auto offsetSegment = edit2d::offset_segment(sourceSegment, 4.0);
    expect(offsetSegment.has_value(), "offset core operation");
    EntityId offsetId = 0;
    if (offsetSegment.has_value()) {
        auto addOffset = std::make_unique<AddEntityCommand>(
            LineEntity{*offsetSegment});
        auto* addOffsetPtr = addOffset.get();
        expect(history.apply(document, std::move(addOffset)), "offset apply");
        offsetId = addOffsetPtr->id();
        expect(offsetId != extendId && document.find(offsetId) != nullptr,
               "offset distinct entity");
        expect(history.undo(document), "offset undo");
        expect(document.find(offsetId) == nullptr, "offset undo removes result");
        expect(history.redo(document), "offset redo");
        expect(document.find(offsetId) != nullptr, "offset redo restores result");
    }

    // SELECT sanity after multiple editing operations.
    const auto trimHit = selection::hit_test(document, {115.0, 0.2}, 1.0);
    expect(trimHit.has_value() && trimHit->id == trimId,
           "edited trim line remains selectable");

    // SAVE/OPEN regression for edited geometry set.
    const std::string data = persistence::serialize_project(document, blocks);
    const auto loaded = persistence::deserialize_project(data);
    expect(loaded.has_value(), "edited workflow save/open");
    if (loaded.has_value()) {
        expect(loaded->document.size() == document.size(),
               "edited workflow entity count");
        expect(loaded->document.find(baseId) != nullptr,
               "move persisted");
        expect(loaded->document.find(copyId) != nullptr,
               "copy persisted");
        expect(loaded->document.find(circleId) != nullptr,
               "scale persisted");
        expect(loaded->document.find(mirrorId) != nullptr,
               "mirror persisted");
        expect(loaded->document.find(trimId) != nullptr,
               "trim persisted");
        expect(loaded->document.find(extendId) != nullptr,
               "extend persisted");
        if (offsetId != 0) {
            expect(loaded->document.find(offsetId) != nullptr,
                   "offset persisted");
        }
    }

    if (failures != 0) {
        std::cerr << failures << " edit workflow regression test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All edit-tool workflow regression tests passed\n";
    return EXIT_SUCCESS;
}
