#include "Engine.h"

#include "ParamMap.h"

#include <algorithm>
#include <cmath>

namespace bb {

namespace {

// The parameters of one oscillator, by role.
struct OscParams {
    int filterDw, filterPos, reverbDw, reverbFeedback, delayDw, delayTime;
    int on, sample, vol, pan, attack, decay, sustain, release, oct, detune;
};

constexpr OscParams kOscParams[Engine::kOscs] = {
    {param::filterDw1, param::filterPos1, param::reverbDw1, param::reverbFeedback1, param::delayDw1, param::delayTime1,
     param::osc1On, param::osc1Sample, param::osc1Vol, param::osc1Pan, param::osc1Attack, param::osc1Decay, param::osc1Sustain, param::osc1Release,
     param::osc1Oct, param::osc1Detune},
    {param::filterDw2, param::filterPos2, param::reverbDw2, param::reverbFeedback2, param::delayDw2, param::delayTime2,
     param::osc2On, param::osc2Sample, param::osc2Vol, param::osc2Pan, param::osc2Attack, param::osc2Decay, param::osc2Sustain, param::osc2Release,
     param::osc2Oct, param::osc2Detune},
};

struct LfoParams {
    int amp, time, type, map, sync, retrig, offset;
};

constexpr LfoParams kLfoParams[Engine::kLfos] = {
    {param::lfo1Amp, param::lfo1Time, param::lfo1Type, param::lfo1Map, param::lfo1Sync, param::lfo1Retrig, param::lfo1Offset},
    {param::lfo2Amp, param::lfo2Time, param::lfo2Type, param::lfo2Map, param::lfo2Sync, param::lfo2Retrig, param::lfo2Offset},
    {param::lfo3Amp, param::lfo3Time, param::lfo3Type, param::lfo3Map, param::lfo3Sync, param::lfo3Retrig, param::lfo3Offset},
};

// Above 0.9 the output bends smoothly toward 1 instead of clipping. Below, it is untouched.
float guard(float x) {
    const float a = std::fabs(x);
    if (!(a < 1.0e6f)) {
        return 0.0f; // NaN or Inf
    }
    if (a <= 0.9f) {
        return x;
    }
    const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f);
    return x < 0.0f ? -y : y;
}

} // namespace

Engine::Engine(double sampleRate, std::uint32_t seed)
    : sampleRate_(sampleRate > 0.0 ? sampleRate : 48000.0),
      dt_(static_cast<float>(1.0 / sampleRate_)),
      bus0_(sampleRate_),
      bus1_(sampleRate_),
      lfo_{Lfo(seed ^ 0xA5A5A5A5u), Lfo(seed * 2654435761u + 1u), Lfo(seed * 2246822519u + 3u)} {
    for (int i = 0; i < param::count; ++i) {
        base_[i] = kParams[i].def;
        if (kParams[i].lfo) {
            lfoTargets_[lfoTargetCount_++] = i;
        }
    }
    smoothK_ = static_cast<float>(1.0 - std::exp(-kControlFrames / (0.02 * sampleRate_)));
}

void Engine::setParam(int index, double value) {
    if (index < 0 || index >= param::count || !std::isfinite(value)) {
        return;
    }
    const ParamSpec& spec = kParams[index];
    double v = std::clamp(value, spec.min, spec.max);
    if (spec.kind != ParamKind::Float) {
        v = std::round(v);
    }
    base_[index] = v;
}

double Engine::param(int index) const {
    return (index >= 0 && index < param::count) ? base_[index] : 0.0;
}

float Engine::normalized(int index) const {
    const ParamSpec& spec = kParams[index];
    const double range = spec.max - spec.min;
    return range > 0.0 ? static_cast<float>((base_[index] - spec.min) / range) : 0.0f;
}

bool Engine::setFxOrder(int osc, int a, int b, int c) {
    const int order[kEffectCount] = {a, b, c};
    if (osc < 0 || osc >= kOscs || !isPermutation(order)) {
        return false;
    }
    bool same = true;
    for (int i = 0; i < kEffectCount; ++i) {
        same = same && buses_[osc]->order(i) == order[i];
    }
    if (!same) {
        buses_[osc]->setOrder(order);
    }
    return true;
}

int Engine::fxOrder(int osc, int position) const {
    if (osc < 0 || osc >= kOscs || position < 0 || position >= kEffectCount) {
        return -1;
    }
    return buses_[osc]->order(position);
}

bool Engine::setSample(int index, const SampleData& data, const SampleMeta& meta, int rootNote) {
    if (index < 0 || index >= kMaxSamples || !data.valid()) {
        return false;
    }
    samples_[index] = {data, meta, std::clamp(rootNote, 0, 127)};
    // Voices already holding this sample restart on it, behind a reset fade.
    for (int osc = 0; osc < kOscs; ++osc) {
        if (oscSample_[osc] == index || pendingSample_[osc] == index) {
            buses_[osc]->requestReset();
        }
    }
    return true;
}

void Engine::push(const Event& e) {
    if (eventCount_ < kMaxEvents) {
        events_[eventCount_++] = e;
    }
}

void Engine::transport(const Transport& t, int frame) {
    Event e;
    e.frame = frame;
    e.kind = EventKind::Transport;
    e.transport = t;
    push(e);
}

void Engine::midi(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    Event e;
    e.frame = frame;
    e.kind = EventKind::Midi;
    e.status = status;
    e.d1 = d1;
    e.d2 = d2;
    push(e);
}

void Engine::allOff(int frame, OutBuffer&) {
    Event e;
    e.frame = frame;
    e.kind = EventKind::AllOff;
    push(e);
}

bool Engine::handle(const Event& e) {
    switch (e.kind) {
    case EventKind::Transport:
        if (e.transport.bpm > 0.0 && std::isfinite(e.transport.bpm)) {
            bpm_ = e.transport.bpm;
        }
        if (std::isfinite(e.transport.beat)) {
            beat_ = e.transport.beat;
        }
        playing_ = e.transport.playing;
        return true;
    case EventKind::AllOff:
        releaseAll();
        return false;
    case EventKind::Midi:
    default: {
        const int type = e.status & 0xF0;
        if (type == 0x90 && e.d2 > 0) {
            noteOn(e.d1 & 0x7F, e.d2 & 0x7F);
        } else if (type == 0x80 || type == 0x90) {
            noteOff(e.d1 & 0x7F);
        } else if (type == 0xB0) {
            if (e.d1 == 64) {
                sustainPedal_ = e.d2 >= 64;
                if (!sustainPedal_) {
                    for (Voice& v : voices_) {
                        if (v.sustained) {
                            v.sustained = false;
                            releaseVoice(v);
                        }
                    }
                }
            } else if (e.d1 == 120 || e.d1 == 123) {
                releaseAll();
                return false;
            }
        }
        return true;
    }
    }
}

void Engine::startVoice(Voice& voice, int note, float velocity) {
    voice.note = note;
    voice.velocity = velocity;
    voice.age = ++ageCounter_;
    voice.held = true;
    voice.sustained = false;
    voice.pendingNote = -1;
    voice.time = 0.0;
    for (int osc = 0; osc < kOscs; ++osc) {
        voice.env[osc].start(times(osc));
        voice.playhead[osc] = NotePlayhead{0.0};
        startPlayer(voice, osc);
    }
}

// (Re)starts the oscillator's sample from its start, at the voice's time now: a new note, or the oscillator's
// sample changed under a sounding voice. Note-off and release times stay as they are.
void Engine::startPlayer(Voice& voice, int osc) {
    voice.playhead[osc].noteStart = voice.time;
    voice.player[osc].reset();
    voice.noteRatio[osc] = std::exp2((voice.note - sampleFor(osc).rootNote) / 12.0);
    voice.player[osc].setPitchRatio(voice.noteRatio[osc] * pitchRatio_[osc]);
}

AdsrEnv::Times Engine::times(int osc) const {
    return {attack_[osc], decay_[osc], sustain_[osc], sampleRate_};
}

// Note off for one voice: each oscillator's release starts now, and its sample player learns when it ends.
void Engine::releaseVoice(Voice& voice) {
    for (int osc = 0; osc < kOscs; ++osc) {
        if (voice.env[osc].releasing() || voice.env[osc].idle()) {
            continue;
        }
        const double length = voice.env[osc].release(release_[osc], sampleRate_);
        voice.playhead[osc].noteEnd = voice.time;
        voice.playhead[osc].releaseEnd = voice.time + length;
    }
}

// Stolen: a 5 ms release, whatever the knob says.
void Engine::killVoice(Voice& voice) {
    for (int osc = 0; osc < kOscs; ++osc) {
        if (voice.env[osc].idle()) {
            continue;
        }
        const double length = voice.env[osc].kill(sampleRate_);
        voice.playhead[osc].noteEnd = std::min(voice.playhead[osc].noteEnd, voice.time);
        voice.playhead[osc].releaseEnd = voice.time + length;
    }
}

void Engine::noteOn(int note, int velocity) {
    for (int k = 0; k < kLfos; ++k) {
        if (base_[kLfoParams[k].retrig] >= 0.5) {
            lfo_[k].retrig(beat_);
        }
    }
    const float v = static_cast<float>(velocity) / 127.0f;
    for (Voice& voice : voices_) {
        if (!voice.active()) {
            startVoice(voice, note, v);
            return;
        }
    }
    // Steal: the oldest released voice, then the oldest held one. A voice already being stolen is last.
    Voice* best = nullptr;
    for (int pass = 0; pass < 3 && best == nullptr; ++pass) {
        for (Voice& voice : voices_) {
            if (pass < 2 && voice.pendingNote >= 0) {
                continue;
            }
            if (pass == 0 && !voice.releasing()) {
                continue;
            }
            if (best == nullptr || voice.age < best->age) {
                best = &voice;
            }
        }
    }
    best->note = -1;
    best->held = false;
    best->sustained = false;
    best->pendingNote = note;
    best->pendingVelocity = v;
    killVoice(*best);
}

void Engine::noteOff(int note) {
    for (Voice& voice : voices_) {
        if (voice.pendingNote == note) {
            voice.pendingNote = -1; // released before its 5 ms steal fade ended
        }
        if (voice.note == note && voice.held) {
            voice.held = false;
            if (sustainPedal_) {
                voice.sustained = true;
            } else {
                releaseVoice(voice);
            }
        }
    }
}

void Engine::releaseAll() {
    for (Voice& voice : voices_) {
        voice.held = false;
        voice.sustained = false;
        voice.pendingNote = -1;
        releaseVoice(voice);
    }
}

void Engine::controlTick() {
    // 1. What the host set, normalized.
    float prevAmp[kLfos];
    for (int k = 0; k < kLfos; ++k) {
        prevAmp[k] = effective_[kLfoParams[k].amp];
    }
    for (int i = 0; i < param::count; ++i) {
        effective_[i] = normalized(i);
    }

    // 2. LFO modulation: base + amp x (lfo - 0.5), in normalized space. The amps are the previous tick's
    //    effective ones, so one LFO may modulate another's amp (both ways) without a loop.
    for (int k = 0; k < kLfos; ++k) {
        const int map = static_cast<int>(base_[kLfoParams[k].map]);
        if (map >= 0 && map < lfoTargetCount_) {
            const int target = lfoTargets_[map];
            effective_[target] = clamp01(effective_[target] + prevAmp[k] * (lfoOut_[k] - 0.5f));
        }
    }

    // 3. Floats glide toward their target (20 ms) so knob and LFO moves never zipper.
    //    Delay time picks a division; the delay glides between divisions itself.
    for (int i = 0; i < param::count; ++i) {
        const bool glide = smoothedReady_ && kParams[i].kind == ParamKind::Float && i != param::delayTime1 &&
                           i != param::delayTime2;
        smoothed_[i] = glide ? smoothed_[i] + (effective_[i] - smoothed_[i]) * smoothK_ : effective_[i];
    }
    smoothedReady_ = true;

    auto value = [this](int i) {
        const ParamSpec& p = kParams[i];
        return static_cast<float>(p.min + smoothed_[i] * (p.max - p.min));
    };
    auto discrete = [this](int i) {
        const ParamSpec& p = kParams[i];
        return static_cast<int>(std::lround(p.min + effective_[i] * (p.max - p.min)));
    };

    // 4. Advance the LFOs for the next tick.
    const double elapsed = kControlFrames / sampleRate_;
    for (int k = 0; k < kLfos; ++k) {
        const LfoParams& p = kLfoParams[k];
        LfoInput in;
        in.shape = static_cast<LfoShape>(std::clamp(discrete(p.type), 0, 3));
        in.sync = discrete(p.sync) != 0;
        in.retrig = discrete(p.retrig) != 0;
        in.time = value(p.time);
        in.offset = value(p.offset);
        lfoOut_[k] = lfo_[k].tick(in, beat_, bpm_, elapsed);
    }

    // 5. Resolve what the voices and the buses read.
    const int unisonVoices = std::clamp(discrete(param::unisonVoices), 0, ivandaw::fx::Unison::kMaxVoices);
    const float unisonValue = value(param::unisonValue);
    for (int osc = 0; osc < kOscs; ++osc) {
        const OscParams& p = kOscParams[osc];
        OscBus& bus = *buses_[osc];

        const float filterDw = value(p.filterDw);
        // Brimstone: a low-pass on both oscillators (Axolotl morphs low → band → high). Filter position = cutoff.
        bus.fx.filter.type = ivandaw::fx::FilterType::Low;
        bus.fx.filter.slope = ivandaw::fx::FilterSlope::Db12;
        bus.fx.filter.filterPosition = value(p.filterPos);
        bus.fx.filter.res = filterRes(filterDw);
        bus.fx.filter.mix = filterMix(filterDw);

        const float reverbFeedback = value(p.reverbFeedback);
        bus.fx.reverb.decay = reverbDecaySeconds(reverbFeedback);
        bus.fx.reverb.feedback = reverbFeedback;
        bus.fx.reverb.mix = value(p.reverbDw);

        const float delayDw = value(p.delayDw);
        bus.fx.delay.delayTime = delaySeconds(value(p.delayTime), bpm_);
        bus.fx.delay.feedback = invLerp(0.0f, 1.0f, delayDw);
        bus.fx.delay.mix = delayDw;

        bus.unison.type = ivandaw::fx::UnisonType::Detune;
        bus.unison.voices = unisonVoices;
        bus.unison.param = unisonCents(unisonValue);
        bus.unison.mix = unisonValue;

        // "Should reset effects": on/off and sample changes, on an edge only.
        bus.setOn(discrete(p.on) != 0);
        const int sample = std::clamp(discrete(p.sample), 0, kMaxSamples - 1);
        if (sample != pendingSample_[osc]) {
            pendingSample_[osc] = sample;
            bus.requestReset();
        }

        attack_[osc] = attackSeconds(value(p.attack));
        decay_[osc] = decaySeconds(value(p.decay));
        sustain_[osc] = value(p.sustain);
        release_[osc] = releaseSeconds(value(p.release));
        bus.vol = value(p.vol);
        bus.pan = panGains(value(p.pan));

        // Octave and detune retune the playing voices too (a pitch change; the read position carries on).
        const double ratio = pitchRatio(std::clamp(discrete(p.oct), -2, 2), value(p.detune));
        if (ratio != pitchRatio_[osc]) {
            pitchRatio_[osc] = ratio;
            for (Voice& voice : voices_) {
                voice.player[osc].setPitchRatio(voice.noteRatio[osc] * ratio);
            }
        }
    }
    mix_ = oscMixGains(value(param::oscMix), static_cast<MixType>(std::clamp(discrete(param::mixType), 0, 2)));
}

void Engine::renderFrame(float* left, float* right) {
    Sample in[kOscs] = {};
    const bool live[kOscs] = {!bus0_.silent(), !bus1_.silent()};
    for (Voice& voice : voices_) {
        if (voice.pendingNote >= 0 && voice.env[0].idle() && voice.env[1].idle()) {
            startVoice(voice, voice.pendingNote, voice.pendingVelocity);
        }
        if (!voice.active()) {
            continue;
        }
        for (int osc = 0; osc < kOscs; ++osc) {
            if (!live[osc]) {
                // An oscillator that is off holds a key-down voice (so it sounds again when switched back on),
                // but never keeps a released voice alive.
                if (voice.releasing()) {
                    voice.env[osc] = AdsrEnv{};
                }
                continue;
            }
            const float level = voice.env[osc].next(times(osc));
            if (voice.env[osc].idle()) {
                continue;
            }
            const BuiltInSample& sample = sampleFor(osc);
            const Sample s = voice.player[osc].play(sample.data, sample.meta, voice.playhead[osc], voice.time);
            const float g = level * voice.velocity;
            in[osc].l += s.l * g;
            in[osc].r += s.r * g;
        }
        voice.time += 1.0 / sampleRate_;
    }

    Sample out{};
    const float gains[kOscs] = {mix_.a, mix_.b};
    for (int osc = 0; osc < kOscs; ++osc) {
        const Sample o = buses_[osc]->process(in[osc], dt_);
        if (buses_[osc]->takeJustReset()) {
            // At silence: the oscillator's voices restart on its (new) sample.
            oscSample_[osc] = pendingSample_[osc];
            for (Voice& voice : voices_) {
                if (voice.note >= 0 && !voice.env[osc].idle()) {
                    startPlayer(voice, osc);
                }
            }
        }
        out.l += o.l * gains[osc];
        out.r += o.r * gains[osc];
    }
    if (left != nullptr) {
        *left += guard(out.l * kMasterGain);
    }
    if (right != nullptr) {
        *right += guard(out.r * kMasterGain);
    }
}

void Engine::process(int frames, float* left, float* right, OutBuffer&) {
    int e = 0;
    for (int i = 0; i < frames; ++i) {
        while (e < eventCount_ && events_[e].frame <= i) {
            if (!handle(events_[e++])) {
                e = eventCount_; // CC 120 / 123: drop what was queued after it
            }
        }
        if (tickLeft_ <= 0) {
            controlTick();
            tickLeft_ = kControlFrames;
        }
        --tickLeft_;
        renderFrame(left != nullptr ? left + i : nullptr, right != nullptr ? right + i : nullptr);
        beat_ += bpm_ / 60.0 / sampleRate_;
    }
    // Events at or past the end of this call (a host bug, or frames == 0) apply now.
    while (e < eventCount_) {
        if (!handle(events_[e++])) {
            break;
        }
    }
    eventCount_ = 0;
}

double Engine::tailSeconds() const {
    double tail = 0.0;
    for (int osc = 0; osc < kOscs; ++osc) {
        const OscBus& bus = *buses_[osc];
        const double reverb = bus.fx.reverb.mix > 0.0f ? bus.fx.reverb.decay * bus.fx.reverb.feedback : 0.0;
        // A feedback delay falls 60 dB after log(0.001) / log(feedback) repeats.
        const double fb = std::min(static_cast<double>(bus.fx.delay.feedback), 0.95);
        const double delay =
            (bus.fx.delay.mix > 0.0f && fb > 0.001) ? bus.fx.delay.delayTime * std::log(0.001) / std::log(fb) : 0.0;
        tail = std::max(tail, static_cast<double>(release_[osc]) + std::max(reverb, delay));
    }
    return tail;
}

int Engine::activeVoices() const {
    int n = 0;
    for (const Voice& voice : voices_) {
        n += voice.active() ? 1 : 0;
    }
    return n;
}

} // namespace bb
