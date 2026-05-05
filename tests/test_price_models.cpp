#include <gtest/gtest.h>
#include "market_price.hpp" 

// Compile-time check
static_assert(PriceModel<SimpleRandomWalk>, "SimpleRandomWalk must satisfy PriceModel");
static_assert(PriceModel<GaussianRandomWalk>, "GaussianRandomWalk must satisfy PriceModel");

TEST(PriceModelTest, SimpleRandomWalkBasic) {
    std::mt19937_64 gen(42); // Fixed seed for determinism
    double start = 100.0;
    double step = 1.0;
    float p = 0.5;
    SimpleRandomWalk walk(start, p, step, gen);

    double next = walk.next_price();
    
    // It should either be 101.0 or 99.0
    EXPECT_TRUE(next == 101.0 || next == 99.0);
}

TEST(PriceModelTest, SimpleRandomWalkDirectional) {
    std::mt19937_64 gen(42);
    // Setting p = 1.0 means it MUST only go up
    SimpleRandomWalk walk(100.0, 1.0, 1.0, gen);

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(walk.next_price(), 100.0 + (i + 1));
    }
}

TEST(PriceModelTest, GaussianWalkDistribution) {
    std::mt19937_64 gen(12345);
    double start = 100.0;
    double drift = 0.5;
    double std_dev = 0.1;
    GaussianRandomWalk walk(start, drift, std_dev, gen);

    int iterations = 1000;
    double total_moved = 0;

    for (int i = 0; i < iterations; ++i) {
        double prev = (i == 0) ? start : total_moved + start; // tracking logic
        double current = walk.next_price();
        total_moved += (current - (i == 0 ? start : prev)); 
    }

    // After 1000 steps with 0.5 drift, we expect to be around 500 units up
    double expected_mean_move = drift * iterations;
    EXPECT_NEAR(total_moved, expected_mean_move, 10.0); // Within tolerance
}