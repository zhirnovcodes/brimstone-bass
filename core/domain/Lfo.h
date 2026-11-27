#pragma once

// One global LFO. Output 0..1. Advanced once per control tick by the engine.

#include "Rng.h"

#include <cstdint>

namespace bb {

enum class LfoShape : std::uint8_t { Tri = 0, Square = 1, Sine = 2, Rand = 3, SawSoft = 4, Saw = 5, SawInv = 6, SawSoftInv = 7 };

// Saw: 1 falling to 0 each cycle. Saw inv: 0 rising to 1.

// Saw soft: a rising saw whose ramp is a tanh curve, steep at the start, easing into the top, then the drop.
// Saw soft inv: the same upside down: 1 falling fast, easing into 0, then the jump back to 1.
// Higher = more curved.
inline constexpr double kSawSoftCurve = 3.0;

struct LfoInput {
    LfoShape shape = LfoShape::Sine;
    bool sync = true;     // period in beats from the transport, else in seconds
    bool retrig = false;  // phase restarts on every note on
    float time = 0.6f;    // knob 0..1: a beat division when synced, 0.1..8 s when free
    float offset = 0.0f;  // 0..1 = 0..360 degrees, added to the phase. No effect on Rand.
};

class Lfo {
public:
    explicit Lfo(std::uint32_t seed) : rng_(seed) {}

    // beat = the engine's beat clock now; elapsed = seconds since the previous tick.
    float tick(const LfoInput& in, double beat, double bpm, double elapsed);

    // A note on, with retrig on: the phase restarts at the offset.
    void retrig(double beat);

    // The value one shape gives at phase 0..1 (the phase already includes the offset).
    static float shape(LfoShape shape, double phase);

private:
    Rng rng_;
    double freePhase_ = 0.0;
    double retrigBeat_ = 0.0;
    float walk_ = 0.5f;   // Rand: brown-noise walk
    float smooth_ = 0.5f; // Rand: the walk, smoothed
};

} // namespace bb
