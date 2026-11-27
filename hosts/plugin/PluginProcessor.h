#pragma once

// Brimstone Bass as a JUCE instrument (VST3 / AU / Standalone). A translator only: every musical decision is in
// bb::Engine. Parameters come from params.json (same ids, ranges and defaults as the web build); the saved
// state is the same JSON as the web getState().data (State.h).

#include "Engine.h"
#include "State.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace bb::plugin {

class Processor final : public juce::AudioProcessor {
public:
    Processor();
    ~Processor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override; // PluginEditor.cpp
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tail_.load(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Message thread. Effects order is state, not a parameter: only the UI and setStateInformation change it.
    void setFxOrder(int osc, const FxOrder& order);
    FxOrder fxOrder(int osc) const;

    // Index = params.json index. The editor reads and sets them on the message thread.
    juce::RangedAudioParameter& parameter(int index) { return *parameters_[static_cast<size_t>(index)]; }

    StateData currentState() const;
    void applyState(const StateData& state);

    // The preset the sound came from (saved with the state); empty = none. Any thread but the audio thread.
    juce::String presetName() const;
    void setPresetName(const juce::String& name);

private:
    struct Sample {
        juce::AudioBuffer<float> audio;
        double rate = 0.0;
        int rootNote = 60;
        SampleMeta meta; // attack, loop, release, crossfade (samples.json)
    };

    static BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void loadSamples();
    double plainValue(int index) const;
    void followTransport(int frames);

    std::vector<Sample> samples_; // decoded once; the engine keeps pointers into them
    juce::AudioProcessorValueTreeState params_;
    std::array<std::atomic<float>*, param::count> raw_{};
    std::array<juce::RangedAudioParameter*, param::count> parameters_{};

    // Audio thread only.
    std::unique_ptr<Engine> engine_;
    OutBuffer out_;
    std::array<double, param::count> sent_{}; // last plain value given to the engine
    double sampleRate_ = 44100.0;
    bool havePosition_ = false;
    bool playing_ = false;
    double bpm_ = 0.0;
    double expectedBeat_ = 0.0;

    // Message thread -> audio thread.
    std::array<std::atomic<int>, 2> fxOrder_{};  // packed a | b << 4 | c << 8
    std::atomic<bool> fxOrderDirty_{true};
    std::atomic<bool> allOffPending_{false};

    std::atomic<double> tail_{0.0};

    mutable juce::SpinLock nameLock_;
    juce::String presetName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};

} // namespace bb::plugin
