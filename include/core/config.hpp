#pragma once

#include "primitives.hpp"
#include <cstdint>

namespace config {
    inline constexpr uint64_t DEFAULT_RNG_SEED = 42; 
    inline constexpr std::size_t  ORDER_BOOK_DEPTH = 16; // Max number of levels on each side
    inline constexpr TraderId USER_ID{0}; // Max number of levels on each side
}