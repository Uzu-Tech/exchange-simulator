#pragma once
#include <cstdint>
#include "primitives.hpp"

struct SimInfo {
    size_t num_ticks;
    uint64_t seed;
    PositionLimit position_limit;
};