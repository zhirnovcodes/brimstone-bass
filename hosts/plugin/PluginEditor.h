#pragma once

// The plugin window: the same skin as the IvanDAW build (apps/web/src/ui), in a web view (WebView2 on Windows,
// WKWebView on macOS). The page talks to the processor only through the functions and events below; the sound
// stays in the processor. Bridge on the page side: apps/web/src/vst.ts.

#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <memory>

namespace bb::plugin {

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int kWidth = 260;  // the device at Figma size, as the web manifest
    static constexpr int kHeight = 306;

    explicit Editor(Processor& processor);
    ~Editor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options browserOptions();
    juce::var snapshot();
    void setParam(const juce::String& id, double normalized, const juce::String& gesture);
    void endGestures();

    Processor& processor_;
    std::unique_ptr<juce::WebBrowserComponent> browser_;
    juce::Label missing_;
    bool pageReady_ = false;
    std::array<float, param::count> shown_{};   // the normalized value the page shows
    std::array<bool, param::count> gesture_{}; // a drag from the page is open
    std::array<FxOrder, 2> shownOrder_{kDefaultFxOrder, kDefaultFxOrder};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};

} // namespace bb::plugin
