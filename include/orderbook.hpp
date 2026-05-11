#pragma once

#include <vector>
#include <span>
#include <algorithm>
#include "types.hpp"
#include "trade.hpp"

namespace detail {
    template<Side S>
    constexpr Side opposite() {
        if constexpr (S == Side::BID) return Side::ASK;
        else return Side::BID;
    }
}

template<Side S>
class OrderDepth {
public:
    explicit OrderDepth(size_t level_capacity) {
        levels.reserve(level_capacity);
    }

    std::span<const PriceLevel> view() const noexcept {
        return levels;
    }

    bool crosses_best_price(Price incoming) const {
        if (levels.empty()) return false;

        if constexpr (S == Side::BID)
            return incoming <= levels.back().price;
        else
            return incoming >= levels.back().price;
    }

    void match_opposing_order(OrderRequest& order_rq, std::vector<Trade>& trades) {
        while (order_rq.volume > Volume{0} && !levels.empty() &&
               (order_rq.type == OrderType::MARKET || crosses_best_price(order_rq.price)))
        {
            PriceLevel& best_level = levels.back();
            BookOrder& market_order = best_level.orders.front();

            Volume trade_vol = std::min(order_rq.volume, market_order.volume);
            market_order.volume -= trade_vol;
            order_rq.volume -= trade_vol;
            best_level.total_volume -= trade_vol;

            TraderId  buyer{0}, seller{0};
            if constexpr (S == Side::BID) {
                seller = order_rq.id;
                buyer  = market_order.id;
            } else {
                buyer  = order_rq.id;
                seller = market_order.id;
            }

            trades.push_back(Trade{best_level.price, trade_vol, buyer, seller});

            if (market_order.volume == Volume{0}) {
                best_level.orders.pop_front();
                if (best_level.orders.empty())
                    levels.pop_back();
            }
        }
    }

    void place_like_order(const OrderRequest& order_rq) {
        if (order_rq.volume == Volume{0} || order_rq.type == OrderType::MARKET) return;

        BookOrder new_order{order_rq.id, order_rq.volume};

        auto rev_it = std::find_if(levels.rbegin(), levels.rend(),
            [&](const PriceLevel& level) {
                if constexpr (S == Side::BID)
                    return level.price <= order_rq.price;
                else
                    return level.price >= order_rq.price;
            });

        if (rev_it != levels.rend() && rev_it->price == order_rq.price) {
            rev_it->orders.push_back(new_order);
            rev_it->total_volume += order_rq.volume;
            return;
        }

        levels.insert(rev_it.base(),
                      PriceLevel{order_rq.price, order_rq.volume, OrderQueue{new_order}});
    }

    void clear() noexcept { levels.clear(); }

private:
    std::vector<PriceLevel> levels;
};

class OrderBook {
public:
    explicit OrderBook(size_t level_capacity)
        : bids_(level_capacity), asks_(level_capacity) {}

    void add_order(OrderRequest& order_rq) {
        if (order_rq.side == Side::BID)
            add_side<Side::BID>(order_rq);
        else
            add_side<Side::ASK>(order_rq);
    }

    std::span<const Trade> trades() const noexcept { return trades_; }
    std::span<const PriceLevel> bids() const noexcept { return bids_.view(); }
    std::span<const PriceLevel> asks() const noexcept { return asks_.view(); }

    void clear() {
        bids_.clear();
        asks_.clear();
        trades_.clear();
    }

private:
    OrderDepth<Side::BID> bids_;
    OrderDepth<Side::ASK> asks_;
    std::vector<Trade>    trades_;

    template<Side S>
    OrderDepth<S>& depth() {
        if constexpr (S == Side::BID) return bids_;
        else return asks_;
    }

    template<Side S>
    void add_side(OrderRequest& order_rq) {
        OrderDepth<S>& own_depth = depth<S>();
        OrderDepth<detail::opposite<S>()>& cross_depth = depth<detail::opposite<S>()>();

        cross_depth.match_opposing_order(order_rq, trades_);
        own_depth.place_like_order(order_rq);
    }
};