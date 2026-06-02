#pragma once

#include "market_types.hpp"
#include "primitives.hpp"
#include "npy.hpp"
#include <tuple>

template<typename T>
concept Log = requires(T log) {
    { log.npy_fields() };
};

#pragma pack(push, 1)

struct PerformanceLog {
    Tick tick;
    double pnl;
    Position position;

    static constexpr auto npy_fields() {
        return std::make_tuple(
            std::make_pair("tick", NpyType<Tick::Underlying>::code),
            std::make_pair("pnl", NpyType<double>::code),
            std::make_pair("position", NpyType<Position::Underlying>::code)
        );
    }
};

struct QuoteLog {
    Tick tick;
    TraderId id;
    Price price;
    Volume volume;
    Side side;

    static constexpr auto npy_fields() {
        return std::make_tuple(
            std::make_pair("tick", NpyType<Tick::Underlying>::code),
            std::make_pair("id", NpyType<TraderId::Underlying>::code),
            std::make_pair("price", NpyType<Price::Underlying>::code),
            std::make_pair("volume", NpyType<Volume::Underlying>::code),
            std::make_pair("side", NpyType<uint8_t>::code)
        );
    }
};

struct TradeLog {
    Tick tick;
    TraderId buyer_id;
    TraderId seller_id;
    Price price;
    Volume volume;

    static constexpr auto npy_fields() {
        return std::make_tuple(
            std::make_pair("tick", NpyType<Tick::Underlying>::code),
            std::make_pair("buyer_id", NpyType<TraderId::Underlying>::code),
            std::make_pair("seller_id", NpyType<TraderId::Underlying>::code),
            std::make_pair("price", NpyType<Price::Underlying>::code),
            std::make_pair("volume", NpyType<Volume::Underlying>::code)
        );
    }
};

#pragma pack(pop)