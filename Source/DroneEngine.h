// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include "Styles.h"
#include <array>
#include <functional>

namespace canopy
{
struct EngineParams
{
    float master     = 0.8f;   // 0..1
    float swell      = 0.7f;   // 0..1  (volume + brightness "build" control)
    float attack     = 3.0f;   // seconds
    float release    = 5.0f;   // seconds
    int   layerStyle[3]  = { 0, 1, -1 };     // index into styles(), -1 = off
    float layerLevel[3]  = { 0.8f, 0.35f, 0.5f };
    float sub        = 0.4f;
    float brightness = 0.5f;   // 0..1, 0.5 = neutral
    float motion     = 0.5f;   // 0..1, 0.5 = neutral
    float width      = 0.7f;   // stereo spread
    float eqLow      = 0.0f;   // dB
    float eqMid      = 0.0f;
    float eqHigh     = 0.0f;
    float reverbMix  = 0.35f;
    float reverbSize = 0.7f;
};

struct PitchSpec { int midi; float weight; };

struct PitchSet
{
    std::array<PitchSpec, 8> p {};
    int count = 0;
    void add (int midi, float w) { if (count < 8) p[(size_t) count++] = { midi, w }; }
};

PitchSet pitchesFor (int rootPc, DroneMode mode);
PitchSet pitchesForChord (const std::array<int, 16>& notes, int numNotes);

class Voice
{
public:
    static constexpr int maxUnits = 8;
    static constexpr int maxOscs  = 6;

    struct Unit
    {
        std::array<Osc, maxOscs> oscs {};
        int numOscs = 0;
        juce::dsp::StateVariableTPTFilter<float> filter;
        float freq = 0, baseCutoff = 0, weight = 0;
        float fLfoPhase = 0, aLfoPhase = 0;
        float panL = 0.7f, panR = 0.7f;
    };

    void prepare (double sampleRate);
    void start (const Style& s, const PitchSet& pitches, int layer, float attackSec, float width, juce::Random& rng);
    void release (float releaseSec);
    void render (float* L, float* R, int n, float brightnessMul, float motionMul);

    bool  isActive() const noexcept   { return active; }
    bool  isReleasing() const noexcept{ return releasing; }
    int   getLayer() const noexcept   { return layer; }
    const Style* getStyle() const noexcept { return style; }

private:
    const Style* style = nullptr;
    std::array<Unit, maxUnits> units;
    int numUnits = 0;
    int layer = 0;
    double sr = 48000.0;
    float env = 0, envTarget = 0, envStep = 0;
    bool active = false, releasing = false;
};

class DroneEngine
{
public:
    static constexpr int maxVoices = 40;

    DroneEngine();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParams (const EngineParams& p) { params = p; }
    const EngineParams& getParams() const  { return params; }

    // Start a new drone; existing voices crossfade out.
    void trigger (const PitchSet& pitches);
    void releaseAll();
    void releaseAll (float seconds);
    void panic();

    // Optional extra source (e.g. sample layer) mixed in before the bus FX.
    std::function<void (juce::AudioBuffer<float>&)> extraSource;

    void process (juce::AudioBuffer<float>& out);

    bool anyVoiceActive() const noexcept;
    int  numActiveVoices() const noexcept;
    float peakLevel() const noexcept { return lastPeak; }

private:
    void renderLayer (int layer, int n);
    void updateEq();

    EngineParams params;
    double sr = 48000.0;
    int blockSize = 512;
    juce::Random rng;

    std::array<Voice, maxVoices> voices;
    juce::AudioBuffer<float> layerBuf, mixBuf;

    struct LayerFx
    {
        juce::dsp::Chorus<float> chorus;
        juce::dsp::IIR::Filter<float> f1L, f1R, f2L, f2R;
        juce::SmoothedValue<float> level;
    };
    std::array<LayerFx, 4> layerFx;   // 3 style layers + sub

    juce::dsp::StateVariableTPTFilter<float> swellFilter;
    juce::SmoothedValue<float> swellGain, masterGain;
    juce::dsp::IIR::Filter<float> eqL[3], eqR[3];
    float eqCache[3] = { 1e9f, 1e9f, 1e9f };
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    float lastPeak = 0.0f;
};
} // namespace canopy
