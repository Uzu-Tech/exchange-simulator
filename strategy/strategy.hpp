#pragma once

#include "market_types.hpp"
#include <vector>
#include "orderbook.hpp"
#include "primitives.hpp"
#include <concepts>

template<typename T>
concept Strategy = requires(
    T strategy, 
    const TradingState& state, 
    const OrderBook& orderbook,
    PositionLimit position_limit,
    std::vector<OrderRequest>& orders
) {
    {strategy.get_orders(state, orderbook, position_limit, orders)} -> std::same_as<void>;
};