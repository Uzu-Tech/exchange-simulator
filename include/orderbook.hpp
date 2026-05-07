#pragma once

#include <deque>
#include <cstdint>
#include <vector>
#include <span>
#include <optional>
#include "types.hpp"
#include "trade.hpp"

template<Side S>
class OrderDepth {
public:
    OrderDepth(size_t level_capacity) {
        levels.reserve(level_capacity);
    }

    std::span<const PriceLevel> view() const noexcept { 
        return levels; 
    }
    const PriceLevel& best_price() const { return levels.back(); } 

    bool crosses_best_price(Price incoming) const {
        if constexpr (S == Side::BID) {
            return incoming <= best_price().price;
        } else {
            return incoming >= best_price().price;
        }
    }

    void clear() {
        levels.clear();
    }
    
private:
    std::vector<PriceLevel> levels;
};

class OrderBook {
public:
    OrderBook(size_t level_capacity) : bids_(level_capacity), asks_(level_capacity) {}
    
    void add_order(const OrderRequest& order_rq);

    std::span<const Trade> trades() const { return trades_; }
    std::span<const PriceLevel> bids() const { return bids_.view(); }
    std::span<const PriceLevel> asks() const { return asks_.view(); }

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
    void add_side(const OrderRequest& order_rq);
    
    template<Side S>
    OrderDepth<S>& depth() {
        if constexpr (S == Side::BID) return bids_;
        else return asks_;
    }
};