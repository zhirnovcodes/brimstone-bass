#include "ChainStage.h"
#include "OscBus.h"
#include "Rng.h"
#include "TestKit.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace bb;

namespace {

constexpr double kRate = 48000.0;
constexpr float kDt = static_cast<float>(1.0 / kRate);

void permutations() {
    const int ok[3] = {2, 0, 1};
    const int dup[3] = {0, 0, 1};
    const int range[3] = {0, 1, 3};
    CHECK(isPermutation(ok));
    CHECK(!isPermutation(dup));
    CHECK(!isPermutation(range));
}

ChainParams wetParams() {
    ChainParams p;
    p.delay.delayTime = 0.05f;
    p.delay.feedback = 0.5f;
    p.delay.mix = 0.5f;
    p.reverb.decay = 1.0f;
    p.reverb.mix = 0.5f;
    p.filter.type = ivandaw::fx::FilterType::Morph;
    p.filter.res = 0.3f;
    p.filter.mix = 0.8f;
    return p;
}

// Noise through a chain whose filter sweeps, so the order matters (all three effects are linear:
// with fixed settings their order would not change the sound).
std::vector<float> runSweep(const int order[3]) {
    ChainParams p = wetParams();
    auto chain = std::make_unique<Chain>(kRate, p);
    CHECK(chain->setOrder(order));
    Rng rng(7);
    std::vector<float> out;
    for (int i = 0; i < 24000; ++i) {
        p.filter.filterPosition = 0.2f + 0.6f * static_cast<float>(i) / 24000.0f;
        const float x = i < 4800 ? rng.bipolar() * 0.5f : 0.0f;
        out.push_back(chain->Apply({x, x}, kDt).l);
    }
    return out;
}

void order() {
    ChainParams p;
    auto chain = std::make_unique<Chain>(kRate, p);
    CHECK(chain->order(0) == kFilter && chain->order(1) == kDelay && chain->order(2) == kReverb);
    const int bad[3] = {1, 1, 2};
    CHECK(!chain->setOrder(bad));
    CHECK(chain->order(0) == kFilter);

    const int a[3] = {kFilter, kDelay, kReverb};
    const int b[3] = {kDelay, kReverb, kFilter};
    const std::vector<float> x = runSweep(a);
    const std::vector<float> y = runSweep(b);
    double diff = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        diff = std::max(diff, static_cast<double>(std::fabs(x[i] - y[i])));
    }
    NOTE("order: largest difference with a sweeping filter %.4f\n", diff);
    CHECK(diff > 1e-3);
}

void resetLeavesNoTail() {
    ChainParams p = wetParams();
    auto chain = std::make_unique<Chain>(kRate, p);
    Rng rng(9);
    for (int i = 0; i < 9600; ++i) {
        const float x = rng.bipolar();
        chain->Apply({x, x}, kDt);
    }
    chain->Reset();
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i) {
        const Sample s = chain->Apply({0.0f, 0.0f}, kDt);
        peak = std::max({peak, std::fabs(s.l), std::fabs(s.r)});
    }
    CHECK_NEAR(peak, 0.0, 1e-9);
}

void busGate() {
    auto bus = std::make_unique<OscBus>(kRate);
    bus->fx = wetParams();
    // A steady tone through the bus; a reset closes the gate, resets, and reopens without a jump.
    float prev = 0.0f, maxStep = 0.0f;
    bool sawSilence = false;
    int resetAt = -1;
    for (int i = 0; i < 9600; ++i) {
        if (i == 4800) {
            const int order[3] = {kReverb, kFilter, kDelay};
            bus->setOrder(order);
            CHECK(bus->order(0) == kReverb); // shown at once, applied at silence
        }
        const float x = 0.3f * static_cast<float>(std::sin(2.0 * 3.14159265 * 220.0 * i / kRate));
        const float y = bus->process({x, x}, kDt).l;
        if (bus->takeJustReset()) {
            resetAt = i;
        }
        if (i > 4800 && y == 0.0f) {
            sawSilence = true;
        }
        if (i > 0) {
            maxStep = std::max(maxStep, std::fabs(y - prev));
        }
        prev = y;
    }
    CHECK(sawSilence);
    CHECK(resetAt > 4800 && resetAt <= 4800 + 241); // 5 ms at 48 kHz
    NOTE("bus: largest step through a reset %.4f\n", maxStep);
    CHECK(maxStep < 0.1f);

    // Off: silent, and stays so.
    bus->setOn(false);
    for (int i = 0; i < 480; ++i) {
        bus->process({0.5f, 0.5f}, kDt);
    }
    CHECK(bus->silent());
    CHECK_NEAR(bus->process({0.5f, 0.5f}, kDt).l, 0.0, 0.0);
}

} // namespace

void chainTests() {
    permutations();
    order();
    resetLeavesNoTail();
    busGate();
}
