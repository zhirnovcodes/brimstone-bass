#include "PluginEditor.h"

#include "BrimstoneBassUi.h"

#include <cstring>
#include <optional>
#include <vector>

namespace bb::plugin {

namespace {

juce::String mimeFor(const juce::String& file) {
    if (file.endsWithIgnoreCase(".html")) {
        return "text/html;charset=utf-8";
    }
    if (file.endsWithIgnoreCase(".js")) {
        return "text/javascript;charset=utf-8";
    }
    return "application/octet-stream";
}

// The page's files, built into the plugin: index.html and skin.js (the skin with its CSS, font and images).
std::optional<juce::WebBrowserComponent::Resource> serve(const juce::String& url) {
    juce::String file = url.upToFirstOccurrenceOf("?", false, false).fromLastOccurrenceOf("/", false, false);
    if (file.isEmpty()) {
        file = "index.html";
    }
    for (int i = 0; i < BrimstoneBassUi::namedResourceListSize; ++i) {
        if (!file.equalsIgnoreCase(BrimstoneBassUi::originalFilenames[i])) {
            continue;
        }
        int size = 0;
        const char* data = BrimstoneBassUi::getNamedResource(BrimstoneBassUi::namedResourceList[i], size);
        if (data == nullptr || size <= 0) {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(static_cast<size_t>(size));
        std::memcpy(bytes.data(), data, bytes.size());
        return juce::WebBrowserComponent::Resource{std::move(bytes), mimeFor(file)};
    }
    return std::nullopt;
}

int indexOf(const juce::String& id) {
    for (int i = 0; i < param::count; ++i) {
        if (id == kParams[i].id) {
            return i;
        }
    }
    return -1;
}

juce::var orderVar(const FxOrder& o) { return juce::Array<juce::var>{o[0], o[1], o[2]}; }

} // namespace

Editor::Editor(Processor& processor) : juce::AudioProcessorEditor(processor), processor_(processor) {
    const auto options = browserOptions();
    if (juce::WebBrowserComponent::areOptionsSupported(options)) {
        browser_ = std::make_unique<juce::WebBrowserComponent>(options);
        addAndMakeVisible(*browser_);
        browser_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        startTimerHz(30);
    } else {
#if JUCE_WINDOWS
        missing_.setText("Install the Microsoft Edge WebView2 Runtime to open this window.", juce::dontSendNotification);
#else
        missing_.setText("This window could not open its web view.", juce::dontSendNotification);
#endif
        missing_.setJustificationType(juce::Justification::centred);
        missing_.setColour(juce::Label::textColourId, juce::Colours::black);
        addAndMakeVisible(missing_);
    }
    setResizable(false, false);
    setSize(kWidth, kHeight);
}

Editor::~Editor() {
    stopTimer();
    endGestures();
    browser_ = nullptr;
}

void Editor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xfff4f4f4)); }

void Editor::resized() {
    if (browser_ != nullptr) {
        browser_->setBounds(getLocalBounds());
    } else {
        missing_.setBounds(getLocalBounds().reduced(16));
    }
}

juce::var Editor::snapshot() {
    auto params = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < param::count; ++i) {
        shown_[static_cast<size_t>(i)] = processor_.parameter(i).getValue();
        params->setProperty(kParams[i].id, shown_[static_cast<size_t>(i)]);
    }
    juce::Array<juce::var> orders;
    for (int osc = 0; osc < 2; ++osc) {
        shownOrder_[static_cast<size_t>(osc)] = processor_.fxOrder(osc);
        orders.add(orderVar(shownOrder_[static_cast<size_t>(osc)]));
    }
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("params", juce::var(params.release()));
    result->setProperty("fxOrder", orders);
    shownName_ = processor_.presetName();
    result->setProperty("name", shownName_);
    return juce::var(result.release());
}

void Editor::setParam(const juce::String& id, double normalized, const juce::String& gesture) {
    const int index = indexOf(id);
    if (index < 0 || !std::isfinite(normalized)) {
        return;
    }
    const auto i = static_cast<size_t>(index);
    auto& p = processor_.parameter(index);
    const float n = juce::jlimit(0.0f, 1.0f, static_cast<float>(normalized));
    // DAWs record automation between begin and end. A single change (a click, a step) is a gesture of its own.
    const bool single = gesture.isEmpty() && !gesture_[i];
    if ((gesture == "begin" || single) && !gesture_[i]) {
        p.beginChangeGesture();
        gesture_[i] = true;
    }
    p.setValueNotifyingHost(n);
    if ((gesture == "end" || single) && gesture_[i]) {
        p.endChangeGesture();
        gesture_[i] = false;
    }
    // The page already shows it (choices snap: the timer sends the snapped value if it differs).
    shown_[i] = n;
}

void Editor::endGestures() {
    for (int i = 0; i < param::count; ++i) {
        if (gesture_[static_cast<size_t>(i)]) {
            processor_.parameter(i).endChangeGesture();
            gesture_[static_cast<size_t>(i)] = false;
        }
    }
}

void Editor::timerCallback() {
    if (!pageReady_ || browser_ == nullptr) {
        return;
    }
    // Changes that did not come from the page: DAW automation, MIDI learn, a loaded state, choice snapping.
    auto changes = std::make_unique<juce::DynamicObject>();
    bool any = false;
    for (int i = 0; i < param::count; ++i) {
        const float n = processor_.parameter(i).getValue();
        if (n != shown_[static_cast<size_t>(i)]) {
            shown_[static_cast<size_t>(i)] = n;
            changes->setProperty(kParams[i].id, n);
            any = true;
        }
    }
    if (any) {
        browser_->emitEventIfBrowserIsVisible("params", juce::var(changes.release()));
    }
    for (int osc = 0; osc < 2; ++osc) {
        const FxOrder order = processor_.fxOrder(osc);
        if (order != shownOrder_[static_cast<size_t>(osc)]) {
            shownOrder_[static_cast<size_t>(osc)] = order;
            auto event = std::make_unique<juce::DynamicObject>();
            event->setProperty("osc", osc);
            event->setProperty("order", orderVar(order));
            browser_->emitEventIfBrowserIsVisible("fxOrder", juce::var(event.release()));
        }
    }
    // A state the DAW loaded names its preset too.
    const juce::String name = processor_.presetName();
    if (name != shownName_) {
        shownName_ = name;
        browser_->emitEventIfBrowserIsVisible("presetName", name);
    }
}

namespace {

// The folder SAVE and LOAD on the PRESETS screen start in: the last one used (message thread), else Documents.
juce::File& lastPresetFolder() {
    static juce::File folder;
    return folder;
}

juce::File presetFolder() {
    const juce::File& last = lastPresetFolder();
    return last.isDirectory() ? last : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
}

void rememberPresetFolder(const juce::File& file) { lastPresetFolder() = file.getParentDirectory(); }

} // namespace

void Editor::savePresetFile(const juce::String& name, const juce::String& text, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
    chooser_ = std::make_unique<juce::FileChooser>("Save preset", presetFolder().getChildFile(name + ".brimstone"), "*.brimstone");
    const auto mode = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;
    chooser_->launchAsync(mode, [this, text, completion](const juce::FileChooser& chooser) {
        juce::File file = chooser.getResult();
        if (file == juce::File()) {
            completion(juce::String()); // cancelled
            return;
        }
        file = file.withFileExtension("brimstone");
        // The file's name is the preset's name, inside the file too.
        const juce::String saved = file.getFileNameWithoutExtension();
        juce::var data = juce::JSON::parse(text);
        if (auto* object = data.getDynamicObject()) {
            object->setProperty("name", saved);
        }
        if (!data.isObject() || !file.replaceWithText(juce::JSON::toString(data) + "\n")) {
            completion(juce::String());
            return;
        }
        rememberPresetFolder(file);
        processor_.setPresetName(saved);
        shownName_ = saved;
        completion(saved);
    });
}

void Editor::loadPresetFile(juce::WebBrowserComponent::NativeFunctionCompletion completion) {
    chooser_ = std::make_unique<juce::FileChooser>("Load preset", presetFolder(), "*.brimstone");
    const auto mode = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser_->launchAsync(mode, [completion](const juce::FileChooser& chooser) {
        const juce::File file = chooser.getResult();
        if (!file.existsAsFile() || file.getSize() > 1024 * 1024) {
            completion(juce::var()); // cancelled, or not a preset
            return;
        }
        rememberPresetFolder(file);
        // The page reads it (presets.ts parsePreset) and loads it with loadState.
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("name", file.getFileNameWithoutExtension());
        result->setProperty("text", file.loadFileAsString());
        completion(juce::var(result.release()));
    });
}

juce::WebBrowserComponent::Options Editor::browserOptions() {
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    juce::WebBrowserComponent::Options options;
#if JUCE_WINDOWS
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BrimstoneBass").getChildFile("WebView2");
    folder.createDirectory();
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                  .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                                              .withUserDataFolder(folder)
                                              .withStatusBarDisabled()
                                              .withBackgroundColour(juce::Colour(0xfff4f4f4)));
#endif
    return options.withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider([](const juce::String& url) { return serve(url); })
        .withNativeFunction("ready", [this](const juce::Array<juce::var>&, Completion completion) {
            pageReady_ = true;
            completion(snapshot());
        })
        .withNativeFunction("setParam", [this](const juce::Array<juce::var>& args, Completion completion) {
            if (args.size() >= 2) {
                setParam(args[0].toString(), static_cast<double>(args[1]), args.size() > 2 ? args[2].toString() : juce::String());
            }
            completion({});
        })
        .withNativeFunction("setFxOrder", [this](const juce::Array<juce::var>& args, Completion completion) {
            if (args.size() >= 2 && args[1].isArray() && args[1].size() == 3) {
                const int osc = static_cast<int>(args[0]);
                const FxOrder order{static_cast<int>(args[1][0]), static_cast<int>(args[1][1]), static_cast<int>(args[1][2])};
                if ((osc == 0 || osc == 1) && isFxOrder(order)) {
                    processor_.setFxOrder(osc, order);
                    shownOrder_[static_cast<size_t>(osc)] = order;
                }
            }
            completion({});
        })
        .withNativeFunction("loadState", [this](const juce::Array<juce::var>& args, Completion completion) {
            if (!args.isEmpty()) {
                endGestures();
                const StateData state = readState(juce::JSON::toString(args[0], true));
                processor_.applyState(state);
                shownName_ = state.name; // the page already shows it
            }
            completion({});
        })
        .withNativeFunction("savePresetFile", [this](const juce::Array<juce::var>& args, Completion completion) {
            if (args.size() < 2) {
                completion(juce::String());
                return;
            }
            savePresetFile(args[0].toString(), args[1].toString(), std::move(completion));
        })
        .withNativeFunction("loadPresetFile", [this](const juce::Array<juce::var>&, Completion completion) {
            loadPresetFile(std::move(completion));
        });
}

} // namespace bb::plugin

juce::AudioProcessorEditor* bb::plugin::Processor::createEditor() { return new bb::plugin::Editor(*this); }
