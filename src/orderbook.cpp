#include "orderbook.hpp"
#include "types.hpp"
#include "ranges"

// Helpers
namespace {
    template<Side S>
    constexpr Side opposite() {
        if constexpr (S == Side::BID) return Side::ASK;
        else return Side::BID;
    }
}

void OrderBook::add_order(const OrderRequest& order_rq) {
    if (order_rq.side == Side::BID) {
        add_side<Side::BID>(order_rq);
    } else {
        add_side<Side::ASK>(order_rq);
    }
}

template<Side S>
void OrderBook::add_side(const OrderRequest& order_rq) {
    // Match orders to other side
    OrderDepth<S>& own_depth = depth<S>();
    OrderDepth<opposite<S>> cross_depth = depth<opposite<S>>();

    if (cross_depth.crosses_best_price(order_rq.price)) {
        
    }

}