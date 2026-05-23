#pragma once

#include <string>
#include <tuple>

#include "active_types.hpp"
#include "config.hpp"
#include "maker.hpp"
#include "pcg_extras.hpp"
#include "primitives.hpp"
#include "rng.hpp"
#include "sim_price.hpp"
#include "simulator.hpp"
#include "taker.hpp"
#include "validator.hpp"
#include "Yaml.hpp"

namespace SimBuilder {
class IdGenerator {
public:
    TraderId next_id() { return TraderId{id++}; };

private:
    TraderId::Underlying id = 1;
};

template<typename Maker, size_t Index>
Maker build_single_maker(RandomEngine& rng, IdGenerator& id_gen, Config& makers_array) {
    Config& maker_node = makers_array[Index];
    return Maker{
        id_gen.next_id(),
        rng.make_child(),
        typename Maker::ConfigParams(maker_node["params"])
    };
}

template<MarketMaker<ActivePriceModel>... Makers, size_t... Is>
std::tuple<Makers...> build_makers_impl(
    std::index_sequence<Is...>,
    RandomEngine& rng,
    IdGenerator& id_gen,
    Config& makers_array
) {
    return std::make_tuple(build_single_maker<Makers, Is>(rng, id_gen, makers_array)...);
}

template<MarketMaker<ActivePriceModel>... Makers>
std::tuple<Makers...> build_makers(
    TypeList<Makers...>,
    RandomEngine& rng,
    IdGenerator& id_gen,
    Config& makers_array
) {
    return build_makers_impl<Makers...>(
        std::index_sequence_for<Makers...>{},
        rng,
        id_gen,
        makers_array
    );
}

template<typename Taker, size_t Index>
Taker build_single_taker(RandomEngine& rng, IdGenerator& id_gen, Config& takers_array) {
    auto& taker_node = takers_array[Index];
    return Taker{
        id_gen.next_id(),
        rng.make_child(),
        typename Taker::ConfigParams(taker_node["params"])
    };
}

template<MarketTaker<ActivePriceModel>... Takers, size_t... Is>
std::tuple<Takers...> build_takers_impl(
    std::index_sequence<Is...>,
    RandomEngine& rng,
    IdGenerator& id_gen,
    Config& takers_array
) {
    return std::make_tuple(build_single_taker<Takers, Is>(rng, id_gen, takers_array)...);
}

template<MarketTaker<ActivePriceModel>... Takers>
std::tuple<Takers...> build_takers(
    TypeList<Takers...>,
    RandomEngine& rng,
    IdGenerator& id_gen,
    Config& takers_array
) {
    return build_takers_impl<Takers...>(
        std::index_sequence_for<Takers...>{},
        rng,
        id_gen,
        takers_array
    );
}

inline Simulator<ActiveMakers, ActiveTakers> build_sim(Config& config) {
    Validator::validate_config(config);

    Config sim_node = config["simulation"];

    uint64_t seed{};
    if (!sim_node["seed"].IsNone()) {
        seed = sim_node["seed"].As<uint64_t>();
    } else {
        pcg_extras::seed_seq_from<std::random_device> seed_source;
        seed = pcg_extras::generate_one<uint64_t>(seed_source);
    }

    size_t num_timestamps = sim_node["num_timestamps"].As<size_t>();
    PositionLimit position_limit = as_type<PositionLimit>(sim_node["position_limit"]);

    RandomEngine root_rng{seed};

    Config model_node = config["price_model"];
    typename ActivePriceModel::ConfigParams model_params(model_node["params"]);
    ActivePriceModel model{root_rng.make_child(), model_params};

    // 3. Instantiate ActiveStrategy using its config block
    Config strategy_node = config["strategy"];
    typename ActiveStrategy::ConfigParams strategy_params(strategy_node["params"]);
    ActiveStrategy strategy{strategy_params};

    IdGenerator id_gen{};

    Config maker_node = config["makers"];
    auto makers_tuple = build_makers(ActiveMakers{}, root_rng, id_gen, maker_node);

    // Assuming a single RandomTaker instance for ActiveTakers
    Config taker_node = config["takers"];
    auto takers_tuple = build_takers(ActiveTakers{}, root_rng, id_gen, taker_node);

    // 5. Construct and return the fully-wired simulator
    return Simulator<ActiveMakers, ActiveTakers>{
        seed,
        num_timestamps,
        position_limit,
        std::move(model),
        std::move(strategy),
        std::move(makers_tuple),
        std::move(takers_tuple)
    };
}
}  // namespace SimBuilder