// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#include "SampleLayer.h"
#include "Styles.h"

namespace canopy
{
SampleLayer::SampleLayer()
{
    formats.registerBasicFormats();
}

void SampleLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    panic();
}

bool SampleLayer::loadSlot (int key, const juce::File& file, juce::String& error)
{
    if (key < 0 || key > 11) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "Could not read " + file.getFileName(); return false; }
    const juce::int64 len = reader->lengthInSamples;
    if (len <= 0 || len > (juce::int64) (reader->sampleRate * 600.0)) { error = "File is empty or longer than 10 minutes"; return false; }

    auto buf = std::make_shared<juce::AudioBuffer<float>> (2, (int) len);
    reader->read (buf.get(), 0, (int) len, 0, true, true);
    if (reader->numChannels == 1) buf->copyFrom (1, 0, *buf, 0, 0, (int) len);

    // Short fades at both ends so the loop point never clicks.
    const int fade = std::min ((int) len / 4, (int) (reader->sampleRate * 0.25));
    buf->applyGainRamp (0, fade, 0.0f, 1.0f);
    buf->applyGainRamp ((int) len - fade, fade, 1.0f, 0.0f);

    Slot s;
    s.name = file.getFileNameWithoutExtension();
    s.path = file.getFullPathName();
    s.buffer = buf;
    s.sampleRate = reader->sampleRate;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        slots[(size_t) key] = std::move (s);
    }
    return true;
}

void SampleLayer::clearSlot (int key)
{
    if (key < 0 || key > 11) return;
    const juce::SpinLock::ScopedLockType sl (lock);
    slots[(size_t) key] = Slot();
}

void SampleLayer::clearAll()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (auto& s : slots) s = Slot();
}

bool SampleLayer::anyLoaded() const
{
    for (auto& s : slots) if (s.buffer) return true;
    return false;
}

static int parseKeyToken (const juce::String& tok)
{
    if (tok.isEmpty()) return -1;
    const juce::juce_wchar c0 = juce::CharacterFunctions::toUpperCase (tok[0]);
    static const int bases[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    if (c0 < 'A' || c0 > 'G') return -1;
    int pc = bases[c0 - 'A'];
    int idx = 1;
    if (tok.length() > 1)
    {
        if (tok[1] == '#') { pc += 1; idx = 2; }
        else if (tok[1] == 'b') { pc -= 1; idx = 2; }
    }
    const juce::String rest = tok.substring (idx).toLowerCase();
    const bool ok = rest.isEmpty() || rest == "m" || rest.startsWith ("maj") || rest.startsWith ("min")
                 || (rest.length() == 1 && juce::CharacterFunctions::isDigit (rest[0]));
    if (! ok) return -1;
    return (pc + 12) % 12;
}

int SampleLayer::keyFromFilename (const juce::String& nameIn)
{
    const juce::String name = nameIn.replaceCharacters ("_-.()[],", "        ");
    auto tokens = juce::StringArray::fromTokens (name, " ", "");
    // Scan from the end: key names usually come last ("Warm Pad - Eb.wav").
    for (int i = tokens.size() - 1; i >= 0; --i)
    {
        const int k = parseKeyToken (tokens[i].trim());
        if (k >= 0) return k;
    }
    return -1;
}

void SampleLayer::trigger (int key, float attackSec, float releaseSec, bool autoTranspose)
{
    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (! tl.isLocked()) return;

    int src = -1;
    int semis = 0;
    if (slots[(size_t) key].buffer) src = key;
    else if (autoTranspose)
    {
        int bestDist = 99;
        for (int i = 0; i < 12; ++i)
        {
            if (! slots[(size_t) i].buffer) continue;
            int d = key - i;                      // prefer small transpositions, wrap within an octave
            while (d > 6) d -= 12;
            while (d < -6) d += 12;
            if (std::abs (d) < bestDist) { bestDist = std::abs (d); src = i; semis = d; }
        }
    }

    for (auto& p : playing)
        if (p.active && ! p.releasing) { p.releasing = true; p.envTarget = 0; p.envStep = -std::max (p.env, 0.05f) / (float) std::max (1.0, releaseSec * sr); }

    if (src < 0) return;
    for (auto& p : playing)
    {
        if (p.active) continue;
        p.buf = slots[(size_t) src].buffer;
        p.rate = (slots[(size_t) src].sampleRate / sr) * std::pow (2.0, semis / 12.0);
        p.pos = 0.0;
        p.env = 0; p.envTarget = 1; p.envStep = 1.0f / (float) std::max (1.0, attackSec * sr);
        p.releasing = false; p.active = true;
        return;
    }
}

void SampleLayer::releaseAll (float releaseSec)
{
    for (auto& p : playing)
        if (p.active && ! p.releasing) { p.releasing = true; p.envTarget = 0; p.envStep = -std::max (p.env, 0.05f) / (float) std::max (1.0, releaseSec * sr); }
}

void SampleLayer::panic()
{
    for (auto& p : playing) { p.active = false; p.buf.reset(); }
}

void SampleLayer::render (juce::AudioBuffer<float>& mix)
{
    const int n = mix.getNumSamples();
    float* L = mix.getWritePointer (0);
    float* R = mix.getWritePointer (1);
    for (auto& p : playing)
    {
        if (! p.active || ! p.buf) { if (p.active) p.active = false; continue; }
        const auto& b = *p.buf;
        const int len = b.getNumSamples();
        const float* sL = b.getReadPointer (0);
        const float* sR = b.getReadPointer (1);
        for (int k = 0; k < n; ++k)
        {
            p.env += p.envStep;
            if ((p.envStep > 0 && p.env >= p.envTarget) || (p.envStep < 0 && p.env <= p.envTarget))
            {
                p.env = p.envTarget; p.envStep = 0;
                if (p.releasing) { p.active = false; p.buf.reset(); break; }
            }
            const int i0 = (int) p.pos;
            const int i1 = (i0 + 1) % len;
            const float fr = (float) (p.pos - (double) i0);
            const float e = p.env * p.env * level;
            L[k] += (sL[i0] + (sL[i1] - sL[i0]) * fr) * e;
            R[k] += (sR[i0] + (sR[i1] - sR[i0]) * fr) * e;
            p.pos += p.rate;
            if (p.pos >= (double) len) p.pos -= (double) len;
        }
    }
}
} // namespace canopy
