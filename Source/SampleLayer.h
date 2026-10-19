// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace canopy
{
// Plays the user's own pad files (WAV/AIFF/MP3/FLAC), one slot per key, looped,
// with the same attack/release crossfade as the synth voices. If a key has no
// file, the nearest loaded slot is transposed to cover it.
class SampleLayer
{
public:
    struct Slot
    {
        juce::String name;
        juce::String path;
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100.0;
    };

    SampleLayer();
    void prepare (double sampleRate, int maxBlock);

    // Message thread: load/clear a slot. Returns false on failure.
    bool loadSlot (int key, const juce::File& file, juce::String& error);
    void clearSlot (int key);
    void clearAll();
    const Slot& getSlot (int key) const { return slots[(size_t) key]; }
    bool anyLoaded() const;

    // Guess which key a file is for from its name ("Pad - C#.wav", "Warm_Db.mp3", "pads F# minor.wav").
    static int keyFromFilename (const juce::String& name);

    // Audio thread.
    void trigger (int key, float attackSec, float releaseSec, bool autoTranspose);
    void releaseAll (float releaseSec);
    void panic();
    void setLevel (float l) { level = l; }
    void render (juce::AudioBuffer<float>& mix);

private:
    struct Playing
    {
        std::shared_ptr<juce::AudioBuffer<float>> buf;
        double pos = 0.0, rate = 1.0;
        float env = 0, envTarget = 0, envStep = 0;
        bool active = false, releasing = false;
    };

    std::array<Slot, 12> slots;
    std::array<Playing, 6> playing;
    juce::SpinLock lock;
    double sr = 48000.0;
    float level = 0.8f;
    juce::AudioFormatManager formats;
};
} // namespace canopy
