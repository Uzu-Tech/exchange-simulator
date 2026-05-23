#pragma once
#include <concepts>
#include <pcg_random.hpp>
#include <random>

#include "config.hpp"
#include "rng.hpp"

template<typename T>
concept PriceModel = requires(T model, pcg32 gen, const typename T::ConfigParams& params) {
    T(gen, params);
    { model.next_price() } -> std::same_as<void>;
    { model.current_price() } -> std::same_as<double>;
};

class SimpleRandomWalk {
public:
    struct ConfigParams {
        double start_price;
        double p;
        double step;

        explicit ConfigParams(Config& node) {
            ConfigParser(node)
                .bind("start_price", start_price)
                .bind("p", p)
                .bind("step", step);
        }

        ConfigParams() = default;
    };

    explicit SimpleRandomWalk(pcg32 gen, const ConfigParams& params)
        : gen(gen),
          current_price_(params.start_price),
          prev_price_(params.start_price),
          p_(params.p),
          step(params.step) {}

    void next_price() noexcept { 
        prev_price_ = current_price_;
        current_price_ += (fast_uniform(gen) < p_ ? 1.0 : -1.0) * step;
    }
    double current_price() const noexcept { return current_price_; }
    double price_change() const noexcept { return current_price_ - prev_price_; }


private:
    double current_price_;
    double prev_price_;
    double step;
    pcg32 gen;
    double p_;
};

class GaussianRandomWalk {
public:
    struct ConfigParams {
        double start_price;
        double drift;
        double std_dev;

        explicit ConfigParams(Config& node) {
            ConfigParser(node)
                .bind("start_price", start_price)
                .bind("drift", drift)
                .bind("std_dev", std_dev);
        }

        ConfigParams() = default;
    };

    explicit GaussianRandomWalk(pcg32 gen, const ConfigParams& params)
        : gen(gen),
          current_price_(params.start_price),
          dist(params.drift, params.std_dev) {}

    void next_price() noexcept { current_price_ += dist(gen); }

    double current_price() const noexcept { return current_price_; }

private:
    double current_price_;
    pcg32 gen;
    std::normal_distribution<double> dist;
};