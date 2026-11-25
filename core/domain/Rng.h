#pragma once

#include <cstdint>

namespace bb {

// xorshift32. Deterministic for a seed, so tests and both hosts get the same random LFO.
class Rng {
public:
    explicit Rng(std::uint32_t seed) : state_(seed != 0 ? seed : 0x9E3779B9u) {}

    std::uint32_t next() {
        std::uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    // -1 .. 1
    float bipolar() { return static_cast<float>(next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }

private:
    std::uint32_t state_;
};

} // namespace bb
