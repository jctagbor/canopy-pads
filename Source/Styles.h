// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include "Wavetable.h"
#include <vector>

namespace canopy
{
struct OscDef
{
    Wave  wave;
    int   octave;        // relative octave shift
    float detuneCents;   // static detune
    float gain;
};

// A "style" is a complete pad voice recipe: oscillator stack, filter behaviour,
// slow modulation and bus effects. New styles are just new table rows.
struct Style
{
    const char* id;
    const char* name;
    const char* blurb;
    std::vector<OscDef> oscs;
    float cutoffMul;      // filter cutoff as multiple of the pitch frequency
    float resonance;      // 0.1 .. 1.5
    float filterLfoRate;  // Hz
    float filterLfoDepth; // fraction of cutoff
    float ampLfoRate;     // Hz
    float ampLfoDepth;    // 0..1
    bool  chorus;
    bool  formant;        // vowel-ish bandpass colouring on the layer bus
    float trim;           // output gain trim
};

inline const std::vector<Style>& styles()
{
    static const std::vector<Style> s = {
        { "warm", "Warm", "Soft, rounded analog warmth. The all-purpose bed.",
          { { Wave::Saw, 0, -8.f, 0.45f }, { Wave::Saw, 0, 8.f, 0.45f }, { Wave::Triangle, -1, 0.f, 0.55f }, { Wave::Saw, 1, 3.f, 0.12f } },
          3.2f, 0.45f, 0.06f, 0.35f, 0.045f, 0.18f, true, false, 0.55f },

        { "shimmer", "Shimmer", "Airy, high, glistening. Lifts a quiet moment.",
          { { Wave::Triangle, 1, -11.f, 0.4f }, { Wave::Triangle, 1, 11.f, 0.4f }, { Wave::Sine, 2, 4.f, 0.3f }, { Wave::Saw, 2, -5.f, 0.12f }, { Wave::Sine, 1, 702.f, 0.2f } },
          9.0f, 0.3f, 0.09f, 0.3f, 0.07f, 0.25f, true, false, 0.42f },

        { "dark", "Dark", "Low, brooding, weighty. For reflective and tension moments.",
          { { Wave::Sine, -1, 0.f, 0.7f }, { Wave::Square, -1, 5.f, 0.25f }, { Wave::Saw, 0, -6.f, 0.3f }, { Wave::Triangle, 0, 7.f, 0.3f } },
          1.6f, 0.6f, 0.04f, 0.4f, 0.035f, 0.2f, false, false, 0.6f },

        { "glass", "Glass", "Pure, bell-like and clean. Sits above a piano beautifully.",
          { { Wave::Sine, 0, 0.f, 0.6f }, { Wave::Sine, 1, 3.f, 0.45f }, { Wave::Sine, 2, -4.f, 0.3f }, { Wave::Triangle, 1, 1200.f, 0.12f } },
          12.0f, 0.2f, 0.05f, 0.15f, 0.11f, 0.3f, true, false, 0.5f },

        { "choir", "Choir", "Vowel-shaped, human, hymn-like.",
          { { Wave::Saw, 0, -10.f, 0.4f }, { Wave::Saw, 0, 10.f, 0.4f }, { Wave::Saw, 0, 0.f, 0.3f }, { Wave::Triangle, 1, 5.f, 0.2f } },
          4.0f, 0.5f, 0.07f, 0.25f, 0.05f, 0.15f, true, true, 0.5f },

        { "strings", "Strings", "Wide, bowed ensemble. Fills a room.",
          { { Wave::Saw, 0, -14.f, 0.35f }, { Wave::Saw, 0, 14.f, 0.35f }, { Wave::Saw, 0, -4.f, 0.3f }, { Wave::Saw, 1, 6.f, 0.18f }, { Wave::Saw, -1, 0.f, 0.3f } },
          5.0f, 0.4f, 0.08f, 0.3f, 0.06f, 0.12f, true, false, 0.45f },

        { "organ", "Organ", "Drawbar-style octaves. Church, but gentle.",
          { { Wave::Sine, 0, 0.f, 0.6f }, { Wave::Sine, 1, 0.f, 0.45f }, { Wave::Sine, 1, 702.f, 0.3f }, { Wave::Sine, 2, 0.f, 0.25f }, { Wave::Sine, -1, 0.f, 0.35f } },
          20.0f, 0.1f, 0.03f, 0.05f, 0.9f, 0.06f, false, false, 0.5f },

        { "ocean", "Ocean", "Slow, swelling, tidal movement. Very ambient.",
          { { Wave::Saw, 0, -18.f, 0.3f }, { Wave::Saw, 0, 18.f, 0.3f }, { Wave::Triangle, 0, 0.f, 0.4f }, { Wave::Triangle, 1, -9.f, 0.22f }, { Wave::Sine, -1, 0.f, 0.5f } },
          2.4f, 0.55f, 0.03f, 0.8f, 0.025f, 0.45f, true, false, 0.55f },
    };
    return s;
}

inline int styleIndexFor (const juce::String& id)
{
    const auto& s = styles();
    for (int i = 0; i < (int) s.size(); ++i)
        if (id == s[(size_t) i].id) return i;
    return 0;
}

// Special internal style used by the always-available sub layer.
inline const Style& subStyle()
{
    static const Style s { "sub", "Sub", "", { { Wave::Sine, 0, 0.f, 0.8f }, { Wave::Sine, -1, 0.f, 0.4f } },
                           30.0f, 0.1f, 0.02f, 0.0f, 0.03f, 0.08f, false, false, 0.7f };
    return s;
}

static const char* const kNoteNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
static const char* const kNoteNamesSharp[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

enum class DroneMode { Root = 0, Major, Minor, Chord };
static const char* const kModeNames[4] = { "Root", "Major", "Minor", "Chord" };
} // namespace canopy
