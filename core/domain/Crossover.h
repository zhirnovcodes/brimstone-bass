#pragma once

// Splits an oscillator at 120 Hz: the lows stay mono and dry, the highs go on to unison, effects and pan.
//
// Linkwitz-Riley, 4th order (24 dB/oct): each band is two Butterworth 2-pole stages in a row (TPT state-variable
// filters). The two bands are in phase at every frequency, so low + high is flat: what the split does not
// change, the sum gives back (as an all-pass: same level, a gentle phase turn around 120 Hz).
// The low band is taken from the mid (L + R) / 2, so everything below the split is mono.

#include <ivandaw/fx/Sample.h>

#include <algorithm>
#include <cmath>

namespace bb {

using ivandaw::fx::Sample;

class Crossover {
public:
    static constexpr double kSplitHz = 120.0;

    struct Bands {
        float low = 0.0f; // mono, both sides get it
        Sample high;
    };

    explicit Crossover(double sampleRate, double splitHz = kSplitHz) {
        const double g = std::tan(3.14159265358979323846 * std::min(splitHz, sampleRate * 0.45) / sampleRate);
        const double k = 1.41421356237309504880; // Butterworth: Q = 1/sqrt(2)
        a1_ = static_cast<float>(1.0 / (1.0 + g * (g + k)));
        a2_ = static_cast<float>(g * a1_);
        a3_ = static_cast<float>(g * a2_);
        k_ = static_cast<float>(k);
    }

    Bands process(Sample in) {
        Bands out;
        const float mid = 0.5f * (in.l + in.r);
        out.low = lowpass(low_[1], lowpass(low_[0], mid));
        out.high.l = highpass(highL_[1], highpass(highL_[0], in.l));
        out.high.r = highpass(highR_[1], highpass(highR_[0], in.r));
        return out;
    }

    void reset() {
        for (State* bank : {low_, highL_, highR_}) {
            bank[0] = {};
            bank[1] = {};
        }
    }

private:
    struct State {
        float ic1 = 0.0f;
        float ic2 = 0.0f;
    };

    // One TPT SVF step (Zavalishin); returns {band, low} through v1, v2.
    void tick(State& s, float x, float& v1, float& v2) const {
        const float v3 = x - s.ic2;
        v1 = a1_ * s.ic1 + a2_ * v3;
        v2 = s.ic2 + a2_ * s.ic1 + a3_ * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
    }

    float lowpass(State& s, float x) const {
        float v1, v2;
        tick(s, x, v1, v2);
        return v2;
    }

    float highpass(State& s, float x) const {
        float v1, v2;
        tick(s, x, v1, v2);
        return x - k_ * v1 - v2;
    }

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f, k_ = 0.0f;
    State low_[2];
    State highL_[2];
    State highR_[2];
};

} // namespace bb
