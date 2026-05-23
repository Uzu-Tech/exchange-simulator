#pragma once

#include <limits>
#include <cstdint>
#include <pcg_random.hpp>

class RandomEngine {
public:
    explicit RandomEngine(uint64_t seed)
        : seed_(seed) {}

    pcg32 make_child() noexcept { return pcg32(seed_, next_stream_++); }
    uint64_t seed() const { return seed_; }

private:
    uint64_t seed_;
    uint64_t next_stream_ = 0;
};

inline double fast_uniform(pcg32& gen) noexcept {
    return static_cast<double>(gen()) / 
           static_cast<double>(std::numeric_limits<uint32_t>::max());
}