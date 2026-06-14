#pragma once

#include <cstdint>
#include <vector>

#include "active_types.hpp"
#include "maker.hpp"
#include "market_types.hpp"
#include "metrics.hpp"
#include "orderbook.hpp"
#include "primitives.hpp"
#include "rng.hpp"
#include "taker.hpp"
#include "logger.hpp"

template<typename Makers, typename Takers>
class Simulator;

template<MarketMaker<ActivePriceModel>... Makers, MarketTaker<ActivePriceModel>... Takers>
class Simulator<TypeList<Makers...>, TypeList<Takers...>> {
public:
    Simulator(
        uint64_t seed,
        size_t num_ticks,
        PositionLimit limit,
        ActivePriceModel model,
        ActiveStrategy strategy,
        std::tuple<Makers...> makers,
        std::tuple<Takers...> takers,
        SingleRunLogger* logger
    )
        : rng(seed),
          model(model),
          strategy(strategy),
          makers(makers),
          takers(takers),
          num_ticks_(num_ticks),
          position_limit_(limit),
          logger(logger) {}

    SimulatorResults run() {
        MetricsCollector collector(logger);
        TradingState state{};
        std::vector<OrderRequest> strategy_orders{};
        strategy_orders.reserve(10);

        for (size_t _ = 0; _ < num_ticks_; _++) {
            collector.on_tick_start({state.position, model.price_change()});
            fill_orderbook(state.tick);

            strategy_orders.clear();
            strategy.get_orders(state, orderbook, position_limit_, strategy_orders);
            handle_strategy_order_requests(state.position, strategy_orders, collector);
            fill_bot_trades(state.position, collector);

            collector.on_tick_end({state.position});
            state.prev_trades = orderbook.trades();

            orderbook.clear();
            model.next_price();
            ++state.tick;
        }

        return collector.results();
    }

    uint64_t seed() const noexcept { return rng.seed(); }

private:
    RandomEngine rng;

    OrderBook orderbook;
    ActivePriceModel model;
    ActiveStrategy strategy;

    std::tuple<Makers...> makers;
    std::tuple<Takers...> takers;

    size_t num_ticks_;
    PositionLimit position_limit_;

    SingleRunLogger* logger;

    void fill_orderbook(Tick tick) {
        std::apply(
            [&](auto&... instances) {
                (
                    [&](MarketMaker<ActivePriceModel> auto& maker) {
                        auto quote = maker.make_market(model);
                        if (quote.ask) {
                            orderbook.add_order(*quote.ask);
                            if (logger) log_quote(*quote.ask, tick);
                        }

                        if (quote.bid) {
                            orderbook.add_order(*quote.bid);
                            if (logger) log_quote(*quote.bid, tick);
                        }
                    }(instances),
                    ...
                );  // Fold over the instances
            },
            makers
        );
    }

    void log_quote(const OrderRequest& order_rq, Tick tick) const {
        logger->log_quote_data({
            .tick = tick,
            .id = order_rq.id,
            .price = order_rq.price,
            .volume = order_rq.volume,
            .side = order_rq.side
        });
    }

    void handle_strategy_order_requests(
        Position& position,
        std::vector<OrderRequest>& strategy_orders,
        MetricsCollector& collector
    ) {
        for (auto& order_rq : strategy_orders) {
            Volume remaining = (order_rq.side == Side::BID)
                                   ? position_limit_.remaining_long(position)
                                   : position_limit_.remaining_short(position);
            order_rq.volume = std::min(order_rq.volume, remaining);

            size_t prev_size = orderbook.trades().size();

            Price expected = order_rq.price;
            auto new_trades = orderbook.add_order(order_rq);
            Volume traded_vol = collector.on_order_fills(OrderFillEvent{
                new_trades,
                order_rq.type,
                expected,
                order_rq.side,
                model.current_price()
            });
            if (order_rq.side == Side::BID)
                position += traded_vol;
            else
                position -= traded_vol;
        }
    }

    void fill_bot_trades(Position& position, MetricsCollector& collector) {
        std::apply(
            [&](auto&... instances) {
                ([&](MarketTaker<ActivePriceModel> auto& taker
                 ) { handle_bot_trade(taker, position, collector); }(instances),
                 ...);  // Fold over the instances
            },
            takers
        );
    }

    void handle_bot_trade(
        MarketTaker<ActivePriceModel> auto& taker,
        Position& position,
        MetricsCollector& collector
    ) {
        auto bot_order = taker.get_order(orderbook, model);
        if (bot_order) {
            auto bot_trades = orderbook.add_order(*bot_order);
            Volume bot_strategy_volume =
                collector.on_bot_trades({bot_trades, model.current_price()});
            if (bot_order->side == Side::ASK)
                position += bot_strategy_volume;
            else
                position -= bot_strategy_volume;
        }
    }
};