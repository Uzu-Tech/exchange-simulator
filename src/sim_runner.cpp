#include "sim_runner.hpp"

#include <execution>

#include "sim_builder.hpp"  // active_types pulls in here, not into cli.cpp
#include "printer.hpp"

void SimRunner::single_run(SimInfo info, Config& config, Logger* logger) {
    auto sim = SimBuilder::build_sim(info, config, logger);
    auto results = sim.run();
    Printer::print_single_run_results(results, info);
}

void SimRunner::monte_carlo(const SimInfo& info, Config& config, size_t num_runs, bool verbose) {
    pcg32 master_gen(info.seed);

    std::vector<size_t> indices(num_runs);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t idx) {
        auto seed = fast_seed(master_gen);
        auto sim = SimBuilder::build_sim({info.num_ticks, seed, info.position_limit}, config);
        sim.run();
    });

    std::cout << "All Done\n";
}