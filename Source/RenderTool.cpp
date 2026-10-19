// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

// Headless test tool: renders drones to WAV without a host.
//   canopy_render <outDir> [seconds] [styleId] [key 0-11] [mode 0-3]
#include <JuceHeader.h>
#include "Exporter.h"

int main (int argc, char** argv)
{
    using namespace canopy;
    juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("render_out"));
    outDir.createDirectory();
    const double seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 12.0;
    const juce::String styleId = argc > 3 ? argv[3] : "warm";
    const int key = argc > 4 ? juce::String (argv[4]).getIntValue() : 0;
    const int mode = argc > 5 ? juce::String (argv[5]).getIntValue() : 1;

    EngineParams p;
    p.layerStyle[0] = styleIndexFor (styleId);
    p.layerStyle[1] = -1;
    p.layerLevel[0] = 0.9f;
    p.attack = 2.0f;
    p.release = 3.0f;

    const auto file = outDir.getChildFile (Exporter::suggestedFileName (p, key, (DroneMode) mode));
    const juce::int64 t0 = juce::Time::currentTimeMillis();
    auto err = Exporter::renderKeyToWav (p, key, (DroneMode) mode, seconds, 48000.0, file);
    const double dt = (double) (juce::Time::currentTimeMillis() - t0) / 1000.0;
    if (err.isNotEmpty()) { std::cerr << "ERROR: " << err << std::endl; return 1; }
    std::cout << "Wrote " << file.getFullPathName() << "  (" << seconds << "s audio rendered in " << dt << "s)" << std::endl;
    return 0;
}
