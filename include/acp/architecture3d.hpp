#pragma once

#include "acp/document.hpp"
#include "acp/model3d.hpp"

#include <optional>
#include <vector>

namespace acp::architecture3d {

struct WallSpec {
    geo::Vec2 start{};
    geo::Vec2 end{};
    double thickness{200.0};
    double height{3000.0};
    double base_z{0.0};
};

struct SlabSpec {
    std::vector<geo::Vec2> boundary;
    double thickness{200.0};
    double top_z{0.0};
};

struct ColumnSpec {
    geo::Vec2 center{};
    double width{400.0};
    double depth{400.0};
    double height{3000.0};
    double base_z{0.0};
    double rotation{0.0};
};

[[nodiscard]] bool valid(const WallSpec& wall) noexcept;
[[nodiscard]] bool valid(const SlabSpec& slab) noexcept;
[[nodiscard]] bool valid(const ColumnSpec& column) noexcept;

[[nodiscard]] std::optional<geo3d::Mesh> make_wall(const WallSpec& wall);
[[nodiscard]] std::optional<geo3d::Mesh> make_slab(const SlabSpec& slab);
[[nodiscard]] std::optional<geo3d::Mesh> make_column(const ColumnSpec& column);

[[nodiscard]] model3d::Scene walls_from_lines(
    const Document& document,
    double thickness,
    double height,
    double base_z = 0.0);

[[nodiscard]] model3d::Scene slabs_from_closed_polylines(
    const Document& document,
    double thickness,
    double top_z = 0.0);

} // namespace acp::architecture3d
