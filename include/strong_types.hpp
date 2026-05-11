#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <limits>
#include <exception>
#include <source_location>

using PriceUnderlying = uint32_t;

using Wide = int64_t;

inline void hard_assert(
    bool cond,
    const char* type,
    const char* msg,
    const std::source_location loc = std::source_location::current()
) {
    if (!cond) [[unlikely]] {
        std::fprintf(stderr, "FATAL [%s]: %s\nLocation: %s:%u\nFunction: %s\n",
                     type, msg, loc.file_name(), loc.line(), loc.function_name());
        std::terminate();
    }
}

struct TraderId  {
public:
    using Underlying = uint64_t;
    explicit TraderId (Underlying v) : value(v) {}
    // Comparision
    auto operator<=>(const TraderId &) const = default;
private:
    Underlying value;
};

class PriceDelta {
public:
    using Underlying = int32_t;
    explicit PriceDelta(Underlying v) : value_(v) {}
    // Comparison
    auto operator<=>(const PriceDelta&) const = default;
    // Arithmetic
    PriceDelta operator-() const { return PriceDelta{-value_}; }
    PriceDelta operator+(const PriceDelta& other) const {
        return PriceDelta{value_ + other.value()};
    }
    const Underlying value() const { return value_; }

private:
    Underlying value_;
};

class Price {
public:
    using Underlying = uint32_t;
    explicit Price(Underlying v) : value(v) {}
    // Comparision
    auto operator<=>(const Price&) const = default;
    // Arithmetic
    Price operator+(const PriceDelta& delta) const {
        Wide res = static_cast<Wide>(value) + delta.value();
        hard_assert(res >= 0, "Price", "Resulting price cannot be negative");
        hard_assert(res <= std::numeric_limits<Underlying>::max(), "Price", "Price overflow");
        return Price{static_cast<Underlying>(res)};
    }
    Price operator-(const PriceDelta& delta) const { return *this + -delta; }

    Price& operator+=(const PriceDelta& delta) { *this = *this + delta; return *this; }
    Price& operator-=(const PriceDelta& delta) { *this = *this - delta; return *this; }

private:
    Underlying value;
};

inline Price convert_type_to_price(double sim_price) {
    return Price{static_cast<Price::Underlying>(std::round(sim_price))};
}

class Volume {
public:
    using Underlying = uint32_t;

    explicit Volume(Underlying v) : value_(v) {}
    const Underlying value() const { return value_; }

    auto operator<=>(const Volume&) const = default;

    // Standard Arithmetic
    Volume operator+(const Volume& other) const {
        Wide res = static_cast<Wide>(value_) + other.value();
        hard_assert(res <= std::numeric_limits<Underlying>::max(), "Volume", "Overflow");
        return Volume{static_cast<Underlying>(res)};
    }

    Volume operator-(const Volume& other) const {
        hard_assert(value_ >= other.value(), "Volume", "Underflow (Negative Volume)");
        return Volume{value_ - other.value()};
    }

    Volume& operator+=(const Volume& other) { *this = *this + other; return *this; }
    Volume& operator-=(const Volume& other) { *this = *this - other; return *this; }

private:
    Underlying value_;
};

class Position {
public:
    using Underlying = int32_t;
    Position() = default;
    explicit Position(Underlying v): value_(v) {}
    // Comparision
    auto operator<=>(const Position&) const = default;

    Position operator+(const Volume& v) const {
        int64_t res = static_cast<int64_t>(value_) + v.value();
        return Position{static_cast<Position>(res)};
    }

    Position operator-(const Volume& v) const {
        int64_t res = static_cast<int64_t>(value_) - v.value();
        return Position{static_cast<Position>(res)};
    }

    Position& operator+=(const Volume& other) { *this = *this + other; return *this; }
    Position& operator-=(const Volume& other) { *this = *this + other; return *this; }

private:
    Underlying value_ = 0;
};