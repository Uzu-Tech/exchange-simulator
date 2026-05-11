#pragma once

#include <concepts>
#include "strong_types.hpp"
#include "types.hpp"
#include <optional>
#include <random>
#include <pcg_random.hpp>

struct MakerQuote {
    std::optional<OrderRequest> bid;
    std::optional<OrderRequest> ask;

    MakerQuote(std::optional<OrderRequest> bid, std::optional<OrderRequest> ask) : bid(bid), ask(ask) {
        hard_assert(bid || ask, "MakerQuote", "Invalid quote, no bid or ask");
        hard_assert((!bid || !ask) || bid->price <= ask->price, "MakerQuote", "Invalid quote, bid is greater than ask");
    }
};

template<typename T>
concept MarketMaker = requires(T maker, double sim_price) {
    {maker.make_market(sim_price)} -> std::same_as<MakerQuote>;
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

    MakerQuote make_market(double sim_price) {
        Price mid_price = convert_type_to_price(sim_price);

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