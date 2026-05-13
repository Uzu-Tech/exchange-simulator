#pragma once

#include "config.hpp"
#include "primitives.hpp"
#include "market_types.hpp"
#include <span>


struct TradingState {
    Timestamp<config::TIMESTAMP_TICK_SIZE> timestamp;
    std::span<const PriceLevel> bids;
    std::span<const PriceLevel> asks;
    std::span<const Trade> prev_trades;
    Position position;
};