#pragma once

// The saved sound: the same JSON as the IvanDAW web build's getState().data (apps/web/src/state.ts).
//   { "v": 1, "params": { "<params.json id>": <plain value>, ... }, "fxOrder": [[2,0,1],[2,0,1]] }
// A project saved in one host loads in the other. Changing it needs a version bump and a reader for the old one.

#include "ParamsJson.h"

#include <juce_core/juce_core.h>

#include <array>

namespace bb::plugin {

inline constexpr int kStateVersion = 1;

using FxOrder = std::array<int, 3>;            // a permutation of 0 delay, 1 reverb, 2 filter
inline constexpr FxOrder kDefaultFxOrder = {2, 0, 1}; // filter -> delay -> reverb

struct StateData {
    std::array<double, param::count> values{}; // plain values, index = params.json index
    std::array<FxOrder, 2> fxOrder{kDefaultFxOrder, kDefaultFxOrder};
};

StateData defaultState();

bool isFxOrder(const FxOrder& order);

// In range, finite, and whole for int / choice / boolean. Not finite: the default.
double clampPlain(int index, double value);

juce::String writeState(const StateData& state);

// Anything in: unknown ids are ignored, missing or bad values become defaults.
StateData readState(const juce::String& json);

} // namespace bb::plugin
