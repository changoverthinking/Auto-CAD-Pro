#include "acp/hatch.hpp"

#include <cmath>

namespace acp::hatch {

bool valid(const HatchEntity& value) noexcept {
    if (value.boundary.size() < 3) {
        return false;
    }
    if (!std::isfinite(value.angle) ||
        !std::isfinite(value.spacing) ||
        value.spacing <= geo::kEpsilon) {
        return false;
    }
    return value.solid || !value.pattern.empty();
}

double perimeter(const HatchEntity& value) noexcept {
    return valid(value)
        ? geo::polyline_length(value.boundary, true)
        : 0.0;
}

} // namespace acp::hatch
