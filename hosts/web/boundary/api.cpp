// C ABI over bb::Engine for the IvanDAW web build: a standalone wasm the AudioWorklet instantiates.
// Every index is a params.json index; values are plain (min..max).

#include "Engine.h"

#include <cstdint>
#include <cstdlib>
#include <new>

#define BB_EXPORT extern "C" __attribute__((used, visibility("default")))

namespace {

struct Instance {
    bb::Engine engine;
    bb::OutBuffer out;
    Instance(double rate, std::uint32_t seed) : engine(rate, seed) {}
};

} // namespace

BB_EXPORT void* bb_alloc(int bytes) { return bytes > 0 ? std::malloc(static_cast<std::size_t>(bytes)) : nullptr; }

BB_EXPORT void bb_free(void* p) { std::free(p); }

BB_EXPORT Instance* bb_create(double rate, std::uint32_t seed) {
    if (!(rate > 0)) {
        return nullptr;
    }
    void* memory = std::malloc(sizeof(Instance));
    return memory != nullptr ? new (memory) Instance(rate, seed) : nullptr;
}

BB_EXPORT void bb_destroy(Instance* i) {
    if (i != nullptr) {
        i->~Instance();
        std::free(i);
    }
}

BB_EXPORT void bb_set_param(Instance* i, int index, double value) {
    if (i != nullptr) {
        i->engine.setParam(index, value);
    }
}

BB_EXPORT double bb_param(Instance* i, int index) { return i != nullptr ? i->engine.param(index) : 0.0; }

// Returns 1 when accepted (a permutation of 0 delay, 1 reverb, 2 filter).
BB_EXPORT int bb_set_fx_order(Instance* i, int osc, int a, int b, int c) {
    return (i != nullptr && i->engine.setFxOrder(osc, a, b, c)) ? 1 : 0;
}

// The caller allocates left / right with bb_alloc and keeps them alive while the engine lives.
// right may be 0 for a mono sample. attack, loop, release: the sample's metadata, seconds of the recording
// (samples.json). crossfade <= 0 means the default. Returns 1 when accepted.
BB_EXPORT int bb_set_sample(Instance* i, int index, const float* left, const float* right, int frames, double rate,
                            int rootNote, double attack, double loop, double release, double crossfade) {
    if (i == nullptr) {
        return 0;
    }
    bb::SampleData data;
    data.left = left;
    data.right = right;
    data.frames = frames;
    data.rate = rate;
    bb::SampleMeta meta;
    meta.attack = attack;
    meta.loop = loop;
    meta.release = release;
    meta.crossfade = crossfade;
    return i->engine.setSample(index, data, meta, rootNote) ? 1 : 0;
}

BB_EXPORT void bb_transport(Instance* i, int playing, double bpm, double beat, int num, int den, int frame) {
    if (i == nullptr) {
        return;
    }
    bb::Transport t;
    t.playing = playing != 0;
    t.bpm = bpm;
    t.beat = beat;
    t.numerator = num;
    t.denominator = den;
    i->engine.transport(t, frame);
}

BB_EXPORT void bb_midi(Instance* i, int frame, int status, int d1, int d2) {
    if (i != nullptr) {
        i->engine.midi(frame, static_cast<std::uint8_t>(status), static_cast<std::uint8_t>(d1),
                       static_cast<std::uint8_t>(d2));
    }
}

BB_EXPORT void bb_all_off(Instance* i, int frame) {
    if (i != nullptr) {
        i->engine.allOff(frame, i->out);
    }
}

// Clears left / right, then renders `frames` frames into them.
BB_EXPORT void bb_process(Instance* i, int frames, float* left, float* right) {
    if (i == nullptr || frames <= 0) {
        return;
    }
    for (int k = 0; k < frames; ++k) {
        if (left != nullptr) {
            left[k] = 0.0f;
        }
        if (right != nullptr) {
            right[k] = 0.0f;
        }
    }
    i->out.clear();
    i->engine.process(frames, left, right, i->out);
}

BB_EXPORT int bb_active_voices(Instance* i) { return i != nullptr ? i->engine.activeVoices() : 0; }

// An instrument emits no events; these keep the ABI the same shape as every IvanDAW plugin.
BB_EXPORT int bb_out_count(Instance* i) { return i != nullptr ? i->out.size() : 0; }
BB_EXPORT int bb_out_frame(Instance* i, int k) { return i != nullptr ? i->out[k].frame : 0; }
BB_EXPORT int bb_out_kind(Instance* i, int k) { return i != nullptr ? static_cast<int>(i->out[k].kind) : 0; }
BB_EXPORT int bb_out_bytes(Instance* i, int k) {
    if (i == nullptr) {
        return 0;
    }
    const bb::OutEvent& e = i->out[k];
    return e.b0 | (e.b1 << 8) | (e.b2 << 16);
}
BB_EXPORT int bb_out_slot(Instance* i, int k) { return i != nullptr ? i->out[k].slot : 0; }
BB_EXPORT float bb_out_value(Instance* i, int k) { return i != nullptr ? i->out[k].value : 0.0f; }
