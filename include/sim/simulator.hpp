#pragma once

#include "market_types.hpp"
#include "orderbook.hpp"
#include "primitives.hpp"
#include "sim_price.hpp"
#include "rng.hpp"
#include "metrics.hpp"
#include "strategy.hpp"
#include "maker.hpp"
#include "taker.hpp"
#include "pure_market_maker.hpp"
#include <vector>

template<typename... Ts> struct TypeList {};

template<PriceModel Model, Strategy Trader, typename Makers, typename Takers>
class Simulator;

template<PriceModel Model, Strategy Trader, MarketMaker<Model>... Makers, MarketTaker<Model>... Takers>
class Simulator<Model, Trader, TypeList<Makers...>, TypeList<Takers...>> {
public:
    Simulator(uint64_t seed, size_t num_ticks, PositionLimit limit, 
            Model model, Trader trader, std::tuple<Makers...> makers, std::tuple<Takers...> takers)
        : rng(seed), model(model), trader(trader), makers(makers), takers(takers),
        num_timestamps(num_ticks), position_limit(limit) {}

    SimulatorResults run() {
        MetricsCollector collector;
        TradingState state{};

        for (size_t _ = 0; _ < num_timestamps; _++) {
            fill_orderbook();

            std::vector<OrderRequest> trader_orders = trader.get_orders(state, orderbook,  position_limit);
            handle_trader_order_requests(state.position, trader_orders, collector);
            fill_bot_trades(state.position, collector);
            collector.on_tick_end({state.position});

            state.prev_trades = orderbook.trades();
            
            orderbook.clear();
            model.next_price();
            ++state.timestamp;
        }

        return collector.results();
    }

private:
    RandomEngine rng;

    OrderBook orderbook;
    Model model;
    Trader trader;

    std::tuple<Makers...> makers;
    std::tuple<Takers...> takers;

    size_t num_timestamps;
    PositionLimit position_limit;

    void fill_orderbook() {
        std::apply([&](auto&... instances) {
            ([&](MarketMaker<Model> auto& maker) { 
                auto quote = maker.make_market(model);
                if (quote.ask) orderbook.add_order(*quote.ask);
                if (quote.bid) orderbook.add_order(*quote.bid);
            }(instances), ...); // Fold over the instances
        }, makers);
    }

    void handle_trader_order_requests(
        Position& position, 
        std::vector<OrderRequest>& trader_orders,
        MetricsCollector& collector
    ) {
        for (auto& order_rq : trader_orders) {
            Volume remaining = (order_rq.side == Side::BID)
                ? position_limit.remaining_long(position)
                : position_limit.remaining_short(position);
            order_rq.volume = std::min(order_rq.volume, remaining);

            size_t prev_size = orderbook.trades().size();

            Price expected = order_rq.price;
            auto new_trades = orderbook.add_order(order_rq);
            Volume traded_vol = collector.on_order_fills(
                OrderFillEvent{new_trades, order_rq.type, expected, order_rq.side, model.current_price()}
            );
            if (order_rq.side == Side::BID) position += traded_vol;
            else position -= traded_vol;
        }
    } 

    void fill_bot_trades(Position& position, MetricsCollector& collector) {
        std::apply([&](auto&... instances) {
            ([&](MarketTaker<Model> auto& taker) { 
                handle_bot_trade(taker, position, collector);
            }(instances), ...); // Fold over the instances
        }, takers);
    }

    void handle_bot_trade(MarketTaker<Model> auto& taker, Position& position, MetricsCollector& collector) {
        auto bot_order = taker.get_order(orderbook, model);
        if (bot_order) {
            auto bot_trades = orderbook.add_order(*bot_order);
            Volume bot_trader_volume = collector.on_bot_trades({bot_trades, model.current_price()});
            if (bot_order->side == Side::ASK) position += bot_trader_volume;
            else position -= bot_trader_volume;
        }
    }
};