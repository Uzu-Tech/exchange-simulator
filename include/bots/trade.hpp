#pragma once

#include "primitives.hpp"

struct Trade {
    Price price;
    Volume volume;
    TraderId  buyer_id;
    TraderId  seller_id;
};