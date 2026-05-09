#pragma once
#include <concepts>
#include <cmath>

template<typename T>
concept Numeric = (std::floating_point<T> || std::integral<T>)
                  && !std::same_as<T, bool> && !std::same_as<T, char>;

template<Numeric T>
struct Metric {
    T total_;
    double mean_ = 0.0;
    double sum_sq_diffs_ = 0.0; 
    int n = 0;

    Metric() = default;
    explicit Metric(T initial) { update(initial); }
    
    void update(T val) {
        n++;
        total_ += val;

        double delta1 = static_cast<double>(val) - mean_;
        mean_ += delta1 / n;

        double delta2 = static_cast<double>(val) - mean_;
        sum_sq_diffs_ += delta1 * delta2;
    }

    T total() const { return total_; }
    double mean() const { return mean_; }
    double variance() const {
        return (n >= 2) ? (sum_sq_diffs_ / (n - 1)) : 0.0;
    }
    double std_dev() const {
        return std::sqrt(variance());
    }
};

struct SimulatorResults {
    Metric<double> pnl;
    Metric<int> takes;
    Metric<int> makes;
    Metric<double> fill_quality;
    Metric<double> slippage;
    Metric<int> position;
    Metric<double> max_drawdown;
};