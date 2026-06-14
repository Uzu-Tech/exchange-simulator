#pragma once
#include <atomic>
#include <cmath>
#include <concepts>
#include <algorithm>
#include <execution>

#include "market_types.hpp"
#include "primitives.hpp"
#include "config.hpp"
#include "logger.hpp"

template<typename T>
concept Numeric = (std::floating_point<T> || std::integral<T>) && !std::same_as<T, bool> &&
                  !std::same_as<T, char>;

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
        if (n > 1) {
            ;
            var_ = ((n - 2) * var_ + delta1 * delta2) / (n - 1);
        }
    }

    T total() const { return total_; }
    double mean() const { return mean_; }
    double variance() const { return var_; }
    double std_dev() const { return std::sqrt(variance()); }

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

    double percentage() const { return (total == 0) ? 0.0 : static_cast<double>(counts) / total; }
    int count() const { return counts; }

private:
    int total = 0;
    int counts = 0;
};

class DrawDown {
public:
    void update(double pnl) {
        current_equity += pnl;
        count++;

        if (count == 1 || current_equity > peak_equity) {
            peak_equity = current_equity;
        }

        double current_dd = peak_equity - current_equity;
        mean_drawdown += (current_dd - mean_drawdown) / count;

        if (current_dd > max_drawdown) {
            max_drawdown = current_dd;
        }
    }

    double max() const {
        return max_drawdown;
    }

    double mean() const {
        return mean_drawdown;
    }

private:
    double peak_equity= 0.0;
    double max_drawdown = 0.0;
    double current_equity= 0.0;

    int count = 0;
    double mean_drawdown = 0.0;

    double prev_equity = 0.0;
    double mean_pnl = 0.0;
};

struct SimulatorResults {
    Metric<double> pnl;
    CountMetric takes;
    CountMetric makes;
    CountMetric fills;
    Metric<double> fill_vol;
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

struct OnTickStart {
    Position position;
    double price_change;
};

class MetricsCollector {
public:

    MetricsCollector(SingleRunLogger* logger) : logger(logger) {} 

    void on_tick_start(const OnTickStart& event) {
        tick_pnl = event.price_change * event.position.value();
    }

    Volume on_order_fills(const OrderFillEvent& event) {
        Price expected_price =
            (event.type == OrderType::MARKET) ? event.trades.front().price : event.expected_price;

        double total_cost = 0.0;
        Volume total_volume{0};

        for (const auto& trade : event.trades) {
            total_cost += trade.volume * trade.price;
            total_volume += trade.volume;

            if (event.side == Side::BID) {
                tick_pnl += (event.sim_price - price_to_double(trade.price)) * trade.volume.value();
            } else {
                tick_pnl += (price_to_double(trade.price) - event.sim_price) * trade.volume.value();
            }

            if (logger) log_trade(trade);
        }

        if (total_volume != Volume{0}) {
            double vwap = total_cost / total_volume.value();
            double slippage = (event.side == Side::BID) ? vwap - price_to_double(expected_price)
                                                        : price_to_double(expected_price) - vwap;
            results_.slippage.update(slippage);
        }

        results_.makes.update(bool(total_volume == Volume{0}));
        results_.takes.update(bool(total_volume != Volume{0}));
        return total_volume;
    }

    Volume on_bot_trades(const BotFillEvent& event) {
        Volume total_volume{0};

        for (const auto& trade : event.bot_trades) {
            if (!(trade.buyer_id == config::USER_ID || trade.seller_id == config::USER_ID)) {
                continue;
            }

            total_volume += trade.volume;

            double unit_pnl{};
            if (trade.buyer_id == config::USER_ID || trade.seller_id == config::USER_ID) {
                int sign = static_cast<int>(trade.buyer_id == config::USER_ID) * 2 - 1; // branchless calculation
                unit_pnl = (event.sim_price - price_to_double(trade.price)) * sign;
                results_.fills.update(true);
                results_.fill_vol.update(trade.volume.value());
            }

            tick_pnl += unit_pnl * trade.volume.value();
            results_.fill_quality.update(unit_pnl);

            if (logger) log_trade(trade);
        }

        return total_volume;
    }

    void on_tick_end(const TickEndEvent& event) {
        results_.position.update(event.position.value());
        results_.drawdown.update(tick_pnl);
        results_.pnl.update(tick_pnl);
        if (logger) logger->log_performance_data({current_tick, tick_pnl, event.position});
        ++current_tick;
    }

    void log_trade(const Trade& trade) {
        logger->log_trade_data({
            .tick = current_tick,
            .buyer_id = trade.buyer_id,
            .seller_id = trade.seller_id,
            .price = trade.price,
            .volume = trade.volume
        });
    }

    const SimulatorResults& results() const { return results_; }

private:
    SimulatorResults results_;
    double tick_pnl = 0.0;
    SingleRunLogger* logger;
    Tick current_tick{0};
};

struct MonteCarloResults {
    size_t num_runs;
    double pnl_mean;
    double pnl_std;
    double win_rate;

    double pnl_5;
    double pnl_50;
    double pnl_95;

    double c_var;
    double mean_max_dd;
    double skew;

    double inventory_bias;
    double avg_fill_rate;
    double avg_slippage;
};

class MonteCarloCollector {
public:
    MonteCarloCollector(size_t num_runs) : pnl_per_run(num_runs) {}

    void update(SimulatorResults results, size_t idx, uint64_t seed, MonteCarloLogger* logger) {
        pnl_per_run[idx] = results.pnl.total();
        max_dd_sum += results.drawdown.max();
        position_sum += results.position.mean();
        slippage_sum += results.slippage.mean();
        fill_rate_sum += 2 * static_cast<double>(results.fills.count()) / results.makes.count();
        if (logger) {
            logger->log_monte_carlo_data({
                .seed = seed,
                .pnl = results.pnl.total(),
                .max_dd = results.drawdown.max()
            }, idx);
        }
    }

    MonteCarloResults results() {
        size_t num_runs = pnl_per_run.size();

        auto [pnl_mean, pnl_std, pnl_skew, win_rate] = general_stats();

        std::ranges::sort(pnl_per_run);
        double pnl_5 = get_percentile(0.05);
        double pnl_50 = get_percentile(0.5);
        double pnl_95 = get_percentile(0.95);

        double c_var = calculate_cvar(pnl_5);
        double mean_max_dd = max_dd_sum / num_runs;

        double inventory_bias = static_cast<double>(position_sum) / num_runs;
        double avg_fill_rate = fill_rate_sum / num_runs;
        double avg_slippage = slippage_sum / num_runs;

        return MonteCarloResults{
            num_runs, pnl_mean, pnl_std, win_rate,
            pnl_5, pnl_50, pnl_95,
            c_var, mean_max_dd, pnl_skew,
            inventory_bias, avg_fill_rate, avg_slippage
        };
    }

    std::tuple<double, double, double, double> general_stats() const {
        double mean = 0;
        double M2 = 0;
        double M3 = 0;
        double win_rate = 0;
        int n = 0;

        for (double pnl : pnl_per_run) {
            n++;

            if (pnl > 0) win_rate += 1.0 / pnl_per_run.size();

            double delta = pnl - mean;
            double delta_n = delta / n;
            double delta_n2 = delta_n * delta_n;

            M3 += delta * delta_n2 * (n - 1) * (n - 2) - 3 * delta_n * M2;
            M2 += delta * (pnl - (mean + delta_n));
            mean += delta_n;
        }

        double std = (n > 1)? sqrt(M2 / (n - 1)) : 0;
        double skew = (n > 2)? (sqrt(n * (n - 1)) / (n - 2)) * (M3 / (n * pow(std, 3))) : 0;
        return {mean, std, skew, win_rate};
    }

    double get_percentile(double percentile) {
        if (pnl_per_run.empty()) return 0.0;

        double rank = percentile * (pnl_per_run.size() - 1);
        size_t lower = static_cast<size_t>(rank);
        size_t upper = std::min(lower + 1, pnl_per_run.size() - 1);
        double weight = rank - lower;

        return pnl_per_run[lower] * (1.0 - weight) + pnl_per_run[upper] * weight;
    }

    double calculate_cvar(double percentile_value) {
        if (pnl_per_run.empty()) return 0.0;

        auto percentile_it = std::ranges::upper_bound(pnl_per_run, percentile_value);
        auto count = std::ranges::distance(pnl_per_run.begin(), percentile_it);
        if (count == 0) return 0.0;
        auto sum = std::reduce(std::execution::par_unseq, pnl_per_run.begin(), percentile_it, 0.0);
        return sum / count;
    }

private:
    std::vector<double> pnl_per_run;
    std::atomic<double> max_dd_sum;
    std::atomic<int> position_sum;
    std::atomic<double> slippage_sum;
    std::atomic<double> fill_rate_sum;
};