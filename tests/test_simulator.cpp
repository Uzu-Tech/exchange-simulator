#include <gtest/gtest.h>
#include "market_types.hpp"
#include "metrics.hpp"
#include "orderbook.hpp"

// ---------------------------------------------------------------------------
// MetricsCollector tests — test the event handler in isolation
// ---------------------------------------------------------------------------
class MetricsCollectorTest : public ::testing::Test {
protected:
    MetricsCollector collector;
};

TEST_F(MetricsCollectorTest, OnOrderFillsUpdatesSlippage) {
    // Single fill at exactly expected price — zero slippage
    Trade trade{Price{100}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{100},
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_NEAR(collector.results().slippage.mean(), 0.0, 1e-9);
}

TEST_F(MetricsCollectorTest, OnOrderFillsSlippagePositiveWhenPaidMore) {
    // Filled at 105, expected 100 on a buy — paid 5 more than expected
    Trade trade{Price{105}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{100},
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_GT(collector.results().slippage.mean(), 0.0);
}

TEST_F(MetricsCollectorTest, OnOrderFillsSlippageNegativeWhenSoldLess) {
    // Sold at 95, expected 100 on a sell — received 5 less than expected
    Trade trade{Price{95}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{100},
        Side::ASK,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_GT(collector.results().slippage.mean(), 0.0);
}

TEST_F(MetricsCollectorTest, MarketOrderUsesFirstFillAsExpected) {
    // Market order — expected price taken from first fill, slippage = 0 for single fill
    Trade trade{Price{100}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::MARKET,
        Price{0},     // sentinel — should be ignored
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_NEAR(collector.results().slippage.mean(), 0.0, 1e-9);
}

TEST_F(MetricsCollectorTest, UnfilledOrderCountsAsMake) {
    // Empty trades span = order rested on book (make)
    std::vector<Trade> trades{};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{100},
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_EQ(collector.results().makes.count(), 1);
    EXPECT_EQ(collector.results().takes.count(), 0);
}

TEST_F(MetricsCollectorTest, FilledOrderCountsAsTake) {
    Trade trade{Price{100}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{100},
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    EXPECT_EQ(collector.results().takes.count(), 1);
    EXPECT_EQ(collector.results().makes.count(), 0);
}

TEST_F(MetricsCollectorTest, PnlPositiveWhenBuyBelowSimPrice) {
    // Bought at 90, sim price 100 — unrealised gain of 10 per unit
    Trade trade{Price{90}, Volume{10}, TraderId{1}, TraderId{2}};
    std::vector<Trade> trades{trade};

    OrderFillEvent event{
        std::span<const Trade>{trades},
        OrderType::LIMIT,
        Price{90},
        Side::BID,
        100.0   // sim price above fill
    };

    collector.on_order_fills(event);
    EXPECT_GT(collector.results().pnl.total(), 0.0);
}

TEST_F(MetricsCollectorTest, OnTickEndUpdatesPosition) {
    collector.on_tick_end({Position{50}});
    EXPECT_EQ(collector.results().position.mean(), 50.0);
}

TEST_F(MetricsCollectorTest, BotTradeUpdatesFilllQualityForUser) {
    // Bot fills a resting USER order
    Trade trade{Price{100}, Volume{5}, config::USER_ID, TraderId{99}};
    std::vector<Trade> trades{trade};

    BotFillEvent event{std::span<const Trade>{trades}, 100.0};
    collector.on_bot_trades(event);

    // Fill at sim price — fill quality should be zero
    EXPECT_NEAR(collector.results().fill_quality.mean(), 0.0, 1e-9);
}

// ---------------------------------------------------------------------------
// Integration — MetricsCollector + OrderBook together
// ---------------------------------------------------------------------------
TEST(SimulatorIntegrationTest, SlippageAccumulatesAcrossMultipleFills) {
    MetricsCollector collector;
    OrderBook book;

    // Post two ask levels
    auto ask1 = OrderRequest::limit(TraderId{99}, Price{100}, Volume{5},  Side::ASK);
    auto ask2 = OrderRequest::limit(TraderId{99}, Price{105}, Volume{5},  Side::ASK);
    book.add_order(ask1);
    book.add_order(ask2);

    // Large market buy sweeps both levels
    auto bid = OrderRequest::market(TraderId{1}, Volume{10}, Side::BID);
    auto new_trades = book.add_order(bid);

    // First fill = expected price (100), second fill at 105 = slippage
    OrderFillEvent event{
        new_trades,
        OrderType::MARKET,
        Price{0},
        Side::BID,
        100.0
    };

    collector.on_order_fills(event);
    // Slippage should be positive — paid above first fill price on average
    EXPECT_GT(collector.results().slippage.mean(), 0.0);
}

TEST(SimulatorIntegrationTest, PositionTrackedCorrectlyAcrossTicks) {
    MetricsCollector collector;

    collector.on_tick_end({Position{10}});
    collector.on_tick_end({Position{20}});
    collector.on_tick_end({Position{15}});

    EXPECT_NEAR(collector.results().position.mean(), 15.0, 1e-9);
}

TEST(SimulatorIntegrationTest, DrawdownUpdatesAfterPnlSwing) {
    MetricsCollector collector;
    OrderBook book;

    // Buy at 100
    auto ask = OrderRequest::limit(TraderId{99}, Price{100}, Volume{10}, Side::ASK);
    book.add_order(ask);
    auto bid = OrderRequest::market(TraderId{1}, Volume{10}, Side::BID);
    auto trades = book.add_order(bid);

    // Sim price above fill — positive pnl
    OrderFillEvent buy_event{trades, OrderType::MARKET, Price{0}, Side::BID, 110.0};
    collector.on_order_fills(buy_event);

    book.clear();

    // Sim price below fill — negative pnl, triggers drawdown
    auto ask2 = OrderRequest::limit(TraderId{99}, Price{100}, Volume{10}, Side::ASK);
    book.add_order(ask2);
    auto bid2 = OrderRequest::market(TraderId{1}, Volume{10}, Side::BID);
    auto trades2 = book.add_order(bid2);

    OrderFillEvent loss_event{trades2, OrderType::MARKET, Price{0}, Side::BID, 90.0};
    collector.on_order_fills(loss_event);

    // Max drawdown should be positive — we went from profit to loss
    EXPECT_GT(collector.results().drawdown.max(), 0.0);
}
