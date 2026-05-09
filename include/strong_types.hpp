#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <concepts>
#include <limits>
#include <exception>

inline void hard_assert(bool cond, const char* type, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FATAL: [%s]: %s\n", type, msg);
        std::terminate();
    }
}

template<typename Tag, std::unsigned_integral Underlying>
struct StrongUInt {
    Underlying value;

    explicit StrongUInt(Underlying v) : value(v) {}
    auto operator<=>(const StrongUInt&) const = default;
};

template<typename Tag, std::unsigned_integral Underlying>
struct ArithmeticUInt : StrongUInt<Tag, Underlying> {
    using Base = StrongUInt<Tag, Underlying>;
    using Base::Base;

    ArithmeticUInt operator+(const ArithmeticUInt& other) const {
        using Wide = uint64_t;
        Wide result = static_cast<Wide>(this->value) + other.value;
        hard_assert(result <= std::numeric_limits<Underlying>::max(), Tag::name, "unsigned overflow");
        return ArithmeticUInt{static_cast<Underlying>(result)};
    }

    ArithmeticUInt operator-(const ArithmeticUInt& other) const {
        hard_assert(this->value >= other.value, Tag::name, "unsigned underflow");
        return ArithmeticUInt{static_cast<Underlying>(this->value - other.value)};
    }

    ArithmeticUInt& operator+=(const ArithmeticUInt& other) { *this = *this + other; return *this; }
    ArithmeticUInt& operator-=(const ArithmeticUInt& other) { *this = *this - other; return *this; }
};

// Domain tags
struct PriceTag { static constexpr const char* name = "Price"; };
struct VolumeTag { static constexpr const char* name = "Volume"; };
struct OrderIdTag { static constexpr const char* name = "OrderId"; };

// Discrete price in ticks. Unsigned — a negative price is a logic bug.
using Price = ArithmeticUInt<PriceTag,  uint32_t>;
// Quantity in lots. Unsigned — a negative volume is a logic bug.
using Volume = ArithmeticUInt<VolumeTag, uint32_t>;
// Unique order identifier. No arithmetic — adding two IDs is nonsense.
using OrderId = StrongUInt<OrderIdTag, uint64_t>;