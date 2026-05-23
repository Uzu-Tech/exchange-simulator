#pragma once

#include <concepts>
#include <optional>
#include <pcg_random.hpp>
#include <random>

#include "config.hpp"
#include "market_types.hpp"
#include "primitives.hpp"
#include "sim_price.hpp"

struct MakerQuote {
    std::optional<OrderRequest> bid;
    std::optional<OrderRequest> ask;

    MakerQuote(std::optional<OrderRequest> bid, std::optional<OrderRequest> ask)
        : bid(bid),
          ask(ask) {
        hard_assert(bid || ask, "MakerQuote", "Invalid quote, no bid or ask");
        hard_assert(
            (!bid || !ask) || bid->price <= ask->price,
            "MakerQuote",
            "Invalid quote, bid is greater than ask"
        );
    }
};

template<typename T, typename Model>
concept MarketMaker =
    PriceModel<Model> &&
    requires(
        T maker,
        const Model& model,
        TraderId id,
        pcg32 gen,
        const typename T::ConfigParams& params  // Enforce the new configuration pattern
    ) {
        T(id, gen, params);  // Constructor verification
        { maker.make_market(model) } -> std::same_as<MakerQuote>;
    };

class SymmetricMaker {
public:
    static constexpr std::string_view name = "SymmetricMaker";

    struct ConfigParams {
        ConfigParams() = default;

        PriceDelta half_spread;
        Volume min_volume;
        Volume max_volume;

        explicit ConfigParams(Config& node) {
            ConfigParser(node)
                .bind("half_spread", half_spread)
                .bind("min_volume", min_volume)
                .bind("max_volume", max_volume);
        }
    };

    explicit SymmetricMaker(TraderId id, pcg32 gen, const ConfigParams& params)
        : id(id),
          gen(gen),
          half_spread(params.half_spread),
          volume_dist(params.min_volume.value(), params.max_volume.value()) {}

    // 3. Trading Execution Logic (Completely unchanged)
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
    const TraderId id;
    pcg32 gen;
    std::uniform_int_distribution<Volume::Underlying> volume_dist;
};