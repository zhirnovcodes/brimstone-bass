#pragma once

// One oscillator's bus. The voices arrive already enveloped (sample player x ADSR x velocity) and summed; then:
//
//   in -> Crossover (120 Hz) -+-> low (mono) ------------------------------------+-> x vol -> x gate -> out
//                             +-> high -> Unison -> Chain[order] -> pan ---------+
//
// So the bass under 120 Hz is always mono and dry, and unison, delay, reverb, filter and pan shape only what is
// above it. The filters are linear, so splitting the voices' sum is the same as splitting every voice.
//
// The gate fades the bus in and out over 5 ms. Turning the oscillator on or off, changing its sample or its
// effect order "resets effects": the gate closes, the crossover, Unison and the chain are reset at silence, then
// it reopens.

#include "ChainStage.h"
#include "Crossover.h"
#include "ParamMap.h"

#include <ivandaw/fx.h>

namespace bb {

class OscBus {
public:
    static constexpr float kGateSeconds = 0.005f;

    // Resolved by the engine every control tick.
    ChainParams fx;
    ivandaw::fx::UnisonParams unison; // voices 0 = Unison bypassed
    PanGains pan;                     // on the highs only
    float vol = 1.0f;                 // on the whole oscillator, after the bands are summed

    explicit OscBus(double sampleRate) : crossover_(sampleRate), unison_(sampleRate), chain_(sampleRate, fx) {
        unison.type = ivandaw::fx::UnisonType::Detune;
        unison.voices = 0;
    }

    void setOn(bool on) {
        if (on != on_) {
            on_ = on;
            resetPending_ = true;
        }
    }
    bool on() const { return on_; }

    void requestReset() { resetPending_ = true; }

    // Applied with a reset, at silence.
    void setOrder(const int order[kEffectCount]) {
        if (!isPermutation(order)) {
            return;
        }
        for (int i = 0; i < kEffectCount; ++i) {
            pendingOrder_[i] = order[i];
        }
        hasPendingOrder_ = true;
        resetPending_ = true;
    }
    // The order the UI shows: the pending one if a change is waiting for silence.
    int order(int position) const { return hasPendingOrder_ ? pendingOrder_[position] : chain_.order(position); }

    // Off and fully faded out: the voices may skip this oscillator.
    bool silent() const { return !on_ && gate_ == 0.0f && !resetPending_; }

    // True once, right after a reset happened (the engine swaps samples then).
    bool takeJustReset() {
        const bool was = justReset_;
        justReset_ = false;
        return was;
    }

    Sample process(Sample in, float dt) {
        const float target = (on_ && !resetPending_) ? 1.0f : 0.0f;
        const float step = dt / kGateSeconds;
        if (gate_ < target) {
            gate_ = gate_ + step < target ? gate_ + step : target;
        } else if (gate_ > target) {
            gate_ = gate_ - step > target ? gate_ - step : target;
        }
        if (gate_ == 0.0f && resetPending_) {
            crossover_.reset();
            unison_.Reset();
            chain_.Reset();
            if (hasPendingOrder_) {
                chain_.setOrder(pendingOrder_);
                hasPendingOrder_ = false;
            }
            resetPending_ = false;
            justReset_ = true;
        }
        if (gate_ == 0.0f) {
            return {};
        }
        const Crossover::Bands bands = crossover_.process(in);
        Sample high = bands.high;
        if (unison.voices > 0) {
            high = unison_.Apply(unison, high, dt);
        }
        high = chain_.Apply(high, dt);
        const float g = vol * gate_;
        return {(bands.low + high.l * pan.l) * g, (bands.low + high.r * pan.r) * g};
    }

private:
    Crossover crossover_;
    ivandaw::fx::Unison unison_;
    Chain chain_;
    float gate_ = 1.0f;
    bool on_ = true;
    bool resetPending_ = false;
    bool justReset_ = false;
    int pendingOrder_[kEffectCount] = {};
    bool hasPendingOrder_ = false;
};

} // namespace bb
