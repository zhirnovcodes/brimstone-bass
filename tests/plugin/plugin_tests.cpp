// JUCE host checks without a DAW: the processor makes sound from MIDI, and its saved state is the same JSON
// as the IvanDAW web build (apps/web/src/state.ts), both ways.

#include "../TestKit.h"

#include "BrimstoneBassData.h"
#include "PluginProcessor.h"
#include "State.h"

#include <cmath>

using namespace bb;
using namespace bb::plugin;

// The tests run the processor without a window (the editor and its web view live in PluginEditor.cpp).
juce::AudioProcessorEditor* bb::plugin::Processor::createEditor() { return nullptr; }

namespace {

constexpr double kRate = 48000.0;

int indexOf(const char* id) {
    for (int i = 0; i < param::count; ++i) {
        if (juce::String(kParams[i].id) == id) {
            return i;
        }
    }
    return -1;
}

const char* resource(const char* file, int& size) {
    for (int i = 0; i < BrimstoneBassData::namedResourceListSize; ++i) {
        if (juce::String(BrimstoneBassData::originalFilenames[i]) == file) {
            return BrimstoneBassData::getNamedResource(BrimstoneBassData::namedResourceList[i], size);
        }
    }
    size = 0;
    return nullptr;
}

double rms(const juce::AudioBuffer<float>& audio) {
    double sum = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch) {
        for (int k = 0; k < audio.getNumSamples(); ++k) {
            const double x = audio.getSample(ch, k);
            sum += x * x;
        }
    }
    return std::sqrt(sum / juce::jmax(1, audio.getNumChannels() * audio.getNumSamples()));
}

// Renders `blocks` blocks; MIDI goes into the first one.
double render(Processor& p, int blocks, int blockSize, juce::MidiBuffer first = {}) {
    juce::AudioBuffer<float> audio(2, blockSize);
    double last = 0.0;
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) {
            midi = first;
        }
        p.processBlock(audio, midi);
        for (int ch = 0; ch < 2; ++ch) {
            for (int k = 0; k < blockSize; ++k) {
                CHECK(std::isfinite(audio.getSample(ch, k)));
            }
        }
        last = rms(audio);
    }
    return last;
}

void builtInData() {
    // params.json in the binary matches ParamsJson.h (both from the same file), and every choice has its names.
    int size = 0;
    const char* data = resource("params.json", size);
    CHECK(data != nullptr);
    const juce::var json = juce::JSON::parse(juce::String::fromUTF8(data, size));
    const auto* list = json["params"].getArray();
    CHECK(list != nullptr && list->size() == param::count);
    for (int i = 0; list != nullptr && i < list->size(); ++i) {
        const juce::var& entry = (*list)[i];
        CHECK(entry["id"].toString() == kParams[i].id);
        if (kParams[i].kind == ParamKind::Choice) {
            const auto* names = entry["choices"].getArray();
            CHECK(names != nullptr && names->size() == static_cast<int>(kParams[i].max - kParams[i].min) + 1);
        }
    }

    // Every sample in samples.json is built in, and the "osc sample" choices list them all.
    data = resource("samples.json", size);
    CHECK(data != nullptr);
    const juce::var samples = juce::JSON::parse(juce::String::fromUTF8(data, size));
    const auto* entries = samples["samples"].getArray();
    CHECK(entries != nullptr && entries->size() == static_cast<int>(kParams[param::osc1Sample].max) + 1);
    for (int i = 0; entries != nullptr && i < entries->size(); ++i) {
        int bytes = 0;
        CHECK(resource((*entries)[i]["file"].toString().toRawUTF8(), bytes) != nullptr && bytes > 1000);
    }
    NOTE("built in: %d params, %d samples\n", param::count, entries != nullptr ? entries->size() : 0);
}

void stateFormat() {
    // Defaults, written and read back.
    const StateData defaults = defaultState();
    const juce::String json = writeState(defaults);
    CHECK(json.startsWith("{\"v\":1,\"name\":\"\",\"params\":{\"filterDw1\":"));
    CHECK(json.contains("\"fxOrder\":[[2,0,1],[2,0,1]]"));
    const StateData back = readState(json);
    for (int i = 0; i < param::count; ++i) {
        CHECK_NEAR(back.values[static_cast<size_t>(i)], defaults.values[static_cast<size_t>(i)], 1e-12);
    }
    CHECK(back.fxOrder[0] == kDefaultFxOrder && back.fxOrder[1] == kDefaultFxOrder);
    CHECK(back.name.isEmpty());

    // What the web build saves (JSON.stringify of buildState, also a .brimstone preset): read as is.
    const StateData web = readState(
        R"({"v":1,"name":"Sub drop","params":{"filterDw1":0.25,"osc2Sample":5,"unisonVoices":3,"lfo1Sync":0,"osc1Vol":1.2},)"
        R"("fxOrder":[[0,1,2],[1,2,0]]})");
    CHECK(web.name == "Sub drop");
    StateData named = defaultState();
    named.name = "Growl";
    CHECK(readState(writeState(named)).name == "Growl");
    CHECK_NEAR(web.values[param::filterDw1], 0.25, 1e-12);
    CHECK_NEAR(web.values[param::osc2Sample], 5, 0);
    CHECK_NEAR(web.values[param::unisonVoices], 3, 0);
    CHECK_NEAR(web.values[param::lfo1Sync], 0, 0);
    CHECK_NEAR(web.values[param::filterPos1], kParams[param::filterPos1].def, 0); // missing: default
    CHECK((web.fxOrder[0] == FxOrder{0, 1, 2}));
    CHECK((web.fxOrder[1] == FxOrder{1, 2, 0}));

    // Bad input: unknown ids ignored, out of range clamped, not a number or not a permutation -> default.
    const StateData bad = readState(
        R"({"v":1,"params":{"nope":3,"filterDw1":7,"osc1Sample":2.6,"filterPos1":"x"},"fxOrder":[[0,0,1],[2,1]]})");
    CHECK_NEAR(bad.values[param::filterDw1], 1, 0);
    CHECK_NEAR(bad.values[param::osc1Sample], 3, 0);
    CHECK_NEAR(bad.values[param::filterPos1], kParams[param::filterPos1].def, 0);
    CHECK(bad.fxOrder[0] == kDefaultFxOrder && bad.fxOrder[1] == kDefaultFxOrder);
    const StateData junk = readState("not json");
    CHECK_NEAR(junk.values[param::filterPos1], kParams[param::filterPos1].def, 0);
}

void processorState() {
    // Save from one processor, load into another: the same values and order come back.
    Processor a;
    StateData s = defaultState();
    s.values[param::filterDw1] = 0.4;
    s.values[param::osc1Sample] = 5; // E.Piano (7 samples: 0..6)
    s.values[param::unisonVoices] = 2;
    s.values[param::lfo2Retrig] = 1;
    s.values[indexOf("osc1Pan")] = -0.5;
    s.fxOrder = {FxOrder{1, 0, 2}, FxOrder{0, 2, 1}};
    s.name = "Growl";
    a.applyState(s);

    juce::MemoryBlock saved;
    a.getStateInformation(saved);
    Processor b;
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    const StateData got = b.currentState();
    for (int i = 0; i < param::count; ++i) {
        CHECK_NEAR(got.values[static_cast<size_t>(i)], s.values[static_cast<size_t>(i)], 1e-6);
    }
    CHECK((got.fxOrder[0] == FxOrder{1, 0, 2}));
    CHECK((got.fxOrder[1] == FxOrder{0, 2, 1}));
    CHECK(got.name == "Growl");

    // The saved bytes are the web JSON, nothing around it.
    const juce::String text = juce::String::fromUTF8(static_cast<const char*>(saved.getData()), static_cast<int>(saved.getSize()));
    CHECK(text.startsWith("{\"v\":1,"));
    CHECK(text.contains("\"osc1Sample\":5"));
}

void processorSound() {
    for (const int blockSize : {64, 512}) {
        Processor p;
        p.setPlayConfigDetails(0, 2, kRate, blockSize);
        p.prepareToPlay(kRate, blockSize);

        CHECK(render(p, 4, blockSize) < 1e-6); // silent before any note

        juce::MidiBuffer on;
        on.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        // Listen at the same moment for every block size: 250 ms in, past the default attack.
        const int quarterSecond = static_cast<int>(kRate * 0.25) / blockSize;
        const double playing = render(p, quarterSecond, blockSize, on);
        NOTE("block %d: note on, RMS %.4f\n", blockSize, playing);
        CHECK(playing > 0.01);

        // CC 123 releases everything; after the longest release plus the reverb tail it is silent.
        juce::MidiBuffer off;
        off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        const int seconds = static_cast<int>(std::ceil(p.getTailLengthSeconds())) + 9;
        const double after = render(p, static_cast<int>(kRate * seconds) / blockSize, blockSize, off);
        NOTE("block %d: %d s after CC 123, RMS %.6f, tail %.1f s\n", blockSize, seconds, after, p.getTailLengthSeconds());
        CHECK(after < 0.001);

        // Loading a state releases held notes (every note-on has a note-off path).
        render(p, 20, blockSize, on);
        juce::MemoryBlock saved;
        p.getStateInformation(saved);
        p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        const double released = render(p, static_cast<int>(kRate * seconds) / blockSize, blockSize);
        CHECK(released < 0.001);
        p.releaseResources();
    }
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    std::printf("== built-in data\n");
    builtInData();
    std::printf("== state format\n");
    stateFormat();
    std::printf("== processor state\n");
    processorState();
    std::printf("== processor sound\n");
    processorSound();
    if (test::failures() > 0) {
        std::printf("%d check(s) failed\n", test::failures());
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
