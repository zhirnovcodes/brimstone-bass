#include "PluginProcessor.h"

#include "BrimstoneBassData.h"

#include <cmath>
#include <limits>

namespace bb::plugin {

namespace {

// A file built into the plugin (CMake juce_add_binary_data), by its original file name.
const char* findResource(const char* file, int& size) {
    for (int i = 0; i < BrimstoneBassData::namedResourceListSize; ++i) {
        if (juce::String(BrimstoneBassData::originalFilenames[i]) == file) {
            return BrimstoneBassData::getNamedResource(BrimstoneBassData::namedResourceList[i], size);
        }
    }
    size = 0;
    return nullptr;
}

juce::var readJson(const char* file) {
    int size = 0;
    const char* data = findResource(file, size);
    return data != nullptr ? juce::JSON::parse(juce::String::fromUTF8(data, size)) : juce::var();
}

int packOrder(const FxOrder& o) { return o[0] | (o[1] << 4) | (o[2] << 8); }

FxOrder unpackOrder(int packed) { return {packed & 0xF, (packed >> 4) & 0xF, (packed >> 8) & 0xF}; }

} // namespace

Processor::BusesProperties Processor::buses() {
    return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout Processor::layout() {
    // Ids, ranges and defaults come from ParamsJson.h (CMake writes it from params.json). Labels, choice names
    // and "automatable" come from the same params.json, built into the plugin.
    const juce::var json = readJson("params.json");
    const juce::Array<juce::var>* list = json["params"].getArray();
    jassert(list != nullptr && list->size() == param::count);

    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (int i = 0; i < param::count; ++i) {
        const ParamSpec& p = kParams[i];
        const juce::var entry = (list != nullptr && i < list->size()) ? (*list)[i] : juce::var();
        jassert(entry["id"].toString() == p.id);

        const juce::ParameterID id{p.id, 1};
        const juce::String label = entry["label"].isString() ? entry["label"].toString() : juce::String(p.id);
        // "automatable": false hides it from DAW automation and MIDI learn (docs/plan.md, MIDI control).
        const bool automatable = entry["automatable"].isBool() ? static_cast<bool>(entry["automatable"]) : true;

        switch (p.kind) {
        case ParamKind::Float:
            result.add(std::make_unique<juce::AudioParameterFloat>(
                id, label, juce::NormalisableRange<float>(static_cast<float>(p.min), static_cast<float>(p.max)),
                static_cast<float>(p.def), juce::AudioParameterFloatAttributes().withAutomatable(automatable)));
            break;
        case ParamKind::Int:
            result.add(std::make_unique<juce::AudioParameterInt>(
                id, label, static_cast<int>(p.min), static_cast<int>(p.max), static_cast<int>(p.def),
                juce::AudioParameterIntAttributes().withAutomatable(automatable)));
            break;
        case ParamKind::Choice: {
            const int count = static_cast<int>(p.max - p.min) + 1;
            juce::StringArray choices;
            if (const auto* names = entry["choices"].getArray()) {
                for (const auto& name : *names) {
                    choices.add(name.toString());
                }
            }
            for (int k = choices.size(); k < count; ++k) {
                choices.add(juce::String(static_cast<int>(p.min) + k));
            }
            choices.removeRange(count, choices.size() - count);
            result.add(std::make_unique<juce::AudioParameterChoice>(
                id, label, choices, static_cast<int>(p.def - p.min),
                juce::AudioParameterChoiceAttributes().withAutomatable(automatable)));
            break;
        }
        case ParamKind::Boolean:
        case ParamKind::Trigger:
            result.add(std::make_unique<juce::AudioParameterBool>(
                id, label, p.def > 0.5, juce::AudioParameterBoolAttributes().withAutomatable(automatable)));
            break;
        }
    }
    return result;
}

Processor::Processor() : AudioProcessor(buses()), params_(*this, nullptr, "params", layout()) {
    for (int i = 0; i < param::count; ++i) {
        raw_[static_cast<size_t>(i)] = params_.getRawParameterValue(kParams[i].id);
        parameters_[static_cast<size_t>(i)] = params_.getParameter(kParams[i].id);
        jassert(raw_[static_cast<size_t>(i)] != nullptr && parameters_[static_cast<size_t>(i)] != nullptr);
    }
    for (auto& order : fxOrder_) {
        order.store(packOrder(kDefaultFxOrder));
    }
    loadSamples();
}

void Processor::loadSamples() {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const juce::var json = readJson("samples.json");
    const juce::Array<juce::var>* list = json["samples"].getArray();
    if (list == nullptr) {
        jassertfalse;
        return;
    }
    // Index = position in samples.json = the "osc sample" choice index.
    samples_.resize(static_cast<size_t>(juce::jmin(list->size(), Engine::kMaxSamples)));
    for (size_t index = 0; index < samples_.size(); ++index) {
        const juce::var& entry = (*list)[static_cast<int>(index)];
        int size = 0;
        const char* data = findResource(entry["file"].toString().toRawUTF8(), size);
        if (data == nullptr) {
            jassertfalse;
            continue;
        }
        std::unique_ptr<juce::AudioFormatReader> reader(
            formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(data, static_cast<size_t>(size), false)));
        if (reader == nullptr || reader->lengthInSamples <= 1) {
            jassertfalse;
            continue;
        }
        Sample& s = samples_[index];
        const int channels = juce::jmin(2, static_cast<int>(reader->numChannels));
        const int frames = static_cast<int>(reader->lengthInSamples);
        s.audio.setSize(channels, frames);
        reader->read(&s.audio, 0, frames, 0, true, channels > 1);
        s.rate = reader->sampleRate;
        s.rootNote = static_cast<int>(entry["rootNote"]);
        s.meta.attack = static_cast<double>(entry["attack"]);
        s.meta.loop = static_cast<double>(entry["loop"]);
        s.meta.release = static_cast<double>(entry["release"]);
        s.meta.crossfade = entry.hasProperty("crossfade") ? static_cast<double>(entry["crossfade"]) : 0.0;
    }
}

void Processor::prepareToPlay(double sampleRate, int) {
    // A new engine allocates its delay lines and reverb tanks here, off the audio thread.
    sampleRate_ = sampleRate;
    engine_ = std::make_unique<Engine>(sampleRate, static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt()));
    for (size_t index = 0; index < samples_.size(); ++index) {
        const Sample& s = samples_[index];
        if (s.audio.getNumSamples() <= 1) {
            continue;
        }
        SampleData data;
        data.left = s.audio.getReadPointer(0);
        data.right = s.audio.getNumChannels() > 1 ? s.audio.getReadPointer(1) : nullptr;
        data.frames = s.audio.getNumSamples();
        data.rate = s.rate;
        engine_->setSample(static_cast<int>(index), data, s.meta, s.rootNote);
    }
    sent_.fill(std::numeric_limits<double>::quiet_NaN());
    fxOrderDirty_.store(true);
    allOffPending_.store(false);
    havePosition_ = false;
    playing_ = false;
}

void Processor::releaseResources() {
    // The next prepareToPlay builds a new engine; no voice outlives this one.
    engine_.reset();
}

bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

double Processor::plainValue(int index) const {
    const ParamSpec& p = kParams[index];
    const double raw = raw_[static_cast<size_t>(index)]->load();
    // A choice's raw value is its index; the plain value starts at min.
    return clampPlain(index, p.kind == ParamKind::Choice ? p.min + raw : raw);
}

void Processor::followTransport(int frames) {
    auto* head = getPlayHead();
    if (head == nullptr) {
        return;
    }
    const auto position = head->getPosition();
    if (!position.hasValue()) {
        return;
    }
    Transport t;
    t.playing = position->getIsPlaying();
    t.bpm = position->getBpm().orFallback(120.0);
    t.beat = position->getPpqPosition().orFallback(0.0);
    if (const auto sig = position->getTimeSignature()) {
        t.numerator = sig->numerator;
        t.denominator = sig->denominator;
    }
    if (!(t.bpm > 0.0) || !std::isfinite(t.beat)) {
        return;
    }

    // Sent only on play / stop, a tempo change or a jump (loop, locate), as IvanDAW does. In between the
    // engine follows its own clock.
    const double blockBeats = frames * t.bpm / 60.0 / sampleRate_;
    const bool jumped = std::abs(t.beat - expectedBeat_) > blockBeats + 1e-9;
    if (!havePosition_ || t.playing != playing_ || t.bpm != bpm_ || jumped) {
        if (havePosition_ && playing_ && !t.playing) {
            engine_->allOff(0, out_); // DAW stop: no note hangs
        }
        engine_->transport(t, 0);
    }
    havePosition_ = true;
    playing_ = t.playing;
    bpm_ = t.bpm;
    expectedBeat_ = t.playing ? t.beat + blockBeats : t.beat;
}

void Processor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int frames = audio.getNumSamples();
    audio.clear();
    if (engine_ == nullptr || frames <= 0) {
        midi.clear();
        return;
    }
    out_.clear();

    if (allOffPending_.exchange(false)) {
        engine_->allOff(0, out_);
    }

    // 1. Effects order (state, from the UI or setStateInformation).
    if (fxOrderDirty_.exchange(false)) {
        for (int osc = 0; osc < 2; ++osc) {
            const FxOrder o = unpackOrder(fxOrder_[static_cast<size_t>(osc)].load());
            engine_->setFxOrder(osc, o[0], o[1], o[2]);
        }
    }

    // 2. Parameters that changed since the last block.
    for (int i = 0; i < param::count; ++i) {
        const double v = plainValue(i);
        if (v != sent_[static_cast<size_t>(i)]) {
            engine_->setParam(i, v);
            sent_[static_cast<size_t>(i)] = v;
        }
    }

    // 3. Host transport at frame 0.
    followTransport(frames);

    // 4. MIDI in, at its sample position.
    for (const auto m : midi) {
        const auto* d = m.data;
        if (m.numBytes >= 1 && m.numBytes <= 3 && m.samplePosition >= 0 && m.samplePosition < frames) {
            engine_->midi(m.samplePosition, d[0], m.numBytes > 1 ? d[1] : 0, m.numBytes > 2 ? d[2] : 0);
        }
    }
    midi.clear();

    // 5. Render.
    float* left = audio.getNumChannels() > 0 ? audio.getWritePointer(0) : nullptr;
    float* right = audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr;
    engine_->process(frames, left, right, out_);
    tail_.store(engine_->tailSeconds());
}

void Processor::setFxOrder(int osc, const FxOrder& order) {
    if ((osc != 0 && osc != 1) || !isFxOrder(order)) {
        return;
    }
    fxOrder_[static_cast<size_t>(osc)].store(packOrder(order));
    fxOrderDirty_.store(true);
}

FxOrder Processor::fxOrder(int osc) const { return unpackOrder(fxOrder_[static_cast<size_t>(osc == 1)].load()); }

StateData Processor::currentState() const {
    StateData state;
    for (int i = 0; i < param::count; ++i) {
        state.values[static_cast<size_t>(i)] = plainValue(i);
    }
    state.fxOrder = {fxOrder(0), fxOrder(1)};
    state.name = presetName();
    return state;
}

juce::String Processor::presetName() const {
    const juce::SpinLock::ScopedLockType lock(nameLock_);
    return presetName_;
}

void Processor::setPresetName(const juce::String& name) {
    const juce::SpinLock::ScopedLockType lock(nameLock_);
    presetName_ = name;
}

void Processor::applyState(const StateData& state) {
    for (int i = 0; i < param::count; ++i) {
        const ParamSpec& p = kParams[i];
        const double plain = clampPlain(i, state.values[static_cast<size_t>(i)]);
        auto* parameter = parameters_[static_cast<size_t>(i)];
        const float raw = static_cast<float>(p.kind == ParamKind::Choice ? plain - p.min : plain);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(raw));
    }
    for (int osc = 0; osc < 2; ++osc) {
        setFxOrder(osc, state.fxOrder[static_cast<size_t>(osc)]);
    }
    setPresetName(state.name);
    // A new sound releases the old one's notes.
    allOffPending_.store(true);
}

void Processor::getStateInformation(juce::MemoryBlock& destData) {
    const juce::String json = writeState(currentState());
    destData.replaceAll(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void Processor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) {
        return;
    }
    applyState(readState(juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes)));
}

} // namespace bb::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new bb::plugin::Processor(); }
