#include <gtest/gtest.h>
#include "maker.hpp"
#include "rng.hpp"

static_assert(MarketMaker<SymmetricMaker, SimpleRandomWalk>,   "SymmetricMaker must satisfy MarketMaker<SimpleRandomWalk>");
static_assert(MarketMaker<SymmetricMaker, GaussianRandomWalk>, "SymmetricMaker must satisfy MarketMaker<GaussianRandomWalk>");

static SymmetricMaker make_symmetric(
    uint64_t trader_id   = 1,
    int32_t  half_spread = 2,
    uint32_t min_vol     = 5,
    uint32_t max_vol     = 10,
    uint64_t seed        = 42
) {
    RandomEngine rng{seed};
    SymmetricMaker::ConfigParams params{};
    params.half_spread = PriceDelta{half_spread};
    params.min_volume  = Volume{min_vol};
    params.max_volume  = Volume{max_vol};

    return SymmetricMaker{TraderId{trader_id}, rng.make_child(), params};
}

static SimpleRandomWalk make_model(double price, uint64_t seed = 1) {
    RandomEngine rng{seed};
    SimpleRandomWalk::ConfigParams params{};
    params.start_price = price;
    params.p           = 0.5;
    params.step        = 0.0;
    return SimpleRandomWalk{rng.make_child(), params};
}

// ---------------------------------------------------------------------------
// MakerQuote Invariants
// ---------------------------------------------------------------------------
TEST(MakerQuoteTest, BidAndAskPresent) {
    auto maker = make_symmetric();
    auto model = make_model(100.0);
    auto quote = maker.make_market(model);

    EXPECT_TRUE(quote.bid.has_value());
    EXPECT_TRUE(quote.ask.has_value());
}

TEST(MakerQuoteTest, BidBelowAsk) {
    auto maker = make_symmetric();
    auto model = make_model(100.0);
    auto quote = maker.make_market(model);

    EXPECT_TRUE(quote.bid->price <= quote.ask->price);
}

TEST(MakerQuoteTest, BidAndAskSymmetricAroundMid) {
    auto maker = make_symmetric(1, 5);
    auto model = make_model(100.0);
    auto quote = maker.make_market(model);

    EXPECT_EQ(quote.bid->price, Price{95});
    EXPECT_EQ(quote.ask->price, Price{105});
}

TEST(MakerQuoteTest, CorrectSides) {
    auto maker = make_symmetric();
    auto model = make_model(100.0);
    auto quote = maker.make_market(model);

    EXPECT_EQ(quote.bid->side, Side::BID);
    EXPECT_EQ(quote.ask->side, Side::ASK);
}

TEST(MakerQuoteTest, VolumeWithinBounds) {
    auto maker = make_symmetric(1, 2, 5, 10);
    auto model = make_model(100.0);
    for (int i = 0; i < 100; ++i) {
        auto quote = maker.make_market(model);
        EXPECT_TRUE(quote.bid->volume >= Volume{5} && quote.bid->volume <= Volume{10});
        EXPECT_TRUE(quote.ask->volume >= Volume{5} && quote.ask->volume <= Volume{10});
    }
}

TEST(MakerQuoteTest, AssertsOnNoBidOrAsk) {
    EXPECT_DEATH(
        MakerQuote(std::nullopt, std::nullopt),
        "Invalid quote, no bid or ask"
    );
}

TEST(MakerQuoteTest, AssertsOnBidAboveAsk) {
    auto bid = OrderRequest::limit(TraderId{1}, Price{110}, Volume{5}, Side::BID);
    auto ask = OrderRequest::limit(TraderId{1}, Price{100}, Volume{5}, Side::ASK);

    EXPECT_DEATH(
        MakerQuote(bid, ask),
        "Invalid quote, bid is greater than ask"
    );
}

TEST(SymmetricMakerTest, ReproducibleWithSameSeed) {
    auto model = make_model(100.0);
    auto q1 = make_symmetric(1, 2, 5, 10, 42).make_market(model);
    auto q2 = make_symmetric(1, 2, 5, 10, 42).make_market(model);

    EXPECT_EQ(q1.bid->volume, q2.bid->volume);
    EXPECT_EQ(q1.ask->volume, q2.ask->volume);
}