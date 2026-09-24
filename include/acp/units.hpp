#pragma once

#include <stdexcept>

namespace acp::units {

enum class LengthUnit {
    Millimeter,
    Centimeter,
    Meter,
    Inch,
    Foot,
};

// Existing 2D/architectural data is authored in millimeters. Keep that as the
// canonical internal unit until a versioned project-format migration can be
// performed safely. All external bridges must convert explicitly.
inline constexpr LengthUnit kCanonicalLengthUnit = LengthUnit::Millimeter;

[[nodiscard]] constexpr double meters_per_unit(LengthUnit unit) {
    switch (unit) {
        case LengthUnit::Millimeter: return 0.001;
        case LengthUnit::Centimeter: return 0.01;
        case LengthUnit::Meter: return 1.0;
        case LengthUnit::Inch: return 0.0254;
        case LengthUnit::Foot: return 0.3048;
    }
    return 1.0;
}

[[nodiscard]] constexpr double convert_length(
    double value,
    LengthUnit from,
    LengthUnit to) {

    return value * meters_per_unit(from) / meters_per_unit(to);
}

[[nodiscard]] constexpr double canonical_to_meters(double value) {
    return convert_length(value, kCanonicalLengthUnit, LengthUnit::Meter);
}

} // namespace acp::units
