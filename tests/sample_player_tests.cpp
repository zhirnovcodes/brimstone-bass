#include "SamplePlayer.h"
#include "TestKit.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace bb;

namespace {

constexpr double kRate = 48000.0; // engine and sample rate in these tests (pitch ratio 1 = as recorded)
constexpr double kPi = 3.14159265358979323846;

// A ramp: frame i holds the value i. Reading it gives back the position the player read, in frames.
struct Ramp {
    std::vector<float> frames;
    SampleData data;
    explicit Ramp(double seconds) : frames(static_cast<std::size_t>(seconds * kRate) + 1) {
        for (std::size_t i = 0; i < frames.size(); ++i) {
            frames[i] = static_cast<float>(i);
        }
        data = {frames.data(), nullptr, static_cast<int>(frames.size()), kRate};
    }
};

// Plays from t = 0 one engine frame at a time (as a voice does) and returns the output at each frame.
std::vector<float> run(SamplePlayer& p, const SampleData& d, const SampleMeta& m, const NotePlayhead& n, double seconds,
                       double engineRate = kRate) {
    std::vector<float> out(static_cast<std::size_t>(seconds * engineRate));
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = p.play(d, m, n, static_cast<double>(i) / engineRate).l;
    }
    return out;
}

float at(const std::vector<float>& out, double seconds, double engineRate = kRate) {
    return out[static_cast<std::size_t>(std::llround(seconds * engineRate))];
}

// Biggest jump between two neighbouring output frames: a click shows up here.
float maxStep(const std::vector<float>& out, std::size_t from, std::size_t to) {
    float worst = 0.0f;
    for (std::size_t i = std::max<std::size_t>(from, 1); i < std::min(to, out.size()); ++i) {
        worst = std::max(worst, std::fabs(out[i] - out[i - 1]));
    }
    return worst;
}

void silenceOutsideTheNote() {
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    const NotePlayhead note{0.5, 1.0, 1.5};
    const auto out = run(p, ramp.data, meta, note, 2.0);
    CHECK(at(out, 0.25) == 0.0f);  // before noteStart
    CHECK(at(out, 0.75) != 0.0f);  // held
    CHECK(at(out, 1.25) != 0.0f);  // releasing
    CHECK(at(out, 1.75) == 0.0f);  // after releaseEnd
    CHECK(!p.active());
    // Bad data: silence, no crash.
    SamplePlayer q;
    CHECK(q.play(SampleData{}, meta, note, 0.6).l == 0.0f);
}

void attackThenLoop() {
    // attack 0.2 s, loop 0.5 s (0.2 .. 0.7), crossfade 0.05 s. Held for 3 s.
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    const auto out = run(p, ramp.data, meta, NotePlayhead{0.0}, 3.0);
    // The attack piece, then straight into the first loop pass: no seam at 0.2 s.
    CHECK_NEAR(at(out, 0.1), 0.1 * kRate, 1.0);
    CHECK_NEAR(at(out, 0.3), 0.3 * kRate, 1.0);
    CHECK_NEAR(at(out, 0.6), 0.6 * kRate, 1.0);
    // Second pass (past the first seam's crossfade, 0.65 .. 0.70): back at the loop start.
    // At t = 0.8 the loop has run 0.15 s since the 0.65 s seam: 0.2 + 0.05 (fade overlap) + 0.1 = 0.35 s.
    CHECK_NEAR(at(out, 0.8), 0.35 * kRate, 1.0);
    // Each later pass lasts loop - crossfade = 0.45 s, so the position at t repeats every 0.45 s.
    CHECK_NEAR(at(out, 0.8 + 0.45), at(out, 0.8), 1.0);
    CHECK_NEAR(at(out, 0.8 + 4 * 0.45), at(out, 0.8), 1.0);
    // Never reads past the loop end while held (ramp value = position).
    float most = 0.0f;
    for (std::size_t i = static_cast<std::size_t>(0.75 * kRate); i < out.size(); ++i) {
        most = std::max(most, out[i]);
    }
    CHECK(most <= 0.7 * kRate * 1.42f); // two heads near loopEnd in equal power sum to at most √2 × value
}

void seamsDoNotClick() {
    // A 110 Hz sine; the loop length is not a whole number of periods, so a hard cut would click.
    const int frames = static_cast<int>(2.0 * kRate);
    std::vector<float> sine(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        sine[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(2.0 * kPi * 110.0 * i / kRate));
    }
    const SampleData data{sine.data(), nullptr, frames, kRate};
    const float smooth = static_cast<float>(2.0 * kPi * 110.0 / kRate); // the sine's own biggest step
    SamplePlayer p;
    const auto out = run(p, data, SampleMeta{0.1, 0.4567, 0.0, 0.05}, NotePlayhead{0.0, 1.5, 1.9}, 2.0);
    const float worst = maxStep(out, 0, static_cast<std::size_t>(1.9 * kRate) - 1);
    NOTE("sine loop: biggest step %.4f, the sine alone %.4f\n", worst, smooth);
    CHECK(worst < smooth * 2.0f);
    // After releaseEnd: silence (the ADSR is 0 there anyway).
    CHECK(at(out, 1.95) == 0.0f);
}

void releasePlaysLoopThenTail() {
    // 2 s file, loop 0.2 .. 0.7, tail = the last 0.3 s (1.7 .. 2.0). Note off at 1.0, release over at 2.0.
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    const NotePlayhead note{0.0, 1.0, 2.0};
    const auto out = run(p, ramp.data, meta, note, 2.0);
    // 1.0 .. 1.7: still looping (positions inside the loop).
    CHECK(at(out, 1.2) >= 0.2 * kRate && at(out, 1.2) <= 0.7 * kRate * 1.42);
    CHECK(at(out, 1.6) >= 0.2 * kRate && at(out, 1.6) <= 0.7 * kRate * 1.42);
    // 1.7 .. 2.0: the tail, timed to end at releaseEnd. Past its 10 ms fade-in, position = end - time left.
    CHECK_NEAR(at(out, 1.75), (2.0 - 0.25) * kRate, 1.0);
    CHECK_NEAR(at(out, 1.9), (2.0 - 0.1) * kRate, 1.0);
}

void shortReleaseStartsTailPartWay() {
    // Release window 0.1 s, shorter than the 0.3 s tail: the tail starts at note-off, 0.2 s in.
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    const NotePlayhead note{0.0, 1.0, 1.1};
    const auto out = run(p, ramp.data, meta, note, 1.2);
    CHECK(p.inTail() == false); // reset after releaseEnd
    CHECK_NEAR(at(out, 1.05), (2.0 - 0.05) * kRate, 1.0);
}

void noTailKeepsLooping() {
    Ramp ramp(2.0);
    SamplePlayer p;
    const auto out = run(p, ramp.data, SampleMeta{0.2, 0.5, 0.0, 0.05}, NotePlayhead{0.0, 1.0, 1.8}, 1.8);
    CHECK(at(out, 1.75) >= 0.2 * kRate && at(out, 1.75) <= 0.7 * kRate * 1.42);
}

void noLoopPlaysThrough() {
    // loop 0: one-shot. Plays the whole file, then silence even while held.
    Ramp ramp(1.0);
    SamplePlayer p;
    const auto out = run(p, ramp.data, SampleMeta{0.2, 0.0, 0.0, 0.0}, NotePlayhead{0.0}, 1.5);
    CHECK_NEAR(at(out, 0.9), 0.9 * kRate, 1.0);
    CHECK(at(out, 1.2) == 0.0f);
}

void pitchRatio() {
    // An octave up reads twice as fast; the tail lasts half as long in playhead time.
    Ramp ramp(2.0);
    SamplePlayer p;
    p.setPitchRatio(2.0);
    const SampleMeta meta{0.4, 1.0, 0.3, 0.05};
    const auto out = run(p, ramp.data, meta, NotePlayhead{0.0, 1.0, 2.0}, 2.0);
    CHECK_NEAR(at(out, 0.1), 0.2 * kRate, 2.0);
    // Tail: 0.3 s of recording = 0.15 s of playhead, so it starts at 1.85.
    CHECK(at(out, 1.8) < 1.4 * kRate * 1.42); // still looping
    CHECK_NEAR(at(out, 1.95), (2.0 - 0.05 * 2.0) * kRate, 2.0);
}

void pitchChangeDoesNotJump() {
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{1.5, 0.0, 0.0, 0.0};
    const NotePlayhead note{0.0};
    float before = 0.0f;
    for (int i = 0; i <= static_cast<int>(0.5 * kRate); ++i) {
        before = p.play(ramp.data, meta, note, i / kRate).l;
    }
    p.setPitchRatio(1.5); // detune moved by an LFO
    const float after = p.play(ramp.data, meta, note, (0.5 * kRate + 1) / kRate).l;
    CHECK_NEAR(after - before, 1.5, 1e-3); // one frame at the new rate, no jump
}

void blockSplitDoesNotMatter() {
    // The voice may call at engine rate in any blocks; a coarser engine rate must land on the same positions.
    Ramp ramp(2.0);
    SamplePlayer a;
    SamplePlayer b;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    const NotePlayhead note{0.0, 1.0, 2.0};
    const auto fine = run(a, ramp.data, meta, note, 2.0, 48000.0);
    const auto coarse = run(b, ramp.data, meta, note, 2.0, 44100.0);
    for (double t : {0.1, 0.3, 0.9, 1.3, 1.8}) {
        CHECK_NEAR(at(coarse, t, 44100.0), at(fine, t, 48000.0), 2.0);
    }
}

void newNoteStartsOver() {
    Ramp ramp(2.0);
    SamplePlayer p;
    const SampleMeta meta{0.2, 0.5, 0.3, 0.05};
    for (int i = 0; i < 4800; ++i) {
        p.play(ramp.data, meta, NotePlayhead{0.0}, i / kRate);
    }
    // Same voice, a new note at 0.2 s: reads from the file's start again.
    CHECK_NEAR(p.play(ramp.data, meta, NotePlayhead{0.2}, 0.2).l, 0.0, 1e-6);
    CHECK_NEAR(p.play(ramp.data, meta, NotePlayhead{0.2}, 0.2 + 100 / kRate).l, 100.0, 1e-3);
    // A held note keeps sounding far into the session.
    SamplePlayer held;
    float last = 0.0f;
    for (int i = 0; i < static_cast<int>(60.0 * 1000); ++i) {
        last = held.play(ramp.data, meta, NotePlayhead{0.0}, i / 1000.0).l; // 1 kHz steps, 60 s
    }
    CHECK(last > 0.0f);
}

} // namespace

void samplePlayerTests() {
    silenceOutsideTheNote();
    attackThenLoop();
    seamsDoNotClick();
    releasePlaysLoopThenTail();
    shortReleaseStartsTailPartWay();
    noTailKeepsLooping();
    noLoopPlaysThrough();
    pitchRatio();
    pitchChangeDoesNotJump();
    blockSplitDoesNotMatter();
    newNoteStartsOver();
}
