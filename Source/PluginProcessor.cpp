// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace canopy
{
static juce::StringArray styleChoices()
{
    juce::StringArray a { "Off" };
    for (auto& s : styles()) a.add (s.name);
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout CanopyProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using I = juce::AudioParameterInt;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    auto pid = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    const auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
    const auto sec = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " s"; });
    const auto db  = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });

    L.add (std::make_unique<F> (pid (ID::master), "Master", R (0.f, 1.f), 0.8f, pct));
    L.add (std::make_unique<F> (pid (ID::swell), "Swell", R (0.f, 1.f), 0.7f, pct));
    L.add (std::make_unique<F> (pid (ID::attack), "Attack", R (0.1f, 20.f, 0.01f, 0.5f), 3.0f, sec));
    L.add (std::make_unique<F> (pid (ID::release), "Release", R (0.1f, 30.f, 0.01f, 0.5f), 5.0f, sec));
    L.add (std::make_unique<B> (pid (ID::latch), "Latch", true));
    L.add (std::make_unique<C> (pid (ID::mode), "Mode", juce::StringArray { "Root", "Major", "Minor", "Chord" }, 1));
    auto sc = styleChoices();
    L.add (std::make_unique<C> (pid (ID::l1style), "Layer 1 Style", sc, 1));
    L.add (std::make_unique<F> (pid (ID::l1level), "Layer 1 Level", R (0.f, 1.f), 0.8f, pct));
    L.add (std::make_unique<C> (pid (ID::l2style), "Layer 2 Style", sc, 2));
    L.add (std::make_unique<F> (pid (ID::l2level), "Layer 2 Level", R (0.f, 1.f), 0.35f, pct));
    L.add (std::make_unique<C> (pid (ID::l3style), "Layer 3 Style", sc, 0));
    L.add (std::make_unique<F> (pid (ID::l3level), "Layer 3 Level", R (0.f, 1.f), 0.5f, pct));
    L.add (std::make_unique<F> (pid (ID::sub), "Sub", R (0.f, 1.f), 0.4f, pct));
    L.add (std::make_unique<F> (pid (ID::brightness), "Brightness", R (0.f, 1.f), 0.5f, pct));
    L.add (std::make_unique<F> (pid (ID::motion), "Motion", R (0.f, 1.f), 0.5f, pct));
    L.add (std::make_unique<F> (pid (ID::width), "Width", R (0.f, 1.f), 0.7f, pct));
    L.add (std::make_unique<F> (pid (ID::eqlow), "EQ Low", R (-12.f, 12.f), 0.f, db));
    L.add (std::make_unique<F> (pid (ID::eqmid), "EQ Mid", R (-12.f, 12.f), 0.f, db));
    L.add (std::make_unique<F> (pid (ID::eqhigh), "EQ High", R (-12.f, 12.f), 0.f, db));
    L.add (std::make_unique<F> (pid (ID::revmix), "Reverb Mix", R (0.f, 1.f), 0.35f, pct));
    L.add (std::make_unique<F> (pid (ID::revsize), "Reverb Size", R (0.f, 1.f), 0.7f, pct));
    L.add (std::make_unique<F> (pid (ID::samplelevel), "Sample Level", R (0.f, 1.f), 0.8f, pct));
    L.add (std::make_unique<B> (pid (ID::autotranspose), "Auto-transpose samples", true));
    L.add (std::make_unique<I> (pid (ID::zonelow), "Drone Zone Low", 0, 127, 24));
    L.add (std::make_unique<I> (pid (ID::zonehigh), "Drone Zone High", 0, 127, 35));
    L.add (std::make_unique<B> (pid (ID::sustainlatch), "Sustain pedal holds drone", true));
    return L;
}

CanopyProcessor::CanopyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CanopyPads", createLayout())
{
    getSetlist();
    apvts.state.getOrCreateChildWithName ("midimap", nullptr);
    apvts.state.getOrCreateChildWithName ("samples", nullptr);
}

CanopyProcessor::~CanopyProcessor() {}

bool CanopyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CanopyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    sampleLayer.prepare (sampleRate, samplesPerBlock);
    engine.extraSource = [this] (juce::AudioBuffer<float>& mix) { sampleLayer.render (mix); };
    numHeld = 0;
    chordDirty = false;
    sustainDown = false;
}

EngineParams CanopyProcessor::currentEngineParams() const
{
    EngineParams p;
    p.master = paramValue (ID::master);
    p.swell = paramValue (ID::swell);
    p.attack = paramValue (ID::attack);
    p.release = paramValue (ID::release);
    p.layerStyle[0] = (int) paramValue (ID::l1style) - 1;
    p.layerStyle[1] = (int) paramValue (ID::l2style) - 1;
    p.layerStyle[2] = (int) paramValue (ID::l3style) - 1;
    p.layerLevel[0] = paramValue (ID::l1level);
    p.layerLevel[1] = paramValue (ID::l2level);
    p.layerLevel[2] = paramValue (ID::l3level);
    p.sub = paramValue (ID::sub);
    p.brightness = paramValue (ID::brightness);
    p.motion = paramValue (ID::motion);
    p.width = paramValue (ID::width);
    p.eqLow = paramValue (ID::eqlow);
    p.eqMid = paramValue (ID::eqmid);
    p.eqHigh = paramValue (ID::eqhigh);
    p.reverbMix = paramValue (ID::revmix);
    p.reverbSize = paramValue (ID::revsize);
    return p;
}

// ---------------------------------------------------------------- commands
void CanopyProcessor::pushCommand (Command c)
{
    int s1, n1, s2, n2;
    cmdFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { cmdBuffer[s1] = c; cmdFifo.finishedWrite (1); }
}
void CanopyProcessor::uiSetKey (int pc)       { pushCommand ({ CmdSetKey, pc }); }
void CanopyProcessor::uiStop()                { pushCommand ({ CmdStop, 0 }); }
void CanopyProcessor::uiPanic()               { pushCommand ({ CmdPanic, 0 }); }
void CanopyProcessor::uiSetMode (DroneMode m) { setParamPlain (ID::mode, (float) m); }
void CanopyProcessor::uiToggleLatch()         { setParamPlain (ID::latch, paramValue (ID::latch) > 0.5f ? 0.f : 1.f); }
void CanopyProcessor::uiCycleMode()           { setParamPlain (ID::mode, (float) (((int) paramValue (ID::mode) + 1) % 4)); }

void CanopyProcessor::setParamNormalised (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, v));
}
void CanopyProcessor::setParamPlain (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

// ---------------------------------------------------------------- audio thread
void CanopyProcessor::setDrone (int pc, DroneMode mode)
{
    pc = ((pc % 12) + 12) % 12;
    currentKey = pc;
    engine.trigger (pitchesFor (pc, mode == DroneMode::Chord ? DroneMode::Root : mode));
    const auto& p = engine.getParams();
    sampleLayer.trigger (pc, p.attack, p.release, paramValue (ID::autotranspose) > 0.5f);
}

void CanopyProcessor::triggerChord()
{
    if (numHeld <= 0) return;
    int lowest = 127;
    for (int i = 0; i < numHeld; ++i) lowest = std::min (lowest, heldNotes[(size_t) i]);
    currentKey = lowest % 12;
    engine.trigger (pitchesForChord (heldNotes, numHeld));
    const auto& p = engine.getParams();
    sampleLayer.trigger (lowest % 12, p.attack, p.release, paramValue (ID::autotranspose) > 0.5f);
}

void CanopyProcessor::stopDrone()
{
    engine.releaseAll();
    sampleLayer.releaseAll (engine.getParams().release);
    currentKey = -1;
}

void CanopyProcessor::handleMidi (const juce::MidiMessage& m, int)
{
    const int ch = m.getChannel();
    const bool latch = paramValue (ID::latch) > 0.5f;
    const auto mode = (DroneMode) (int) paramValue (ID::mode);
    const int zLo = (int) paramValue (ID::zonelow), zHi = (int) paramValue (ID::zonehigh);

    auto perform = [this] (MidiAction a, int value)
    {
        switch (a)
        {
            case MidiAction::Next:        reqNext = 1; triggerAsyncUpdate(); break;
            case MidiAction::Prev:        reqPrev = 1; triggerAsyncUpdate(); break;
            case MidiAction::Stop:        stopDrone(); break;
            case MidiAction::Panic:       engine.panic(); sampleLayer.panic(); currentKey = -1; break;
            case MidiAction::LatchToggle: reqLatchToggle = 1; triggerAsyncUpdate(); break;
            case MidiAction::ModeCycle:   reqModeCycle = 1; triggerAsyncUpdate(); break;
            case MidiAction::Swell:       reqSwell = (float) value / 127.0f; triggerAsyncUpdate(); break;
            case MidiAction::Volume:      reqVolume = (float) value / 127.0f; triggerAsyncUpdate(); break;
            default: break;
        }
    };

    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        const int lt = learnTarget.load();
        if (lt >= 0)
        {
            auto& mp = mappings[lt];
            mp.type = 1; mp.number = note; mp.channel = 0;
            learnTarget = -1;
            reqLearnedAction = lt;
            triggerAsyncUpdate();
            return;
        }
        const int zl = zoneLearnStep.load();
        if (zl == 1) { reqZoneLow = note; zoneLearnStep = 2; triggerAsyncUpdate(); return; }
        if (zl == 2) { reqZoneHigh = note; zoneLearnStep = 0; triggerAsyncUpdate(); return; }

        for (int a = 0; a < (int) MidiAction::Count; ++a)
        {
            auto& mp = mappings[a];
            if (mp.type == 1 && mp.number == note && (mp.channel == 0 || mp.channel == ch))
            {
                if (a < (int) MidiAction::Swell) perform ((MidiAction) a, 127);
                return;
            }
        }

        if (note >= zLo && note <= zHi)
        {
            bool present = false;
            for (int i = 0; i < numHeld; ++i) if (heldNotes[(size_t) i] == note) present = true;
            if (! present && numHeld < 16) heldNotes[(size_t) numHeld++] = note;
            releasePendingOnSustain = false;
            if (mode == DroneMode::Chord) chordDirty = true;
            else setDrone (note % 12, mode);
        }
        return;
    }

    if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[(size_t) i] == note) { heldNotes[(size_t) i] = heldNotes[(size_t) numHeld - 1]; --numHeld; break; }
        for (int a = 0; a < (int) MidiAction::Count; ++a)
            if (mappings[a].type == 1 && mappings[a].number == note) return;
        if (note >= zLo && note <= zHi && ! latch)
        {
            if (sustainDown && paramValue (ID::sustainlatch) > 0.5f) { releasePendingOnSustain = true; return; }
            if (mode == DroneMode::Chord) chordDirty = true;
            else if (numHeld == 0) stopDrone();
        }
        return;
    }

    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int val = m.getControllerValue();
        const int lt = learnTarget.load();
        if (lt >= 0 && cc != 64)
        {
            auto& mp = mappings[lt];
            mp.type = 2; mp.number = cc; mp.channel = 0;
            learnTarget = -1;
            reqLearnedAction = lt;
            triggerAsyncUpdate();
            return;
        }
        bool consumed = false;
        for (int a = 0; a < (int) MidiAction::Count; ++a)
        {
            auto& mp = mappings[a];
            if (mp.type == 2 && mp.number == cc && (mp.channel == 0 || mp.channel == ch))
            {
                if (a >= (int) MidiAction::Swell) perform ((MidiAction) a, val);
                else if (val >= 64) perform ((MidiAction) a, val);
                consumed = true;
            }
        }
        if (consumed) return;

        if (cc == 64)
        {
            sustainDown = val >= 64;
            sustainHeld = sustainDown;
            if (! sustainDown && releasePendingOnSustain)
            {
                releasePendingOnSustain = false;
                if (! latch && numHeld == 0)
                {
                    if (mode == DroneMode::Chord) chordDirty = true;
                    else stopDrone();
                }
            }
        }
        else if (cc == 120 || cc == 123)
        {
            stopDrone();
        }
    }
}

void CanopyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    engine.setParams (currentEngineParams());
    sampleLayer.setLevel (paramValue (ID::samplelevel));

    // UI commands
    {
        int s1, n1, s2, n2;
        cmdFifo.prepareToRead (64, s1, n1, s2, n2);
        auto handle = [this] (const Command& c)
        {
            switch (c.type)
            {
                case CmdSetKey: numHeld = 0; chordDirty = false; setDrone (c.a, (DroneMode) (int) paramValue (ID::mode)); break;
                case CmdStop:   numHeld = 0; chordDirty = false; stopDrone(); break;
                case CmdPanic:  numHeld = 0; chordDirty = false; engine.panic(); sampleLayer.panic(); currentKey = -1; break;
                default: break;
            }
        };
        for (int i = 0; i < n1; ++i) handle (cmdBuffer[s1 + i]);
        for (int i = 0; i < n2; ++i) handle (cmdBuffer[s2 + i]);
        cmdFifo.finishedRead (n1 + n2);
    }

    for (const auto meta : midi) handleMidi (meta.getMessage(), meta.samplePosition);
    midi.clear();

    // Chord mode: resolve at most every 60 ms so a strummed chord becomes one drone.
    samplesSinceChordTrigger += n;
    if (chordDirty && samplesSinceChordTrigger > (juce::int64) (0.06 * sr))
    {
        chordDirty = false;
        samplesSinceChordTrigger = 0;
        if (numHeld > 0) triggerChord();
        else if (paramValue (ID::latch) < 0.5f) stopDrone();
    }

    engine.process (buffer);
    activeVoices = engine.numActiveVoices();
    outputPeak = engine.peakLevel();
}

// ---------------------------------------------------------------- async (message thread)
void CanopyProcessor::handleAsyncUpdate()
{
    if (reqNext.exchange (0)) nextItem();
    if (reqPrev.exchange (0)) prevItem();
    if (reqLatchToggle.exchange (0)) uiToggleLatch();
    if (reqModeCycle.exchange (0)) uiCycleMode();
    const float sw = reqSwell.exchange (-1.0f);
    if (sw >= 0.0f) setParamNormalised (ID::swell, sw);
    const float vol = reqVolume.exchange (-1.0f);
    if (vol >= 0.0f) setParamNormalised (ID::master, vol);
    const int zlo = reqZoneLow.exchange (-1);
    if (zlo >= 0) setParamPlain (ID::zonelow, (float) zlo);
    const int zhi = reqZoneHigh.exchange (-1);
    if (zhi >= 0)
    {
        int lo = (int) paramValue (ID::zonelow), hi = zhi;
        if (hi < lo) std::swap (lo, hi);
        setParamPlain (ID::zonelow, (float) lo);
        setParamPlain (ID::zonehigh, (float) hi);
    }
    const int learned = reqLearnedAction.exchange (-1);
    if (learned >= 0) writeMappingToState ((MidiAction) learned);
    stateChanged.sendChangeMessage();
}

// ---------------------------------------------------------------- MIDI map persistence
void CanopyProcessor::writeMappingToState (MidiAction a)
{
    auto mapTree = apvts.state.getOrCreateChildWithName ("midimap", nullptr);
    juce::ValueTree node;
    for (auto c : mapTree) if ((int) c["action"] == (int) a) node = c;
    if (! node.isValid()) { node = juce::ValueTree ("map"); node.setProperty ("action", (int) a, nullptr); mapTree.appendChild (node, nullptr); }
    auto& mp = mappings[(int) a];
    node.setProperty ("type", mp.type.load(), nullptr);
    node.setProperty ("number", mp.number.load(), nullptr);
    node.setProperty ("channel", mp.channel.load(), nullptr);
}

void CanopyProcessor::refreshMappingsFromState()
{
    for (auto& mp : mappings) { mp.type = 0; mp.number = -1; mp.channel = 0; }
    auto mapTree = apvts.state.getChildWithName ("midimap");
    for (auto c : mapTree)
    {
        const int a = c["action"];
        if (a < 0 || a >= (int) MidiAction::Count) continue;
        mappings[a].type = (int) c["type"];
        mappings[a].number = (int) c["number"];
        mappings[a].channel = (int) c["channel"];
    }
}

void CanopyProcessor::clearMapping (MidiAction a)
{
    auto& mp = mappings[(int) a];
    mp.type = 0; mp.number = -1; mp.channel = 0;
    writeMappingToState (a);
    stateChanged.sendChangeMessage();
}

juce::String CanopyProcessor::midiNoteName (int note)
{
    return juce::String (kNoteNamesSharp[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
}

juce::String CanopyProcessor::describeMapping (MidiAction a) const
{
    auto& mp = mappings[(int) a];
    if (mp.type == 1) return "Note " + midiNoteName (mp.number) + " (" + juce::String (mp.number.load()) + ")";
    if (mp.type == 2) return "CC " + juce::String (mp.number.load());
    return "-";
}

// ---------------------------------------------------------------- setlist
static const char* const kItemParams[] = { "mode", "l1style", "l1level", "l2style", "l2level", "l3style", "l3level",
                                           "sub", "brightness", "motion", "width", "revmix", "revsize", "swell", "attack",
                                           "release", "samplelevel", "eqlow", "eqmid", "eqhigh" };

void CanopyProcessor::addItemFromCurrent (const juce::String& name)
{
    juce::ValueTree item ("item");
    item.setProperty ("name", name.isEmpty() ? "Item " + juce::String (getSetlist().getNumChildren() + 1) : name, nullptr);
    getSetlist().appendChild (item, nullptr);
    updateItemFromCurrent (getSetlist().getNumChildren() - 1);
}

void CanopyProcessor::updateItemFromCurrent (int index)
{
    auto item = getSetlist().getChild (index);
    if (! item.isValid()) return;
    const int key = currentKey.load();
    item.setProperty ("key", key, nullptr);
    for (auto* id : kItemParams) item.setProperty (id, paramValue (id), nullptr);
    stateChanged.sendChangeMessage();
}

void CanopyProcessor::applyItem (const juce::ValueTree& item)
{
    for (auto* id : kItemParams)
        if (item.hasProperty (id)) setParamPlain (id, (float) (double) item[id]);
    const int key = item.getProperty ("key", -1);
    if (key >= 0) uiSetKey (key);
}

void CanopyProcessor::goToItem (int index)
{
    auto item = getSetlist().getChild (index);
    if (! item.isValid()) return;
    currentItem = index;
    applyItem (item);
    stateChanged.sendChangeMessage();
}

void CanopyProcessor::nextItem()
{
    const int n = getSetlist().getNumChildren();
    if (n == 0) return;
    goToItem (juce::jmin (currentItem + 1, n - 1));
}
void CanopyProcessor::prevItem()
{
    const int n = getSetlist().getNumChildren();
    if (n == 0) return;
    goToItem (juce::jmax (currentItem - 1, 0));
}
void CanopyProcessor::removeItem (int index)
{
    getSetlist().removeChild (index, nullptr);
    if (currentItem >= getSetlist().getNumChildren()) currentItem = getSetlist().getNumChildren() - 1;
    stateChanged.sendChangeMessage();
}
void CanopyProcessor::moveItem (int index, int delta)
{
    const int n = getSetlist().getNumChildren();
    const int to = index + delta;
    if (index < 0 || index >= n || to < 0 || to >= n) return;
    getSetlist().moveChild (index, to, nullptr);
    if (currentItem == index) currentItem = to;
    else if (currentItem == to) currentItem = index;
    stateChanged.sendChangeMessage();
}
void CanopyProcessor::renameItem (int index, const juce::String& name)
{
    auto item = getSetlist().getChild (index);
    if (item.isValid()) { item.setProperty ("name", name, nullptr); stateChanged.sendChangeMessage(); }
}

bool CanopyProcessor::saveSetlist (const juce::File& f)
{
    juce::Array<juce::var> arr;
    for (auto item : getSetlist())
    {
        auto* o = new juce::DynamicObject();
        for (int i = 0; i < item.getNumProperties(); ++i)
        {
            auto name = item.getPropertyName (i);
            o->setProperty (name, item[name]);
        }
        arr.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("app", "Canopy Pads");
    root->setProperty ("version", 1);
    root->setProperty ("items", arr);
    return f.replaceWithText (juce::JSON::toString (juce::var (root), false));
}

bool CanopyProcessor::loadSetlist (const juce::File& f, juce::String& error)
{
    auto parsed = juce::JSON::parse (f);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr || ! root->hasProperty ("items")) { error = "Not a Canopy setlist file"; return false; }
    auto* items = root->getProperty ("items").getArray();
    if (items == nullptr) { error = "Setlist has no items"; return false; }
    auto list = getSetlist();
    list.removeAllChildren (nullptr);
    for (auto& v : *items)
    {
        auto* o = v.getDynamicObject();
        if (o == nullptr) continue;
        juce::ValueTree item ("item");
        for (auto& p : o->getProperties()) item.setProperty (p.name, p.value, nullptr);
        list.appendChild (item, nullptr);
    }
    currentItem = list.getNumChildren() > 0 ? 0 : -1;
    stateChanged.sendChangeMessage();
    return true;
}

// ---------------------------------------------------------------- samples
bool CanopyProcessor::loadSampleSlot (int key, const juce::File& f, juce::String& error)
{
    if (! sampleLayer.loadSlot (key, f, error)) return false;
    auto tree = apvts.state.getOrCreateChildWithName ("samples", nullptr);
    juce::ValueTree node;
    for (auto c : tree) if ((int) c["key"] == key) node = c;
    if (! node.isValid()) { node = juce::ValueTree ("slot"); node.setProperty ("key", key, nullptr); tree.appendChild (node, nullptr); }
    node.setProperty ("path", f.getFullPathName(), nullptr);
    stateChanged.sendChangeMessage();
    return true;
}

void CanopyProcessor::clearSampleSlot (int key)
{
    sampleLayer.clearSlot (key);
    auto tree = apvts.state.getOrCreateChildWithName ("samples", nullptr);
    for (int i = tree.getNumChildren(); --i >= 0;)
        if ((int) tree.getChild (i)["key"] == key) tree.removeChild (i, nullptr);
    stateChanged.sendChangeMessage();
}

int CanopyProcessor::loadSampleFolder (const juce::File& dir, juce::String& report)
{
    int loaded = 0;
    juce::StringArray skipped;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg"))
    {
        const int key = SampleLayer::keyFromFilename (f.getFileNameWithoutExtension());
        if (key < 0) { skipped.add (f.getFileName()); continue; }
        juce::String err;
        if (loadSampleSlot (key, f, err)) ++loaded; else skipped.add (f.getFileName() + " (" + err + ")");
    }
    report = juce::String (loaded) + " pad" + (loaded == 1 ? "" : "s") + " loaded.";
    if (! skipped.isEmpty()) report += "\nCould not place: " + skipped.joinIntoString (", ");
    return loaded;
}

// ---------------------------------------------------------------- state
void CanopyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void CanopyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            getSetlist();
            apvts.state.getOrCreateChildWithName ("midimap", nullptr);
            apvts.state.getOrCreateChildWithName ("samples", nullptr);
            refreshMappingsFromState();
            sampleLayer.clearAll();
            for (auto c : apvts.state.getChildWithName ("samples"))
            {
                juce::File f (c["path"].toString());
                juce::String err;
                if (f.existsAsFile()) sampleLayer.loadSlot ((int) c["key"], f, err);
            }
            currentItem = getSetlist().getNumChildren() > 0 ? 0 : -1;
            stateChanged.sendChangeMessage();
        }
    }
}

juce::AudioProcessorEditor* CanopyProcessor::createEditor() { return new CanopyEditor (*this); }
} // namespace canopy

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new canopy::CanopyProcessor(); }
