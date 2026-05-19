#pragma once

#include <concepts>
#include "primitives.hpp"
#include "market_types.hpp"
#include <optional>
#include <random>
#include <pcg_random.hpp>
#include "sim_price.hpp"

struct MakerQuote {
    std::optional<OrderRequest> bid;
    std::optional<OrderRequest> ask;

    MakerQuote(std::optional<OrderRequest> bid, std::optional<OrderRequest> ask) : bid(bid), ask(ask) {
        hard_assert(bid || ask, "MakerQuote", "Invalid quote, no bid or ask");
        hard_assert((!bid || !ask) || bid->price <= ask->price, "MakerQuote", "Invalid quote, bid is greater than ask");
    }
};

template<typename T, typename Model> 
concept MarketMaker = 
    PriceModel<Model> && 
    requires(T maker, const Model &model) {
        { maker.make_market(model) } -> std::same_as<MakerQuote>;
    };

class SymmetricMaker {
public:
    explicit SymmetricMaker(
        TraderId  id, 
        PriceDelta half_spread, 
        Volume min_volume, 
        Volume max_volume, 
        pcg32 gen
    ) 
        : id(id), half_spread(half_spread), volume_dist(min_volume.value(), max_volume.value()), gen(gen) {}

    MakerQuote make_market(const PriceModel auto& model) {
        Price mid_price = convert_double_to_price(model.current_price());

        Price bid_price = mid_price - half_spread;
        Price ask_price = mid_price + half_spread;
        
        Volume bid_volume{volume_dist(gen)};
        Volume ask_volume{volume_dist(gen)};

        return MakerQuote{
            OrderRequest::limit(id, bid_price, bid_volume, Side::BID),
            OrderRequest::limit(id, ask_price, ask_volume, Side::ASK)
        };
    }


private:
    const PriceDelta half_spread;
    const TraderId  id;
    pcg32 gen;
    std::uniform_int_distribution<Volume::Underlying> volume_dist;
};