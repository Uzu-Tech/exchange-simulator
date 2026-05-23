#include <gtest/gtest.h>
#include "taker.hpp"
#include "rng.hpp"

static_assert(MarketTaker<RandomTaker, SimpleRandomWalk>,   "RandomTaker must satisfy MarketTaker<SimpleRandomWalk>");
static_assert(MarketTaker<RandomTaker, GaussianRandomWalk>, "RandomTaker must satisfy MarketTaker<GaussianRandomWalk>");

static OrderBook make_book() {
    OrderBook book;
    auto ask = OrderRequest::limit(TraderId{99}, Price{100}, Volume{100}, Side::ASK);
    auto bid = OrderRequest::limit(TraderId{99}, Price{90},  Volume{100}, Side::BID);
    book.add_order(ask);
    book.add_order(bid);
    return book;
}

static SimpleRandomWalk make_model(double price = 100.0, uint64_t seed = 1) {
    RandomEngine rng{seed};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = price;
    params.p           = 0.5;
    params.step        = 0.0;
    return SimpleRandomWalk{rng.make_child(), params};
}

static RandomTaker make_taker(
    double   trade_prob = 1.0,
    uint32_t min_vol    = 5,
    uint32_t max_vol    = 10,
    uint64_t seed       = 42,
    uint64_t trader_id  = 1
) {
    RandomEngine rng{seed};
    RandomTaker::ConfigParams params{};
    params.trade_prob = trade_prob;
    params.min_volume = Volume{min_vol};
    params.max_volume = Volume{max_vol};

    return RandomTaker{TraderId{trader_id}, rng.make_child(), params};
}

// ---------------------------------------------------------------------------
// Basic Behavior
// ---------------------------------------------------------------------------
TEST(RandomTakerTest, AlwaysFiresAtProbOne) {
    auto book  = make_book();
    auto model = make_model();
    auto taker = make_taker(1.0);

    for (int i = 0; i < 50; ++i)
        EXPECT_TRUE(taker.get_order(book, model).has_value());
}

TEST(RandomTakerTest, NeverFiresAtProbZero) {
    auto book  = make_book();
    auto model = make_model();
    auto taker = make_taker(0.0);

    for (int i = 0; i < 50; ++i)
        EXPECT_FALSE(taker.get_order(book, model).has_value());
}

TEST(RandomTakerTest, ReturnsMarketOrder) {
    auto book  = make_book();
    auto model = make_model();
    auto taker = make_taker(1.0);
    auto order = taker.get_order(book, model);

    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->type, OrderType::MARKET);
}

TEST(RandomTakerTest, VolumeWithinBounds) {
    auto book  = make_book();
    auto model = make_model();
    auto taker = make_taker(1.0, 5, 10);

    for (int i = 0; i < 100; ++i) {
        auto order = taker.get_order(book, model);
        ASSERT_TRUE(order.has_value());
        EXPECT_TRUE(order->volume >= Volume{5} && order->volume <= Volume{10});
    }
}

TEST(RandomTakerTest, ReproducibleWithSameSeed) {
    auto book  = make_book();
    auto model = make_model();
    std::vector<bool> run1, run2;

    {
        auto taker = make_taker(0.5, 5, 10, 42);
        for (int i = 0; i < 100; ++i)
            run1.push_back(taker.get_order(book, model).has_value());
    }
    {
        auto taker = make_taker(0.5, 5, 10, 42);
        for (int i = 0; i < 100; ++i)
            run2.push_back(taker.get_order(book, model).has_value());
    }

    EXPECT_EQ(run1, run2);
}