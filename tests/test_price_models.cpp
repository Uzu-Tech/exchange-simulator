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
    SimpleRandomWalk walk{100.0, 0.5, 1.0, rng.make_child()};

    walk.next_price();
    double price = walk.current_price();

    EXPECT_TRUE(price == 101.0 || price == 99.0);
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysUp) {
    RandomEngine rng{42};
    SimpleRandomWalk walk{100.0, 1.0, 1.0, rng.make_child()};

    for (int i = 0; i < 10; ++i) {
        walk.next_price();
        EXPECT_EQ(walk.current_price(), 100.0 + (i + 1));
    }
}

TEST(PriceModelTest, SimpleRandomWalkAlwaysDown) {
    RandomEngine rng{42};
    SimpleRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};

    for (int i = 0; i < 10; ++i) {
        walk.next_price();
        EXPECT_EQ(walk.current_price(), 100.0 - (i + 1));
    }
}

TEST(PriceModelTest, SimpleRandomWalkZeroStepStaysFlat) {
    RandomEngine rng{42};
    SimpleRandomWalk walk{100.0, 0.5, 0.0, rng.make_child()};

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

    GaussianRandomWalk walk{start, drift, std_dev, rng.make_child()};

    for (int i = 0; i < iterations; ++i)
        walk.next_price();

    double total_move    = walk.current_price() - start;
    double expected_move = drift * iterations;

    // std error ~ 0.1*sqrt(1000) ~ 3.16, tolerance of 15 is ~5 std errors
    EXPECT_NEAR(total_move, expected_move, 15.0);
}

TEST(PriceModelTest, GaussianWalkZeroDriftStaysNearStart) {
    RandomEngine rng{42};
    GaussianRandomWalk walk{100.0, 0.0, 0.001, rng.make_child()};

    for (int i = 0; i < 1000; ++i)
        walk.next_price();

    // With tiny std_dev and zero drift, price should stay very close to start
    EXPECT_NEAR(walk.current_price(), 100.0, 1.0);
}

// ---------------------------------------------------------------------------
// Reproducibility — applies to both models
// ---------------------------------------------------------------------------
TEST(PriceModelTest, SimpleWalkReproducibleWithSameSeed) {
    double p1, p2;

    {
        RandomEngine rng{99};
        SimpleRandomWalk walk{100.0, 0.5, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p1 = walk.current_price();
    }
    {
        RandomEngine rng{99};
        SimpleRandomWalk walk{100.0, 0.5, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p2 = walk.current_price();
    }

    EXPECT_EQ(p1, p2);
}

TEST(PriceModelTest, GaussianWalkReproducibleWithSameSeed) {
    double p1, p2;

    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p1 = walk.current_price();
    }
    {
        RandomEngine rng{99};
        GaussianRandomWalk walk{100.0, 0.0, 1.0, rng.make_child()};
        for (int i = 0; i < 100; ++i) walk.next_price();
        p2 = walk.current_price();
    }

    EXPECT_EQ(p1, p2);
}

TEST(PriceModelTest, DifferentStreamsAreIndependent) {
    RandomEngine rng{42};
    GaussianRandomWalk walk1{100.0, 0.0, 1.0, rng.make_child()};
    GaussianRandomWalk walk2{100.0, 0.0, 1.0, rng.make_child()};

    walk1.next_price();
    walk2.next_price();

    EXPECT_NE(walk1.current_price(), walk2.current_price());
}
