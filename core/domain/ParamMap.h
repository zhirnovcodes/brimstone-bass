#pragma once

// Conversions from knob values (0..1) to what the effects and voices take.
// The web UI mirrors the labels in apps/web/src/display.ts.

#include <algorithm>
#include <cmath>

namespace bb {

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float invLerp(float a, float b, float v) {
    return b == a ? 0.0f : std::clamp((v - a) / (b - a), 0.0f, 1.0f);
}

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// Beat divisions, short to long. "t" = triplet = x2/3. Values are in beats.
// Delay: 1/16t .. 4 (the first 14). LFO: 1/16t .. 32 (all 20; 16t / 16 = 4 bars, 32t / 32 = 8 bars of 4/4).
inline constexpr int kDelayDivisions = 14;
inline constexpr int kLfoDivisions = 20;
inline constexpr double kDivisionBeats[kLfoDivisions] = {
    1.0 / 24, 1.0 / 16, // 1/16t, 1/16
    1.0 / 12, 1.0 / 8,  // 1/8t, 1/8
    1.0 / 6, 1.0 / 4,   // 1/4t, 1/4
    1.0 / 3, 1.0 / 2,   // 1/2t, 1/2
    2.0 / 3, 1.0,       // 1t, 1
    4.0 / 3, 2.0,       // 2t, 2
    8.0 / 3, 4.0,       // 4t, 4
    16.0 / 3, 8.0,      // 8t, 8
    32.0 / 3, 16.0,     // 16t, 16
    64.0 / 3, 32.0,     // 32t, 32
};

// A knob 0..1 picks one of `count` divisions.
inline int divisionIndex(float knob, int count) {
    return static_cast<int>(std::lround(clamp01(knob) * static_cast<float>(count - 1)));
}

inline double delayBeats(float knob) { return kDivisionBeats[divisionIndex(knob, kDelayDivisions)]; }
inline double lfoBeats(float knob) { return kDivisionBeats[divisionIndex(knob, kLfoDivisions)]; }

// The delay line holds 4 beats at 30 BPM. Slower tempos are clamped for the delay only.
inline constexpr double kMinDelayBpm = 30.0;
inline constexpr float kMaxDelaySeconds = 8.0f;

inline float delaySeconds(float knob, double bpm) {
    const double b = std::max(bpm, kMinDelayBpm);
    return static_cast<float>(delayBeats(knob) * 60.0 / b);
}

// LFO period when not synced: 0.1 .. 8 s, log scale.
inline double lfoFreeSeconds(float knob) { return 0.1 * std::pow(80.0, static_cast<double>(clamp01(knob))); }

inline float attackSeconds(float knob) { return lerp(0.01f, 1.0f, clamp01(knob)); }
inline float decaySeconds(float knob) { return lerp(0.01f, 8.0f, clamp01(knob)); }
inline float releaseSeconds(float knob) { return lerp(0.01f, 8.0f, clamp01(knob)); }

inline float reverbDecaySeconds(float feedback) { return lerp(0.05f, 30.0f, clamp01(feedback)); }
inline float filterRes(float dryWet) { return lerp(0.0f, 0.5f, clamp01(dryWet)); }
inline float filterMix(float dryWet) { return clamp01(dryWet) * 0.88f; }
inline float unisonCents(float value) { return clamp01(value) * 100.0f; }

// Oscillator pitch: octave offset (-4..+2) and detune (-24..+24 cents) as a playback-rate factor.
inline double pitchRatio(int octave, float cents) {
    return std::exp2(static_cast<double>(octave) + static_cast<double>(cents) / 1200.0);
}

// Stereo balance for a stereo source: centre = unity on both sides, a side fades as the pan moves away from it.
struct PanGains {
    float l = 1.0f;
    float r = 1.0f;
};
inline PanGains panGains(float pan) {
    const float p = std::clamp(pan, -1.0f, 1.0f);
    return {std::min(1.0f, 1.0f - p), std::min(1.0f, 1.0f + p)};
}

} // namespace bb
