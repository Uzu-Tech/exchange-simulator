#pragma once

#include "types.hpp"

struct Trade {
    Price price;
    Volume volume;
    OrderId buyer_id;
    OrderId seller_id;
};