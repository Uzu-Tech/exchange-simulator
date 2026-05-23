#include <gtest/gtest.h>
#include "sim_price.hpp"
#include "simulator.hpp"
#include "rng.hpp"
#include "pure_market_maker.hpp"

using TestSim = Simulator<
    TypeList<SymmetricMaker>,
    TypeList<RandomTaker>
>;

static TestSim make_simulator(size_t num_ticks = 1000, uint64_t seed = 42) {
    RandomEngine rng{seed};

    SimpleRandomWalk::ConfigParams walk_params{};
    walk_params.start_price = 10'000.0;
    walk_params.p           = 0.5;
    walk_params.step        = 1.0;

    PureMarketMaker::ConfigParams strat_params{};
    strat_params.offset = PriceDelta{1};

    SymmetricMaker::ConfigParams maker_params{};
    maker_params.half_spread = PriceDelta{2};
    maker_params.min_volume  = Volume{5};
    maker_params.max_volume  = Volume{10};

    RandomTaker::ConfigParams taker_params{};
    taker_params.trade_prob = 0.05;
    taker_params.min_volume = Volume{1};
    taker_params.max_volume = Volume{5};
    
    return TestSim{
        seed,
        num_ticks,
        PositionLimit{100},
        SimpleRandomWalk{rng.make_child(), walk_params},
        PureMarketMaker{strat_params},
        std::make_tuple(SymmetricMaker{TraderId{1}, rng.make_child(), maker_params}),
        std::make_tuple(RandomTaker{TraderId{2}, rng.make_child(), taker_params})
    };
}

TEST(SimulatorRunTest, RunsWithoutCrashing) {
    make_simulator(1000).run();
    SUCCEED();
}

TEST(SimulatorRunTest, ZeroTicksProducesZeroResults) {
    auto results = make_simulator(0).run();

    EXPECT_EQ(results.takes.count(), 0);
    EXPECT_EQ(results.makes.count(), 0);
    EXPECT_EQ(results.pnl.total(),   0.0);
}

TEST(SimulatorRunTest, ReproducibleWithSameSeed) {
    auto r1 = make_simulator(100, 42).run();
    auto r2 = make_simulator(100, 42).run();

    EXPECT_EQ(r1.pnl.total(),     r2.pnl.total());
    EXPECT_EQ(r1.slippage.mean(), r2.slippage.mean());
    EXPECT_EQ(r1.position.mean(), r2.position.mean());
    EXPECT_EQ(r1.takes.count(),   r2.takes.count());
    EXPECT_EQ(r1.makes.count(),   r2.makes.count());
}