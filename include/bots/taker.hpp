#pragma once

#include <concepts>
#include <optional>
#include <pcg_random.hpp>
#include <random>

#include "config.hpp"
#include "market_types.hpp"
#include "orderbook.hpp"
#include "primitives.hpp"
#include "sim_price.hpp"
#include "rng.hpp"

template<typename T, typename Model>
concept MarketTaker =
    PriceModel<Model> && requires(
                             T taker,
                             const OrderBook& order_book,
                             const Model& model,
                             TraderId id,
                             pcg32 gen,
                             const typename T::ConfigParams& params  // Fixed: Added typename
                         ) {
        T(id, gen, params);
        { taker.get_order(order_book, model) } -> std::same_as<std::optional<OrderRequest>>;
    };

class RandomTaker {
public:
    static constexpr std::string_view name = "RandomTaker";

    struct ConfigParams {
        ConfigParams() = default;

        double trade_prob;
        Volume min_volume;
        Volume max_volume;

        explicit ConfigParams(Config& node) {
            ConfigParser(node)
                .bind("trade_prob", trade_prob)
                .bind("min_volume", min_volume)
                .bind("max_volume", max_volume);
        }
    };

    explicit RandomTaker(TraderId id, pcg32 gen, const ConfigParams& params)
        : id(id),
          gen(gen),
          trade_prob_(params.trade_prob),
          volume_dist(params.min_volume.value(), params.max_volume.value()) {}

    std::optional<OrderRequest> get_order(
        const OrderBook& order_book,
        const PriceModel auto& model
    ) {
        if (fast_uniform(gen) < trade_prob_) [[unlikely]] {
            return OrderRequest::market(
                id,
                Volume{volume_dist(gen)},
                (fast_uniform(gen) < 0.5) ? Side::BID : Side::ASK
            );
        }
        return std::nullopt;
    }

private:
    TraderId id;
    pcg32 gen;
    std::uniform_int_distribution<Volume::Underlying> volume_dist;
    double trade_prob_;
};