#pragma once

#include <type_traits>

namespace acp::detail {
// Import can set the next counter to zero by reaching the unsigned limit.
// Reuse free nonzero IDs after wrap, and fail without modifying the map if full.
template<class Map, class Id>
Id allocate_id(const Map& entries, Id& next) {
    static_assert(std::is_unsigned_v<Id>);
    if (next == 0) next = 1;
    const Id start = next;
    do {
        const Id candidate = next;
        ++next;
        if (next == 0) next = 1;
        if (!entries.contains(candidate)) return candidate;
    } while (next != start);
    return 0;
}
} // namespace acp::detail
