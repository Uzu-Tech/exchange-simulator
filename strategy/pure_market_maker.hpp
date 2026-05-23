#pragma once

#include "market_types.hpp"
#include <vector>
#include "orderbook.hpp"
#include "primitives.hpp"
#include "config.hpp"

class PureMarketMaker {
public:
    struct ConfigParams {
        PriceDelta offset;

        explicit ConfigParams(Config& node) {
            ConfigParser(node)
                .bind("offset", offset);
        }

        ConfigParams() = default;
    };

    PureMarketMaker(const ConfigParams& params) : offset(params.offset) {}

    void get_orders(
        const TradingState& state, 
        const OrderBook& orderbook, 
        PositionLimit position_limit,
        std::vector<OrderRequest>& orders
    ) {
        Volume buy_capacity = position_limit.remaining_long(state.position);
        Volume sell_capacity = position_limit.remaining_short(state.position);

        auto best_bid = orderbook.bids().best();
        auto best_ask = orderbook.asks().best();

        if (best_bid && buy_capacity > Volume{0}) {
            orders.push_back(
                OrderRequest::limit(config::USER_ID, best_bid->price + offset, buy_capacity, Side::BID)
            );
        }

        if (best_ask&& sell_capacity > Volume{0}) {
            orders.push_back(
                OrderRequest::limit(config::USER_ID, best_ask->price - offset, sell_capacity, Side::ASK)
            );
        }
    }

private:
    PriceDelta offset;
};