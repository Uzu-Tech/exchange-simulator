#pragma once

#include <cmath>
#include <variant>
#include <Yaml.hpp>

#include "pcg_random.hpp"
#include "primitives.hpp"

namespace config {
inline constexpr TraderId USER_ID{0};
}

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
    return T{ static_cast<typename T::Underlying>(node.template As<typename T::Underlying>()) };
}

class ConfigParser {
public:
    explicit ConfigParser(Config& node)
        : m_node(node) {}

    template<typename T>
    ConfigParser& bind(std::string_view key, T& target) {
        if (m_node[std::string(key)].IsNone()) {
            throw std::runtime_error(
                "Config error: Missing required key: " + std::string(key)
            );
        }

        try {
            target = as_type<T>(m_node[std::string(key)]);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Config error: Invalid value for key '" + std::string(key) + "': " + e.what()
            );
        }

        return *this;
    }

private:
    Config& m_node;
};