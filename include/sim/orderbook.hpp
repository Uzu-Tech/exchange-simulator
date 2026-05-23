#pragma once

#include <algorithm>
#include <ranges>
#include <span>
#include <vector>

#include "config.hpp"
#include "market_types.hpp"
#include "primitives.hpp"

namespace detail {
template<Side S>
constexpr Side opposite() {
    if constexpr (S == Side::BID)
        return Side::ASK;
    else
        return Side::BID;
}
}  // namespace detail

template<Side S>
class OrderDepth {
public:
    OrderDepth() { levels.reserve(config::INITIAL_ORDER_BOOK_DEPTH); }

    auto view() const noexcept { return levels | std::views::reverse; }

    const PriceLevel* best() const {
        return (levels.empty()) ? nullptr : &levels.back();
    }

    const PriceLevel* worst() const {
        return (levels.empty()) ? nullptr : &levels.front();
    }

    bool crosses_best_price(Price incoming) const {
        if (levels.empty()) return false;

        if constexpr (S == Side::BID)
            return incoming <= levels.back().price;
        else
            return incoming >= levels.back().price;
    }

    std::span<const Trade> match_opposing_order(
        OrderRequest& order_rq,
        std::vector<Trade>& trades
    ) {
        size_t prev_trade_size = trades.size();

        while (order_rq.volume > Volume{0} && !levels.empty() &&
               (order_rq.type == OrderType::MARKET || crosses_best_price(order_rq.price))) {
            PriceLevel& best_level = levels.back();
            BookOrder& market_order = best_level.orders.front();

            Volume trade_vol = consume(best_level, order_rq.volume);
            trades.push_back(get_trade(best_level.price, trade_vol, order_rq.id));
            remove_if_filled(best_level);
        }
        // new trades
        return std::span<const Trade>(trades.begin() + prev_trade_size, trades.end());
    }

    void place_like_order(const OrderRequest& order_rq) {
        if (order_rq.volume == Volume{0} || order_rq.type == OrderType::MARKET) return;

        BookOrder new_order{order_rq.id, order_rq.volume};

        auto rev_it = std::find_if(levels.rbegin(), levels.rend(), [&](const PriceLevel& level) {
            if constexpr (S == Side::BID)
                return level.price <= order_rq.price;
            else
                return level.price >= order_rq.price;
        });

        if (rev_it != levels.rend() && rev_it->price == order_rq.price) {
            rev_it->orders.push_back(new_order);
            rev_it->volume += order_rq.volume;
            return;
        }

        levels.insert(
            rev_it.base(),
            PriceLevel{order_rq.price, order_rq.volume, OrderQueue{new_order}}
        );
    }

    void clear() noexcept { levels.clear(); }

private:
    std::vector<PriceLevel> levels;

    Volume consume(PriceLevel& level, Volume& taker_vol) {
        BookOrder& market_order = level.orders.front();
        Volume trade_vol = std::min(taker_vol, market_order.volume);
        market_order.volume -= trade_vol;
        taker_vol -= trade_vol;
        level.volume -= trade_vol;
        return trade_vol;
    }

    Trade get_trade(Price price, Volume volume, TraderId taker_id) {
        BookOrder& market_order = levels.back().orders.front();

        if constexpr (S == Side::BID) {
            return Trade{
                .price = price,
                .volume = volume,
                .buyer_id = market_order.id,
                .seller_id = taker_id
            };
        } else {
            return Trade{
                .price = price,
                .volume = volume,
                .buyer_id = taker_id,
                .seller_id = market_order.id
            };
        }
    }

    void remove_if_filled(PriceLevel& level) {
        if (level.orders.front().volume == Volume{0}) {
            level.orders.pop_front();
            if (level.orders.empty()) levels.pop_back();
        }
    }
};

class OrderBook {
public:
    OrderBook() = default;

    std::span<const Trade> add_order(OrderRequest& order_rq) {
        if (order_rq.side == Side::BID)
            return add_side<Side::BID>(order_rq);
        else
            return add_side<Side::ASK>(order_rq);
    }

    std::span<const Trade> trades() const noexcept { return trades_; }
    auto& bids() const noexcept { return bids_; }
    auto& asks() const noexcept { return asks_; }

    void clear() {
        bids_.clear();
        asks_.clear();
        trades_.clear();
    }

private:
    OrderDepth<Side::BID> bids_;
    OrderDepth<Side::ASK> asks_;
    std::vector<Trade> trades_;

    template<Side S>
    OrderDepth<S>& depth() {
        if constexpr (S == Side::BID)
            return bids_;
        else
            return asks_;
    }

    template<Side S>
    std::span<const Trade> add_side(OrderRequest& order_rq) {
        OrderDepth<S>& own_depth = depth<S>();
        OrderDepth<detail::opposite<S>()>& cross_depth = depth<detail::opposite<S>()>();

        auto new_trades = cross_depth.match_opposing_order(order_rq, trades_);
        own_depth.place_like_order(order_rq);
        return new_trades;
    }
};