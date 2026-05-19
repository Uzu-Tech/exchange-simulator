#include <gtest/gtest.h>

#include "orderbook.hpp"
#include "primitives.hpp"
#include "market_types.hpp"

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void TearDown() override {
        book.clear();
    }
};

TEST_F(OrderBookTest, AddsBidLimitOrder) {
    auto order = OrderRequest::limit(TraderId {1}, Price{100}, Volume{10}, Side::BID);

    book.add_order(order);

    auto bids = book.bids();

    ASSERT_EQ(bids.view().size(), 1);
    EXPECT_EQ(bids.view()[0].price,        Price{100});
    EXPECT_EQ(bids.view()[0].volume, Volume{10});

    ASSERT_EQ(bids.view()[0].orders.size(), 1);
    EXPECT_EQ(bids.view()[0].orders.front().id, TraderId {1});
}

TEST_F(OrderBookTest, AddsAskLimitOrder) {
    auto order = OrderRequest::limit(TraderId {1}, Price{105}, Volume{7}, Side::ASK);

    book.add_order(order);

    auto asks = book.asks();

    ASSERT_EQ(asks.view().size(), 1);
    EXPECT_EQ(asks.view()[0].price,        Price{105});
    EXPECT_EQ(asks.view()[0].volume, Volume{7});
}

TEST_F(OrderBookTest, AggregatesOrdersAtSamePrice) {
    auto order1 = OrderRequest::limit(TraderId {1}, Price{100}, Volume{5},  Side::BID);
    auto order2 = OrderRequest::limit(TraderId {2}, Price{100}, Volume{8},  Side::BID);

    book.add_order(order1);
    book.add_order(order2);

    auto bids = book.bids();

    ASSERT_EQ(bids.view().size(), 1);
    EXPECT_EQ(bids.view()[0].volume, Volume{13});

    ASSERT_EQ(bids.view()[0].orders.size(), 2);
    EXPECT_EQ(bids.view()[0].orders[0].id, TraderId {1});
    EXPECT_EQ(bids.view()[0].orders[1].id, TraderId {2});
}

TEST_F(OrderBookTest, BidCrossesAskAndTrades) {
    auto ask = OrderRequest::limit(TraderId {1}, Price{100}, Volume{10}, Side::ASK);
    auto bid = OrderRequest::limit(TraderId {2}, Price{100}, Volume{10}, Side::BID);

    book.add_order(ask);
    book.add_order(bid);

    EXPECT_TRUE(book.asks().view().empty());
    EXPECT_TRUE(book.bids().view().empty());

    auto trades = book.trades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price,     Price{100});
    EXPECT_EQ(trades[0].volume,    Volume{10});
    EXPECT_EQ(trades[0].buyer_id,  TraderId {2});
    EXPECT_EQ(trades[0].seller_id, TraderId {1});
}

TEST_F(OrderBookTest, PartialFillLeavesRemainingLiquidity) {
    auto ask = OrderRequest::limit(TraderId {1}, Price{100}, Volume{10}, Side::ASK);
    auto bid = OrderRequest::limit(TraderId {2}, Price{100}, Volume{4},  Side::BID);

    book.add_order(ask);
    book.add_order(bid);

    auto asks = book.asks();

    ASSERT_EQ(asks.view().size(), 1);
    EXPECT_EQ(asks.view()[0].orders.front().volume, Volume{6});

    auto trades = book.trades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].volume, Volume{4});
}

TEST_F(OrderBookTest, FIFOIsRespected) {
    auto ask1 = OrderRequest::limit(TraderId {1}, Price{100}, Volume{5}, Side::ASK);
    auto ask2 = OrderRequest::limit(TraderId {2}, Price{100}, Volume{5}, Side::ASK);
    auto bid  = OrderRequest::limit(TraderId {3}, Price{100}, Volume{7}, Side::BID);

    book.add_order(ask1);
    book.add_order(ask2);
    book.add_order(bid);

    auto asks = book.asks();

    ASSERT_EQ(asks.view().size(), 1);
    ASSERT_EQ(asks.view()[0].orders.size(), 1);
    EXPECT_EQ(asks.view()[0].orders.front().id,     TraderId {2});
    EXPECT_EQ(asks.view()[0].orders.front().volume, Volume{3});
}

TEST_F(OrderBookTest, MarketOrderConsumesLiquidity) {
    auto ask        = OrderRequest::limit(TraderId {1}, Price{100}, Volume{10}, Side::ASK);
    auto market_bid = OrderRequest::market(TraderId {2}, Volume{10}, Side::BID);

    book.add_order(ask);
    book.add_order(market_bid);

    EXPECT_TRUE(book.asks().view().empty());

    auto trades = book.trades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price,  Price{100});
    EXPECT_EQ(trades[0].volume, Volume{10});
}

TEST_F(OrderBookTest, MarketOrderDoesNotRestOnBook) {
    auto market_bid = OrderRequest::market(TraderId {1}, Volume{10}, Side::BID);

    book.add_order(market_bid);

    EXPECT_TRUE(book.bids().view().empty());
    EXPECT_TRUE(book.asks().view().empty());
}

TEST_F(OrderBookTest, MarketOrderPartialFillEmptyBook) {
    auto ask        = OrderRequest::limit(TraderId {1}, Price{100}, Volume{5},  Side::ASK);
    auto market_bid = OrderRequest::market(TraderId {2}, Volume{10}, Side::BID);

    book.add_order(ask);
    book.add_order(market_bid);

    // Liquidity exhausted — remaining 5 volume unmatched, nothing rests
    EXPECT_TRUE(book.asks().view().empty());
    EXPECT_TRUE(book.bids().view().empty());

    auto trades = book.trades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].volume, Volume{5});
}

TEST_F(OrderBookTest, NonCrossingOrdersRemainOnBook) {
    auto bid = OrderRequest::limit(
        TraderId {1}, Price{90},  Volume{10}, Side::BID
    );
    auto ask = OrderRequest::limit(
        TraderId {2}, Price{100}, Volume{10}, Side::ASK
    );

    book.add_order(bid);
    book.add_order(ask);

    EXPECT_EQ(book.bids().view().size(), 1);
    EXPECT_EQ(book.asks().view().size(), 1);
    EXPECT_TRUE(book.trades().empty());
}