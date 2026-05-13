#include <gtest/gtest.h>
#include "taker.hpp"
#include "rng.hpp"

static_assert(Taker<RandomTaker>, "RandomTaker must satisfy Taker");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static OrderBook make_book() {
    OrderBook book;
    auto ask = OrderRequest::limit(TraderId{99}, Price{100}, Volume{100}, Side::ASK);
    auto bid = OrderRequest::limit(TraderId{99}, Price{90},  Volume{100}, Side::BID);
    book.add_order(ask);
    book.add_order(bid);
    return book;
}

static RandomTaker make_taker(
    double   trade_prob = 1.0,   // default fires every tick for deterministic tests
    uint32_t min_vol    = 5,
    uint32_t max_vol    = 10,
    uint64_t seed       = 42,
    uint64_t trader_id  = 1
) {
    RandomEngine rng{seed};
    return RandomTaker{
        TraderId{trader_id},
        trade_prob,
        Volume{min_vol},
        Volume{max_vol},
        rng.make_child()
    };
}

// ---------------------------------------------------------------------------
// Basic behaviour
// ---------------------------------------------------------------------------
TEST(RandomTakerTest, AlwaysFiresAtProbOne) {
    auto book  = make_book();
    auto taker = make_taker(1.0);

    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(taker.get_order(book, 100.0).has_value());
    }
}

TEST(RandomTakerTest, NeverFiresAtProbZero) {
    auto book  = make_book();
    auto taker = make_taker(0.0);

    for (int i = 0; i < 50; ++i) {
        EXPECT_FALSE(taker.get_order(book, 100.0).has_value());
    }
}

TEST(RandomTakerTest, ReturnsMarketOrder) {
    auto book  = make_book();
    auto taker = make_taker(1.0);
    auto order = taker.get_order(book, 100.0);

    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->type, OrderType::MARKET);
}

TEST(RandomTakerTest, VolumeWithinBounds) {
    auto book  = make_book();
    auto taker = make_taker(1.0, 5, 10);

    for (int i = 0; i < 100; ++i) {
        auto order = taker.get_order(book, 100.0);
        ASSERT_TRUE(order.has_value());
        EXPECT_TRUE(order->volume >= Volume{5} && order->volume <= Volume{10});
    }
}

TEST(RandomTakerTest, ProducesBothSides) {
    auto book  = make_book();
    auto taker = make_taker(1.0, 5, 10, 42);

    bool saw_bid = false;
    bool saw_ask = false;

    for (int i = 0; i < 100; ++i) {
        auto order = taker.get_order(book, 100.0);
        ASSERT_TRUE(order.has_value());
        if (order->side == Side::BID) saw_bid = true;
        if (order->side == Side::ASK) saw_ask = true;
        if (saw_bid && saw_ask) break;
    }

    EXPECT_TRUE(saw_bid);
    EXPECT_TRUE(saw_ask);
}

// ---------------------------------------------------------------------------
// Statistical property — fire rate approximates trade_prob
// ---------------------------------------------------------------------------
TEST(RandomTakerTest, FireRateApproximatesProbability) {
    auto book  = make_book();
    auto taker = make_taker(0.1, 5, 10, 42);

    int fires  = 0;
    int trials = 10000;

    for (int i = 0; i < trials; ++i) {
        if (taker.get_order(book, 100.0).has_value()) fires++;
    }

    double rate = static_cast<double>(fires) / trials;

    // Expected 10%, tolerance of 2% = ~6 standard errors at n=10000
    EXPECT_NEAR(rate, 0.1, 0.02);
}

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------
TEST(RandomTakerTest, ReproducibleWithSameSeed) {
    auto book = make_book();

    std::vector<bool> run1, run2;

    {
        auto taker = make_taker(0.5, 5, 10, 42);
        for (int i = 0; i < 100; ++i)
            run1.push_back(taker.get_order(book, 100.0).has_value());
    }
    {
        auto taker = make_taker(0.5, 5, 10, 42);
        for (int i = 0; i < 100; ++i)
            run2.push_back(taker.get_order(book, 100.0).has_value());
    }

    EXPECT_EQ(run1, run2);
}

TEST(RandomTakerTest, DifferentSeedsProduceDifferentSequences) {
    auto book = make_book();

    bool diverged = false;
    auto taker1 = make_taker(0.5, 5, 10, 42);
    auto taker2 = make_taker(0.5, 5, 10, 99);

    for (int i = 0; i < 100; ++i) {
        if (taker1.get_order(book, 100.0).has_value() !=
            taker2.get_order(book, 100.0).has_value()) {
            diverged = true;
            break;
        }
    }

    EXPECT_TRUE(diverged);
}