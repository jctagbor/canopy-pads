// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include "DroneEngine.h"

namespace canopy
{
// Renders a drone offline (no host, no realtime) and writes it as a 24-bit WAV.
// This is what lets you take the sound into MainStage/Ableton/Logic as plain
// audio pads too, or hand a USB stick of pads to another musician.
struct Exporter
{
    // Returns an error string, or empty on success.
    static juce::String renderKeyToWav (const EngineParams& params, int rootPc, DroneMode mode,
                                        double seconds, double sampleRate, const juce::File& outFile,
                                        std::function<bool (float progress)> progress = nullptr);

    static juce::String suggestedFileName (const EngineParams& params, int rootPc, DroneMode mode);
};
} // namespace canopy
