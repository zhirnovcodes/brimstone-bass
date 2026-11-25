#pragma once

// Attack / decay / sustain / release gain for one oscillator of one voice.
//
// The C++ port of ivan-daw-core's `Envelope` (src/envelope.ts, itself from Beatality's `Curve`), with decay and
// sustain added:
//   - start(): rises to 1 from the gain it has now, at one attack step (1 / attack frames) per frame. A restart
//     during a release rises from the gain reached, so nothing jumps.
//   - then falls from 1 to the sustain level over the decay time, and holds the sustain level while the key is down.
//   - release(): falls to 0 over the release time from the gain reached, not from 1. A release from mid-attack is
//     shorter in height, not in length. Its length is fixed when it starts, so the voice knows when it will end
//     (the sample player times its tail to that moment).
// Counted in frames; the last frame of each stage lands exactly on its target.
//
// The sustain level is read every frame (the engine glides it), so a sustain move while a note holds is heard.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bb {

class AdsrEnv {
public:
    // Every stolen voice fades out this fast before it plays its new note.
    static constexpr double kKillSeconds = 0.005;

    struct Times {
        double attack = 0.01;  // seconds
        double decay = 0.01;   // seconds
        float sustain = 1.0f;  // level 0..1
        double rate = 48000.0; // frames per second
    };

    void start(const Times& t) {
        const double frames = std::max(0.0, std::round(t.attack * t.rate));
        const double remaining = std::ceil((1.0 - gain_) * frames - 1e-9);
        if (remaining <= 0.0) {
            gain_ = 1.0f;
            beginDecay(t);
            return;
        }
        stage_ = Stage::Attack;
        step_ = static_cast<float>(1.0 / frames);
        remaining_ = static_cast<int>(remaining);
    }

    // Starts the release from the current gain. Returns its length in seconds (0 when already silent / idle).
    double release(double seconds, double rate) {
        if (stage_ == Stage::Idle) {
            return 0.0;
        }
        const int frames = static_cast<int>(std::max(0.0, std::round(seconds * rate)));
        if (frames == 0 || gain_ == 0.0f) {
            gain_ = 0.0f;
            stage_ = Stage::Idle;
            return 0.0;
        }
        stage_ = Stage::Release;
        remaining_ = frames;
        return frames / rate;
    }

    // A release of kKillSeconds, whatever the release knob says.
    double kill(double rate) { return release(kKillSeconds, rate); }

    // Advances one frame and returns its gain.
    float next(const Times& t) {
        switch (stage_) {
        case Stage::Idle:
            gain_ = 0.0f;
            break;
        case Stage::Attack:
            if (--remaining_ > 0) {
                gain_ = std::min(1.0f, gain_ + step_);
            } else {
                gain_ = 1.0f;
                beginDecay(t);
            }
            break;
        case Stage::Decay:
            // A straight line to the sustain level that lands on it on the last frame, even while it moves.
            gain_ += (t.sustain - gain_) / static_cast<float>(remaining_);
            if (--remaining_ <= 0) {
                gain_ = t.sustain;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            gain_ = t.sustain;
            break;
        case Stage::Release:
            gain_ -= gain_ / static_cast<float>(remaining_);
            if (--remaining_ <= 0) {
                gain_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
        }
        return gain_;
    }

    bool idle() const { return stage_ == Stage::Idle; }
    bool releasing() const { return stage_ == Stage::Release; }
    float gain() const { return gain_; }

private:
    enum class Stage : std::uint8_t { Idle, Attack, Decay, Sustain, Release };

    void beginDecay(const Times& t) {
        const int frames = static_cast<int>(std::max(0.0, std::round(t.decay * t.rate)));
        if (frames == 0) {
            gain_ = t.sustain;
            stage_ = Stage::Sustain;
            return;
        }
        stage_ = Stage::Decay;
        remaining_ = frames;
    }

    Stage stage_ = Stage::Idle;
    float gain_ = 0.0f;
    float step_ = 0.0f;
    int remaining_ = 0;
};

} // namespace bb
