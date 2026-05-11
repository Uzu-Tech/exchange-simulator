#include <gtest/gtest.h>
#include "sim_price.hpp"
#include "rng.hpp"

// Compile-time checks — these are already in sim_price.hpp but kept here
// as documentation that these types are used as PriceModels in tests.
static_assert(PriceModel<SimpleRandomWalk>,   "SimpleRandomWalk must satisfy PriceModel");
static_assert(PriceModel<GaussianRandomWalk>, "GaussianRandomWalk must satisfy PriceModel");

TEST(PriceModelTest, SimpleRandomWalkMovesOneStep) {
    RandomEngine rng{42};
    SimpleRandomWalk walk{100.0, 0.5, 1.0, rng.make_child()};

    double next = walk.next_price();

    // With step=1.0 the price must move exactly one step in either direction
    EXPECT_TRUE(next == 101.0 || next == 99.0);
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysUp) {
    RandomEngine rng{42};
    // p=1.0 means every step goes up
    SimpleRandomWalk walk{100.0, 1.0, 1.0, rng.make_child()};

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(walk.next_price(), 100.0 + (i + 1));
    }
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysDown) {
    RandomEngine rng{42};
    // p=0.0 means every step goes down
    SimpleRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(walk.next_price(), 100.0 - (i + 1));
    }
}

TEST(PriceModelTest, GaussianWalkTracksExpectedDrift) {
    RandomEngine rng{12345};
    double start      = 100.0;
    double drift      = 0.5;
    double std_dev    = 0.1;
    int    iterations = 1000;

    GaussianRandomWalk walk{start, drift, std_dev, rng.make_child()};

    // Run the walk and read final price directly — no manual tracking needed
    for (int i = 0; i < iterations; ++i)
        walk.next_price();

    double total_move    = walk.current_price() - start;
    double expected_move = drift * iterations;

    // With std_dev=0.1 over 1000 steps, std error ~ 0.1*sqrt(1000) ~ 3.16
    // Tolerance of 15 is ~5 standard errors — extremely unlikely to fail
    EXPECT_NEAR(total_move, expected_move, 15.0);
}

TEST(PriceModelTest, ReproducibleWithSameSeed) {
    double price1, price2;

    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) price1 = walk.next_price();
    }
    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) price2 = walk.next_price();
    }

    EXPECT_EQ(price1, price2);
}

TEST(PriceModelTest, DifferentStreamsAreIndependent) {
    RandomEngine rng{42};
    GaussianRandomWalk walk1{100.0, 0.0, 1.0, rng.make_child()};
    GaussianRandomWalk walk2{100.0, 0.0, 1.0, rng.make_child()};

    // Two children from the same master should produce different sequences
    EXPECT_NE(walk1.next_price(), walk2.next_price());
}