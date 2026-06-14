#include "cli.hpp"

#include <algorithm>
#include <CLI/CLI.hpp>
#include <random>
#include <string>
#include <vector>
#include <filesystem>

#include "config.hpp"
#include "primitives.hpp"
#include "validator.hpp"
#include "sim_info.hpp"
#include "sim_runner.hpp"
#include "pcg_random.hpp"

namespace fs = std::filesystem;

namespace SimCLI {
inline void apply_override(Config& root, const std::string& kv) {
    auto eq = kv.find('=');
    if (eq == std::string::npos)
        throw std::runtime_error("Override error: expected key=value, got '" + kv + "'");

    std::string key = kv.substr(0, eq);
    std::string value = kv.substr(eq + 1);

    if (key.empty()) throw std::runtime_error("Override error: empty key in '" + kv + "'");
    if (value.empty()) throw std::runtime_error("Override error: empty value in '" + kv + "'");

    auto dot = key.find_first_of(".:");
    if (dot == std::string::npos)
        throw std::runtime_error("Override error: expected <section>.<field>, got '" + key + "'");

    std::string section = key.substr(0, dot);
    std::string field = key.substr(dot + 1);

    // Check if field starts with an index (e.g. "0:trade_prob")
    auto next_sep = field.find_first_of(".:");
    std::string first_field = (next_sep != std::string::npos) ? field.substr(0, next_sep) : field;
    bool is_indexed =
        !first_field.empty() && std::all_of(first_field.begin(), first_field.end(), ::isdigit);

    if (!(section == "simulation") && !(section == "monte_carlo")) {
        if (is_indexed) {
            // takers:0:trade_prob -> takers.0.params.trade_prob
            std::string index = first_field;
            std::string rest = field.substr(next_sep + 1);
            key = section + "." + index + ".params." + rest;
        } else {
            // price_model:step -> price_model.params.step
            key = section + ".params." + field;
        }
    }

    std::vector<std::string> tokens;
    std::string token;
    for (char c : key) {
        if (c == '.' || c == ':') {
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
        bool is_index = std::all_of(tokens[i].begin(), tokens[i].end(), ::isdigit);
        if (is_index) {
            node = &(*node)[std::stoi(tokens[i])];  // integer index for arrays
        } else {
            node = &(*node)[tokens[i]];  // string key for maps
        }
        if (node->IsNone())
            throw std::runtime_error(
                "Override error: '" + key + "' does not exist at segment '" + tokens[i] + "'"
            );
    }
    (*node)[tokens.back()] = value;
}

inline SimInfo get_sim_info(
    Config& root,
    size_t cli_num_ticks,
    uint64_t cli_seed,
    PositionLimit::Underlying cli_position_limit
) {
    auto num_ticks = cli_num_ticks;
    auto seed = cli_seed;
    auto position_limit = PositionLimit{cli_position_limit};

    if (cli_num_ticks == 0) {
        num_ticks = as_type<size_t>(root["num_ticks"]);
    }

    if (cli_position_limit == 0) {
        position_limit = as_type<PositionLimit>(root["position_limit"]);
    }

    if (seed != 0) return {num_ticks, seed, position_limit};

    auto& seed_node = root["seed"];
    bool use_random_seed = seed_node.IsNone();

    if (!use_random_seed) {
        try {
            uint64_t parsed = seed_node.As<uint64_t>();
            use_random_seed = (parsed == 0);
            if (!use_random_seed) seed = parsed;
        } catch (...) {
            use_random_seed = true;
        }
    }

    if (use_random_seed) {
        pcg_extras::seed_seq_from<std::random_device> seed_source;
        seed = pcg_extras::generate_one<uint64_t>(seed_source);
    }

    return {num_ticks, seed, position_limit};
}

// ---------------------------------------------------------------------------
// Build and return a configured CLI::App
// ---------------------------------------------------------------------------
int run(int argc, char** argv) {
    CLI::App app{"Market Simulator"};

    std::string config_path = "config.yml";
    app.add_option("--config", config_path, "Path to config.yml")->check(CLI::ExistingFile);

    Tick::Underlying num_ticks{};
    uint64_t seed{};
    PositionLimit::Underlying position_limit{};
    size_t num_runs = 1;
    bool verbose = false;
    std::string log_dir{};
    std::string clean_dir{};

    app.add_option("--num_ticks", num_ticks, "Number of ticks in single simulator run")
        ->check(CLI::PositiveNumber);
    app.add_option(
           "--position_limit",
           position_limit,
           "Limit to how much of the security you can hold or own at any time"
    )
        ->check(CLI::PositiveNumber);
    app.add_option("--seed", seed, "Seed for random generator, used for reproducible runs")
        ->check(CLI::PositiveNumber);
    app.add_option("--runs", num_runs, "Number of Monte Carlo runs (default: 1)")
        ->check(CLI::PositiveNumber);
    app.add_flag("--verbose", verbose, "Print results for every individual run");
    app.add_option("--out", log_dir, "Directory to place the data for a run, saved as three binary .npy files");
    app.add_flag("--clean_logs", log_dir, "Print results for every individual run");

    std::vector<std::string> overrides;
    app.add_option(
           "--set",
           overrides,
           "Override a config field: --set <path>=<value>\n"
           "  e.g. --set simulation.num_ticks=1000\n"
           "       --set makers.0.half_spread=15\n"
           "       --set price_model.step=2"
    )
        ->allow_extra_args(false);

    CLI11_PARSE(app, argc, argv);

    Yaml::Node root;
    Yaml::Parse(root, config_path.c_str());

    Validator::validate_config(root);
    SimInfo info = get_sim_info(root, num_ticks, seed, position_limit);

    // Fill from config if CLI not specified
    if (num_runs == 1 && !root["runs"].IsNone()) {
        num_runs = as_type<size_t>(root["runs"]);
    }

    if (log_dir.empty() && !root["log_dir"].IsNone()) {
        log_dir = as_type<std::string>(root["log_dir"]);
    }

    std::unique_ptr<SingleRunLogger> single_logger{};
    std::unique_ptr<MonteCarloLogger> mc_logger{};
    if (!log_dir.empty()) {
        // Patch root with the final resolved values so the saved config is fully reproducible
        root["runs"] = std::to_string(num_runs);
        root["log_dir"] = log_dir;
        root["num_ticks"] = std::to_string(info.num_ticks);
        root["seed"] = std::to_string(info.seed);
        root["position_limit"] = std::to_string(info.position_limit.value());

        // Now serialize and construct Logger
        std::string config_contents;
        Yaml::Serialize(root, config_contents);

        if (num_runs >= 1) {
            mc_logger = std::make_unique<MonteCarloLogger>(
                fs::path(log_dir), config_contents, fs::path(config_path), num_runs
            );
        } else {
            single_logger = std::make_unique<SingleRunLogger>(
                fs::path(log_dir), config_contents, fs::path(config_path), info.num_ticks
            );
        }
    }

    for (const auto& kv : overrides) {
        try {
            apply_override(root, kv);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    try {
        if (num_runs > 1) {
            SimRunner::monte_carlo(info, root, num_runs, verbose, mc_logger.get());
        } else {
            SimRunner::single_run(info, root, single_logger.get());
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
}  // namespace SimCLI