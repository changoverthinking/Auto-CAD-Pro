#pragma once

#include "acp/document.hpp"

namespace acp::transform {

[[nodiscard]] geo::Vec2 rotate_point(geo::Vec2 point, geo::Vec2 origin, double radians) noexcept;
[[nodiscard]] geo::Vec2 scale_point(geo::Vec2 point, geo::Vec2 origin, double factor) noexcept;
[[nodiscard]] geo::Vec2 mirror_point(geo::Vec2 point, geo::Segment axis) noexcept;

void translate(Entity& entity, geo::Vec2 delta) noexcept;
void rotate(Entity& entity, geo::Vec2 origin, double radians) noexcept;
bool scale_uniform(Entity& entity, geo::Vec2 origin, double factor) noexcept;
bool mirror(Entity& entity, geo::Segment axis) noexcept;
[[nodiscard]] Entity translated_copy(const Entity& entity, geo::Vec2 delta) noexcept;

} // namespace acp::transform
