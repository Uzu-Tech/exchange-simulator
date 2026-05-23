#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <Yaml.hpp>

#include "pcg_random.hpp"
#include "primitives.hpp"

namespace config {
inline constexpr uint64_t DEFAULT_RNG_SEED = 42;
inline constexpr std::size_t INITIAL_ORDER_BOOK_DEPTH = 16;  // Initial number of levels on each side
inline constexpr std::size_t MAX_ORDERS_PER_LEVEL = 8; // Max number of orders in one price level
inline constexpr TraderId USER_ID{0};
inline constexpr unsigned int TIMESTAMP_TICK_SIZE{100};
}  // namespace config

using SettingValue = std::variant<bool, int, double, std::string>;
using Config = Yaml::Node;

template<typename T>
concept CustomType = requires { typename T::Underlying; };

template<typename T>
    requires(!CustomType<T>)
T as_type(Config& node) {
    return node.template As<T>();
}

template<CustomType T>
T as_type(Config& node) {
    return T{node.template As<typename T::Underlying>()};
}

class ConfigParser {
public:
    explicit ConfigParser(Config& node)
        : m_node(node) {}

    template<typename T>
    ConfigParser& bind(std::string_view key, T& target) {
        if (m_node[std::string(key)].IsNone()) {
            throw std::runtime_error(
                "Configuration error: Missing required key: " + std::string(key)
            );
        }

        try {
            target = as_type<T>(m_node[std::string(key)]);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Configuration error: Invalid value for key '" + std::string(key) + "': " + e.what()
            );
        }

        return *this;
    }

private:
    Config& m_node;
};