#include <gtest/gtest.h>
#include "sim_price.hpp"
#include "rng.hpp"

static_assert(PriceModel<SimpleRandomWalk>,   "SimpleRandomWalk must satisfy PriceModel");
static_assert(PriceModel<GaussianRandomWalk>, "GaussianRandomWalk must satisfy PriceModel");

// ---------------------------------------------------------------------------
// SimpleRandomWalk
// ---------------------------------------------------------------------------
TEST(PriceModelTest, SimpleRandomWalkMovesOneStep) {
    RandomEngine rng{42};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.p           = 0.5;
    params.step        = 1.0;

    SimpleRandomWalk walk{rng.make_child(), params};
    walk.next_price();
    double price = walk.current_price();

    EXPECT_TRUE(price == 101.0 || price == 99.0);
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysUp) {
    RandomEngine rng{42};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.p           = 1.0;
    params.step        = 1.0;

    SimpleRandomWalk walk{rng.make_child(), params};
    for (int i = 0; i < 10; ++i) {
        walk.next_price();
        EXPECT_EQ(walk.current_price(), 100.0 + (i + 1));
    }
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysDown) {
    RandomEngine rng{42};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.p           = 0.0;
    params.step        = 1.0;

    SimpleRandomWalk walk{rng.make_child(), params};
    for (int i = 0; i < 10; ++i) {
        walk.next_price();
        EXPECT_EQ(walk.current_price(), 100.0 - (i + 1));
    }
}

TEST(PriceModelTest, SimpleRandomWalkZeroStepStaysFlat) {
    RandomEngine rng{42};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.p           = 0.5;
    params.step        = 0.0;

    SimpleRandomWalk walk{rng.make_child(), params};
    for (int i = 0; i < 50; ++i) {
        walk.next_price();
        EXPECT_EQ(walk.current_price(), 100.0);
    }
}

// ---------------------------------------------------------------------------
// GaussianRandomWalk
// ---------------------------------------------------------------------------
TEST(PriceModelTest, GaussianWalkTracksExpectedDrift) {
    RandomEngine rng{12345};
    double start      = 100.0;
    double drift      = 0.5;
    double std_dev    = 0.1;
    int    iterations = 1000;

    GaussianRandomWalk::ConfigParams params{};
    params.start_price = start;
    params.drift       = drift;
    params.std_dev     = std_dev;

    GaussianRandomWalk walk{rng.make_child(), params};
    for (int i = 0; i < iterations; ++i)
        walk.next_price();

    double total_move    = walk.current_price() - start;
    double expected_move = drift * iterations;

    EXPECT_NEAR(total_move, expected_move, 15.0);
}

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------
TEST(PriceModelTest, SimpleWalkReproducibleWithSameSeed) {
    double p1, p2;
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.p           = 0.5;
    params.step        = 1.0;

    {
        RandomEngine rng{99};
        SimpleRandomWalk walk{rng.make_child(), params};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p1 = walk.current_price();
    }
    {
        RandomEngine rng{99};
        SimpleRandomWalk walk{rng.make_child(), params};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p2 = walk.current_price();
    }

    EXPECT_EQ(p1, p2);
}

TEST(PriceModelTest, GaussianWalkReproducibleWithSameSeed) {
    double p1, p2;
    GaussianRandomWalk::ConfigParams params{};
    params.start_price = 100.0;
    params.drift       = 0.0;
    params.std_dev     = 1.0;

    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{rng.make_child(), params};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p1 = walk.current_price();
    }
    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{rng.make_child(), params};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p2 = walk.current_price();
    }

    EXPECT_EQ(p1, p2);
}