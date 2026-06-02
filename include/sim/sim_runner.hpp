#pragma once
#include "sim_info.hpp"
#include "config.hpp"
#include "logger.hpp"

namespace SimRunner {
    void single_run(SimInfo info, Config& config, Logger* logger);
    void monte_carlo(
        const SimInfo& info,
        Config& config,
        size_t num_runs,
        bool verbose
    );
}