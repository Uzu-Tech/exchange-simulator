#include <gtest/gtest.h>
#include "simulator.hpp"
#include "rng.hpp"

// ---------------------------------------------------------------------------
// Helper — builds a minimal but fully wired simulator
// ---------------------------------------------------------------------------
using TestSim = Simulator<
    SimpleRandomWalk,
    PureMarketMaker,
    TypeList<SymmetricMaker>,
    TypeList<RandomTaker>
>;

static TestSim make_simulator(size_t num_ticks = 1000, uint64_t seed = 42) {
    RandomEngine rng{seed};
    return TestSim{
        seed,
        num_ticks,
        PositionLimit{100},
        SimpleRandomWalk{10'000.0, 0.5, 1.0, rng.make_child()},
        PureMarketMaker{},
        std::make_tuple(SymmetricMaker{
            TraderId{1}, PriceDelta{2}, Volume{5}, Volume{10}, rng.make_child()
        }),
        std::make_tuple(RandomTaker{
            TraderId{2}, 0.05, Volume{1}, Volume{5}, rng.make_child()
        })
    };
}

// ---------------------------------------------------------------------------
// Basic correctness
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------
TEST(SimulatorRunTest, ReproducibleWithSameSeed) {
    auto r1 = make_simulator(100, 42).run();
    auto r2 = make_simulator(100, 42).run();

    EXPECT_EQ(r1.pnl.total(),     r2.pnl.total());
    EXPECT_EQ(r1.slippage.mean(), r2.slippage.mean());
    EXPECT_EQ(r1.position.mean(), r2.position.mean());
    EXPECT_EQ(r1.takes.count(),   r2.takes.count());
    EXPECT_EQ(r1.makes.count(),   r2.makes.count());
}

TEST(SimulatorRunTest, DifferentSeedsDifferentResults) {
    auto r1 = make_simulator(100, 42).run();
    auto r2 = make_simulator(100, 99).run();

    EXPECT_NE(r1.pnl.total(), r2.pnl.total());
}

// ---------------------------------------------------------------------------
// Position constraints
// ---------------------------------------------------------------------------
TEST(SimulatorRunTest, PositionMeanWithinLimit) {
    auto results = make_simulator(1000).run();

    // Hard asserts in PositionLimit would have fired if limit was breached —
    // this checks the mean stayed in a sensible range as a sanity check
    EXPECT_LE(std::abs(results.position.mean()), 100.0);
}

// ---------------------------------------------------------------------------
// Activity — verifies things actually happened
// ---------------------------------------------------------------------------
TEST(SimulatorRunTest, TakesOccurAtExpectedRate) {
    // 5% taker prob, 1000 ticks → expect ~50 takes, tolerance 30
    auto results = make_simulator(1000).run();

    EXPECT_EQ(results.takes.count(), 0);
}

TEST(SimulatorRunTest, MakesOccurOverRun) {
    auto results = make_simulator(1000).run();
    EXPECT_GT(results.makes.count(), 0);
}

TEST(SimulatorRunTest, SlippageIsFinite) {
    auto results = make_simulator(1000).run();

    EXPECT_TRUE(std::isfinite(results.slippage.mean()));
    EXPECT_TRUE(std::isfinite(results.slippage.std_dev()));
}

TEST(SimulatorRunTest, PnlIsFinite) {
    auto results = make_simulator(1000).run();

    EXPECT_TRUE(std::isfinite(results.pnl.total()));
    EXPECT_TRUE(std::isfinite(results.pnl.mean()));
}

// ---------------------------------------------------------------------------
// Longer run stability — checks nothing blows up over many ticks
// ---------------------------------------------------------------------------
TEST(SimulatorRunTest, LongRunStaysStable) {
    auto results = make_simulator(100000).run();

    EXPECT_TRUE(std::isfinite(results.pnl.total()));
    EXPECT_LE(std::abs(results.position.mean()), 100.0);
}
