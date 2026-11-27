#include "Lfo.h"

#include "ParamMap.h"

#include <cmath>

namespace bb {

namespace {

double frac(double x) { return x - std::floor(x); }

} // namespace

float Lfo::shape(LfoShape shape, double phase) {
    const double p = frac(phase);
    switch (shape) {
    case LfoShape::Tri:
        // Starts at 0.5 rising, 1 at a quarter, 0 at three quarters.
        if (p < 0.25) {
            return static_cast<float>(0.5 + 2.0 * p);
        }
        if (p < 0.75) {
            return static_cast<float>(1.5 - 2.0 * p);
        }
        return static_cast<float>(2.0 * p - 1.5);
    case LfoShape::Square:
        return p < 0.5 ? 1.0f : 0.0f;
    case LfoShape::Saw:
        return static_cast<float>(1.0 - p);
    case LfoShape::SawInv:
        return static_cast<float>(p);
    case LfoShape::SawSoftInv:
        return static_cast<float>(1.0 - std::tanh(kSawSoftCurve * p) / std::tanh(kSawSoftCurve));
    case LfoShape::SawSoft:
        // 0 at the start of the cycle, rising fast, flattening toward 1, then back to 0.
        return static_cast<float>(std::tanh(kSawSoftCurve * p) / std::tanh(kSawSoftCurve));
    case LfoShape::Sine:
    default:
        return static_cast<float>(0.5 + 0.5 * std::sin(2.0 * 3.14159265358979323846 * p));
    }
}

void Lfo::retrig(double beat) {
    retrigBeat_ = beat;
    freePhase_ = 0.0;
}

float Lfo::tick(const LfoInput& in, double beat, double bpm, double elapsed) {
    const double safeBpm = bpm > 0.0 ? bpm : 120.0;
    const double periodSeconds = in.sync ? lfoBeats(in.time) * 60.0 / safeBpm : lfoFreeSeconds(in.time);

    // The free phase always runs, so switching sync off continues smoothly.
    freePhase_ = frac(freePhase_ + elapsed / periodSeconds);

    if (in.shape == LfoShape::Rand) {
        // Brown noise: a random walk whose spread over one period is about the full range,
        // kept inside 0..1 by reflection, then smoothed so it glides instead of stepping.
        const double step = std::sqrt(elapsed / periodSeconds) * 0.5;
        walk_ += static_cast<float>(rng_.bipolar() * step);
        if (walk_ > 1.0f) {
            walk_ = 2.0f - walk_;
        }
        if (walk_ < 0.0f) {
            walk_ = -walk_;
        }
        walk_ = clamp01(walk_);
        const double k = 1.0 - std::exp(-elapsed / (periodSeconds * 0.125));
        smooth_ += static_cast<float>((walk_ - smooth_) * k);
        return smooth_;
    }

    double phase;
    if (in.sync) {
        // From the beat clock, never accumulated, so it stays on the grid after loops and tempo changes.
        const double beats = lfoBeats(in.time);
        phase = (in.retrig ? beat - retrigBeat_ : beat) / beats;
    } else {
        phase = freePhase_;
    }
    return shape(in.shape, phase + static_cast<double>(in.offset));
}

} // namespace bb
