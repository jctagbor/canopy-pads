// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

// Headless tests: drives CanopyProcessor::processBlock directly with MIDI, no host, no audio device.
#include <JuceHeader.h>
#include "PluginProcessor.h"

using namespace canopy;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << std::endl; else { std::cout << "  FAIL " << msg << std::endl; ++failures; } } while (0)

struct Host
{
    CanopyProcessor proc;
    juce::AudioBuffer<float> buf { 2, 512 };
    juce::MidiBuffer midi;
    double sr = 48000.0;
    Host() { proc.prepareToPlay (sr, 512); setParam (ID::revmix, 0.0f); }   // no reverb tail: tests assert true silence
    float run (int blocks)   // returns peak over the run, pumping the message thread between blocks
    {
        float pk = 0;
        for (int i = 0; i < blocks; ++i)
        {
            proc.processBlock (buf, midi);
            midi.clear();
            pk = std::max (pk, buf.getMagnitude (0, 512));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        return pk;
    }
    float runSeconds (double s) { return run ((int) (s * sr / 512)); }
    void noteOn (int n) { midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0); }
    void noteOff (int n) { midi.addEvent (juce::MidiMessage::noteOff (1, n), 0); }
    void cc (int c, int v) { midi.addEvent (juce::MidiMessage::controllerEvent (1, c, v), 0); }
    void setParam (const juce::String& id, float plain) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (plain)); }
    float param (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << "== Latch mode: note in zone starts a drone, stays after release" << std::endl;
    {
        Host h;
        h.setParam (ID::attack, 0.5f); h.setParam (ID::release, 0.5f);
        CHECK (h.run (4) < 1e-6f, "silent before any note");
        h.noteOn (28);                                   // E1, inside default zone C1..B1
        float pk = h.runSeconds (1.0);
        CHECK (h.proc.currentKey.load() == 4, "current key is E");
        CHECK (pk > 0.05f, "audio is playing (peak " + juce::String (pk) + ")");
        h.noteOff (28);
        pk = h.runSeconds (1.0);
        CHECK (h.proc.currentKey.load() == 4 && pk > 0.05f, "still sounding after note-off (latched)");
        h.noteOn (31);                                   // G1 -> crossfade to G
        h.runSeconds (1.0);
        CHECK (h.proc.currentKey.load() == 7, "switched to G");
        h.noteOn (60);                                   // middle C, outside zone: ignored
        h.run (2);
        CHECK (h.proc.currentKey.load() == 7, "note outside drone zone ignored");
        h.proc.uiStop();
        h.runSeconds (1.5);
        CHECK (h.proc.currentKey.load() == -1 && h.run (2) < 1e-4f, "stop fades to silence");
        h.setParam (ID::revmix, 0.5f);
        h.proc.uiSetKey (0); h.runSeconds (1.0);
        h.proc.uiStop(); h.runSeconds (0.6);
        const float tail = h.run (2);
        h.runSeconds (6.0);
        CHECK (tail > 1e-4f && h.run (2) < tail * 0.05f, "reverb tail rings after stop, then decays (tail " + juce::String (tail) + ")");
    }
    std::cout << "== Latch off: drone follows held notes, sustain pedal holds it" << std::endl;
    {
        Host h;
        h.setParam (ID::attack, 0.3f); h.setParam (ID::release, 0.3f);
        h.setParam (ID::latch, 0.0f);
        h.noteOn (24); h.runSeconds (0.6);
        CHECK (h.proc.currentKey.load() == 0, "C sounding while held");
        h.noteOff (24); h.runSeconds (0.8);
        CHECK (h.proc.currentKey.load() == -1 && h.run (2) < 1e-4f, "released when note released");
        h.cc (64, 127); h.noteOn (26); h.run (2); h.noteOff (26); h.runSeconds (0.6);
        CHECK (h.proc.currentKey.load() == 2 && h.run (2) > 0.01f, "sustain pedal keeps D sounding after note-off");
        h.cc (64, 0); h.runSeconds (0.8);
        CHECK (h.proc.currentKey.load() == -1, "pedal up releases the drone");
    }
    std::cout << "== Chord mode" << std::endl;
    {
        Host h;
        h.setParam (ID::attack, 0.3f); h.setParam (ID::release, 0.3f);
        h.setParam (ID::mode, 3.0f);
        h.noteOn (24); h.noteOn (28); h.noteOn (31);     // C E G together
        h.runSeconds (0.5);
        CHECK (h.proc.currentKey.load() == 0, "chord root detected as C");
        const int voicesBefore = h.proc.activeVoices.load();
        CHECK (voicesBefore > 0, "chord drone sounding with " + juce::String (voicesBefore) + " voices");
        h.noteOn (33);                                   // add A -> retrigger once
        h.runSeconds (0.5);
        CHECK (h.proc.currentKey.load() == 0, "root unchanged when adding a tone above");
    }
    std::cout << "== MIDI learn + footswitch actions + setlist" << std::endl;
    {
        Host h;
        h.setParam (ID::attack, 0.2f); h.setParam (ID::release, 0.2f);
        h.proc.uiSetKey (5); h.run (2);
        h.proc.addItemFromCurrent ("Worship F");
        h.proc.uiSetKey (9); h.setParam (ID::mode, 2.0f); h.run (2);
        h.proc.addItemFromCurrent ("Prayer A minor");
        CHECK (h.proc.getSetlist().getNumChildren() == 2, "two setlist items");
        h.proc.startLearn (MidiAction::Next);
        h.cc (65, 127); h.run (2);
        CHECK (h.proc.describeMapping (MidiAction::Next) == "CC 65", "learned Next = CC 65 (" + h.proc.describeMapping (MidiAction::Next) + ")");
        h.proc.goToItem (0); h.run (3);
        CHECK (h.proc.currentKey.load() == 5 && (int) h.param (ID::mode) == 1, "go to item 1 -> F major");
        h.cc (65, 127); h.run (3);
        CHECK (h.proc.currentKey.load() == 9 && (int) h.param (ID::mode) == 2, "footswitch CC 65 -> item 2 (A minor)");
        h.proc.startLearn (MidiAction::Swell);
        h.cc (11, 100); h.run (2);
        h.cc (11, 64); h.run (2);
        CHECK (std::abs (h.param (ID::swell) - 64.0f / 127.0f) < 0.01f, "expression pedal drives swell");
        h.proc.startZoneLearn();
        h.noteOn (36); h.run (2); h.noteOn (47); h.run (2);
        CHECK ((int) h.param (ID::zonelow) == 36 && (int) h.param (ID::zonehigh) == 47, "zone learned C2..B2");
        h.noteOn (24); h.run (2);
        CHECK (h.proc.currentKey.load() == 9, "note below the new zone is ignored");

        // State round trip
        juce::MemoryBlock state;
        h.proc.getStateInformation (state);
        Host h2;
        h2.proc.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (h2.proc.getSetlist().getNumChildren() == 2 && h2.proc.describeMapping (MidiAction::Next) == "CC 65"
               && (int) h2.param (ID::zonelow) == 36, "setlist, mapping and zone survive save/restore");
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("canopy_test_setlist.json");
        CHECK (h.proc.saveSetlist (f), "setlist saved to JSON");
        juce::String err;
        Host h3;
        CHECK (h3.proc.loadSetlist (f, err) && h3.proc.getSetlist().getNumChildren() == 2
               && h3.proc.getSetlist().getChild (1)["name"].toString() == "Prayer A minor", "setlist JSON reloads");
    }
    std::cout << "== Sample file key detection" << std::endl;
    {
        struct T { const char* n; int k; };
        const T tests[] = { { "Pad - Eb.wav", 3 }, { "Warm_F#.mp3", 6 }, { "pads C.wav", 0 }, { "Shimmer Bb maj", 10 }, { "Ebm pad", 3 },
                            { "Ambient Bright Pad", -1 }, { "Dark_Ab_120bpm", 8 }, { "A", 9 }, { "G#m", 8 }, { "Pad B", 11 } };
        for (auto& t : tests)
        {
            const int k = SampleLayer::keyFromFilename (t.n);
            CHECK (k == t.k, juce::String (t.n) + " -> " + (k >= 0 ? kNoteNames[k] : "none"));
        }
    }
    std::cout << "== Sample layer plays and transposes" << std::endl;
    {
        Host h;
        h.setParam (ID::attack, 0.2f); h.setParam (ID::release, 0.2f);
        h.setParam (ID::l1level, 0.0f); h.setParam (ID::l2level, 0.0f); h.setParam (ID::sub, 0.0f);
        // Make a 1-second 220 Hz test tone file as a "pad" for A.
        auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("canopy_test_pad_A.wav");
        {
            juce::AudioBuffer<float> tone (1, 48000);
            for (int i = 0; i < 48000; ++i) tone.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 48000.0f));
            wavFile.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wavFile), 48000.0, 1, 16, {}, 0));
            w->writeFromAudioSampleBuffer (tone, 0, 48000);
        }
        juce::String err;
        CHECK (h.proc.loadSampleSlot (9, wavFile, err), "loaded test pad into A slot");
        h.proc.uiSetKey (9);
        float pk = h.runSeconds (1.0);
        CHECK (pk > 0.05f, "sample pad plays for A (peak " + juce::String (pk) + ")");
        h.proc.uiSetKey (0);
        pk = h.runSeconds (1.0);
        CHECK (pk > 0.05f, "auto-transposed to C from the A file");
        h.setParam (ID::autotranspose, 0.0f);
        h.proc.uiSetKey (2); h.runSeconds (1.0);
        CHECK (h.run (2) < 1e-4f, "with auto-transpose off, a key with no file is silent");
    }
    std::cout << (failures == 0 ? "\nALL TESTS PASSED" : "\n" + juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
