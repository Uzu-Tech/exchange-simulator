#include "sim_runner.hpp"

#include <execution>

#include "sim_builder.hpp"  
#include "printer.hpp"
#include "metrics.hpp"

void SimRunner::single_run(SimInfo info, Config& config, SingleRunLogger* logger) {
    auto sim = SimBuilder::build_sim(info, config, logger);
    auto results = sim.run();
    Printer::print_single_run_results(results, info, logger);
}

void SimRunner::monte_carlo(const SimInfo& info, Config& config, size_t num_runs, bool verbose, MonteCarloLogger* logger) {
    MonteCarloCollector mc_collector(num_runs);
    pcg32 master_gen(info.seed);

    std::vector<size_t> indices(num_runs);
    std::iota(indices.begin(), indices.end(), 0);

    if (verbose) {
        std::for_each(indices.begin(), indices.end(), [&](size_t idx) {
            auto seed = fast_seed(master_gen);
            auto sim = SimBuilder::build_sim({info.num_ticks, seed, info.position_limit}, config);
            auto results = sim.run();
            Printer::print_single_run_results(results, info, nullptr);
            mc_collector.update(results, idx, seed, logger);
        });
    } else {
        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t idx) {
            auto seed = fast_seed(master_gen);
            auto sim = SimBuilder::build_sim({info.num_ticks, seed, info.position_limit}, config);
            auto results = sim.run();
            mc_collector.update(results, idx, seed, logger);
        });
    }

    Printer::print_monte_carlo_results(mc_collector.results(), info);
}