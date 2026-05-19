#include "market_types.hpp"
#include <vector>
#include "orderbook.hpp"
#include "primitives.hpp"
#include "config.hpp"

class PureMarketMaker {
public:
    std::vector<OrderRequest> get_orders(const TradingState& state, const OrderBook& orderbook, PositionLimit position_limit) {
        Volume buy_capacity = position_limit.remaining_long(state.position);
        Volume sell_capacity = position_limit.remaining_short(state.position);

        auto best_bid = orderbook.bids().best();
        auto best_ask = orderbook.asks().best();

        std::vector<OrderRequest> orders{};
        PriceDelta edge{1};

        if (best_bid && buy_capacity > Volume{0}) {
            orders.push_back(
                OrderRequest::limit(config::USER_ID, best_bid->price + edge, buy_capacity, Side::BID)
            );
        }

        if (best_ask&& sell_capacity > Volume{0}) {
            orders.push_back(
                OrderRequest::limit(config::USER_ID, best_ask->price - edge, sell_capacity, Side::ASK)
            );
        }

        return orders;
    }
};