#pragma once
#include <concepts>
#include <cmath>
#include "market_types.hpp"

template<typename T>
concept Numeric = (std::floating_point<T> || std::integral<T>)
                  && !std::same_as<T, bool> && !std::same_as<T, char>;

template<Numeric T>
class Metric {
public:
    Metric() = default;
    explicit Metric(T initial) { update(initial); }
    void update(T val) {
        n++;
        total_ += val;

        double delta1 = static_cast<double>(val) - mean_;
        mean_ += delta1 / n;

        double delta2 = static_cast<double>(val) - mean_;
        if (n > 1) {;
            var_ = ((n - 2) * var_ + delta1 * delta2) / (n - 1);
        }
    }

    T total() const { return total_; }
    double mean() const { return mean_; }
    double variance() const { return var_; }
    double std_dev() const {
        return std::sqrt(variance());
    }
private:
    T total_ = T{};
    double mean_ = 0.0;
    double var_ = 0.0; 
    int n = 0;
};

class CountMetric {
public:
    void update(bool occurred) {
        counts += int(occurred);
        total += 1;
    }

    double percentage() const {
        return (total == 0) ? 0.0 : static_cast<double>(counts) / total;
    }
    int count() const { return counts; }

private:
    int total = 0;
    int counts = 0;
};

class DrawDown {
public:
    void update(double pnl) {
        current_profit = pnl;
        count++;

        if (count > 1) {
            double delta1 = pnl - mean_pnl;
            mean_pnl += delta1 / count;

            double delta2 = pnl - mean_pnl;
            if (count > 1) {;
                var_pnl = ((count - 2) * var_pnl + delta1 * delta2) / (count - 1);
            }
        }
        prev_profit = current_profit;

        if (count == 1 || current_profit > peak_profit) {
            peak_profit = current_profit;
        }

        double current_dd = peak_profit - current_profit;
        mean_drawdown += (current_dd - mean_drawdown) / count;

        if (current_dd > max_drawdown) {
            max_drawdown = current_dd;
        }
    }

    double get_volatility() const {
        return std::sqrt(var_pnl);
    }

    double max() const {
        double sigma = get_volatility();
        if (sigma <= 1e-9) return 0.0; // Avoid division by zero/noise
        return max_drawdown / sigma;
    }

    double mean() const {
        double sigma = get_volatility();
        if (sigma <= 1e-9) return 0.0;
        return mean_drawdown / sigma;
    }

private:
    double peak_profit = 0.0;
    double max_drawdown = 0.0;
    double current_profit = 0.0;
    
    int count = 0;
    double mean_drawdown = 0.0;

    double prev_profit = 0.0;
    double mean_pnl = 0.0;
    double var_pnl = 0.0; 
};

struct SimulatorResults {
    Metric<double> pnl;
    CountMetric takes;
    CountMetric makes;
    Metric<double> fill_quality;
    Metric<double> slippage;
    Metric<int> position;
    DrawDown drawdown;
};

struct OrderFillEvent {
    std::span<const Trade> trades;
    OrderType type;
    Price expected_price;
    Side side;
    double sim_price;
};

struct BotFillEvent {
    std::span<const Trade> bot_trades;
    double sim_price;
};

struct TickEndEvent {
    Position position;
};

class MetricsCollector {
public:
    Volume on_order_fills(const OrderFillEvent& event) {
        Price expected_price = (event.type == OrderType::MARKET)? 
            event.trades.front().price :
            event.expected_price;
        
        double total_cost = 0.0;
        Volume total_volume{0};
        double tick_pnl = 0.0;

        for (const auto& trade : event.trades) {
            total_cost += trade.volume * trade.price;
            total_volume += trade.volume;

            if (event.side == Side::BID) {
                tick_pnl += (event.sim_price - price_to_double(trade.price)) * trade.volume.value();
            } else {
                tick_pnl += (price_to_double(trade.price) - event.sim_price) * trade.volume.value();
            }

            double vwap = total_cost / total_volume.value();
            double slippage = (event.side == Side::BID)? 
                vwap - price_to_double(expected_price) :
                price_to_double(expected_price) - vwap;

            results_.slippage.update(slippage);
        }

        results_.makes.update(bool(total_volume == Volume{0})); 
        results_.takes.update(bool(total_volume != Volume{0}));
        results_.pnl.update(tick_pnl);
        results_.drawdown.update(tick_pnl);
        return total_volume;
    }

    Volume on_bot_trades(const BotFillEvent& event) {
        Volume total_volume{0};
        double tick_pnl = 0.0;

        for (const auto& trade : event.bot_trades) {
            if (!(trade.buyer_id == config::USER_ID || trade.seller_id == config::USER_ID)) {
                continue;
            }

            total_volume += trade.volume;

            if (trade.buyer_id == config::USER_ID) {
                tick_pnl += (event.sim_price - price_to_double(trade.price)) * trade.volume.value();
                results_.fill_quality.update(tick_pnl);
            } else if (trade.seller_id == config::USER_ID) {
                tick_pnl += (price_to_double(trade.price) - event.sim_price) * trade.volume.value();
            }

            results_.fill_quality.update(tick_pnl);
        }

        results_.pnl.update(tick_pnl);
        results_.drawdown.update(tick_pnl);
        return total_volume;
    }

    void on_tick_end(const TickEndEvent& event) {
        results_.position.update(event.position.value());
    }

    const SimulatorResults& results() const { return results_; }

private:
    SimulatorResults results_;
};