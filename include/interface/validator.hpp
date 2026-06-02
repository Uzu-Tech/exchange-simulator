#pragma once

#include "config.hpp"

namespace Validator {
inline void validate_config(Config& config) {
    auto require = [](Config& parent, const std::string& key, const std::string& context
                   ) -> Config& {
        auto& node = parent[key];
        if (node.IsNone())
            throw std::runtime_error(
                "Config error [" + context + "]: missing required key '" + key + "'"
            );
        return node;
    };

    require(config, "price_model", "root");
    require(config, "strategy", "root");
    require(config, "makers", "root");
    require(config, "takers", "root");

    auto& num_ticks_node = require(config, "num_ticks", "root");
    auto& pos_limit_node = require(config, "position_limit", "root");

    auto& model_node = config["price_model"];
    require(model_node, "params", "price_model");

    auto& strategy_node = config["strategy"];
    require(strategy_node, "params", "strategy");

    auto& makers_node = config["makers"];
    if (makers_node.Size() == 0)
        throw std::runtime_error("Config error [makers]: must contain at least one maker entry");
    for (size_t i = 0; i < makers_node.Size(); ++i) {
        auto& entry = makers_node[i];
        if (entry["params"].IsNone())
            throw std::runtime_error(
                "Config error [makers[" + std::to_string(i) + "]]: missing 'params' block"
            );
    }

    auto& takers_node = config["takers"];
    if (takers_node.Size() == 0)
        throw std::runtime_error("Config error [takers]: must contain at least one taker entry");

    for (size_t i = 0; i < takers_node.Size(); ++i) {
        auto& entry = takers_node[i];
        if (entry["params"].IsNone())
            throw std::runtime_error(
                "Config error [takers[" + std::to_string(i) + "]]: missing 'params' block"
            );
    }
}

}  // namespace Validator