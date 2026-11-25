#pragma once

// Plays one oscillator's sample for one note, driven by the playhead time.
//
// A sample file has three parts, given in seconds of the recording by its metadata:
//
//   0 ── attack ──┬── loop ──┬────────────── ... ──────────┬── release ── end
//   [  attack     )[  loop   )                              [ tail (last R s) ]
//
// play() compares the playhead time with the note's times:
//   - noteStart .. noteEnd: the attack piece once, then straight on into the loop piece (the first pass has no
//     seam), then the loop piece on repeat. Every seam crossfades equal-power over the sample's crossfade time
//     (Axolotl Pad's loop crossfade).
//   - noteEnd .. releaseEnd: the loop keeps playing, except for the last R seconds (R = metadata release),
//     where it crossfades (kTailFadeSeconds) to the file's tail, timed so the tail ends at releaseEnd.
//     A release shorter than R starts the tail at noteEnd, part-way in, so it still ends at releaseEnd.
//   - before noteStart, after releaseEnd: silence.
//
// The read position advances by (time since the last call) x pitch ratio, so a pitch change (octave, detune, an
// LFO) never jumps in the sample, and the result does not depend on how the host splits blocks. The ADSR
// envelope is applied after this, by the voice.

#include <ivandaw/fx/Sample.h>

#include <limits>

namespace bb {

using ivandaw::fx::Sample;

// The audio of one sample. The host owns the memory; it must outlive every player that reads it.
struct SampleData {
    const float* left = nullptr;
    const float* right = nullptr; // nullptr = mono, left is used for both sides
    int frames = 0;
    double rate = 0.0; // the data's sample rate

    bool valid() const { return left != nullptr && frames > 1 && rate > 0.0; }
    double seconds() const { return static_cast<double>(frames - 1) / rate; }
};

// The sample's metadata file. Seconds of the recording.
struct SampleMeta {
    double attack = 0.0;    // [0, attack): played once at the note's start
    double loop = 0.0;      // [attack, attack + loop): repeats while the note sounds; <= 0: no loop, plays through
    double release = 0.0;   // the file's last `release` seconds: the tail, played as the note dies; 0: no tail
    double crossfade = 0.0; // overlap at the loop seam; <= 0: kDefaultCrossfadeSeconds. At most half the loop.
};

// One note, in playhead seconds.
struct NotePlayhead {
    static constexpr double kHeld = std::numeric_limits<double>::infinity();

    double noteStart = 0.0;
    double noteEnd = kHeld;    // note-off; kHeld while the key is down
    double releaseEnd = kHeld; // the moment the release is over (noteEnd + envelope release)
};

class SamplePlayer {
public:
    static constexpr double kDefaultCrossfadeSeconds = 0.1;
    static constexpr double kTailFadeSeconds = 0.01; // loop → tail

    // Seconds of the recording per playhead second: the note against the sample's root, times octave and detune.
    // Takes effect from the next play() on, without moving the read position.
    void setPitchRatio(double ratio) { ratio_ = ratio > 0.0 ? ratio : 0.0; }
    double pitchRatio() const { return ratio_; }

    // The sample's output at `playheadTime`. A new noteStart, or a playhead that moved back, starts the note over.
    Sample play(const SampleData& data, const SampleMeta& meta, const NotePlayhead& note, double playheadTime);

    // Silence until the next note.
    void reset();
    bool active() const;
    bool inTail() const { return tail_; }

private:
    enum class Fade : unsigned char { None, In, Out };

    struct Head {
        double pos = 0.0; // seconds of the recording
        bool on = false;
        bool spawned = false; // already started the next loop pass
        Fade fade = Fade::None;
        double x = 0.0;      // fade progress 0..1
        double length = 1.0; // fade length, seconds of the recording
        double from = 1.0;   // level an Out fade starts from (a head may leave part-way through its own fade-in)
    };

    static constexpr int kHeads = 4; // two at a loop seam, plus the tail coming in

    void start(const SampleData& data, const SampleMeta& meta, const NotePlayhead& note, double t);
    void advance(double dpos);
    void loopSeams();
    void enterTail(double pos);
    Head& freeHead();
    static double gain(const Head& head);
    static Sample read(const SampleData& data, double seconds);

    Head heads_[kHeads];
    double ratio_ = 1.0;
    bool started_ = false;
    bool looping_ = false;
    bool tail_ = false;
    double noteStart_ = 0.0;
    double last_ = 0.0; // playhead time of the previous call

    // The current note's sample parts, clamped to the file (seconds of the recording).
    double end_ = 0.0;
    double loopStart_ = 0.0;
    double loopEnd_ = 0.0;
    double fade_ = 0.0;
    double release_ = 0.0;
};

} // namespace bb
