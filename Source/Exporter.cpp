// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#include "Exporter.h"

namespace canopy
{
juce::String Exporter::suggestedFileName (const EngineParams& params, int rootPc, DroneMode mode)
{
    juce::String styleName = "Pad";
    if (params.layerStyle[0] >= 0 && params.layerStyle[0] < (int) styles().size())
        styleName = styles()[(size_t) params.layerStyle[0]].name;
    juce::String modeTag = mode == DroneMode::Major ? " maj" : mode == DroneMode::Minor ? " min" : "";
    return "Canopy " + styleName + " - " + juce::String (kNoteNames[rootPc]) + modeTag + ".wav";
}

juce::String Exporter::renderKeyToWav (const EngineParams& paramsIn, int rootPc, DroneMode mode,
                                       double seconds, double sampleRate, const juce::File& outFile,
                                       std::function<bool (float)> progress)
{
    EngineParams params = paramsIn;
    params.attack  = std::min (params.attack, (float) (seconds * 0.25));
    params.release = std::min (params.release, (float) (seconds * 0.25));

    const int block = 512;
    const int total = (int) (seconds * sampleRate);
    DroneEngine engine;
    engine.prepare (sampleRate, block);
    engine.setParams (params);
    engine.trigger (pitchesFor (rootPc, mode == DroneMode::Chord ? DroneMode::Root : mode));

    juce::AudioBuffer<float> out (2, total);
    out.clear();
    juce::AudioBuffer<float> tmp (2, block);
    const int releaseAt = total - (int) ((params.release + 1.5) * sampleRate);
    bool released = false;

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        if (! released && pos >= releaseAt) { engine.releaseAll(); released = true; }
        tmp.setSize (2, n, false, false, true);
        tmp.clear();
        engine.process (tmp);
        out.copyFrom (0, pos, tmp, 0, 0, n);
        out.copyFrom (1, pos, tmp, 1, 0, n);
        if (progress && (pos / block) % 20 == 0)
            if (! progress ((float) pos / (float) total)) return "Cancelled";
    }

    // Normalise to -1 dBFS so exported pads sit at a consistent level.
    const float peak = out.getMagnitude (0, total);
    if (peak > 0.0f) out.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

    outFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
    if (stream == nullptr) return "Could not write " + outFile.getFullPathName();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
    if (writer == nullptr) return "Could not create WAV writer";
    stream.release();   // writer owns the stream now
    if (! writer->writeFromAudioSampleBuffer (out, 0, total)) return "Write failed";
    return {};
}
} // namespace canopy
