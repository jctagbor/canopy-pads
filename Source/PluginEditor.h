// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

namespace canopy
{
struct TabBase : public juce::Component
{
    virtual void refresh() {}       // state (setlist / mappings / samples) changed
    virtual void tick() {}          // 30 Hz
};

class CanopyEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit CanopyEditor (CanopyProcessor&);
    ~CanopyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    CanopyProcessor& proc;
    CanopyLookAndFeel lnf;

    // Header
    juce::Label title, credit, keyLabel, modeLabel, statusLabel;
    juce::TextButton latchButton { "LATCH" }, stopButton { "STOP" }, panicButton { "PANIC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAtt;
    float meterLevel = 0.0f;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Array<TabBase*> tabPages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanopyEditor)
};
} // namespace canopy
