#include <gtest/gtest.h>
#include "maker.hpp"
#include "rng.hpp"

// Compile-time check — SymmetricMaker must satisfy MarketMaker with any PriceModel
static_assert(MarketMaker<SymmetricMaker, SimpleRandomWalk>,   "SymmetricMaker must satisfy MarketMaker<SimpleRandomWalk>");
static_assert(MarketMaker<SymmetricMaker, GaussianRandomWalk>, "SymmetricMaker must satisfy MarketMaker<GaussianRandomWalk>");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static SymmetricMaker make_symmetric(
    uint64_t trader_id   = 1,
    int32_t  half_spread = 2,
    uint32_t min_vol     = 5,
    uint32_t max_vol     = 10,
    uint64_t seed        = 42
) {
    RandomEngine rng{seed};
    return SymmetricMaker{
        TraderId{trader_id},
        PriceDelta{half_spread},
        Volume{min_vol},
        Volume{max_vol},
        rng.make_child()
    };
}

// Returns a SimpleRandomWalk pinned at a fixed price — use for tests that
// just need a stable model without caring about price movement.
static SimpleRandomWalk make_model(double price, uint64_t seed = 1) {
    RandomEngine rng{seed};
    // p=0.5, step=0 keeps the price fixed
    return SimpleRandomWalk{price, 0.5, 0.0, rng.make_child()};
}

// ---------------------------------------------------------------------------
// MakerQuote invariants
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

    // mid = 100, half_spread = 5 → bid = 95, ask = 105
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

TEST(MakerQuoteTest, QuoteUpdatesWithModelPrice) {
    auto maker      = make_symmetric(1, 3);
    auto model_low  = make_model(100.0);
    auto model_high = make_model(200.0);

    auto quote_low  = maker.make_market(model_low);
    auto quote_high = maker.make_market(model_high);

    EXPECT_EQ(quote_low.bid->price,  Price{97});
    EXPECT_EQ(quote_low.ask->price,  Price{103});
    EXPECT_EQ(quote_high.bid->price, Price{197});
    EXPECT_EQ(quote_high.ask->price, Price{203});
}

// ---------------------------------------------------------------------------
// MakerQuote hard_assert guards
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------
TEST(SymmetricMakerTest, ReproducibleWithSameSeed) {
    auto model = make_model(100.0);
    auto q1 = make_symmetric(1, 2, 5, 10, 42).make_market(model);
    auto q2 = make_symmetric(1, 2, 5, 10, 42).make_market(model);

    EXPECT_EQ(q1.bid->volume, q2.bid->volume);
    EXPECT_EQ(q1.ask->volume, q2.ask->volume);
}

TEST(SymmetricMakerTest, DifferentSeedsProduceDifferentVolumes) {
    auto model = make_model(100.0);
    bool diverged = false;

    for (int i = 0; i < 20; ++i) {
        auto q1 = make_symmetric(1, 2, 1, 1000, 42).make_market(model);
        auto q2 = make_symmetric(1, 2, 1, 1000, 99).make_market(model);
        if (q1.bid->volume != q2.bid->volume) { diverged = true; break; }
    }
    EXPECT_TRUE(diverged);
}
