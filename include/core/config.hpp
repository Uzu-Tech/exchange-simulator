#pragma once

#include "primitives.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <string_view>
#include <vector>

namespace config {
    inline constexpr uint64_t DEFAULT_RNG_SEED = 42; 
    inline constexpr std::size_t  ORDER_BOOK_DEPTH = 16; // Max number of levels on each side
    inline constexpr TraderId USER_ID{0}; // Max number of levels on each side
    inline constexpr unsigned int TIMESTAMP_TICK_SIZE{100};
}

using SettingValue = std::variant<bool, int, double, std::string_view>;
using ConfigMap = std::unordered_map<std::string_view, SettingValue>;


template<typename Ret, typename... Args, std::size_t... Is>
Ret invoke_helper(
    Ret(*func)(Args...), // function type: Ret type and Args deduced from this
    const ConfigMap& config, // config from file or CLI
    const std::vector<std::string_view>& keys, 
    std::index_sequence<Is...>
) {
    // Unpacks the pack: get_value<Arg0>(config["key0"]), get_value<Arg1>(config["key1"])...
    return func(std::get<Args>(config.at(keys[Is]))...);
}

template<std::size_t N>
using Indices = std::make_index_sequence<N>;