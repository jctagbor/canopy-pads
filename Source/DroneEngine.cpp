// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#include "DroneEngine.h"

namespace canopy
{
static inline float midiToFreq (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

PitchSet pitchesFor (int rootPc, DroneMode mode)
{
    PitchSet ps;
    const int base = 48 + (rootPc % 12);          // C3 = 48 (130.8 Hz)
    ps.add (base, 0.8f);
    ps.add (base + 7, 0.5f);
    ps.add (base + 12, 0.55f);
    ps.add (base + 19, 0.3f);
    ps.add (base + 24, 0.2f);
    if (mode == DroneMode::Major) { ps.add (base + 16, 0.3f); ps.add (base + 28, 0.1f); }
    if (mode == DroneMode::Minor) { ps.add (base + 15, 0.3f); ps.add (base + 27, 0.1f); }
    return ps;
}

PitchSet pitchesForChord (const std::array<int, 16>& notes, int numNotes)
{
    PitchSet ps;
    if (numNotes <= 0) return ps;
    int lowest = 127;
    for (int i = 0; i < numNotes; ++i) lowest = std::min (lowest, notes[(size_t) i]);
    const int rootPc = lowest % 12;
    const int rootMidi = 48 + rootPc;
    ps.add (rootMidi - 12, 0.6f);
    ps.add (rootMidi, 0.75f);
    for (int i = 0; i < numNotes && ps.count < 7; ++i)
    {
        int m = notes[(size_t) i];
        while (m < 52) m += 12;        // place chord tones from E3 upward
        while (m > 76) m -= 12;
        if (m % 12 == rootPc) continue;
        ps.add (m, 0.45f);
    }
    ps.add (rootMidi + 24, 0.2f);
    return ps;
}

// ---------------------------------------------------------------- Voice
void Voice::prepare (double sampleRate)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    for (auto& u : units)
    {
        u.filter.prepare (spec);
        u.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
    active = false;
}

void Voice::start (const Style& s, const PitchSet& pitches, int layerIndex, float attackSec, float width, juce::Random& r)
{
    style = &s;
    layer = layerIndex;
    numUnits = std::min (pitches.count, maxUnits);
    const auto& wt = Wavetables::get();

    for (int i = 0; i < numUnits; ++i)
    {
        auto& u = units[(size_t) i];
        u.freq   = midiToFreq ((float) pitches.p[(size_t) i].midi);
        u.weight = pitches.p[(size_t) i].weight;
        u.numOscs = std::min ((int) s.oscs.size(), maxOscs);
        for (int o = 0; o < u.numOscs; ++o)
        {
            const auto& d = s.oscs[(size_t) o];
            auto& osc = u.oscs[(size_t) o];
            osc.wave  = d.wave;
            const float jitter = (r.nextFloat() - 0.5f) * 3.0f;        // ±1.5 cents, organic
            const float f = u.freq * std::pow (2.0f, (float) d.octave + (d.detuneCents + jitter) / 1200.0f);
            osc.inc   = f / (float) sr;
            osc.phase = r.nextFloat();
            osc.gain  = d.gain;
            osc.mip   = wt.mipFor (f, (float) sr);
        }
        u.baseCutoff = u.freq * s.cutoffMul;
        u.filter.reset();
        u.filter.setResonance (juce::jlimit (0.1f, 1.5f, s.resonance));
        u.fLfoPhase = r.nextFloat();
        u.aLfoPhase = r.nextFloat();
        // Alternate units left/right, scaled by width.
        const float side = ((i % 2) ? 1.0f : -1.0f) * (0.15f + 0.85f * (float) ((i / 2) % 3) / 2.0f) * width;
        const float angle = (side + 1.0f) * 0.25f * juce::MathConstants<float>::pi;   // 0..pi/2
        u.panL = std::cos (angle);
        u.panR = std::sin (angle);
    }

    env = 0.0f;
    envTarget = 1.0f;
    envStep = 1.0f / (float) std::max (1.0, attackSec * sr);
    releasing = false;
    active = true;
}

void Voice::release (float releaseSec)
{
    if (! active) return;
    releasing = true;
    envTarget = 0.0f;
    envStep = -std::max (env, 0.05f) / (float) std::max (1.0, releaseSec * sr);
}

void Voice::render (float* L, float* R, int n, float brightnessMul, float motionMul)
{
    if (! active) return;
    const auto& wt = Wavetables::get();
    const Style& s = *style;
    const float blockSec = (float) n / (float) sr;
    const float nyq = 0.45f * (float) sr;

    float amp[maxUnits];
    for (int i = 0; i < numUnits; ++i)
    {
        auto& u = units[(size_t) i];
        u.fLfoPhase += s.filterLfoRate * blockSec; if (u.fLfoPhase >= 1.0f) u.fLfoPhase -= 1.0f;
        u.aLfoPhase += s.ampLfoRate   * blockSec; if (u.aLfoPhase >= 1.0f) u.aLfoPhase -= 1.0f;
        const float fl = std::sin (u.fLfoPhase * juce::MathConstants<float>::twoPi);
        const float al = 0.5f + 0.5f * std::sin (u.aLfoPhase * juce::MathConstants<float>::twoPi);
        float cutoff = u.baseCutoff * brightnessMul * (1.0f + s.filterLfoDepth * motionMul * fl);
        cutoff = juce::jlimit (40.0f, nyq, cutoff);
        u.filter.setCutoffFrequency (cutoff);
        amp[i] = u.weight * (1.0f - s.ampLfoDepth * motionMul * al) * s.trim;
    }

    for (int k = 0; k < n; ++k)
    {
        env += envStep;
        if ((envStep > 0 && env >= envTarget) || (envStep < 0 && env <= envTarget))
        {
            env = envTarget;
            envStep = 0.0f;
            if (releasing) { active = false; return; }
        }
        const float e = env * env;   // gentle curve: quieter crossfade midpoint, smoother ear feel
        float l = 0, rr = 0;
        for (int i = 0; i < numUnits; ++i)
        {
            auto& u = units[(size_t) i];
            float x = 0;
            for (int o = 0; o < u.numOscs; ++o) x += u.oscs[(size_t) o].next (wt);
            x = u.filter.processSample (0, x) * amp[i];
            l  += x * u.panL;
            rr += x * u.panR;
        }
        L[k] += l * e;
        R[k] += rr * e;
    }
}

// ---------------------------------------------------------------- Engine
DroneEngine::DroneEngine() {}

void DroneEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    blockSize = maxBlockSize;
    for (auto& v : voices) v.prepare (sr);
    layerBuf.setSize (2, maxBlockSize);
    mixBuf.setSize (2, maxBlockSize);

    juce::dsp::ProcessSpec spec2 { sr, (juce::uint32) maxBlockSize, 2 };
    juce::dsp::ProcessSpec spec1 { sr, (juce::uint32) maxBlockSize, 1 };
    for (auto& lf : layerFx)
    {
        lf.chorus.prepare (spec2);
        lf.chorus.setRate (0.25f);
        lf.chorus.setDepth (0.35f);
        lf.chorus.setCentreDelay (18.0f);
        lf.chorus.setFeedback (0.05f);
        lf.chorus.setMix (0.5f);
        auto c1 = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 640.0, 1.6f);
        auto c2 = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 1180.0, 1.6f);
        lf.f1L.coefficients = c1; lf.f1R.coefficients = c1;
        lf.f2L.coefficients = c2; lf.f2R.coefficients = c2;
        for (auto* f : { &lf.f1L, &lf.f1R, &lf.f2L, &lf.f2R }) f->prepare (spec1);
        lf.level.reset (sr, 0.05);
    }
    swellFilter.prepare (spec2);
    swellFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    swellFilter.setResonance (0.5f);
    swellGain.reset (sr, 0.08);
    masterGain.reset (sr, 0.05);
    for (int i = 0; i < 3; ++i) { eqL[i].prepare (spec1); eqR[i].prepare (spec1); }
    eqCache[0] = eqCache[1] = eqCache[2] = 1e9f;
    reverb.prepare (spec2);
    limiter.prepare (spec2);
    limiter.setThreshold (-1.0f);
    limiter.setRelease (120.0f);
    reset();
}

void DroneEngine::reset()
{
    for (auto& v : voices) v.prepare (sr);
    reverb.reset();
    limiter.reset();
    swellFilter.reset();
    for (int i = 0; i < 3; ++i) { eqL[i].reset(); eqR[i].reset(); }
}

void DroneEngine::releaseAll (float seconds)
{
    for (auto& v : voices) v.release (seconds);
}
void DroneEngine::releaseAll() { releaseAll (params.release); }

void DroneEngine::panic()
{
    for (auto& v : voices) v.prepare (sr);
    reverb.reset();
}

void DroneEngine::trigger (const PitchSet& pitches)
{
    // Crossfade: release everything currently sounding, then start fresh voices.
    for (auto& v : voices)
        if (v.isActive() && ! v.isReleasing())
            v.release (params.release);

    auto startOn = [&] (const Style& s, int layer, const PitchSet& ps)
    {
        for (auto& v : voices)
            if (! v.isActive()) { v.start (s, ps, layer, params.attack, params.width, rng); return; }
        // Pool exhausted: steal the quietest releasing voice.
        for (auto& v : voices)
            if (v.isReleasing()) { v.start (s, ps, layer, params.attack, params.width, rng); return; }
    };

    for (int l = 0; l < 3; ++l)
    {
        const int si = params.layerStyle[l];
        if (si < 0 || si >= (int) styles().size() || params.layerLevel[l] <= 0.001f) continue;
        startOn (styles()[(size_t) si], l, pitches);
    }
    if (params.sub > 0.001f && pitches.count > 0)
    {
        PitchSet subPs;
        int lowest = 127;
        for (int i = 0; i < pitches.count; ++i) lowest = std::min (lowest, pitches.p[(size_t) i].midi);
        subPs.add (lowest - 12 >= 24 ? lowest - 12 : lowest, 1.0f);
        startOn (subStyle(), 3, subPs);
    }
}

bool DroneEngine::anyVoiceActive() const noexcept
{
    for (auto& v : voices) if (v.isActive()) return true;
    return false;
}
int DroneEngine::numActiveVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices) if (v.isActive()) ++n;
    return n;
}

void DroneEngine::renderLayer (int layer, int n)
{
    bool any = false;
    for (auto& v : voices) if (v.isActive() && v.getLayer() == layer) { any = true; break; }
    auto& lf = layerFx[(size_t) layer];
    const float targetLevel = layer == 3 ? params.sub : params.layerLevel[layer];
    lf.level.setTargetValue (targetLevel);
    if (! any) { lf.level.skip (n); return; }

    layerBuf.clear();
    const float brightnessMul = std::pow (4.0f, (params.brightness - 0.5f) * 2.0f);
    const float motionMul = params.motion * 2.0f;
    float* L = layerBuf.getWritePointer (0);
    float* R = layerBuf.getWritePointer (1);
    const Style* styleOnBus = nullptr;
    for (auto& v : voices)
        if (v.isActive() && v.getLayer() == layer)
        {
            v.render (L, R, n, brightnessMul, motionMul);
            if (! v.isReleasing()) styleOnBus = v.getStyle();
        }
    if (styleOnBus == nullptr)
        styleOnBus = layer == 3 ? &subStyle()
                   : (params.layerStyle[layer] >= 0 ? &styles()[(size_t) params.layerStyle[layer]] : nullptr);

    if (styleOnBus != nullptr)
    {
        if (styleOnBus->formant)
        {
            for (int k = 0; k < n; ++k)
            {
                const float l = L[k], r = R[k];
                L[k] = 0.45f * l + 0.9f * lf.f1L.processSample (l) + 0.6f * lf.f2L.processSample (l);
                R[k] = 0.45f * r + 0.9f * lf.f1R.processSample (r) + 0.6f * lf.f2R.processSample (r);
            }
        }
        if (styleOnBus->chorus)
        {
            juce::dsp::AudioBlock<float> blk (layerBuf.getArrayOfWritePointers(), 2, 0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            lf.chorus.process (ctx);
        }
    }
    for (int k = 0; k < n; ++k)
    {
        const float g = lf.level.getNextValue();
        mixBuf.getWritePointer (0)[k] += L[k] * g;
        mixBuf.getWritePointer (1)[k] += R[k] * g;
    }
}

void DroneEngine::updateEq()
{
    const float v[3] = { params.eqLow, params.eqMid, params.eqHigh };
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs (v[i] - eqCache[i]) < 0.01f) continue;
        eqCache[i] = v[i];
        const float g = juce::Decibels::decibelsToGain (v[i]);
        juce::dsp::IIR::Coefficients<float>::Ptr c;
        if (i == 0)      c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 180.0, 0.7f, g);
        else if (i == 1) c = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 900.0, 0.8f, g);
        else             c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4500.0, 0.7f, g);
        eqL[i].coefficients = c;
        eqR[i].coefficients = c;
    }
}

void DroneEngine::process (juce::AudioBuffer<float>& out)
{
    const int n = out.getNumSamples();
    if (n > mixBuf.getNumSamples()) { mixBuf.setSize (2, n, false, false, true); layerBuf.setSize (2, n, false, false, true); }
    mixBuf.clear (0, n);

    for (int layer = 0; layer < 4; ++layer) renderLayer (layer, n);
    if (extraSource) extraSource (mixBuf);

    // Swell: brightness + level build.
    const float sw = juce::jlimit (0.0f, 1.0f, params.swell);
    swellFilter.setCutoffFrequency (juce::jlimit (300.0f, 0.45f * (float) sr, 500.0f + std::pow (sw, 1.6f) * 17000.0f));
    swellGain.setTargetValue (0.3f + 0.7f * sw);
    masterGain.setTargetValue (params.master * params.master);
    updateEq();

    float* L = mixBuf.getWritePointer (0);
    float* R = mixBuf.getWritePointer (1);
    for (int k = 0; k < n; ++k)
    {
        float l = swellFilter.processSample (0, L[k]);
        float r = swellFilter.processSample (1, R[k]);
        const float g = swellGain.getNextValue();
        l *= g; r *= g;
        for (int i = 0; i < 3; ++i) { l = eqL[i].processSample (l); r = eqR[i].processSample (r); }
        L[k] = l; R[k] = r;
    }

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.0f, 1.0f, params.reverbSize);
    rp.damping  = 0.45f;
    rp.wetLevel = params.reverbMix * 0.9f;
    rp.dryLevel = 1.0f - params.reverbMix * 0.5f;
    rp.width    = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    juce::dsp::AudioBlock<float> blk (mixBuf.getArrayOfWritePointers(), 2, 0, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (blk);
    reverb.process (ctx);

    for (int k = 0; k < n; ++k) { const float g = masterGain.getNextValue(); L[k] *= g; R[k] *= g; }
    limiter.process (ctx);

    float pk = 0.0f;
    for (int k = 0; k < n; ++k) pk = std::max (pk, std::max (std::abs (L[k]), std::abs (R[k])));
    lastPeak = pk;

    const int ch = out.getNumChannels();
    if (ch >= 2)
    {
        out.copyFrom (0, 0, mixBuf, 0, 0, n);
        out.copyFrom (1, 0, mixBuf, 1, 0, n);
        for (int c = 2; c < ch; ++c) out.clear (c, 0, n);
    }
    else if (ch == 1)
    {
        out.copyFrom (0, 0, mixBuf, 0, 0, n);
        out.addFrom (0, 0, mixBuf, 1, 0, n);
        out.applyGain (0.5f);
    }
}
} // namespace canopy
