#include <gtest/gtest.h>
#include "market_types.hpp"
#include "strategy.hpp"
#include "pure_market_maker.hpp"
#include "orderbook.hpp"
#include <algorithm>

static_assert(Strategy<PureMarketMaker>, "PureMarketMaker must satisfy Strategy");

static TradingState make_state(int32_t position = 0) {
    return TradingState{
        Tick{0},
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
// Core Strategy Verifications
// ---------------------------------------------------------------------------
TEST(PureMarketMakerTest, EmptyBookProducesNoOrders) {
    OrderBook empty_book;
    PureMarketMaker::ConfigParams params{};
    params.offset = PriceDelta{1};

    PureMarketMaker maker{params};
    std::vector<OrderRequest> orders{};
    maker.get_orders(make_state(), empty_book, PositionLimit{100}, orders);

    EXPECT_TRUE(orders.empty());
}

TEST(PureMarketMakerTest, OnlyBidSideProducesOneBidOrder) {
    OrderBook book;
    OrderRequest order_rq = OrderRequest::limit(TraderId{99}, Price{99}, Volume{10}, Side::BID);
    book.add_order(order_rq);

    PureMarketMaker::ConfigParams params{};
    params.offset = PriceDelta{1};

    PureMarketMaker maker{params};
    std::vector<OrderRequest> orders{};
    maker.get_orders(make_state(), book, PositionLimit{100}, orders);

    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].side, Side::BID);
}

TEST(PureMarketMakerTest, AtLongLimitProducesNoBidOrder) {
    PureMarketMaker::ConfigParams params{};
    params.offset = PriceDelta{1};

    std::vector<OrderRequest> orders{};
    PureMarketMaker{params}.get_orders(
        make_state(100), make_book_with_spread(), PositionLimit{100}, orders);

    EXPECT_EQ(find_side(orders, Side::BID), nullptr);
}

TEST(PureMarketMakerTest, BidVolumeClampsToRemainingLongCapacity) {
    PureMarketMaker::ConfigParams params{};
    params.offset = PriceDelta{1};

    std::vector<OrderRequest> orders{};
    PureMarketMaker{params}.get_orders(
        make_state(97),
        make_book_with_spread(Price{99}, Volume{10}, Price{101}, Volume{10}),
        PositionLimit{100}, orders);

    const auto* bid = find_side(orders, Side::BID);
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->volume, Volume{3});
}