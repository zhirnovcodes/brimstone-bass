#include "Engine.h"
#include "Rng.h"
#include "TestKit.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace bb;

namespace {

constexpr double kRate = 48000.0;

// A 4 s tone at C4 (MIDI 60): attack 0.11 s, loop 0.11 .. 3.5 s. Its last 0.3 s (the tail) is an octave up,
// so a test can hear when the tail plays.
struct TestSample {
    std::vector<float> left, right;
    SampleData data;
    SampleMeta meta;
    TestSample() {
        const int frames = static_cast<int>(kRate * 4.0);
        left.resize(frames);
        right.resize(frames);
        const int tail = frames - static_cast<int>(kRate * 0.3);
        for (int i = 0; i < frames; ++i) {
            const double t = i / kRate;
            const double hz = i < tail ? 261.6256 : 523.2511;
            const float x = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * hz * t));
            left[i] = x;
            right[i] = x;
        }
        data.left = left.data();
        data.right = right.data();
        data.frames = frames;
        data.rate = kRate;
        meta.attack = 0.11;
        meta.loop = 3.39;
        meta.release = 0.0; // the tail tests turn it on
    }
};

const TestSample& sample() {
    static TestSample s;
    return s;
}

std::unique_ptr<Engine> makeEngine() {
    auto e = std::make_unique<Engine>(kRate, 1234u);
    e->setSample(0, sample().data, sample().meta, 60);
    return e;
}

// Effects off, osc 2 off, fast envelope: what the voice plays, plainly.
void dry(Engine& e) {
    for (int i : {param::filterDw1, param::reverbDw1, param::delayDw1, param::filterDw2, param::reverbDw2, param::delayDw2}) {
        e.setParam(i, 0.0);
    }
    e.setParam(param::osc2On, 0.0);
    e.setParam(param::oscMix, 0.0);
    e.setParam(param::osc1Attack, 0.0);
    e.setParam(param::osc1Release, 0.0);
}

struct Render {
    std::vector<float> l, r;
};

// Renders `frames`, calling `at(frame)` before each block. Events go to the engine at their exact frame.
template <typename Script>
Render render(Engine& e, int frames, int block, Script script) {
    Render out;
    out.l.assign(frames, 0.0f);
    out.r.assign(frames, 0.0f);
    OutBuffer ob;
    for (int start = 0; start < frames; start += block) {
        const int n = std::min(block, frames - start);
        script(e, start, n);
        e.process(n, out.l.data() + start, out.r.data() + start, ob);
    }
    return out;
}

double rms(const std::vector<float>& x, int from, int to) {
    double sum = 0.0;
    for (int i = from; i < to; ++i) {
        sum += static_cast<double>(x[i]) * x[i];
    }
    return to > from ? std::sqrt(sum / (to - from)) : 0.0;
}

// Calls `fn(e, frameInBlock)` when absolute frame `at` falls in [start, start + n).
template <typename Fn>
void at(int frame, int start, int n, Fn fn) {
    if (frame >= start && frame < start + n) {
        fn(frame - start);
    }
}

void noteOnOff() {
    auto e = makeEngine();
    dry(*e);
    const int off = static_cast<int>(kRate * 0.5);
    const Render out = render(*e, static_cast<int>(kRate), 128, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 100); });
        at(off, s, n, [&](int f) { en.midi(f, 0x80, 60, 0); });
    });
    const double attackRms = rms(out.l, 0, static_cast<int>(kRate * 0.010));
    NOTE("note on: rms over the first 10 ms %.4f\n", attackRms);
    CHECK(attackRms > 0.01);
    // Release 0.01 s; 10 ms after it the output is silent.
    const int quiet = off + static_cast<int>(kRate * 0.020);
    CHECK(rms(out.l, quiet, quiet + 4800) < 0.001);
    CHECK(e->activeVoices() == 0);
}

// Counts upward zero crossings to measure the played pitch.
double pitchHz(const std::vector<float>& x, int from, int to) {
    int crossings = 0;
    for (int i = from + 1; i < to; ++i) {
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            ++crossings;
        }
    }
    return crossings / ((to - from) / kRate);
}

// Pitch from the first and last upward zero crossing, interpolated: precise enough for a few cents.
double preciseHz(const std::vector<float>& x, int from, int to) {
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (int i = from + 1; i < to; ++i) {
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = (i - 1) + x[i - 1] / static_cast<double>(x[i - 1] - x[i]);
            first = first < 0.0 ? t : first;
            last = t;
            ++crossings;
        }
    }
    return crossings > 1 ? (crossings - 1) * kRate / (last - first) : 0.0;
}

void pitch() {
    for (int note : {48, 60, 72}) {
        auto e = makeEngine();
        dry(*e);
        const Render out = render(*e, static_cast<int>(kRate), 512, [&](Engine& en, int s, int n) {
            at(0, s, n, [&](int f) { en.midi(f, 0x90, static_cast<std::uint8_t>(note), 127); });
        });
        const double hz = pitchHz(out.l, 4800, 43200);
        const double expected = 261.6256 * std::pow(2.0, (note - 60) / 12.0);
        NOTE("note %d: %.1f Hz (expected %.1f)\n", note, hz, expected);
        CHECK(std::fabs(hz - expected) < expected * 0.01);
    }
}

// Octave offset and detune (Hz added to the note's frequency): before the note, and while it plays (the voice retunes).
void octaveAndDetune() {
    struct Case {
        int oct;
        double detuneHz;
    };
    for (const Case c : {Case{-1, 0.0}, Case{2, 0.0}, Case{0, 100.0}, Case{1, -200.0}, Case{-1, 30.0}}) {
        auto e = makeEngine();
        dry(*e);
        e->setParam(param::osc1Oct, c.oct);
        e->setParam(param::osc1Detune, c.detuneHz);
        const Render out = render(*e, static_cast<int>(kRate * 0.4), 512, [&](Engine& en, int s, int n) {
            at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 127); });
        });
        const double hz = preciseHz(out.l, 9600, 19000); // the first loop pass, past the detune glide
        const double expected = 261.6256 * std::pow(2.0, c.oct) + c.detuneHz;
        NOTE("oct %+d detune %+.0f Hz: %.2f Hz (expected %.2f)\n", c.oct, c.detuneHz, hz, expected);
        CHECK(std::fabs(hz - expected) < expected * 0.001); // 1.7 cents
    }
    // While the note plays: an octave up halfway through.
    auto e = makeEngine();
    dry(*e);
    const int half = static_cast<int>(kRate * 0.5);
    const Render out = render(*e, static_cast<int>(kRate), 128, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 127); });
        at(half, s, n, [&](int) { en.setParam(param::osc1Oct, 1.0); });
    });
    CHECK(std::fabs(pitchHz(out.l, 4800, half) - 261.6256) < 3.0);
    CHECK(std::fabs(pitchHz(out.l, half + 4800, 48000) - 523.2511) < 6.0);
}

// The same events at the same frames give the same samples, whatever the block size.
void blockSizes() {
    auto script = [](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) {
            Transport t;
            t.playing = true;
            t.bpm = 128.0;
            t.beat = 0.0;
            en.transport(t, f);
        });
        at(10, s, n, [&](int f) { en.midi(f, 0x90, 60, 100); });
        at(3000, s, n, [&](int f) { en.midi(f, 0x90, 67, 90); });
        at(7777, s, n, [&](int f) { en.midi(f, 0x90, 64, 80); });
        at(20000, s, n, [&](int f) { en.midi(f, 0x80, 60, 0); });
        at(30001, s, n, [&](int f) { en.midi(f, 0xB0, 123, 0); });
        at(40000, s, n, [&](int f) { en.midi(f, 0x90, 72, 127); });
    };
    auto setup = [](Engine& en) {
        en.setParam(param::lfo1Amp, 1.0);
        en.setParam(param::lfo1Map, 1.0);   // filter position 1
        en.setParam(param::filterDw1, 0.7);
        en.setParam(param::lfo2Amp, 0.8);
        en.setParam(param::lfo2Type, 3.0);  // Rand
        en.setParam(param::lfo2Map, 12.0);  // osc mix
        en.setParam(param::lfo3Amp, 0.5);
        en.setParam(param::lfo3Type, 0.0);  // Tri
        en.setParam(param::lfo3Map, 35.0);  // osc 1 detune
        en.setParam(param::osc2Oct, -1.0);
        en.setParam(param::unisonVoices, 3.0);
        en.setParam(param::unisonValue, 0.4);
        en.setFxOrder(1, kReverb, kDelay, kFilter);
    };
    const int frames = 60000;
    auto reference = makeEngine();
    setup(*reference);
    const Render ref = render(*reference, frames, 4096, script);
    CHECK(rms(ref.l, 0, frames) > 0.01);
    for (int block : {1, 64, 128, 512}) {
        auto e = makeEngine();
        setup(*e);
        const Render out = render(*e, frames, block, script);
        float diff = 0.0f;
        for (int i = 0; i < frames; ++i) {
            diff = std::max({diff, std::fabs(out.l[i] - ref.l[i]), std::fabs(out.r[i] - ref.r[i])});
        }
        if (diff > 1e-6f) {
            NOTE("block %d differs by %.3g\n", block, diff);
        }
        CHECK(diff <= 1e-6f);
    }
}

void allSoundOff() {
    auto e = makeEngine();
    dry(*e);
    const int off = 24000;
    const Render out = render(*e, 48000, 256, [&](Engine& en, int s, int n) {
        for (int k = 0; k < 8; ++k) {
            at(k * 10, s, n, [&](int f) { en.midi(f, 0x90, static_cast<std::uint8_t>(52 + k * 3), 100); });
        }
        at(off, s, n, [&](int f) { en.midi(f, 0xB0, 123, 0); });
        at(off, s, n, [&](int f) { en.midi(f, 0x90, 90, 100); }); // queued after CC 123 at the same frame: dropped
    });
    CHECK(rms(out.l, off - 4800, off) > 0.01);
    const int quiet = off + static_cast<int>(kRate * 0.02);
    CHECK(rms(out.l, quiet, 48000) < 0.001);
    CHECK(e->activeVoices() == 0);
}

void stealing() {
    auto e = makeEngine();
    dry(*e);
    render(*e, 4800, 128, [&](Engine& en, int s, int n) {
        for (int k = 0; k < 9; ++k) {
            at(k * 100, s, n, [&](int f) { en.midi(f, 0x90, static_cast<std::uint8_t>(48 + k), 100); });
        }
    });
    CHECK(e->activeVoices() == 8);
    // The 9th note took the oldest voice: releasing note 48 now leaves 8 voices sounding.
    render(*e, 4800, 128, [&](Engine& en, int s, int n) { at(0, s, n, [&](int f) { en.midi(f, 0x80, 48, 0); }); });
    CHECK(e->activeVoices() == 8);
}

void sustain() {
    auto e = makeEngine();
    dry(*e);
    render(*e, 9600, 128, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0xB0, 64, 127); });
        at(10, s, n, [&](int f) { en.midi(f, 0x90, 60, 100); });
        at(2000, s, n, [&](int f) { en.midi(f, 0x80, 60, 0); });
    });
    CHECK(e->activeVoices() == 1); // held by the pedal
    render(*e, 9600, 128, [&](Engine& en, int s, int n) { at(0, s, n, [&](int f) { en.midi(f, 0xB0, 64, 0); }); });
    CHECK(e->activeVoices() == 0);
}

void fxOrderAndReset() {
    auto e = makeEngine();
    CHECK(e->fxOrder(0, 0) == kFilter && e->fxOrder(0, 1) == kDelay && e->fxOrder(0, 2) == kReverb);
    CHECK(!e->setFxOrder(0, 0, 0, 1));
    CHECK(!e->setFxOrder(2, 0, 1, 2));
    CHECK(e->setFxOrder(1, kReverb, kFilter, kDelay));
    CHECK(e->fxOrder(1, 0) == kReverb && e->fxOrder(1, 2) == kDelay);

    // Turning osc 2 off and on while a note holds: no click.
    float prev = 0.0f, maxStep = 0.0f;
    const Render out = render(*e, 48000, 128, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 100); });
        if (s <= 20000 && 20000 < s + n) {
            en.setParam(param::osc2On, 0.0);
        }
        if (s <= 30000 && 30000 < s + n) {
            en.setParam(param::osc2On, 1.0);
        }
    });
    prev = out.l[14999];
    for (int i = 15000; i < 48000; ++i) {
        maxStep = std::max(maxStep, std::fabs(out.l[i] - prev));
        prev = out.l[i];
    }
    NOTE("osc on/off: largest step %.4f\n", maxStep);
    CHECK(maxStep < 0.08f);
    CHECK(e->tailSeconds() > 0.0);
}

void fuzz() {
    auto e = makeEngine();
    Rng rng(42);
    OutBuffer ob;
    std::vector<float> l(256), r(256);
    bool finite = true;
    float peak = 0.0f;
    for (int block = 0; block < static_cast<int>(10.0 * kRate / 256); ++block) {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
        if (rng.next() % 4 == 0) {
            const int index = static_cast<int>(rng.next() % param::count);
            const ParamSpec& p = kParams[index];
            e->setParam(index, p.min + (rng.bipolar() * 0.5 + 0.5) * (p.max - p.min));
        }
        if (rng.next() % 3 == 0) {
            const std::uint8_t note = static_cast<std::uint8_t>(30 + rng.next() % 70);
            const bool on = rng.next() % 2 == 0;
            e->midi(static_cast<int>(rng.next() % 256), on ? 0x90 : 0x80, note, on ? 100 : 0);
        }
        if (rng.next() % 50 == 0) {
            e->setFxOrder(static_cast<int>(rng.next() % 2), 2, 1, 0);
        }
        e->process(256, l.data(), r.data(), ob);
        for (int i = 0; i < 256; ++i) {
            finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
            peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
        }
    }
    NOTE("fuzz: peak %.3f\n", peak);
    CHECK(finite);
    CHECK(peak <= 1.0f);
}

// The note's tail: with a 1 s release and a 0.3 s sample tail, the last 0.3 s of the release play the file's
// tail (an octave up in the test sample); before that, the loop (the root).
void releaseTail() {
    auto e = std::make_unique<Engine>(kRate, 1234u);
    SampleMeta meta = sample().meta;
    meta.release = 0.3;
    e->setSample(0, sample().data, meta, 60);
    dry(*e);
    e->setParam(param::osc1Release, (1.0 - 0.01) / (8.0 - 0.01)); // 1 s
    const int off = static_cast<int>(kRate * 0.5);
    const Render out = render(*e, static_cast<int>(kRate * 1.6), 128, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 127); });
        at(off, s, n, [&](int f) { en.midi(f, 0x80, 60, 0); });
    });
    const double loopHz = pitchHz(out.l, off + 4800, off + static_cast<int>(kRate * 0.65));
    const double tailHz = pitchHz(out.l, off + static_cast<int>(kRate * 0.75), off + static_cast<int>(kRate * 0.95));
    NOTE("release: loop %.1f Hz, then tail %.1f Hz\n", loopHz, tailHz);
    CHECK(std::fabs(loopHz - 261.6) < 4.0);
    CHECK(std::fabs(tailHz - 523.3) < 8.0);
    CHECK(rms(out.l, off + static_cast<int>(kRate * 1.02), static_cast<int>(kRate * 1.6)) < 1e-4); // over at 1 s
    CHECK(e->activeVoices() == 0);
}

// Mono bass: a hard-panned oscillator still puts its lows in the middle; only its highs move.
void monoBass() {
    auto e = makeEngine();
    dry(*e);
    e->setParam(param::osc1Pan, -1.0);
    e->setParam(param::osc1Oct, -2.0); // C2, 65 Hz: below the split
    const Render low = render(*e, 24000, 256, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 60, 127); });
    });
    const double l = rms(low.l, 9600, 24000), r = rms(low.r, 9600, 24000);
    NOTE("65 Hz panned hard left: L %.4f R %.4f\n", l, r);
    CHECK(r > l * 0.8);
    auto hi = makeEngine();
    dry(*hi);
    hi->setParam(param::osc1Pan, -1.0);
    hi->setParam(param::osc1Oct, 1.0); // C5, 523 Hz: above it
    const Render high = render(*hi, 24000, 256, [&](Engine& en, int s, int n) {
        at(0, s, n, [&](int f) { en.midi(f, 0x90, 72, 127); });
    });
    CHECK(rms(high.r, 9600, 24000) < rms(high.l, 9600, 24000) * 0.05);
}

// The filter is a low-pass on both oscillators: open (position 1) it passes a 523 Hz note, closed (0) it removes it.
// (Axolotl's Morph filter is a high-pass at position 1 and would remove the note there.)
void lowPassFilter() {
    auto level = [](int osc, double position) {
        auto e = makeEngine();
        dry(*e);
        if (osc == 1) {
            e->setParam(param::osc1On, 0.0);
            e->setParam(param::osc2On, 1.0);
            e->setParam(param::oscMix, 1.0);
            e->setParam(param::osc2Attack, 0.0);
        }
        e->setParam(osc == 0 ? param::filterDw1 : param::filterDw2, 1.0);
        e->setParam(osc == 0 ? param::filterPos1 : param::filterPos2, position);
        const Render out = render(*e, 24000, 256, [&](Engine& en, int s, int n) {
            at(0, s, n, [&](int f) { en.midi(f, 0x90, 72, 127); });
        });
        return rms(out.l, 9600, 24000);
    };
    for (int osc : {0, 1}) {
        const double open = level(osc, 1.0), closed = level(osc, 0.0);
        NOTE("osc %d filter: open %.4f, closed %.4f\n", osc + 1, open, closed);
        CHECK(open > 0.05);
        CHECK(closed < open * 0.2);
    }
}

} // namespace

void engineTests() {
    noteOnOff();
    pitch();
    octaveAndDetune();
    blockSizes();
    allSoundOff();
    stealing();
    sustain();
    fxOrderAndReset();
    fuzz();
    releaseTail();
    monoBass();
    lowPassFilter();
}
