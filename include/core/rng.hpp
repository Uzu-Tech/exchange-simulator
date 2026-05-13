#pragma once

#include <cstdint>
#include <pcg_random.hpp>

class RandomEngine {
public:
    explicit RandomEngine(uint64_t seed) : seed_(seed) {}
 
    pcg32 make_child() noexcept {
        return pcg32(seed_, next_stream_++);
    }
 
private:
    uint64_t seed_;
    uint64_t next_stream_ = 0;
};
 