#pragma once

#include <CLI/CLI.hpp>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "sim_builder.hpp"

namespace SimCLI {
struct SimInfo {
    uint64_t seed;
    size_t num_ticks;
    PositionLimit position_limit;
};

inline void apply_override(Config& root, const std::string& kv) {
    auto eq = kv.find('=');
    if (eq == std::string::npos)
        throw std::runtime_error("Override error: expected key=value, got '" + kv + "'");

    std::string key = kv.substr(0, eq);
    std::string value = kv.substr(eq + 1);

    if (key.empty()) throw std::runtime_error("Override error: empty key in '" + kv + "'");
    if (value.empty()) throw std::runtime_error("Override error: empty value in '" + kv + "'");

    auto dot = key.find('.');
    if (dot == std::string::npos)
        throw std::runtime_error("Override error: expected <section>.<field>, got '" + key + "'");

    std::string section = key.substr(0, dot);
    std::string field = key.substr(dot + 1);
    key = section + ".params." + field;

    // Tokenize key: "makers[0].params.half_spread" -> ["makers", "0", "params", "half_spread"]
    std::vector<std::string> tokens;
    std::string token;
    for (char c : key) {
        if (c == '.' || c == '[' || c == ']') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) tokens.push_back(token);

    if (tokens.empty())
        throw std::runtime_error("Override error: could not parse key '" + key + "'");

    Yaml::Node* node = &root;
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        node = &(*node)[tokens[i]];
        if (node->IsNone())
            throw std::runtime_error(
                "Override error: '" + key + "' does not exist at segment '" + tokens[i] + "'"
            );
    }
    (*node)[tokens.back()] = value;
}

// ---------------------------------------------------------------------------
// Print results to stdout
// ---------------------------------------------------------------------------
inline void print_results(const SimulatorResults& r, const SimInfo& info) {
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
        "{}{:<14}{:>12} | Seed:{:>4} | (Position Limit:{:>5})\n",
        tag("[Config]"),
        "Num Timestamps:",
        info.num_ticks,
        info.seed,
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
// ---------------------------------------------------------------------------
// Build and return a configured CLI::App
// Populates config_path and overrides via CLI11 parsing
// ---------------------------------------------------------------------------
inline int run(int argc, char** argv) {
    CLI::App app{"Market Simulator"};

    std::string config_path = "config.yml";
    app.add_option("--config", config_path, "Path to config.yml")->check(CLI::ExistingFile);

    std::vector<std::string> overrides;
    app.add_option(
           "--set",
           overrides,
           "Override a config field: --set <path>=<value>\n"
           "  e.g. --set simulation.num_timestamps=1000\n"
           "       --set makers[0].params.half_spread=15\n"
           "       --set price_model.params.step=2"
    )
        ->allow_extra_args(false);

    CLI11_PARSE(app, argc, argv);

    Yaml::Node root;
    Yaml::Parse(root, config_path.c_str());

    for (const auto& kv : overrides) {
        try {
            apply_override(root, kv);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    try {
        auto sim = SimBuilder::build_sim(root);
        auto results = sim.run();
        print_results(results, {sim.seed(), sim.num_timestamps(), sim.position_limit()});
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
}  // namespace SimCLI