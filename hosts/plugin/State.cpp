#include "State.h"

#include <cmath>

namespace bb::plugin {

StateData defaultState() {
    StateData state;
    for (int i = 0; i < param::count; ++i) {
        state.values[static_cast<size_t>(i)] = kParams[i].def;
    }
    return state;
}

bool isFxOrder(const FxOrder& order) {
    bool seen[3] = {false, false, false};
    for (const int x : order) {
        if (x < 0 || x > 2 || seen[x]) {
            return false;
        }
        seen[x] = true;
    }
    return true;
}

double clampPlain(int index, double value) {
    const ParamSpec& p = kParams[index];
    const double v = std::isfinite(value) ? juce::jlimit(p.min, p.max, value) : p.def;
    return p.kind == ParamKind::Float ? v : std::round(v);
}

juce::String writeState(const StateData& state) {
    auto params = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < param::count; ++i) {
        const double v = clampPlain(i, state.values[static_cast<size_t>(i)]);
        // Whole numbers as ints ("osc1Sample":7, not 7.0), as JSON.stringify writes them.
        params->setProperty(kParams[i].id, v == std::floor(v) && std::abs(v) < 1e9 ? juce::var(static_cast<int>(v)) : juce::var(v));
    }
    juce::Array<juce::var> orders;
    for (const FxOrder& order : state.fxOrder) {
        const FxOrder& o = isFxOrder(order) ? order : kDefaultFxOrder;
        orders.add(juce::Array<juce::var>{o[0], o[1], o[2]});
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("v", kStateVersion);
    root->setProperty("params", juce::var(params.release()));
    root->setProperty("fxOrder", orders);
    // Compact, like JSON.stringify in the web build.
    return juce::JSON::toString(juce::var(root.release()), juce::JSON::FormatOptions{}.withSpacing(juce::JSON::Spacing::none));
}

StateData readState(const juce::String& json) {
    StateData state = defaultState();
    const juce::var root = juce::JSON::parse(json);

    if (const auto* params = root["params"].getDynamicObject()) {
        for (int i = 0; i < param::count; ++i) {
            const juce::var& v = params->getProperty(kParams[i].id);
            if (v.isDouble() || v.isInt() || v.isInt64()) {
                state.values[static_cast<size_t>(i)] = clampPlain(i, static_cast<double>(v));
            }
        }
    }

    if (const auto* orders = root["fxOrder"].getArray()) {
        for (int osc = 0; osc < 2 && osc < orders->size(); ++osc) {
            const auto* list = (*orders)[osc].getArray();
            if (list == nullptr || list->size() != 3) {
                continue;
            }
            FxOrder order{};
            bool whole = true;
            for (int k = 0; k < 3; ++k) {
                const juce::var& x = (*list)[k];
                const double d = static_cast<double>(x);
                whole = whole && (x.isInt() || x.isInt64() || x.isDouble()) && d == std::floor(d);
                order[static_cast<size_t>(k)] = whole ? static_cast<int>(d) : -1;
            }
            if (whole && isFxOrder(order)) {
                state.fxOrder[static_cast<size_t>(osc)] = order;
            }
        }
    }
    return state;
}

} // namespace bb::plugin
