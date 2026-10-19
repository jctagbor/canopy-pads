// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include "DroneEngine.h"
#include "SampleLayer.h"
#include <atomic>

namespace canopy
{
namespace ID
{
    static const juce::String master = "master", swell = "swell", attack = "attack", release = "release",
        latch = "latch", mode = "mode", l1style = "l1style", l1level = "l1level", l2style = "l2style", l2level = "l2level",
        l3style = "l3style", l3level = "l3level", sub = "sub", brightness = "brightness", motion = "motion", width = "width",
        eqlow = "eqlow", eqmid = "eqmid", eqhigh = "eqhigh", revmix = "revmix", revsize = "revsize",
        samplelevel = "samplelevel", autotranspose = "autotranspose", zonelow = "zonelow", zonehigh = "zonehigh",
        sustainlatch = "sustainlatch";
}

// Things a footswitch / controller can be mapped to.
enum class MidiAction { Next = 0, Prev, Stop, Panic, LatchToggle, ModeCycle, Swell, Volume, Count };
static const char* const kActionNames[(int) MidiAction::Count] =
    { "Next setlist item", "Previous setlist item", "Stop (fade out)", "Panic (instant silence)",
      "Toggle latch", "Cycle mode", "Swell (CC)", "Master volume (CC)" };

struct MidiMapping
{
    std::atomic<int> type { 0 };     // 0 none, 1 note, 2 cc
    std::atomic<int> number { -1 };
    std::atomic<int> channel { 0 };  // 0 = any
};

class CanopyProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    CanopyProcessor();
    ~CanopyProcessor() override;

    using AudioProcessor::processBlock;
    // --- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- UI entry points (message thread). Thread-safe via a command FIFO.
    void uiSetKey (int pitchClass);
    void uiStop();
    void uiPanic();
    void uiSetMode (DroneMode m);
    void uiToggleLatch();
    void uiCycleMode();

    juce::AudioProcessorValueTreeState apvts;
    SampleLayer sampleLayer;

    // Display state
    std::atomic<int>   currentKey { -1 };       // 0..11 or -1 when silent
    std::atomic<int>   activeVoices { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool>  sustainHeld { false };

    // --- MIDI learn
    MidiMapping mappings[(int) MidiAction::Count];
    std::atomic<int> learnTarget { -1 };        // MidiAction index while learning, -1 idle
    std::atomic<int> zoneLearnStep { 0 };       // 0 idle, 1 waiting for low, 2 waiting for high
    void startLearn (MidiAction a) { learnTarget = (int) a; }
    void cancelLearn() { learnTarget = -1; zoneLearnStep = 0; }
    void clearMapping (MidiAction a);
    void startZoneLearn() { zoneLearnStep = 1; }
    juce::String describeMapping (MidiAction a) const;

    // --- Setlist (message thread)
    juce::ValueTree getSetlist() { return apvts.state.getOrCreateChildWithName ("setlist", nullptr); }
    int  getCurrentItem() const { return currentItem; }
    void addItemFromCurrent (const juce::String& name);
    void updateItemFromCurrent (int index);
    void goToItem (int index);
    void nextItem();
    void prevItem();
    void removeItem (int index);
    void moveItem (int index, int delta);
    void renameItem (int index, const juce::String& name);
    bool saveSetlist (const juce::File& f);
    bool loadSetlist (const juce::File& f, juce::String& error);

    // --- Samples (message thread)
    bool loadSampleSlot (int key, const juce::File& f, juce::String& error);
    void clearSampleSlot (int key);
    int  loadSampleFolder (const juce::File& dir, juce::String& report);

    // Fires on the message thread whenever setlist / mappings / samples change.
    juce::ChangeBroadcaster stateChanged;

    EngineParams currentEngineParams() const;
    static juce::String midiNoteName (int note);

private:
    struct Command { int type; int a; };
    enum CmdType { CmdSetKey = 1, CmdStop, CmdPanic };
    juce::AbstractFifo cmdFifo { 64 };
    Command cmdBuffer[64];
    void pushCommand (Command c);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void handleMidi (const juce::MidiMessage& m, int samplePos);
    void setDrone (int pc, DroneMode mode);
    void triggerChord();
    void stopDrone();
    void refreshMappingsFromState();
    void writeMappingToState (MidiAction a);
    void applyItem (const juce::ValueTree& item);
    float paramValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParamNormalised (const juce::String& id, float normalised);
    void setParamPlain (const juce::String& id, float plain);

    DroneEngine engine;
    double sr = 48000.0;

    // Audio-thread MIDI state
    std::array<int, 16> heldNotes {};
    int numHeld = 0;
    bool chordDirty = false;
    juce::int64 samplesSinceChordTrigger = 1 << 30;
    juce::int64 sampleClock = 0;
    bool sustainDown = false;
    bool releasePendingOnSustain = false;

    // Audio -> message thread requests (handled in handleAsyncUpdate)
    std::atomic<int>   reqNext { 0 }, reqPrev { 0 }, reqLatchToggle { 0 }, reqModeCycle { 0 };
    std::atomic<float> reqSwell { -1.0f }, reqVolume { -1.0f };
    std::atomic<int>   reqZoneLow { -1 }, reqZoneHigh { -1 };
    std::atomic<int>   reqLearnedAction { -1 };
    std::atomic<int>   reqStateChanged { 0 };

    int currentItem = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanopyProcessor)
};
} // namespace canopy
