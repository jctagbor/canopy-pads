// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

// Band-limited wavetables built additively, with 5 "mip" levels so that high
// pitches use fewer harmonics and never alias. Pads live mostly below 1 kHz,
// so this is plenty of fidelity at a tiny CPU cost.
namespace canopy
{
enum class Wave { Sine = 0, Saw, Square, Triangle, NumWaves };

class Wavetables
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numMips   = 5;      // 128, 64, 32, 16, 8 harmonics

    static const Wavetables& get()
    {
        static Wavetables instance;
        return instance;
    }

    // Choose a mip level so that freq * maxHarmonic stays below ~0.45 * sampleRate.
    int mipFor (float freq, float sampleRate) const noexcept
    {
        const float limit = 0.45f * sampleRate;
        for (int m = 0; m < numMips; ++m)
            if (freq * (float) (128 >> m) < limit)
                return m;
        return numMips - 1;
    }

    inline float read (Wave w, int mip, float phase) const noexcept
    {
        const auto& t = tables[(size_t) w][(size_t) mip];
        const float p  = phase * (float) tableSize;
        const int   i0 = (int) p;
        const float fr = p - (float) i0;
        const float a  = t[(size_t) (i0 & (tableSize - 1))];
        const float b  = t[(size_t) ((i0 + 1) & (tableSize - 1))];
        return a + (b - a) * fr;
    }

private:
    Wavetables()
    {
        for (int w = 0; w < (int) Wave::NumWaves; ++w)
            for (int m = 0; m < numMips; ++m)
                build ((Wave) w, m);
    }

    void build (Wave w, int mip)
    {
        auto& t = tables[(size_t) w][(size_t) mip];
        t.fill (0.0f);
        const int maxH = (w == Wave::Sine) ? 1 : (128 >> mip);
        float peak = 0.0f;
        for (int i = 0; i < tableSize; ++i)
        {
            const double x = juce::MathConstants<double>::twoPi * (double) i / (double) tableSize;
            double v = 0.0;
            for (int h = 1; h <= maxH; ++h)
            {
                switch (w)
                {
                    case Wave::Sine:     v += std::sin (x); break;
                    case Wave::Saw:      v += std::sin (h * x) / h; break;
                    case Wave::Square:   if (h & 1) v += std::sin (h * x) / h; break;
                    case Wave::Triangle: if (h & 1) v += (((h / 2) & 1) ? -1.0 : 1.0) * std::sin (h * x) / (double) (h * h); break;
                    default: break;
                }
            }
            t[(size_t) i] = (float) v;
            peak = std::max (peak, std::abs ((float) v));
        }
        if (peak > 0.0f)
            for (auto& s : t) s /= peak;
    }

    std::array<std::array<std::array<float, tableSize>, numMips>, (size_t) Wave::NumWaves> tables;
};

struct Osc
{
    Wave  wave  = Wave::Saw;
    float phase = 0.0f;
    float inc   = 0.0f;
    float gain  = 1.0f;
    int   mip   = 0;

    inline float next (const Wavetables& wt) noexcept
    {
        const float v = wt.read (wave, mip, phase);
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return v * gain;
    }
};
} // namespace canopy
