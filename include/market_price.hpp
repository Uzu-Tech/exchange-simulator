#pragma once
#include <concepts>
#include <random>

template<typename T>
concept PriceModel = requires(T model) {
    {model.next_price()} -> std::same_as<double>;
};

class SimpleRandomWalk {
public:
    SimpleRandomWalk(
        double start_price, 
        double p,
        double step, 
        std::mt19937_64& gen
    ) : current_price(start_price), step(step), gen(gen), dist(p) {}

    double next_price() {
        current_price += (2 * dist(gen) - 1) * step;
        return current_price;
    }

private:
    double current_price;
    double step;
    std::mt19937_64& gen;
    std::bernoulli_distribution dist;
};


class GaussianRandomWalk {
public:
    GaussianRandomWalk(
        double start_price,
        double drift,
        double std_dev,
        std::mt19937_64& gen
    ) : current_price(start_price), gen(gen), dist(drift, std_dev) {}

    double next_price() {
        current_price += dist(gen);
        return current_price;
    }

private:
    double current_price;
    std::mt19937_64& gen;
    std::normal_distribution<double> dist;
};