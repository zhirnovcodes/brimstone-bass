#pragma once

// How the osc mix knob blends oscillator 1 and 2. Three curves, chosen by the mix type parameter.

#include <algorithm>
#include <cmath>

namespace bb {

enum class MixType : int {
    Linear = 0,     // 1-x / x. 0 dB at 0.5 for correlated sources, -3 dB for unrelated.
    EqualPower = 1, // cos / sin. +3 dB / 0 dB. For different samples or pitches.
    BothFull = 2,   // both at full level in the middle. +6 dB / +3 dB. "More is louder".
};

struct MixGains {
    float a = 1.0f; // oscillator 1
    float b = 0.0f; // oscillator 2
};

inline MixGains oscMixGains(float x, MixType type) {
    const float t = std::clamp(x, 0.0f, 1.0f);
    switch (type) {
    case MixType::Linear:
        return {1.0f - t, t};
    case MixType::BothFull:
        return {std::min(1.0f, 2.0f * (1.0f - t)), std::min(1.0f, 2.0f * t)};
    case MixType::EqualPower:
    default: {
        constexpr float kHalfPi = 1.57079632679f;
        return {std::cos(t * kHalfPi), std::sin(t * kHalfPi)};
    }
    }
}

} // namespace bb
