#include "AdsrEnv.h"
#include "Crossover.h"
#include "OscBus.h"
#include "TestKit.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace bb;

namespace {

constexpr double kRate = 48000.0;
constexpr float kDt = static_cast<float>(1.0 / kRate);
constexpr double kPi = 3.14159265358979323846;

// --- ADSR ---------------------------------------------------------------------------------------------------

int framesUntil(AdsrEnv& env, const AdsrEnv::Times& t, float level, bool rising, int limit = 1000000) {
    for (int n = 1; n <= limit; ++n) {
        const float g = env.next(t);
        if (rising ? g >= level : g <= level) {
            return n;
        }
    }
    return -1;
}

void adsr() {
    AdsrEnv::Times t{0.1, 0.2, 0.5f, kRate};
    AdsrEnv env;
    CHECK(env.idle());
    env.start(t);
    CHECK(std::abs(framesUntil(env, t, 1.0f, true) - 4800) <= 1); // 0 → 1 in exactly the attack
    CHECK(std::abs(framesUntil(env, t, 0.5f, false) - 9600) <= 1); // 1 → sustain in exactly the decay
    for (int i = 0; i < 1000; ++i) {
        CHECK_NEAR(env.next(t), 0.5, 1e-6); // holds the sustain level
    }
    // The sustain level is live: a sustain move while held is heard.
    t.sustain = 0.8f;
    CHECK_NEAR(env.next(t), 0.8, 1e-6);

    // Release: from the gain reached, in exactly its length, and it says how long.
    CHECK_NEAR(env.release(0.3, kRate), 0.3, 1e-9);
    CHECK(env.releasing());
    CHECK(std::abs(framesUntil(env, t, 0.0f, false) - 14400) <= 1);
    CHECK(env.idle());
    CHECK_NEAR(env.release(0.3, kRate), 0.0, 0.0); // releasing an idle envelope does nothing

    // Release from mid-attack: lower, not shorter.
    env.start(t);
    for (int i = 0; i < 2400; ++i) {
        env.next(t);
    }
    CHECK_NEAR(env.gain(), 0.5, 1e-3);
    env.release(0.1, kRate);
    CHECK_NEAR(env.next(t), env.gain(), 0.0);
    CHECK_NEAR(env.gain(), 0.5 - 0.5 / 4800, 1e-4); // one release step down (float steps drift ~2e-5)
    // Retrigger during the release: rises from where it is, no jump.
    for (int i = 0; i < 2400; ++i) {
        env.next(t);
    }
    const float before = env.gain();
    env.start(t);
    const float after = env.next(t);
    CHECK(after > before && after - before < 0.001f);

    // Kill: 5 ms whatever the release knob says.
    env.release(8.0, kRate);
    CHECK_NEAR(env.kill(kRate), AdsrEnv::kKillSeconds, 1e-9);
    CHECK(std::abs(framesUntil(env, t, 0.0f, false) - 240) <= 1);

    // Sustain 0: silent after the decay, but the voice stays held (not idle) until note off.
    AdsrEnv pluck;
    AdsrEnv::Times p{0.01, 0.05, 0.0f, kRate};
    pluck.start(p);
    for (int i = 0; i < 9600; ++i) {
        pluck.next(p);
    }
    CHECK_NEAR(pluck.gain(), 0.0, 0.0);
    CHECK(!pluck.idle());

    // Zero-length stages jump straight on.
    AdsrEnv instant;
    AdsrEnv::Times z{0.0, 0.0, 0.7f, kRate};
    instant.start(z);
    CHECK_NEAR(instant.next(z), 0.7, 1e-6);
}

// --- Crossover ----------------------------------------------------------------------------------------------

struct Levels {
    double in = 0.0, low = 0.0, high = 0.0, sum = 0.0, side = 0.0;
};

// RMS of a stereo sine through the crossover, over 1 s after a 0.5 s settle. side = R is -L (pure stereo width).
Levels measure(double hz, bool side = false) {
    Crossover x(kRate);
    Levels lv;
    const int settle = static_cast<int>(kRate * 0.5), n = static_cast<int>(kRate);
    for (int i = 0; i < settle + n; ++i) {
        const float s = static_cast<float>(std::sin(2.0 * kPi * hz * i / kRate));
        const Crossover::Bands b = x.process({s, side ? -s : s});
        if (i >= settle) {
            lv.in += s * s;
            lv.low += b.low * b.low;
            lv.high += b.high.l * b.high.l;
            lv.sum += (b.low + b.high.l) * (b.low + b.high.l);
        }
    }
    for (double* v : {&lv.in, &lv.low, &lv.high, &lv.sum}) {
        *v = std::sqrt(*v / n);
    }
    return lv;
}

double db(double ratio) { return 20.0 * std::log10(std::max(ratio, 1e-12)); }

void crossover() {
    // Low + high gives the input back at every frequency (flat to 0.05 dB).
    double worst = 0.0;
    for (double hz : {20.0, 40.0, 80.0, 120.0, 180.0, 300.0, 1000.0, 5000.0, 12000.0}) {
        const Levels lv = measure(hz);
        worst = std::max(worst, std::fabs(db(lv.sum / lv.in)));
    }
    NOTE("crossover: low + high deviates at most %.3f dB from the input\n", worst);
    CHECK(worst < 0.05);

    // Both bands -6 dB at the split (Linkwitz-Riley), 24 dB/oct past it.
    const Levels at = measure(120.0);
    CHECK_NEAR(db(at.low / at.in), -6.02, 0.1);
    CHECK_NEAR(db(at.high / at.in), -6.02, 0.1);
    const Levels bass = measure(40.0);
    const Levels mids = measure(1000.0);
    NOTE("crossover: 40 Hz high band %.1f dB, 1 kHz low band %.1f dB\n", db(bass.high / bass.in), db(mids.low / mids.in));
    CHECK(db(bass.low / bass.in) > -0.2);
    CHECK(db(bass.high / bass.in) < -35.0);
    CHECK(db(mids.low / mids.in) < -70.0);
    CHECK(db(mids.high / mids.in) > -0.01);

    // Stereo width below the split is gone: the low band is mono (taken from L + R).
    const Levels width = measure(40.0, true);
    CHECK(db(width.low / width.in) < -100.0);
}

// --- Bus: what reaches the lows and the highs -----------------------------------------------------------------

struct Out {
    double l = 0.0, r = 0.0, diff = 0.0;
};

// A stereo sine through a bus for 1.5 s; RMS of the last second.
Out runBus(OscBus& bus, double hz, bool side = false) {
    Out o;
    const int settle = static_cast<int>(kRate * 0.5), n = static_cast<int>(kRate);
    for (int i = 0; i < settle + n; ++i) {
        const float s = 0.5f * static_cast<float>(std::sin(2.0 * kPi * hz * i / kRate));
        const Sample y = bus.process({s, side ? -s : s}, kDt);
        if (i >= settle) {
            o.l += y.l * y.l;
            o.r += y.r * y.r;
            o.diff += (y.l - y.r) * (y.l - y.r);
        }
    }
    o.l = std::sqrt(o.l / n);
    o.r = std::sqrt(o.r / n);
    o.diff = std::sqrt(o.diff / n);
    return o;
}

std::unique_ptr<OscBus> wetBus() {
    auto bus = std::make_unique<OscBus>(kRate);
    // Effects that would change any signal they touch: a closed low-pass, a full-wet delay and reverb.
    bus->fx.filter.type = ivandaw::fx::FilterType::Low;
    bus->fx.filter.filterPosition = 0.0f; // 20 Hz
    bus->fx.filter.mix = 1.0f;
    bus->fx.delay.delayTime = 0.1f;
    bus->fx.delay.feedback = 0.0f;
    bus->fx.delay.mix = 1.0f;
    bus->fx.reverb.mix = 1.0f;
    bus->unison.voices = 4;
    bus->unison.param = 50.0f;
    bus->unison.mix = 1.0f;
    return bus;
}

void busBands() {
    const double sine = 0.5 / std::sqrt(2.0); // RMS of the input

    // 40 Hz: below the split, untouched by the effects, unison and pan; and mono.
    {
        auto bus = wetBus();
        bus->pan = panGains(-1.0f); // hard left: would silence the right side of anything panned
        const Out o = runBus(*bus, 40.0);
        NOTE("bus 40 Hz: L %.4f R %.4f (input %.4f)\n", o.l, o.r, sine);
        CHECK_NEAR(o.l, sine, sine * 0.03);
        CHECK_NEAR(o.r, sine, sine * 0.03);
        CHECK(o.diff < sine * 0.03);
        // Stereo width below 120 Hz collapses to mono (out of phase cancels).
        auto mono = wetBus();
        CHECK(runBus(*mono, 40.0, true).l < sine * 0.03);
    }
    // 3 kHz: above the split, through the effects (the closed low-pass kills it) ...
    {
        auto bus = wetBus();
        const Out o = runBus(*bus, 3000.0);
        NOTE("bus 3 kHz through a closed low-pass: %.5f\n", o.l);
        CHECK(o.l < sine * 0.01);
    }
    // ... and the pan, which touches only the highs.
    {
        auto bus = std::make_unique<OscBus>(kRate); // effects dry
        bus->fx.filter.mix = 0.0f;
        bus->fx.delay.mix = 0.0f;
        bus->fx.reverb.mix = 0.0f;
        bus->pan = panGains(-1.0f);
        const Out o = runBus(*bus, 3000.0);
        CHECK_NEAR(o.l, sine, sine * 0.02);
        CHECK(o.r < sine * 0.01);
        // Volume is on the whole oscillator, after the bands are summed.
        bus->pan = panGains(0.0f);
        bus->vol = 0.5f;
        CHECK_NEAR(runBus(*bus, 3000.0).l, sine * 0.5, sine * 0.02);
        CHECK_NEAR(runBus(*bus, 40.0).l, sine * 0.5, sine * 0.02);
        // Dry effects, centre pan: the bus gives the input back across the split.
        bus->vol = 1.0f;
        for (double hz : {60.0, 120.0, 250.0}) {
            CHECK_NEAR(runBus(*bus, hz).l, sine, sine * 0.01);
        }
    }
}

} // namespace

void pipelineTests() {
    adsr();
    crossover();
    busBands();
}
