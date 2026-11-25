#include "Lfo.h"
#include "OscMix.h"
#include "ParamMap.h"
#include "ParamsJson.h"
#include "TestKit.h"

#include <algorithm>
#include <cstring>
#include <vector>

using namespace bb;

namespace {

void paramsTable() {
    static_assert(param::count == 57, "params.json has 57 entries");
    int lfoTargets = 0;
    for (const ParamSpec& p : kParams) {
        CHECK(p.min <= p.max);
        CHECK(p.def >= p.min && p.def <= p.max);
        lfoTargets += p.lfo ? 1 : 0;
    }
    // The LFO map choices list exactly these.
    CHECK(lfoTargets == 41);
    CHECK(kParams[param::lfo1Map].max == lfoTargets - 1);
    CHECK(kParams[param::lfo2Map].max == lfoTargets - 1);
    CHECK(kParams[param::lfo3Map].max == lfoTargets - 1);
    CHECK(kParams[param::osc1Detune].min == -24.0 && kParams[param::osc1Detune].max == 24.0);
    CHECK(kParams[param::osc2Oct].min == -2.0 && kParams[param::osc2Oct].max == 2.0);
    CHECK(std::strcmp(kParams[param::unisonValue].id, "unisonValue") == 0);
}

void conversions() {
    CHECK_NEAR(delayBeats(0.0f), 1.0 / 24, 1e-12);
    CHECK_NEAR(delayBeats(1.0f), 4.0, 1e-12);
    CHECK_NEAR(delayBeats(9.0f / 13.0f), 1.0, 1e-12);
    CHECK_NEAR(lfoBeats(1.0f), 32.0, 1e-12);                        // 8 bars
    CHECK_NEAR(lfoBeats(18.0f / 19.0f), 64.0 / 3.0, 1e-12);         // 8 bars, triplet
    CHECK_NEAR(lfoBeats(17.0f / 19.0f), 16.0, 1e-12);               // 4 bars
    CHECK_NEAR(lfoBeats(9.0f / 19.0f), 1.0, 1e-12);
    CHECK_NEAR(lfoBeats(static_cast<float>(kParams[param::lfo1Time].def)), 1.0, 1e-12);
    CHECK_NEAR(pitchRatio(0, 0.0f), 1.0, 1e-12);
    CHECK_NEAR(pitchRatio(-2, 0.0f), 0.25, 1e-12);
    CHECK_NEAR(pitchRatio(1, 24.0f), 2.0 * std::exp2(24.0 / 1200.0), 1e-9);
    CHECK_NEAR(delaySeconds(1.0f, 120.0), 2.0, 1e-6);
    CHECK_NEAR(delaySeconds(1.0f, 10.0), kMaxDelaySeconds, 1e-6); // clamped to 30 BPM
    CHECK_NEAR(lfoFreeSeconds(0.0f), 0.1, 1e-9);
    CHECK_NEAR(lfoFreeSeconds(1.0f), 8.0, 1e-9);
    CHECK_NEAR(attackSeconds(0.0f), 0.01, 1e-6);
    CHECK_NEAR(attackSeconds(1.0f), 1.0, 1e-6);
    CHECK_NEAR(releaseSeconds(1.0f), 8.0, 1e-6);
    CHECK_NEAR(decaySeconds(0.0f), 0.01, 1e-6);
    CHECK_NEAR(decaySeconds(1.0f), 8.0, 1e-6);
    CHECK_NEAR(reverbDecaySeconds(0.0f), 0.05, 1e-6);
    CHECK_NEAR(reverbDecaySeconds(1.0f), 30.0, 1e-5);
    CHECK_NEAR(filterRes(1.0f), 0.5, 1e-6);
    CHECK_NEAR(filterMix(1.0f), 0.88, 1e-6);
    CHECK_NEAR(unisonCents(0.15f), 15.0, 1e-4);
    CHECK_NEAR(panGains(0.0f).l, 1.0, 1e-9);
    CHECK_NEAR(panGains(0.0f).r, 1.0, 1e-9);
    CHECK_NEAR(panGains(-1.0f).r, 0.0, 1e-9);
    CHECK_NEAR(panGains(0.5f).l, 0.5, 1e-6);
}

void oscMix() {
    CHECK_NEAR(oscMixGains(0.5f, MixType::Linear).a, 0.5, 1e-6);
    CHECK_NEAR(oscMixGains(0.5f, MixType::EqualPower).a, 0.70710678, 1e-5);
    CHECK_NEAR(oscMixGains(0.5f, MixType::EqualPower).b, 0.70710678, 1e-5);
    CHECK_NEAR(oscMixGains(0.5f, MixType::BothFull).a, 1.0, 1e-6);
    CHECK_NEAR(oscMixGains(0.5f, MixType::BothFull).b, 1.0, 1e-6);
    for (MixType t : {MixType::Linear, MixType::EqualPower, MixType::BothFull}) {
        CHECK_NEAR(oscMixGains(0.0f, t).a, 1.0, 1e-6);
        CHECK_NEAR(oscMixGains(0.0f, t).b, 0.0, 1e-6);
        CHECK_NEAR(oscMixGains(1.0f, t).a, 0.0, 1e-6);
        CHECK_NEAR(oscMixGains(1.0f, t).b, 1.0, 1e-6);
    }
}

void lfo() {
    // Synced, period one beat (knob 9/19), sine: 0.5 at 0, 1 at 1/4, 0.5 at 1/2, 0 at 3/4.
    Lfo l(1);
    LfoInput in;
    in.shape = LfoShape::Sine;
    in.sync = true;
    in.time = 9.0f / 19.0f;
    CHECK_NEAR(l.tick(in, 0.0, 120.0, 0.001), 0.5, 1e-5);
    CHECK_NEAR(l.tick(in, 0.25, 120.0, 0.001), 1.0, 1e-5);
    CHECK_NEAR(l.tick(in, 0.5, 120.0, 0.001), 0.5, 1e-5);
    CHECK_NEAR(l.tick(in, 0.75, 120.0, 0.001), 0.0, 1e-5);
    CHECK_NEAR(l.tick(in, 7.25, 120.0, 0.001), 1.0, 1e-5); // locked to the grid, not accumulated

    // Offset 0.25 = 90 degrees.
    in.offset = 0.25f;
    CHECK_NEAR(l.tick(in, 0.0, 120.0, 0.001), 1.0, 1e-5);
    in.offset = 0.0f;

    // Retrig: the phase restarts at the note's beat.
    in.retrig = true;
    l.retrig(3.3);
    CHECK_NEAR(l.tick(in, 3.3, 120.0, 0.001), 0.5, 1e-5);
    CHECK_NEAR(l.tick(in, 3.55, 120.0, 0.001), 1.0, 1e-5);

    CHECK_NEAR(Lfo::shape(LfoShape::Tri, 0.0), 0.5, 1e-6);
    CHECK_NEAR(Lfo::shape(LfoShape::Tri, 0.25), 1.0, 1e-6);
    CHECK_NEAR(Lfo::shape(LfoShape::Tri, 0.75), 0.0, 1e-6);
    CHECK_NEAR(Lfo::shape(LfoShape::Square, 0.1), 1.0, 1e-6);
    CHECK_NEAR(Lfo::shape(LfoShape::Square, 0.6), 0.0, 1e-6);

    // Free: 1 s period (knob for 1 s), a quarter second = a quarter cycle.
    Lfo f(2);
    LfoInput free;
    free.sync = false;
    free.shape = LfoShape::Sine;
    free.time = static_cast<float>(std::log(10.0) / std::log(80.0)); // 0.1 * 80^x = 1
    float v = 0.0f;
    for (int i = 0; i < 250; ++i) {
        v = f.tick(free, 0.0, 120.0, 0.001);
    }
    CHECK_NEAR(v, 1.0, 1e-3);

    // Rand stays in range and moves.
    Lfo r(3);
    LfoInput rand;
    rand.shape = LfoShape::Rand;
    rand.sync = false;
    rand.time = 0.3f;
    float lo = 1.0f, hi = 0.0f;
    for (int i = 0; i < 100000; ++i) {
        const float x = r.tick(rand, 0.0, 120.0, 32.0 / 48000.0);
        lo = std::min(lo, x);
        hi = std::max(hi, x);
    }
    CHECK(lo >= 0.0f && hi <= 1.0f);
    CHECK(hi - lo > 0.3f);
}

} // namespace

void domainTests() {
    paramsTable();
    conversions();
    oscMix();
    lfo();
}
