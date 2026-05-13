#pragma once

#include <concepts>
#include "orderbook.hpp"
#include "primitives.hpp"
#include "market_types.hpp"
#include <optional>
#include <random>
#include <pcg_random.hpp>

template<typename T>
concept Taker = requires(T taker, const OrderBook& order_book, double sim_price) {
    {taker.get_order(order_book, sim_price)} -> std::same_as<std::optional<OrderRequest>>;
};

class RandomTaker {
public:
    explicit RandomTaker(
        TraderId id,
        double trade_prob,
        Volume min_volume, 
        Volume max_volume, 
        pcg32 gen
    )
        : id(id), trade_arrival(trade_prob), volume_dist(min_volume.value(), max_volume.value()), gen(gen) {}

    std::optional<OrderRequest> get_order(const OrderBook& order_book, double sim_price) {
        if (trade_arrival(gen)) [[unlikely]] {
            return OrderRequest::market(id, Volume{volume_dist(gen)}, (side_dist(gen))? Side::BID : Side::ASK);
        }
        return std::nullopt;
    }

private:
    TraderId id;
    pcg32 gen;
    std::bernoulli_distribution trade_arrival;
    std::uniform_int_distribution<Volume::Underlying> volume_dist;
    std::bernoulli_distribution side_dist{0.5};
};