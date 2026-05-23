#include <gtest/gtest.h>
#include <filesystem>
#include <stdexcept>
#include "sim_builder.hpp"
#include <fstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build and parse a YAML string directly — no file needed
Yaml::Node parse_yaml(const std::string& yaml) {
    auto tmp = std::filesystem::temp_directory_path() / "test_config.yml";
    std::ofstream(tmp) << yaml;
    Yaml::Node root;
    Yaml::Parse(root, tmp.string().c_str());
    return root;
}

// Minimal valid config as a string — tests can override individual fields
std::string valid_config() {
    return R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100

strategy:
  type: "PureMarketMaker"
  params:
    offset: 1

price_model:
  type: "SimpleRandomWalk"
  params:
    start_price: 10000
    p: 0.5
    step: 1

makers:
  - type: "SymmetricMaker"
    params:
      half_spread: 10
      min_volume: 10
      max_volume: 30
  - type: "SymmetricMaker"
    params:
      half_spread: 8
      min_volume: 1
      max_volume: 10

takers:
  - type: "RandomTaker"
    params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 10
)";
}

// ---------------------------------------------------------------------------
// Suite 1: Config validation — bad configs must throw clearly
// ---------------------------------------------------------------------------
class SimBuilderValidationTest : public ::testing::Test {};

TEST(SimBuilderValidationTest, ThrowsOnMissingSimulationBlock) {
    auto root = parse_yaml(R"(
num_timestamps: 10
position_limit: 100
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnMissingNumTimestamps) {
    auto root = parse_yaml(R"(
simulation:
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnZeroNumTimestamps) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 0
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnZeroPositionLimit) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 0
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnMissingPriceModel) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnMissingMakers) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnMissingTakers) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnMakerMissingParams) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - type: "SymmetricMaker"
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnTakerMissingParams) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: 100
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - type: "RandomTaker"
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ThrowsOnInvalidParamType) {
    auto root = parse_yaml(R"(
simulation:
  num_timestamps: 10
  seed: 42
  position_limit: abc
price_model:
  params:
    start_price: 10000
    p: 0.5
    step: 1
strategy:
  params:
    offset: 1
makers:
  - params:
      half_spread: 10
      min_volume: 1
      max_volume: 5
takers:
  - params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 5
)");
    EXPECT_THROW(SimBuilder::build_sim(root), std::runtime_error);
}

TEST(SimBuilderValidationTest, ValidConfigDoesNotThrow) {
    auto root = parse_yaml(valid_config());
    EXPECT_NO_THROW(SimBuilder::build_sim(root));
}

// ---------------------------------------------------------------------------
// Suite 2: Determinism — same seed must produce identical results
// ---------------------------------------------------------------------------
class SimBuilderDeterminismTest : public ::testing::Test {
protected:
    SimulatorResults run_once() {
        auto root = parse_yaml(valid_config());
        return SimBuilder::build_sim(root).run();
    }
};

TEST_F(SimBuilderDeterminismTest, IdenticalSeedProducesIdenticalPnl) {
    auto r1 = run_once();
    auto r2 = run_once();
    EXPECT_DOUBLE_EQ(r1.pnl.mean(),     r2.pnl.mean());
    EXPECT_DOUBLE_EQ(r1.pnl.variance(), r2.pnl.variance());
    EXPECT_DOUBLE_EQ(r1.pnl.total(),    r2.pnl.total());
}

TEST_F(SimBuilderDeterminismTest, IdenticalSeedProducesIdenticalCounts) {
    auto r1 = run_once();
    auto r2 = run_once();
    EXPECT_EQ(r1.takes.count(), r2.takes.count());
    EXPECT_EQ(r1.makes.count(), r2.makes.count());
}

TEST_F(SimBuilderDeterminismTest, IdenticalSeedProducesIdenticalPosition) {
    auto r1 = run_once();
    auto r2 = run_once();
    EXPECT_DOUBLE_EQ(r1.position.mean(),     r2.position.mean());
    EXPECT_DOUBLE_EQ(r1.position.variance(), r2.position.variance());
}

TEST_F(SimBuilderDeterminismTest, DifferentSeedProducesDifferentResults) {
    auto root1 = parse_yaml(valid_config());
    auto root2 = parse_yaml(R"(
simulation:
  num_timestamps: 1000
  seed: 999
  position_limit: 100
strategy:
  type: "PureMarketMaker"
  params:
    offset: 1
price_model:
  type: "SimpleRandomWalk"
  params:
    start_price: 10000
    p: 0.5
    step: 1
makers:
  - type: "SymmetricMaker"
    params:
      half_spread: 10
      min_volume: 10
      max_volume: 30
  - type: "SymmetricMaker"
    params:
      half_spread: 8
      min_volume: 1
      max_volume: 10
takers:
  - type: "RandomTaker"
    params:
      trade_prob: 0.01
      min_volume: 1
      max_volume: 10
)");
    auto r1 = SimBuilder::build_sim(root1).run();
    auto r2 = SimBuilder::build_sim(root2).run();
    // With different seeds, at least one metric should differ
    EXPECT_FALSE(
        r1.pnl.total()    == r2.pnl.total() &&
        r1.takes.count()  == r2.takes.count() &&
        r1.position.mean() == r2.position.mean()
    );
}

// ---------------------------------------------------------------------------
// Suite 3: Result invariants — things that must always be true
// ---------------------------------------------------------------------------
class SimBuilderResultInvariantsTest : public ::testing::Test {
protected:
    SimulatorResults results;
    void SetUp() override {
        auto root = parse_yaml(valid_config());
        results = SimBuilder::build_sim(root).run();
    }
};

TEST_F(SimBuilderResultInvariantsTest, MakeAndTakeCountsAreNonNegative) {
    EXPECT_GE(results.takes.count(), 0);
    EXPECT_GE(results.makes.count(), 0);
}

TEST_F(SimBuilderResultInvariantsTest, MakeAndTakePercentagesSumToOne) {
    // Every strategy order is recorded as either a make or a take
    double sum = results.takes.percentage() + results.makes.percentage();
    EXPECT_NEAR(sum, 1.0, 1e-9);
}

TEST_F(SimBuilderResultInvariantsTest, DrawdownIsNonNegative) {
    // Max drawdown normalised by volatility cannot be negative
    EXPECT_GE(results.drawdown.max(),  0.0);
    EXPECT_GE(results.drawdown.mean(), 0.0);
}

TEST_F(SimBuilderResultInvariantsTest, PositionStaysWithinLimit) {
    // Mean position must be within [-limit, +limit] = [-100, 100]
    EXPECT_LE(std::abs(results.position.mean()), 100.0);
}

TEST_F(SimBuilderResultInvariantsTest, PnlVarianceIsNonNegative) {
    EXPECT_GE(results.pnl.variance(), 0.0);
}

TEST_F(SimBuilderResultInvariantsTest, SlippageVarianceIsNonNegative) {
    EXPECT_GE(results.slippage.variance(), 0.0);
}