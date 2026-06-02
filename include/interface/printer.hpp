#pragma once

#include <format>
#include <iostream>
#include <string_view>

#include "sim_info.hpp"
#include "metrics.hpp"

namespace Printer {
inline void print_single_run_results(const SimulatorResults& r, const SimInfo& info) {
    constexpr std::string_view GRN = "\033[32m";
    constexpr std::string_view RED = "\033[31m";
    constexpr std::string_view CYN = "\033[36m";
    constexpr std::string_view RST = "\033[0m";

    auto color = [&](double v, const std::string& padded_str) {
        if (v > 0) return std::format("{}{}{}", GRN, padded_str, RST);
        if (v < 0) return std::format("{}{}{}", RED, padded_str, RST);
        return padded_str;
    };

    auto tag = [&](std::string_view t) { return std::format("{}{:<12}{}", CYN, t, RST); };

    constexpr int WIDTH = 82;
    std::string border = std::string(WIDTH, '=') + "\n";

    std::cout << border;
    std::cout << std::format("{:^{}}\n", "Simulation Results", WIDTH);
    std::cout << border;

    // [Config]
    std::cout << std::format(
        "{}{:<14}{:>12} | Seed:{:>5} | (Position Limit:{:>5})\n",
        tag("[Config]"),
        "Num Ticks:",
        info.num_ticks,
        info.seed % 10000,
        info.position_limit.value()
    );

    std::cout << std::string(WIDTH, '-') + "\n";

    // [PnL]
    std::cout << std::format(
        "{}{:<14}{} | {:<15}{}  (Std: {:>6.2f})\n",
        tag("[PnL]"),
        "Total PnL:",
        color(r.pnl.total(), std::format("{:>10.2f}", r.pnl.total())),
        "Mean PnL:",
        color(r.pnl.mean(), std::format("{:>9.2f}", r.pnl.mean())),
        r.pnl.std_dev()
    );

    // [Fills]
    std::cout << std::format(
        "{}{:<14}{:>10} | {:<15}{:>9}  (Pct: {:>5.1f}%)\n",
        tag("[Fills]"),
        "Take Orders:",
        r.takes.count(),
        "Make Orders:",
        r.makes.count(),
        r.makes.percentage() * 100.0
    );

    // [Quality]
    std::cout << std::format(
        "{}{:<14}{:>10.4f} | {:<15}{:>9.2f}  (Std: {:>6.2f})\n",
        tag("[Quality]"),
        "Slippage:",
        r.slippage.mean(),
        "Fill Quality:",
        r.fill_quality.mean(),
        r.fill_quality.std_dev()
    );

    // [Risk]
    std::cout << std::format(
        "{}{:<14}{:>10.2f} | {:<15}{}  (Mean:{:>8.2f})\n",
        tag("[Risk]"),
        "Position:",
        r.position.mean(),
        "Max Drawdown:",
        color(-r.drawdown.max(), std::format("{:>9.2f}", -r.drawdown.max())),
        -r.drawdown.mean()
    );

    std::cout << border;
}
}  // namespace Printer

