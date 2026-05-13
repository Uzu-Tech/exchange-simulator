#pragma once
#include <concepts>
#include <random>
#include <pcg_random.hpp>

template<typename T>
concept PriceModel = requires(T model) {
    {model.next_price()} -> std::same_as<double>;
    {model.current_price()} -> std::same_as<double>;
};

class SimpleRandomWalk {
public:
    SimpleRandomWalk(
        double start_price, 
        double p,
        double step, 
        pcg32 gen
    ) : current_price_(start_price), step(step), gen(gen), dist(p) {}

    const double next_price() noexcept {
        current_price_ += (2 * dist(gen) - 1) * step;
        return current_price_;
    }

    double current_price() const noexcept { return current_price_; }

private:
    double current_price_;
    const double step;
    pcg32 gen;
    std::bernoulli_distribution dist;
};


class GaussianRandomWalk {
public:
    GaussianRandomWalk(
        double start_price,
        double drift,
        double std_dev,
        pcg32 gen
    ) : current_price_(start_price), gen(gen), dist(drift, std_dev) {}

    double next_price() noexcept {
        current_price_ += dist(gen);
        return current_price_;
    }

    double current_price() const noexcept { return current_price_; }

private:
    double current_price_;
    pcg32 gen;
    std::normal_distribution<double> dist;
};