#pragma once

#include "strong_types.hpp"

struct Trade {
    Price price;
    Volume volume;
    TraderId  buyer_id;
    TraderId  seller_id;
};