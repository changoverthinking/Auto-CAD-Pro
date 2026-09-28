#include "acp/coordinate_input.hpp"
#include "acp/history.hpp"
#include "acp/persistence.hpp"
#include <iostream>
#include <limits>
#include <memory>

namespace {
int failures=0;
void expect(bool value,const char* label) {
    if (!value) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; }
}
void point(std::string_view text,acp::geo::Vec2 expected,
           std::optional<acp::geo::Vec2> reference=std::nullopt) {
    const auto p=acp::input::parse_coordinate(text,reference);
    expect(p && acp::geo::nearly_equal(*p,expected,1e-8),"coordinate geometry");
}
}
int main() {
    using namespace acp;
    point("100.125,-200.0625",{100.125,-200.0625});
    point("  +1.25e2 , -.5 \t",{125,-0.5});
    point("@10.5,-20.25",{110.5,179.75},geo::Vec2{100,200});
    point("@5<90",{10,25},geo::Vec2{10,20});
    point("@5<-90",{10,15},geo::Vec2{10,20});
    point("@5<450",{10,25},geo::Vec2{10,20});
    point("@0<0",{10,20},geo::Vec2{10,20});
    point("3,4",{3,4},geo::Vec2{100,200});
    for(const auto bad:{"","1","1,2,3","1,,2","nan,2","inf,2","1e999,0",
        "1mm,2","1,2oops","+-1,2","++1,2","@-2<30","2<30","@1,2<3","@1<2<3"})
        expect(!input::parse_coordinate(bad,geo::Vec2{0,0}),"malformed input rejected");
    expect(!input::parse_coordinate("@1,2"),"relative coordinates require anchor");
    expect(!input::parse_coordinate("@1<30"),"relative polar requires anchor");
    const double huge=std::numeric_limits<double>::max();
    expect(!input::parse_coordinate("@1e308,0",geo::Vec2{huge,0}),"overflow rejected");
    expect(!input::parse_coordinate("@1,0",geo::Vec2{std::numeric_limits<double>::infinity(),0}),"invalid anchor rejected");
    expect(!input::parse_coordinate(std::string(257,'1')),"oversize input rejected");

    const auto a=input::parse_coordinate("100.125,200.0625");
    const auto b=input::parse_coordinate("@300.25,-40.125",a);
    expect(a && b,"precise line coordinates parse");
    if(a && b) {
        Document doc; BlockLibrary blocks; History history;
        expect(history.apply(doc,std::make_unique<AddEntityCommand>(LineEntity{{*a,*b}})),"precise line enters history");
        const auto saved=persistence::serialize_project(doc,blocks);
        const auto loaded=persistence::deserialize_project(saved);
        expect(loaded.has_value(),"precise coordinates save/open");
        if(loaded) {
            const auto& line=std::get<LineEntity>(*loaded->document.find(loaded->document.ids().front()));
            expect(line.segment.a.x==100.125 && line.segment.a.y==200.0625 &&
                line.segment.b.x==400.375 && line.segment.b.y==159.9375,"fractional coordinates retained without pixel rounding");
        }
        expect(history.undo(doc) && doc.size()==0,"precise line undo");
        expect(history.redo(doc) && persistence::serialize_project(doc,blocks)==saved,"precise line redo");
    }
    if(!failures) std::cout<<"Coordinate input tests passed\n";
    return failures?1:0;
}
