#include "SamplePlayer.h"

#include <algorithm>
#include <iterator>
#include <cmath>

namespace bb {

namespace {
constexpr double kHalfPi = 1.57079632679489661923;
}

void SamplePlayer::reset() {
    for (Head& head : heads_) {
        head = {};
    }
    started_ = false;
    looping_ = false;
    tail_ = false;
}

bool SamplePlayer::active() const {
    return std::any_of(std::begin(heads_), std::end(heads_), [](const Head& h) { return h.on; });
}

void SamplePlayer::start(const SampleData& data, const SampleMeta& meta, const NotePlayhead& note, double t) {
    reset();
    end_ = data.seconds();
    loopStart_ = std::clamp(meta.attack, 0.0, end_);
    loopEnd_ = std::clamp(meta.attack + std::max(0.0, meta.loop), loopStart_, end_);
    const double frame = 1.0 / data.rate;
    const double loop = loopEnd_ - loopStart_;
    looping_ = loop >= 2.0 * frame;
    const double crossfade = meta.crossfade > 0.0 ? meta.crossfade : kDefaultCrossfadeSeconds;
    fade_ = std::max(frame, std::min(crossfade, loop * 0.5));
    release_ = std::clamp(meta.release, 0.0, end_);

    started_ = true;
    noteStart_ = note.noteStart;
    last_ = t;
    Head& head = heads_[0];
    head.on = true;
    head.pos = (t - note.noteStart) * ratio_; // a first call a little after noteStart starts that far in
}

SamplePlayer::Head& SamplePlayer::freeHead() {
    for (Head& head : heads_) {
        if (!head.on) {
            return head;
        }
    }
    // All busy (a very short loop at a very high pitch): take the quietest one.
    Head* quietest = &heads_[0];
    for (Head& head : heads_) {
        if (gain(head) < gain(*quietest)) {
            quietest = &head;
        }
    }
    return *quietest;
}

double SamplePlayer::gain(const Head& head) {
    switch (head.fade) {
    case Fade::In:
        return std::sin(head.x * kHalfPi);
    case Fade::Out:
        return head.from * std::cos(head.x * kHalfPi);
    case Fade::None:
        break;
    }
    return 1.0;
}

void SamplePlayer::advance(double dpos) {
    for (Head& head : heads_) {
        if (!head.on) {
            continue;
        }
        head.pos += dpos;
        if (head.fade != Fade::None) {
            head.x += dpos / head.length;
            if (head.x >= 1.0) {
                if (head.fade == Fade::Out) {
                    head.on = false;
                    continue;
                }
                head.fade = Fade::None;
                head.x = 0.0;
            }
        }
        if (head.pos > end_) {
            head.on = false; // the end of the file
        }
    }
}

void SamplePlayer::loopSeams() {
    const double loop = loopEnd_ - loopStart_;
    const double seam = loopEnd_ - fade_;
    for (Head& head : heads_) {
        if (!head.on || head.spawned || head.fade == Fade::Out || head.pos < seam) {
            continue;
        }
        const double past = head.pos - seam;
        if (past >= fade_) {
            // Jumped over the whole crossfade (a big step between calls): wrap without one.
            head.pos = loopStart_ + std::fmod(head.pos - loopStart_, loop);
            continue;
        }
        // The next pass starts as far past loopStart as this one is past the seam, so both fades line up and
        // this pass fades out exactly at loopEnd.
        const double from = gain(head);
        head.spawned = true;
        head.fade = Fade::Out;
        head.from = from;
        head.length = fade_;
        head.x = past / fade_;
        Head& next = freeHead();
        if (&next == &head) {
            // No other head to cross into: wrap this one instead.
            head = {};
            head.on = true;
            head.pos = loopStart_ + past;
            continue;
        }
        next = {};
        next.on = true;
        next.pos = loopStart_ + past;
        next.fade = Fade::In;
        next.length = fade_;
        next.x = past / fade_;
    }
}

void SamplePlayer::enterTail(double pos) {
    tail_ = true;
    looping_ = false;
    const double fade = std::max(1e-9, kTailFadeSeconds * std::max(ratio_, 1e-9));
    for (Head& head : heads_) {
        if (head.on) {
            head.from = gain(head);
            head.fade = Fade::Out;
            head.x = 0.0;
            head.length = fade;
        }
    }
    Head& tail = freeHead();
    tail = {};
    tail.on = true;
    tail.spawned = true;
    tail.pos = pos;
    tail.fade = Fade::In;
    tail.length = fade;
}

Sample SamplePlayer::read(const SampleData& data, double seconds) {
    const double f = seconds * data.rate;
    const int last = data.frames - 1;
    const int i = std::clamp(static_cast<int>(f), 0, last);
    const int j = std::min(i + 1, last);
    const float t = static_cast<float>(std::clamp(f - static_cast<double>(i), 0.0, 1.0));
    const float* l = data.left;
    const float* r = data.right != nullptr ? data.right : data.left;
    return {l[i] + (l[j] - l[i]) * t, r[i] + (r[j] - r[i]) * t};
}

Sample SamplePlayer::play(const SampleData& data, const SampleMeta& meta, const NotePlayhead& note, double t) {
    if (!data.valid() || t < note.noteStart) {
        reset();
        return {};
    }
    if (t >= note.releaseEnd) {
        reset();
        return {};
    }
    if (!started_ || note.noteStart != noteStart_ || t < last_) {
        start(data, meta, note, t);
    } else {
        advance((t - last_) * ratio_);
        last_ = t;
    }

    if (looping_) {
        loopSeams();
    }
    // The tail: the last R seconds of the file, placed so that it ends exactly at releaseEnd.
    if (!tail_ && release_ > 0.0 && t >= note.noteEnd) {
        const double left = (note.releaseEnd - t) * ratio_; // seconds of the recording still to play
        if (left <= release_) {
            enterTail(end_ - left);
        }
    }

    Sample out{};
    for (const Head& head : heads_) {
        if (head.on) {
            const float g = static_cast<float>(gain(head));
            const Sample s = read(data, head.pos);
            out.l += s.l * g;
            out.r += s.r * g;
        }
    }
    return out;
}

} // namespace bb
