#pragma once

#include "acp/geometry2d.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace acp::bim {

using ElementId = std::uint64_t;
using ElementTypeId = std::uint64_t;
using LevelId = std::uint64_t;
using MaterialId = std::uint64_t;

inline constexpr ElementId kInvalidElementId = 0;
inline constexpr ElementTypeId kInvalidElementTypeId = 0;
inline constexpr LevelId kInvalidLevelId = 0;
inline constexpr MaterialId kInvalidMaterialId = 0;

enum class ElementKind {
    Wall,
    Slab,
    Beam,
    Column,
    Door,
    Window,
    Roof,
    Stair,
    Room,
    GenericModel,
};

using ParameterValue = std::variant<
    std::monostate,
    bool,
    std::int64_t,
    double,
    std::string>;

using ParameterSet = std::unordered_map<std::string, ParameterValue>;

struct Level {
    LevelId id{kInvalidLevelId};
    std::string name;
    double elevation_mm{0.0};
};

struct Material {
    MaterialId id{kInvalidMaterialId};
    std::string name;
    ParameterSet parameters;
};

struct ElementType {
    ElementTypeId id{kInvalidElementTypeId};
    ElementKind kind{ElementKind::GenericModel};
    std::string name;
    ParameterSet parameters;
    std::vector<MaterialId> materials;
};

struct HostRelationship {
    ElementId hosted{kInvalidElementId};
    ElementId host{kInvalidElementId};
};

struct LocationCurve {
    geo::Vec2 start{};
    geo::Vec2 end{};
};

struct Element {
    ElementId id{kInvalidElementId};
    ElementKind kind{ElementKind::GenericModel};
    ElementTypeId type_id{kInvalidElementTypeId};
    LevelId level_id{kInvalidLevelId};
    std::optional<LevelId> top_level_id;
    double base_offset_mm{0.0};
    double top_offset_mm{0.0};
    double unconnected_height_mm{0.0};
    double thickness_mm{0.0};
    std::optional<LocationCurve> location_curve;
    std::vector<geo::Vec2> footprint;
    std::vector<MaterialId> materials;
    ParameterSet parameters;
    bool structural{false};
};

class ProjectModel {
public:
    [[nodiscard]] LevelId add_level(std::string name, double elevation_mm);
    [[nodiscard]] MaterialId add_material(std::string name, ParameterSet parameters = {});
    [[nodiscard]] ElementTypeId add_type(
        ElementKind kind,
        std::string name,
        ParameterSet parameters = {},
        std::vector<MaterialId> materials = {});
    [[nodiscard]] ElementId add_element(Element element);

    [[nodiscard]] bool erase_element(ElementId id);
    [[nodiscard]] bool set_host(ElementId hosted, ElementId host);
    [[nodiscard]] bool clear_host(ElementId hosted);

    [[nodiscard]] const Level* find_level(LevelId id) const;
    [[nodiscard]] const Material* find_material(MaterialId id) const;
    [[nodiscard]] const ElementType* find_type(ElementTypeId id) const;
    [[nodiscard]] const Element* find_element(ElementId id) const;
    [[nodiscard]] Element* find_element(ElementId id);
    [[nodiscard]] std::optional<ElementId> host_of(ElementId hosted) const;

    [[nodiscard]] std::vector<LevelId> level_ids() const;
    [[nodiscard]] std::vector<ElementId> element_ids() const;
    [[nodiscard]] std::size_t element_count() const noexcept { return elements_.size(); }

private:
    LevelId next_level_id_{1};
    MaterialId next_material_id_{1};
    ElementTypeId next_type_id_{1};
    ElementId next_element_id_{1};

    std::unordered_map<LevelId, Level> levels_;
    std::unordered_map<MaterialId, Material> materials_;
    std::unordered_map<ElementTypeId, ElementType> types_;
    std::unordered_map<ElementId, Element> elements_;
    std::unordered_map<ElementId, ElementId> host_by_element_;
};

} // namespace acp::bim
