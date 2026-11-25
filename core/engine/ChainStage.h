#pragma once

// The per-oscillator effect chain: Delay, Reverb and Filter behind one interface, run in a user-chosen order.

#include "ParamMap.h"

#include <ivandaw/fx.h>

#include <cstdint>

namespace bb {

using ivandaw::fx::Sample;

// What the engine resolves for one oscillator's chain every control tick. Stages read it by reference.
struct ChainParams {
    ivandaw::fx::DelayParams delay;
    ivandaw::fx::ReverbParams reverb;
    ivandaw::fx::FilterParams filter;
};

// One effect in the chain. Virtual calls need no RTTI.
class ChainStage {
public:
    virtual ~ChainStage() = default;
    virtual Sample Apply(Sample sample, float deltaTime) = 0;
    virtual void Reset() = 0;
};

class DelayStage final : public ChainStage {
public:
    DelayStage(double sampleRate, const ivandaw::fx::DelayParams& params)
        : delay_(sampleRate, kMaxDelaySeconds), params_(params) {}
    Sample Apply(Sample sample, float deltaTime) override { return delay_.Apply(params_, sample, deltaTime); }
    void Reset() override { delay_.Reset(); }

private:
    ivandaw::fx::Delay delay_;
    const ivandaw::fx::DelayParams& params_;
};

class ReverbStage final : public ChainStage {
public:
    ReverbStage(double sampleRate, const ivandaw::fx::ReverbParams& params) : reverb_(sampleRate), params_(params) {}
    Sample Apply(Sample sample, float deltaTime) override { return reverb_.Apply(params_, sample, deltaTime); }
    void Reset() override { reverb_.Reset(); }

private:
    ivandaw::fx::Reverb reverb_;
    const ivandaw::fx::ReverbParams& params_;
};

class FilterStage final : public ChainStage {
public:
    explicit FilterStage(const ivandaw::fx::FilterParams& params) : params_(params) {}
    Sample Apply(Sample sample, float deltaTime) override { return filter_.Apply(params_, sample, deltaTime); }
    void Reset() override { filter_.Reset(); }

private:
    ivandaw::fx::Filter filter_;
    const ivandaw::fx::FilterParams& params_;
};

// Effect indexes used by the order arrays.
enum Effect : int { kDelay = 0, kReverb = 1, kFilter = 2, kEffectCount = 3 };

// True when order holds 0, 1 and 2 exactly once each.
inline bool isPermutation(const int order[kEffectCount]) {
    bool seen[kEffectCount] = {};
    for (int i = 0; i < kEffectCount; ++i) {
        if (order[i] < 0 || order[i] >= kEffectCount || seen[order[i]]) {
            return false;
        }
        seen[order[i]] = true;
    }
    return true;
}

class Chain {
public:
    // Filter, then delay, then reverb.
    static constexpr int kDefaultOrder[kEffectCount] = {kFilter, kDelay, kReverb};

    Chain(double sampleRate, const ChainParams& params)
        : delay_(sampleRate, params.delay), reverb_(sampleRate, params.reverb), filter_(params.filter) {
        stages_[kDelay] = &delay_;
        stages_[kReverb] = &reverb_;
        stages_[kFilter] = &filter_;
        setOrder(kDefaultOrder);
    }

    // Ignored unless order is a permutation of 0, 1, 2.
    bool setOrder(const int order[kEffectCount]) {
        if (!isPermutation(order)) {
            return false;
        }
        for (int i = 0; i < kEffectCount; ++i) {
            order_[i] = static_cast<std::uint8_t>(order[i]);
        }
        return true;
    }

    int order(int position) const { return order_[position]; }

    Sample Apply(Sample sample, float deltaTime) {
        for (int i = 0; i < kEffectCount; ++i) {
            sample = stages_[order_[i]]->Apply(sample, deltaTime);
        }
        return sample;
    }

    void Reset() {
        for (ChainStage* stage : stages_) {
            stage->Reset();
        }
    }

private:
    DelayStage delay_;
    ReverbStage reverb_;
    FilterStage filter_;
    ChainStage* stages_[kEffectCount] = {};
    std::uint8_t order_[kEffectCount] = {};
};

} // namespace bb
