#include <gtest/gtest.h>
#include "market_types.hpp"
#include "strategy.hpp"
#include "pure_market_maker.hpp"
#include "orderbook.hpp"
#include <algorithm>

// Compile-time check
static_assert(Strategy<PureMarketMaker>, "PureMarketMaker must satisfy Strategy");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static TradingState make_state(int32_t position = 0) {
    return TradingState{
        Timestamp<config::TIMESTAMP_TICK_SIZE>{0},
        std::span<const Trade>{},
        Position{position}
    };
}

static OrderBook make_book_with_spread(
    Price  bid_price = Price{99},
    Volume bid_vol   = Volume{10},
    Price  ask_price = Price{101},
    Volume ask_vol   = Volume{10}
) {
    OrderBook book;
    auto bid = OrderRequest::limit(TraderId{99}, bid_price, bid_vol, Side::BID);
    auto ask = OrderRequest::limit(TraderId{99}, ask_price, ask_vol, Side::ASK);
    book.add_order(bid);
    book.add_order(ask);
    return book;
}

static const OrderRequest* find_side(const std::vector<OrderRequest>& orders, Side side) {
    if (orders.empty()) return nullptr;
    auto it = std::find_if(orders.begin(), orders.end(),
                           [side](const OrderRequest& o){ return o.side == side; });
    return (it != orders.end()) ? &(*it) : nullptr;
}

// ---------------------------------------------------------------------------
// Empty / partial book
// ---------------------------------------------------------------------------
TEST(PureMarketMakerTest, EmptyBookProducesNoOrders) {
    OrderBook empty_book;
    PureMarketMaker maker;
    auto orders = maker.get_orders(make_state(), empty_book, PositionLimit{100});

    EXPECT_TRUE(orders.empty());
}

TEST(PureMarketMakerTest, OnlyBidSideProducesOneBidOrder) {
    OrderBook book;
    OrderRequest order_rq = OrderRequest::limit(TraderId{99}, Price{99}, Volume{10}, Side::BID);
    book.add_order(order_rq);

    auto orders = PureMarketMaker{}.get_orders(make_state(), book, PositionLimit{100});

    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].side, Side::BID);
}

TEST(PureMarketMakerTest, OnlyAskSideProducesOneAskOrder) {
    OrderBook book;
    OrderRequest order_rq = OrderRequest::limit(TraderId{99}, Price{101}, Volume{10}, Side::ASK);
    book.add_order(order_rq);

    auto orders = PureMarketMaker{}.get_orders(make_state(), book, PositionLimit{100});

    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].side, Side::ASK);
}

// ---------------------------------------------------------------------------
// Quote correctness
// ---------------------------------------------------------------------------
TEST(PureMarketMakerTest, QuotesBothSidesWhenBookHasSpread) {
    auto orders = PureMarketMaker{}.get_orders(
        make_state(), make_book_with_spread(), PositionLimit{100});

    ASSERT_EQ(orders.size(), 2);
    EXPECT_NE(find_side(orders, Side::BID), nullptr);
    EXPECT_NE(find_side(orders, Side::ASK), nullptr);
}

TEST(PureMarketMakerTest, BidPriceIsOneTickAboveBestBid) {
    // best_bid = 99, edge = 1 → bid quote = 100
    auto orders = PureMarketMaker{}.get_orders(
        make_state(), make_book_with_spread(Price{99}, Volume{10}, Price{101}, Volume{10}),
        PositionLimit{100});

    const auto* bid = find_side(orders, Side::BID);
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->price, Price{100});
}

TEST(PureMarketMakerTest, AskPriceIsOneTickBelowBestAsk) {
    // best_ask = 101, edge = 1 → ask quote = 100
    auto orders = PureMarketMaker{}.get_orders(
        make_state(), make_book_with_spread(Price{99}, Volume{10}, Price{101}, Volume{10}),
        PositionLimit{100});

    const auto* ask = find_side(orders, Side::ASK);
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->price, Price{100});
}

TEST(PureMarketMakerTest, OrdersPostedAsUserID) {
    auto orders = PureMarketMaker{}.get_orders(
        make_state(), make_book_with_spread(), PositionLimit{100});

    for (const auto& order : orders)
        EXPECT_EQ(order.id, config::USER_ID);
}

TEST(PureMarketMakerTest, OrdersAreLimitType) {
    auto orders = PureMarketMaker{}.get_orders(
        make_state(), make_book_with_spread(), PositionLimit{100});

    for (const auto& order : orders)
        EXPECT_EQ(order.type, OrderType::LIMIT);
}

// ---------------------------------------------------------------------------
// Position limit clamping
// ---------------------------------------------------------------------------
TEST(PureMarketMakerTest, AtLongLimitProducesNoBidOrder) {
    auto orders = PureMarketMaker{}.get_orders(
        make_state(100), make_book_with_spread(), PositionLimit{100});

    EXPECT_EQ(find_side(orders, Side::BID), nullptr);
}

TEST(PureMarketMakerTest, AtShortLimitProducesNoAskOrder) {
    auto orders = PureMarketMaker{}.get_orders(
        make_state(-100), make_book_with_spread(), PositionLimit{100});

    EXPECT_EQ(find_side(orders, Side::ASK), nullptr);
}

TEST(PureMarketMakerTest, BidVolumeClampsToRemainingLongCapacity) {
    // Long 97, limit 100 → remaining = 3
    auto orders = PureMarketMaker{}.get_orders(
        make_state(97),
        make_book_with_spread(Price{99}, Volume{10}, Price{101}, Volume{10}),
        PositionLimit{100});

    const auto* bid = find_side(orders, Side::BID);
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->volume, Volume{3});
}

TEST(PureMarketMakerTest, AskVolumeClampsToRemainingShortCapacity) {
    // Short 97, limit 100 → remaining = 3
    auto orders = PureMarketMaker{}.get_orders(
        make_state(-97),
        make_book_with_spread(Price{99}, Volume{10}, Price{101}, Volume{10}),
        PositionLimit{100});

    const auto* ask = find_side(orders, Side::ASK);
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->volume, Volume{3});
}
