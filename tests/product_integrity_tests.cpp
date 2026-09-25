#include "acp/block.hpp"
#include "acp/edit2d.hpp"
#include "acp/history.hpp"
#include "acp/id_allocation.hpp"
#include "acp/persistence.hpp"
#include "acp/project_model.hpp"
#include "acp/transform.hpp"

#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace {
int failures = 0;
void expect(bool value, const char* name) {
    if (!value) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
bool same(const acp::BlockPrimitive& a, const acp::BlockPrimitive& b) {
    if (a.index() != b.index()) return false;
    return std::visit([&](const auto& x) {
        using T = std::decay_t<decltype(x)>;
        const auto& y = std::get<T>(b);
        using acp::geo::nearly_equal;
        if constexpr (std::is_same_v<T, acp::LineEntity>) {
            return nearly_equal(x.segment.a,y.segment.a,1e-8) && nearly_equal(x.segment.b,y.segment.b,1e-8);
        } else if constexpr (std::is_same_v<T, acp::CircleEntity>) {
            return nearly_equal(x.circle.center,y.circle.center,1e-8) && nearly_equal(x.circle.radius,y.circle.radius,1e-8);
        } else if constexpr (std::is_same_v<T, acp::ArcEntity>) {
            return nearly_equal(x.arc.center,y.arc.center,1e-8) &&
                nearly_equal(acp::geo::arc_start_point(x.arc),acp::geo::arc_start_point(y.arc),1e-8) &&
                nearly_equal(acp::geo::arc_end_point(x.arc),acp::geo::arc_end_point(y.arc),1e-8) &&
                nearly_equal(acp::geo::arc_sweep(x.arc),acp::geo::arc_sweep(y.arc),1e-8) &&
                x.arc.counter_clockwise == y.arc.counter_clockwise;
        } else {
            if (x.points.size()!=y.points.size() || x.closed!=y.closed) return false;
            for (std::size_t i=0;i<x.points.size();++i)
                if (!nearly_equal(x.points[i],y.points[i],1e-8)) return false;
            return true;
        }
    },a);
}
}

int main() {
    using namespace acp;
    const auto outward = edit2d::offset_circle({{3,4},10},5);
    const auto inward = edit2d::offset_circle({{3,4},10},-4);
    expect(outward && geo::nearly_equal(outward->center,{3,4}) && outward->radius==15,"outward concentric circle offset");
    expect(inward && inward->radius==6,"inward circle offset");
    expect(!edit2d::offset_circle({{0,0},10},-10),"reject collapsed circle offset");
    expect(!edit2d::offset_circle({{0,0},10},-11),"reject inverted circle offset");
    expect(!edit2d::offset_circle({{0,0},10},std::numeric_limits<double>::infinity()),"reject nonfinite offset");
    expect(!edit2d::offset_through_point(CircleEntity{{{0,0},10}},
        {std::numeric_limits<double>::quiet_NaN(),0}),"reject NaN through point");
    const geo::Arc source_arc{{3,4},10,0.2,2.8,false};
    const auto arc_offset=edit2d::offset_arc(source_arc,3);
    expect(arc_offset && arc_offset->radius==13 && arc_offset->start_angle==0.2 &&
        arc_offset->end_angle==2.8 && !arc_offset->counter_clockwise,"arc offset preserves sweep and direction");
    expect(!edit2d::offset_through_point(CircleEntity{{{0,0},10}},{0,0}),"through center is rejected");
    expect(!edit2d::offset_through_point(TextEntity{}, {1,1}),"unsupported offset is explicit");
    for (const Entity source : {Entity{CircleEntity{{{3,4},10}}}, Entity{ArcEntity{source_arc}}}) {
        const auto offset=edit2d::offset_through_point(source,{3,24});
        expect(offset.has_value(),"GUI through-point mode supports circle and arc");
        if (!offset) continue;
        Document doc; BlockLibrary empty; History history;
        (void)doc.insert(source);
        expect(history.apply(doc,std::make_unique<AddEntityCommand>(*offset)),"offset history apply");
        const auto saved=persistence::serialize_project(doc,empty);
        expect(persistence::deserialize_project(saved).has_value(),"offset save/open");
        expect(history.undo(doc) && doc.size()==1,"offset undo removes only new geometry");
        expect(history.redo(doc) && persistence::serialize_project(doc,empty)==saved,"offset redo preserves geometry and id");
    }
    BlockLibrary blocks;
    const auto block = blocks.create("Asymmetric",{2,3},{
        LineEntity{{{2,3},{8,11}}}, CircleEntity{{{4,9},2}},
        ArcEntity{{{3,7},2,0.2,1.7,true}}, PolylineEntity{{{2,3},{8,3},{4,9}},true}});
    for (const geo::Segment axis : {geo::Segment{{0,0},{10,0}},geo::Segment{{1,2},{1,12}},geo::Segment{{-3,4},{8,9}}}) {
        const Entity original=BlockReferenceEntity{block,{17,29},0.37,2.5};
        Entity mirrored=original;
        const auto before=blocks.instantiate(std::get<BlockReferenceEntity>(original));
        expect(transform::mirror(mirrored,axis),"mirror block succeeds");
        const auto after=blocks.instantiate(std::get<BlockReferenceEntity>(mirrored));
        expect(after.size()==before.size(),"mirror preserves primitive count");
        for (std::size_t i=0;i<before.size() && i<after.size();++i) {
            Entity expected=std::visit([](const auto& p)->Entity{return p;},before[i]);
            expect(transform::mirror(expected,axis),"primitive mirror succeeds");
            const auto expected_primitive=std::visit([](const auto& p)->BlockPrimitive{
                using T=std::decay_t<decltype(p)>;
                if constexpr(std::is_same_v<T,LineEntity> || std::is_same_v<T,CircleEntity> ||
                             std::is_same_v<T,ArcEntity> || std::is_same_v<T,PolylineEntity>) return p;
                else return LineEntity{};
            },expected);
            expect(same(after[i],expected_primitive),"block mirror equals geometric reflection of each primitive");
        }
        Document doc; History history;
        const auto id=doc.insert(original);
        expect(history.apply(doc,std::make_unique<UpdateEntityCommand>(id,mirrored)),"mirror history apply");
        const auto saved=persistence::serialize_project(doc,blocks);
        const auto loaded=persistence::deserialize_project(saved);
        expect(loaded.has_value(),"mirrored block save/open");
        if (loaded) {
            const auto restored=loaded->blocks.instantiate(std::get<BlockReferenceEntity>(*loaded->document.find(id)));
            for(std::size_t i=0;i<after.size() && i<restored.size();++i)
                expect(same(after[i],restored[i]),"save/open preserves mirrored geometry");
        }
        expect(history.undo(doc),"mirror undo");
        expect(history.redo(doc),"mirror redo");
        expect(persistence::serialize_project(doc,blocks)==saved,"redo restores exact mirrored project");
        expect(transform::mirror(mirrored,axis),"second mirror succeeds");
        const auto twice=blocks.instantiate(std::get<BlockReferenceEntity>(mirrored));
        for(std::size_t i=0;i<before.size() && i<twice.size();++i)
            expect(same(before[i],twice[i]),"two reflections restore original geometry");
    }
    Document ids;
    expect(ids.insert_with_id(std::numeric_limits<EntityId>::max(),LineEntity{}),"import max entity id");
    expect(ids.insert(LineEntity{})!=0 && ids.find(0)==nullptr,"entity allocation never uses reserved zero");
    expect(ids.insert_layer_with_id(Layer{std::numeric_limits<LayerId>::max(),"Max"}),"import max layer id");
    expect(ids.create_layer("Next")!=0 && ids.layer(0)==nullptr,"layer allocation never uses reserved zero");
    BlockLibrary block_ids;
    expect(block_ids.insert_with_id({std::numeric_limits<BlockId>::max(),"Max",{}, {LineEntity{}}}),"import max block id");
    expect(block_ids.create("Next",{}, {LineEntity{}})!=0 && block_ids.find(0)==nullptr,"block allocation never uses reserved zero");
    std::unordered_map<unsigned char,int> full;
    for (int i=1;i<256;++i) full.emplace(static_cast<unsigned char>(i),i);
    unsigned char next=255;
    expect(detail::allocate_id(full,next)==0 && full.size()==255,"exhausted allocator fails without corrupting entries");
    full.erase(128);
    expect(detail::allocate_id(full,next)==128,"wrapped allocator finds free nonzero slot");

    bim::ProjectModel project;
    const auto a=project.add_element({}); const auto b=project.add_element({}); const auto c=project.add_element({});
    expect(project.set_host(a,b) && project.set_host(b,c),"valid host chain");
    expect(!project.set_host(c,a),"reject indirect cyclic host chain");
    expect(!project.host_of(c).has_value(),"rejected cycle leaves relationships unchanged");
    expect(!project.set_host(b,a),"reject two-element host cycle");
    expect(project.host_of(b)==c,"failed rehost preserves prior host");
    if(!failures) std::cout<<"Product integrity regressions passed\n";
    return failures ? 1 : 0;
}
