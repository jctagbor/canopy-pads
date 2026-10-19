// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#include "PluginEditor.h"
#include "Exporter.h"

namespace canopy
{
using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (col::muted);
        g.setFont (uiFont (12.0f, true));
        g.drawText (title.toUpperCase(), r.reduced (14, 8).removeFromTop (16), juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- Knob
struct Knob : public juce::Component
{
    enum Format { Percent, Seconds, Decibels };
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& name, Format fmt = Percent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider.setWantsKeyboardFocus (false);
        switch (fmt)
        {
            case Percent:  slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }; break;
            case Seconds:  slider.textFromValueFunction = [] (double v) { return juce::String (v, v < 10 ? 1 : 0) + " s"; }; break;
            case Decibels: slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; }; break;
        }
        att = std::make_unique<SliderAtt> (apvts, id, slider);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (uiFont (12.0f, true));
        label.setColour (juce::Label::textColourId, col::muted);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<SliderAtt> att;
};

// ---------------------------------------------------------------- KeyGrid
struct KeyGrid : public juce::Component
{
    std::function<void (int)> onKey;
    int current = -1;
    KeyGrid()
    {
        for (int i = 0; i < 12; ++i)
        {
            auto* b = keys.add (new juce::TextButton());
            juce::String name = kNoteNames[i];
            if (juce::String (kNoteNames[i]) != kNoteNamesSharp[i]) name += " / " + juce::String (kNoteNamesSharp[i]);
            b->setButtonText (name);
            b->setWantsKeyboardFocus (false);
            b->onClick = [this, i] { if (onKey) onKey (i); };
            addAndMakeVisible (b);
        }
    }
    void setCurrent (int k)
    {
        if (current == k) return;
        current = k;
        for (int i = 0; i < 12; ++i) keys[i]->setToggleState (i == k, juce::dontSendNotification);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int cols = 6, rows = 2;
        const int w = r.getWidth() / cols, h = r.getHeight() / rows;
        for (int i = 0; i < 12; ++i)
            keys[i]->setBounds (r.getX() + (i % cols) * w, r.getY() + (i / cols) * h, w, h);
        for (auto* k : keys) k->setBounds (k->getBounds().reduced (4));
    }
    juce::OwnedArray<juce::TextButton> keys;
};

// ---------------------------------------------------------------- Play tab
struct PlayTab : public TabBase
{
    PlayTab (CanopyProcessor& p) : proc (p),
        attack (p.apvts, ID::attack, "Attack", Knob::Seconds), release (p.apvts, ID::release, "Release", Knob::Seconds),
        master (p.apvts, ID::master, "Master"), sub (p.apvts, ID::sub, "Sub")
    {
        keyGrid.onKey = [this] (int k) { proc.uiSetKey (k); };
        addAndMakeVisible (keyGrid);

        for (int i = 0; i < 4; ++i)
        {
            auto* b = modeButtons.add (new juce::TextButton (kModeNames[i]));
            b->setWantsKeyboardFocus (false);
            b->setRadioGroupId (77);
            b->setClickingTogglesState (true);
            b->onClick = [this, i] { proc.uiSetMode ((DroneMode) i); };
            addAndMakeVisible (b);
        }

        swell.setSliderStyle (juce::Slider::LinearHorizontal);
        swell.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        swell.setWantsKeyboardFocus (false);
        swellAtt = std::make_unique<SliderAtt> (p.apvts, ID::swell, swell);
        addAndMakeVisible (swell);
        swellLabel.setText ("SWELL  -  pull down for a quiet, dark bed; push up to build", juce::dontSendNotification);
        swellLabel.setFont (uiFont (12.0f, true));
        swellLabel.setColour (juce::Label::textColourId, col::muted);
        addAndMakeVisible (swellLabel);

        addAndMakeVisible (attack); addAndMakeVisible (release); addAndMakeVisible (master); addAndMakeVisible (sub);

        const juce::String styleIds[3] = { ID::l1style, ID::l2style, ID::l3style };
        const juce::String levelIds[3] = { ID::l1level, ID::l2level, ID::l3level };
        for (int i = 0; i < 3; ++i)
        {
            auto& L = layers[i];
            L.combo.addItem ("Off", 1);
            for (int s = 0; s < (int) styles().size(); ++s) L.combo.addItem (styles()[(size_t) s].name, s + 2);
            L.combo.setWantsKeyboardFocus (false);
            L.comboAtt = std::make_unique<ComboAtt> (p.apvts, styleIds[i], L.combo);
            L.level.setSliderStyle (juce::Slider::LinearHorizontal);
            L.level.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            L.level.setWantsKeyboardFocus (false);
            L.levelAtt = std::make_unique<SliderAtt> (p.apvts, levelIds[i], L.level);
            L.title.setText ("Layer " + juce::String (i + 1), juce::dontSendNotification);
            L.title.setFont (uiFont (12.0f, true));
            L.title.setColour (juce::Label::textColourId, col::muted);
            L.blurb.setFont (uiFont (12.0f));
            L.blurb.setColour (juce::Label::textColourId, col::muted);
            L.combo.onChange = [this, i] { updateBlurb (i); };
            addAndMakeVisible (L.combo); addAndMakeVisible (L.level); addAndMakeVisible (L.title); addAndMakeVisible (L.blurb);
            updateBlurb (i);
        }
        hint.setText ("Click a key, play your MIDI drone zone, or use the computer keys  A W S E D F T G Y H U J  (K = C again).  Space stops, Esc panics.",
                      juce::dontSendNotification);
        hint.setFont (uiFont (12.0f));
        hint.setColour (juce::Label::textColourId, col::muted);
        hint.setJustificationType (juce::Justification::centred);
        hint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (hint);
    }

    void updateBlurb (int i)
    {
        const int sel = layers[i].combo.getSelectedId() - 2;
        layers[i].blurb.setText (sel >= 0 && sel < (int) styles().size() ? styles()[(size_t) sel].blurb : "Layer off", juce::dontSendNotification);
    }

    void tick() override
    {
        keyGrid.setCurrent (proc.currentKey.load());
        const int mode = (int) proc.apvts.getRawParameterValue (ID::mode)->load();
        for (int i = 0; i < 4; ++i) modeButtons[i]->setToggleState (i == mode, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, leftPanel, "Drone");
        drawPanel (g, rightPanel, "Layers");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        hint.setBounds (r.removeFromBottom (20));
        r.removeFromBottom (6);
        rightPanel = r.removeFromRight (juce::jmax (300, r.getWidth() * 38 / 100));
        r.removeFromRight (10);
        leftPanel = r;

        auto L = leftPanel.reduced (14).withTrimmedTop (18);
        auto knobRow = L.removeFromBottom (110);
        L.removeFromBottom (8);
        auto swellArea = L.removeFromBottom (74);
        L.removeFromBottom (8);
        auto modeRow = L.removeFromBottom (44);
        L.removeFromBottom (8);
        keyGrid.setBounds (L);
        const int mw = modeRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i) modeButtons[i]->setBounds (modeRow.removeFromLeft (mw).reduced (4, 0));
        swellLabel.setBounds (swellArea.removeFromTop (18));
        swell.setBounds (swellArea);
        const int kw = knobRow.getWidth() / 4;
        attack.setBounds (knobRow.removeFromLeft (kw));
        release.setBounds (knobRow.removeFromLeft (kw));
        sub.setBounds (knobRow.removeFromLeft (kw));
        master.setBounds (knobRow);

        auto R = rightPanel.reduced (14).withTrimmedTop (22);
        const int rowH = juce::jmin (120, R.getHeight() / 3);
        for (int i = 0; i < 3; ++i)
        {
            auto row = R.removeFromTop (rowH).reduced (0, 6);
            layers[i].title.setBounds (row.removeFromTop (16));
            layers[i].combo.setBounds (row.removeFromTop (30));
            row.removeFromTop (4);
            layers[i].blurb.setBounds (row.removeFromTop (16));
            layers[i].level.setBounds (row.removeFromTop (28));
        }
    }

    CanopyProcessor& proc;
    KeyGrid keyGrid;
    juce::OwnedArray<juce::TextButton> modeButtons;
    juce::Slider swell;
    juce::Label swellLabel, hint;
    std::unique_ptr<SliderAtt> swellAtt;
    Knob attack, release, master, sub;
    struct LayerRow { juce::ComboBox combo; juce::Slider level; juce::Label title, blurb; std::unique_ptr<ComboAtt> comboAtt; std::unique_ptr<SliderAtt> levelAtt; };
    LayerRow layers[3];
    juce::Rectangle<int> leftPanel, rightPanel;
};

// ---------------------------------------------------------------- Sounds tab
struct SoundsTab : public TabBase
{
    SoundsTab (CanopyProcessor& p) : proc (p),
        brightness (p.apvts, ID::brightness, "Brightness"), motion (p.apvts, ID::motion, "Motion"),
        width (p.apvts, ID::width, "Width"), eqLow (p.apvts, ID::eqlow, "Low", Knob::Decibels),
        eqMid (p.apvts, ID::eqmid, "Mid", Knob::Decibels), eqHigh (p.apvts, ID::eqhigh, "High", Knob::Decibels),
        revMix (p.apvts, ID::revmix, "Reverb"), revSize (p.apvts, ID::revsize, "Size"),
        sampleLevel (p.apvts, ID::samplelevel, "Pad Level")
    {
        for (auto* k : { &brightness, &motion, &width, &eqLow, &eqMid, &eqHigh, &revMix, &revSize, &sampleLevel }) addAndMakeVisible (k);

        for (int i = 0; i < 12; ++i)
        {
            auto* b = slots.add (new juce::TextButton());
            b->setWantsKeyboardFocus (false);
            b->onClick = [this, i] { chooseSlotFile (i); };
            addAndMakeVisible (b);
        }
        loadFolder.onClick = [this] { chooseFolder(); };
        clearAll.onClick = [this] { for (int i = 0; i < 12; ++i) proc.clearSampleSlot (i); };
        for (auto* b : { &loadFolder, &clearAll }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
        autoTranspose.setButtonText ("Auto-transpose when a key has no file");
        autoTranspose.setWantsKeyboardFocus (false);
        autoAtt = std::make_unique<ButtonAtt> (p.apvts, ID::autotranspose, autoTranspose);
        addAndMakeVisible (autoTranspose);
        sampleHint.setText ("Click a key to load your own WAV/MP3/AIFF pad for it; right-click to clear. \"Load folder\" reads the key from each file name (\"Pad - Eb.wav\", \"Warm_F#.mp3\"). Files loop with click-free crossfades and play alongside the synth layers.",
                            juce::dontSendNotification);
        sampleHint.setFont (uiFont (12.0f));
        sampleHint.setColour (juce::Label::textColourId, col::muted);
        sampleHint.setJustificationType (juce::Justification::topLeft);
        sampleHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (sampleHint);
        refresh();
    }

    void chooseSlotFile (int key)
    {
        if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu()) { proc.clearSampleSlot (key); return; }
        chooser = std::make_shared<juce::FileChooser> ("Choose a pad file for " + juce::String (kNoteNames[key]),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, key] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                juce::String err;
                if (! proc.loadSampleSlot (key, f, err))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not load pad", err);
            });
    }

    void chooseFolder()
    {
        chooser = std::make_shared<juce::FileChooser> ("Choose a folder of pad files (one per key)",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                auto d = fc.getResult();
                if (! d.isDirectory()) return;
                juce::String report;
                proc.loadSampleFolder (d, report);
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Pads loaded", report);
            });
    }

    void refresh() override
    {
        for (int i = 0; i < 12; ++i)
        {
            const auto& s = proc.sampleLayer.getSlot (i);
            slots[i]->setButtonText (juce::String (kNoteNames[i]) + (s.buffer ? "\n" + s.name : "\n+"));
            slots[i]->setToggleState (s.buffer != nullptr, juce::dontSendNotification);
        }
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, tonePanel, "Tone");
        drawPanel (g, eqPanel, "EQ");
        drawPanel (g, spacePanel, "Space");
        drawPanel (g, samplePanel, "Your own pads");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (150);
        r.removeFromTop (10);
        samplePanel = r;
        tonePanel = top.removeFromLeft (top.getWidth() * 42 / 100); top.removeFromLeft (10);
        eqPanel = top.removeFromLeft (top.getWidth() * 55 / 100); top.removeFromLeft (10);
        spacePanel = top;

        auto t = tonePanel.reduced (10).withTrimmedTop (20);
        const int tw = t.getWidth() / 3;
        brightness.setBounds (t.removeFromLeft (tw)); motion.setBounds (t.removeFromLeft (tw)); width.setBounds (t);
        auto e = eqPanel.reduced (10).withTrimmedTop (20);
        const int ew = e.getWidth() / 3;
        eqLow.setBounds (e.removeFromLeft (ew)); eqMid.setBounds (e.removeFromLeft (ew)); eqHigh.setBounds (e);
        auto s = spacePanel.reduced (10).withTrimmedTop (20);
        const int sw = s.getWidth() / 2;
        revMix.setBounds (s.removeFromLeft (sw)); revSize.setBounds (s);

        auto sp = samplePanel.reduced (14).withTrimmedTop (22);
        auto right = sp.removeFromRight (190);
        sp.removeFromRight (12);
        sampleLevel.setBounds (right.removeFromTop (110));
        right.removeFromTop (6);
        loadFolder.setBounds (right.removeFromTop (34)); right.removeFromTop (6);
        clearAll.setBounds (right.removeFromTop (34)); right.removeFromTop (6);
        autoTranspose.setBounds (right.removeFromTop (40));
        sampleHint.setBounds (sp.removeFromBottom (48));
        sp.removeFromBottom (6);
        const int cols = 6, rows = 2, w = sp.getWidth() / cols, h = sp.getHeight() / rows;
        for (int i = 0; i < 12; ++i)
            slots[i]->setBounds (juce::Rectangle<int> (sp.getX() + (i % cols) * w, sp.getY() + (i / cols) * h, w, h).reduced (4));
    }

    CanopyProcessor& proc;
    Knob brightness, motion, width, eqLow, eqMid, eqHigh, revMix, revSize, sampleLevel;
    juce::OwnedArray<juce::TextButton> slots;
    juce::TextButton loadFolder { "Load folder..." }, clearAll { "Clear all pads" };
    juce::ToggleButton autoTranspose;
    std::unique_ptr<ButtonAtt> autoAtt;
    juce::Label sampleHint;
    std::shared_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> tonePanel, eqPanel, spacePanel, samplePanel;
};

// ---------------------------------------------------------------- Setlist tab
struct SetlistTab : public TabBase, private juce::ListBoxModel
{
    SetlistTab (CanopyProcessor& p) : proc (p)
    {
        list.setModel (this);
        list.setRowHeight (34);
        list.setWantsKeyboardFocus (false);
        list.setColour (juce::ListBox::backgroundColourId, col::panel2);
        list.setColour (juce::ListBox::outlineColourId, col::line);
        list.setOutlineThickness (1);
        addAndMakeVisible (list);
        nameBox.setTextToShowWhenEmpty ("Name (e.g. \"Prayer - Eb\")", col::muted);
        nameBox.setFont (uiFont (14.0f));
        addAndMakeVisible (nameBox);

        addBtn.onClick    = [this] { proc.addItemFromCurrent (nameBox.getText()); nameBox.clear(); list.selectRow (proc.getSetlist().getNumChildren() - 1); };
        updateBtn.onClick = [this] { if (list.getSelectedRow() >= 0) proc.updateItemFromCurrent (list.getSelectedRow()); };
        goBtn.onClick     = [this] { if (list.getSelectedRow() >= 0) proc.goToItem (list.getSelectedRow()); };
        upBtn.onClick     = [this] { const int r = list.getSelectedRow(); if (r > 0) { proc.moveItem (r, -1); list.selectRow (r - 1); } };
        downBtn.onClick   = [this] { const int r = list.getSelectedRow(); if (r >= 0) { proc.moveItem (r, 1); list.selectRow (juce::jmin (r + 1, proc.getSetlist().getNumChildren() - 1)); } };
        renameBtn.onClick = [this] { if (list.getSelectedRow() >= 0 && nameBox.getText().isNotEmpty()) proc.renameItem (list.getSelectedRow(), nameBox.getText()); };
        deleteBtn.onClick = [this] { if (list.getSelectedRow() >= 0) proc.removeItem (list.getSelectedRow()); };
        saveBtn.onClick   = [this] { save(); };
        loadBtn.onClick   = [this] { load(); };
        for (auto* b : { &addBtn, &updateBtn, &goBtn, &upBtn, &downBtn, &renameBtn, &deleteBtn, &saveBtn, &loadBtn })
        { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
        goBtn.setColour (juce::TextButton::buttonColourId, col::sage.withAlpha (0.35f));
        deleteBtn.setColour (juce::TextButton::buttonColourId, col::danger.withAlpha (0.3f));

        hint.setText ("A setlist item remembers the key, mode, layers, swell and tone. Step through it with the arrow keys or a mapped footswitch (MIDI tab). Double-click an item to go to it.",
                      juce::dontSendNotification);
        hint.setFont (uiFont (12.0f));
        hint.setColour (juce::Label::textColourId, col::muted);
        hint.setJustificationType (juce::Justification::topLeft);
        hint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (hint);
    }

    void save()
    {
        chooser = std::make_shared<juce::FileChooser> ("Save setlist", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Canopy Setlist.canopy.json"), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) proc.saveSetlist (f.withFileExtension ("json")); });
    }
    void load()
    {
        chooser = std::make_shared<juce::FileChooser> ("Load setlist", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                juce::String err;
                if (! proc.loadSetlist (f, err))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not load setlist", err);
            });
    }

    int getNumRows() override { return proc.getSetlist().getNumChildren(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        auto item = proc.getSetlist().getChild (row);
        const bool current = row == proc.getCurrentItem();
        g.setColour (selected ? col::gold.withAlpha (0.18f) : (row % 2 ? col::panel2 : col::panel2.brighter (0.03f)));
        g.fillRect (0, 0, w, h);
        if (current) { g.setColour (col::gold); g.fillRect (0, 0, 4, h); }
        g.setColour (col::muted);
        g.setFont (uiFont (12.0f));
        g.drawText (juce::String (row + 1), 12, 0, 28, h, juce::Justification::centredLeft);
        g.setColour (col::text);
        g.setFont (uiFont (15.0f, current));
        g.drawText (item["name"].toString(), 44, 0, w - 200, h, juce::Justification::centredLeft, true);
        const int key = item.getProperty ("key", -1);
        const int mode = (int) (double) item.getProperty ("mode", 1);
        juce::String info = key >= 0 ? juce::String (kNoteNames[key]) + "  " + kModeNames[juce::jlimit (0, 3, mode)] : "-";
        g.setColour (col::gold);
        g.setFont (uiFont (14.0f, true));
        g.drawText (info, w - 150, 0, 138, h, juce::Justification::centredRight);
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { proc.goToItem (row); }
    void selectedRowsChanged (int row) override
    {
        if (row >= 0) nameBox.setText (proc.getSetlist().getChild (row)["name"].toString(), false);
    }

    void refresh() override { list.updateContent(); list.repaint(); }

    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds().reduced (10), "Setlist"); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24).withTrimmedTop (18);
        auto right = r.removeFromRight (220);
        r.removeFromRight (14);
        list.setBounds (r);
        nameBox.setBounds (right.removeFromTop (32)); right.removeFromTop (8);
        auto rowOf = [&] (juce::TextButton& a, juce::TextButton& b)
        {
            auto row = right.removeFromTop (36);
            a.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
            b.setBounds (row.reduced (2, 0));
            right.removeFromTop (6);
        };
        addBtn.setBounds (right.removeFromTop (36)); right.removeFromTop (6);
        goBtn.setBounds (right.removeFromTop (44)); right.removeFromTop (6);
        rowOf (upBtn, downBtn);
        rowOf (renameBtn, updateBtn);
        deleteBtn.setBounds (right.removeFromTop (36)); right.removeFromTop (14);
        rowOf (saveBtn, loadBtn);
        hint.setBounds (right);
    }

    CanopyProcessor& proc;
    juce::ListBox list;
    juce::TextEditor nameBox;
    juce::TextButton addBtn { "+ Add current sound" }, updateBtn { "Overwrite" }, goBtn { "GO" }, upBtn { "Up" }, downBtn { "Down" },
                     renameBtn { "Rename" }, deleteBtn { "Delete" }, saveBtn { "Save..." }, loadBtn { "Load..." };
    juce::Label hint;
    std::shared_ptr<juce::FileChooser> chooser;
};

// ---------------------------------------------------------------- MIDI tab
struct MidiTab : public TabBase
{
    MidiTab (CanopyProcessor& p) : proc (p)
    {
        for (int n = 0; n < 128; ++n) { zoneLow.addItem (CanopyProcessor::midiNoteName (n), n + 1); zoneHigh.addItem (CanopyProcessor::midiNoteName (n), n + 1); }
        zoneLow.onChange  = [this] { if (! updating) setZone (zoneLow.getSelectedId() - 1, zoneHigh.getSelectedId() - 1); };
        zoneHigh.onChange = [this] { if (! updating) setZone (zoneLow.getSelectedId() - 1, zoneHigh.getSelectedId() - 1); };
        for (auto* c : { &zoneLow, &zoneHigh }) { c->setWantsKeyboardFocus (false); addAndMakeVisible (c); }
        zoneLabel.setText ("Drone zone - MIDI notes in this range trigger drones. Everything above it is ignored, so you keep playing your keyboard's own sound.", juce::dontSendNotification);
        zoneLabel.setFont (uiFont (12.0f));
        zoneLabel.setColour (juce::Label::textColourId, col::muted);
        zoneLabel.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (zoneLabel);
        toLabel.setText ("to", juce::dontSendNotification); toLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (toLabel);
        learnZone.onClick = [this] { proc.startZoneLearn(); };
        learnZone.setWantsKeyboardFocus (false);
        addAndMakeVisible (learnZone);
        sustain.setButtonText ("Sustain pedal holds the drone while latch is off");
        sustain.setWantsKeyboardFocus (false);
        sustainAtt = std::make_unique<ButtonAtt> (p.apvts, ID::sustainlatch, sustain);
        addAndMakeVisible (sustain);

        for (int a = 0; a < (int) MidiAction::Count; ++a)
        {
            auto& row = rows[a];
            row.name.setText (kActionNames[a], juce::dontSendNotification);
            row.name.setFont (uiFont (14.0f));
            row.mapping.setFont (uiFont (14.0f, true));
            row.mapping.setColour (juce::Label::textColourId, col::gold);
            row.learn.onClick = [this, a] { proc.startLearn ((MidiAction) a); };
            row.clear.onClick = [this, a] { proc.clearMapping ((MidiAction) a); };
            for (auto* b : { &row.learn, &row.clear }) b->setWantsKeyboardFocus (false);
            addAndMakeVisible (row.name); addAndMakeVisible (row.mapping); addAndMakeVisible (row.learn); addAndMakeVisible (row.clear);
        }
        keysText.setText (
            "A W S E D F T G Y H U J  =  C  Db  D  Eb  E  F  Gb  G  Ab  A  Bb  B      K = C\n"
            "Space = stop (fade out)      Esc = panic (instant silence)      L = toggle latch      M = cycle mode\n"
            "Right / Down = next setlist item      Left / Up = previous      [ and ] = swell down / up      - and = : master volume",
            juce::dontSendNotification);
        keysText.setFont (uiFont (13.0f));
        keysText.setColour (juce::Label::textColourId, col::text);
        keysText.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (keysText);
        refresh();
    }

    void setZone (int lo, int hi)
    {
        if (hi < lo) std::swap (lo, hi);
        if (auto* p = proc.apvts.getParameter (ID::zonelow)) p->setValueNotifyingHost (p->convertTo0to1 ((float) lo));
        if (auto* p = proc.apvts.getParameter (ID::zonehigh)) p->setValueNotifyingHost (p->convertTo0to1 ((float) hi));
    }

    void refresh() override
    {
        for (int a = 0; a < (int) MidiAction::Count; ++a) rows[a].mapping.setText (proc.describeMapping ((MidiAction) a), juce::dontSendNotification);
    }

    void tick() override
    {
        updating = true;
        const int lo = (int) proc.apvts.getRawParameterValue (ID::zonelow)->load();
        const int hi = (int) proc.apvts.getRawParameterValue (ID::zonehigh)->load();
        if (zoneLow.getSelectedId() != lo + 1) zoneLow.setSelectedId (lo + 1, juce::dontSendNotification);
        if (zoneHigh.getSelectedId() != hi + 1) zoneHigh.setSelectedId (hi + 1, juce::dontSendNotification);
        updating = false;
        const int zl = proc.zoneLearnStep.load();
        learnZone.setButtonText (zl == 1 ? "Play the LOWEST key..." : zl == 2 ? "Now the HIGHEST key..." : "Learn range from keyboard");
        learnZone.setToggleState (zl != 0, juce::dontSendNotification);
        const int lt = proc.learnTarget.load();
        for (int a = 0; a < (int) MidiAction::Count; ++a)
        {
            rows[a].learn.setToggleState (lt == a, juce::dontSendNotification);
            rows[a].learn.setButtonText (lt == a ? "Waiting..." : "Learn");
        }
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, zonePanel, "Drone zone");
        drawPanel (g, mapPanel, "Footswitch & controller mapping");
        drawPanel (g, keysPanel, "Computer keyboard");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        zonePanel = r.removeFromTop (124); r.removeFromTop (10);
        keysPanel = r.removeFromBottom (96); r.removeFromBottom (10);
        mapPanel = r;

        auto z = zonePanel.reduced (14).withTrimmedTop (22);
        zoneLabel.setBounds (z.removeFromTop (18));
        z.removeFromTop (8);
        auto zrow = z.removeFromTop (32);
        zoneLow.setBounds (zrow.removeFromLeft (110));
        toLabel.setBounds (zrow.removeFromLeft (30));
        zoneHigh.setBounds (zrow.removeFromLeft (110));
        zrow.removeFromLeft (12);
        learnZone.setBounds (zrow.removeFromLeft (220));
        zrow.removeFromLeft (20);
        sustain.setBounds (zrow);

        auto m = mapPanel.reduced (14).withTrimmedTop (22);
        const int rowH = juce::jmin (36, m.getHeight() / (int) MidiAction::Count);
        for (int a = 0; a < (int) MidiAction::Count; ++a)
        {
            auto row = m.removeFromTop (rowH).reduced (0, 3);
            rows[a].clear.setBounds (row.removeFromRight (70));
            row.removeFromRight (6);
            rows[a].learn.setBounds (row.removeFromRight (110));
            row.removeFromRight (10);
            rows[a].mapping.setBounds (row.removeFromRight (160));
            rows[a].name.setBounds (row);
        }
        keysText.setBounds (keysPanel.reduced (14).withTrimmedTop (22));
    }

    CanopyProcessor& proc;
    juce::ComboBox zoneLow, zoneHigh;
    juce::Label zoneLabel, toLabel, keysText;
    juce::TextButton learnZone;
    juce::ToggleButton sustain;
    std::unique_ptr<ButtonAtt> sustainAtt;
    struct Row { juce::Label name, mapping; juce::TextButton learn { "Learn" }, clear { "Clear" }; };
    Row rows[(int) MidiAction::Count];
    bool updating = false;
    juce::Rectangle<int> zonePanel, mapPanel, keysPanel;
};

// ---------------------------------------------------------------- Export tab
struct RenderJob : public juce::ThreadWithProgressWindow
{
    RenderJob (EngineParams p, juce::Array<int> keysIn, DroneMode m, double secs, double sampleRate, juce::File dirIn, std::function<void (juce::String)> done)
        : ThreadWithProgressWindow ("Rendering pads...", true, true), params (p), keys (keysIn), mode (m), seconds (secs), sr (sampleRate), dir (dirIn), onDone (done) {}

    void run() override
    {
        int i = 0;
        for (int k : keys)
        {
            if (threadShouldExit()) return;
            setStatusMessage ("Rendering " + juce::String (kNoteNames[k]) + "  (" + juce::String (i + 1) + " of " + juce::String (keys.size()) + ")");
            auto file = dir.getChildFile (Exporter::suggestedFileName (params, k, mode));
            auto err = Exporter::renderKeyToWav (params, k, mode, seconds, sr, file,
                [this, i] (float pr) { setProgress (((double) i + pr) / (double) keys.size()); return ! threadShouldExit(); });
            if (err.isNotEmpty() && err != "Cancelled") { error = err; return; }
            ++i;
        }
    }
    void threadComplete (bool cancelled) override
    {
        auto msg = cancelled ? juce::String ("Export cancelled.")
                 : error.isNotEmpty() ? "Export failed: " + error
                 : juce::String (keys.size()) + " pad file" + (keys.size() == 1 ? "" : "s") + " written to\n" + dir.getFullPathName();
        if (onDone) onDone (msg);
    }
    EngineParams params; juce::Array<int> keys; DroneMode mode; double seconds, sr; juce::File dir; juce::String error;
    std::function<void (juce::String)> onDone;
};

struct ExportTab : public TabBase
{
    ExportTab (CanopyProcessor& p) : proc (p)
    {
        seconds.setRange (10.0, 180.0, 1.0);
        seconds.setValue (60.0, juce::dontSendNotification);
        seconds.setSliderStyle (juce::Slider::LinearHorizontal);
        seconds.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
        seconds.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " s"; };
        seconds.setWantsKeyboardFocus (false);
        seconds.updateText();
        addAndMakeVisible (seconds);
        secondsLabel.setText ("Length of each pad file", juce::dontSendNotification);
        secondsLabel.setColour (juce::Label::textColourId, col::muted);
        addAndMakeVisible (secondsLabel);
        mode.addItemList ({ "Root only", "Major", "Minor" }, 1);
        mode.setSelectedId (2, juce::dontSendNotification);
        mode.setWantsKeyboardFocus (false);
        addAndMakeVisible (mode);
        rate.addItemList ({ "44.1 kHz", "48 kHz" }, 1);
        rate.setSelectedId (2, juce::dontSendNotification);
        rate.setWantsKeyboardFocus (false);
        addAndMakeVisible (rate);
        exportAll.onClick = [this] { start (true); };
        exportOne.onClick = [this] { start (false); };
        for (auto* b : { &exportAll, &exportOne }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
        info.setText ("Renders the current sound (all layers, sub, tone, EQ, reverb) to 24-bit WAV files you can drop into MainStage, Ableton, Logic, a sampler, or a phone. "
                      "Files are named like \"Canopy Warm - Eb maj.wav\", normalised to -1 dBFS, with a smooth fade in and out so they loop or crossfade cleanly. "
                      "Use this when you want Canopy's sound somewhere the plugin can't run.",
                      juce::dontSendNotification);
        info.setFont (uiFont (13.0f));
        info.setColour (juce::Label::textColourId, col::muted);
        info.setJustificationType (juce::Justification::topLeft);
        info.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (info);
        status.setFont (uiFont (13.0f, true));
        status.setColour (juce::Label::textColourId, col::sage);
        status.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (status);
    }

    void start (bool all)
    {
        chooser = std::make_shared<juce::FileChooser> ("Choose a folder for the pad files", juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, all] (const juce::FileChooser& fc)
            {
                auto dir = fc.getResult();
                if (! dir.isDirectory()) return;
                juce::Array<int> keys;
                if (all) for (int k = 0; k < 12; ++k) keys.add (k);
                else keys.add (juce::jmax (0, proc.currentKey.load()));
                const DroneMode m = (DroneMode) (mode.getSelectedId() - 1);
                const double sr = rate.getSelectedId() == 1 ? 44100.0 : 48000.0;
                job = std::make_unique<RenderJob> (proc.currentEngineParams(), keys, m, seconds.getValue(), sr, dir,
                    [this] (juce::String msg)
                    {
                        status.setText (msg, juce::dontSendNotification);
                        juce::MessageManager::callAsync ([this] { job.reset(); });
                    });
                status.setText ("Rendering...", juce::dontSendNotification);
                job->launchThread();
            });
    }

    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds().reduced (10), "Export pads as audio files"); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24).withTrimmedTop (18);
        info.setBounds (r.removeFromTop (70));
        r.removeFromTop (10);
        secondsLabel.setBounds (r.removeFromTop (18));
        seconds.setBounds (r.removeFromTop (34).withTrimmedRight (r.getWidth() / 3));
        r.removeFromTop (12);
        auto row = r.removeFromTop (34);
        mode.setBounds (row.removeFromLeft (160)); row.removeFromLeft (10);
        rate.setBounds (row.removeFromLeft (120));
        r.removeFromTop (16);
        auto brow = r.removeFromTop (46);
        exportAll.setBounds (brow.removeFromLeft (260)); brow.removeFromLeft (10);
        exportOne.setBounds (brow.removeFromLeft (220));
        r.removeFromTop (14);
        status.setBounds (r.removeFromTop (60));
    }

    CanopyProcessor& proc;
    juce::Slider seconds;
    juce::Label secondsLabel, info, status;
    juce::ComboBox mode, rate;
    juce::TextButton exportAll { "Export all 12 keys..." }, exportOne { "Export current key..." };
    std::shared_ptr<juce::FileChooser> chooser;
    std::unique_ptr<RenderJob> job;
};

// ---------------------------------------------------------------- Editor
CanopyEditor::CanopyEditor (CanopyProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    title.setText ("CANOPY", juce::dontSendNotification);
    title.setFont (uiFont (26.0f, true));
    title.setColour (juce::Label::textColourId, col::gold);
    addAndMakeVisible (title);
    credit.setText ("by John Carl Tagbor", juce::dontSendNotification);
    credit.setFont (uiFont (11.0f));
    credit.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (credit);
    keyLabel.setFont (uiFont (54.0f, true));
    keyLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (keyLabel);
    modeLabel.setFont (uiFont (13.0f, true));
    modeLabel.setJustificationType (juce::Justification::centred);
    modeLabel.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (modeLabel);
    statusLabel.setFont (uiFont (12.0f));
    statusLabel.setColour (juce::Label::textColourId, col::muted);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    latchButton.setClickingTogglesState (true);
    latchAtt = std::make_unique<ButtonAtt> (p.apvts, ID::latch, latchButton);
    stopButton.onClick = [this] { proc.uiStop(); };
    panicButton.onClick = [this] { proc.uiPanic(); };
    panicButton.setColour (juce::TextButton::buttonColourId, col::danger.withAlpha (0.35f));
    for (auto* b : { &latchButton, &stopButton, &panicButton }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }

    auto addTab = [this] (const juce::String& name, TabBase* page)
    {
        tabs.addTab (name, col::bg, page, true);
        tabPages.add (page);
    };
    addTab ("Play", new PlayTab (p));
    addTab ("Sounds", new SoundsTab (p));
    addTab ("Setlist", new SetlistTab (p));
    addTab ("MIDI & Keys", new MidiTab (p));
    addTab ("Export", new ExportTab (p));
    tabs.setTabBarDepth (38);
    tabs.setWantsKeyboardFocus (false);
    addAndMakeVisible (tabs);

    proc.stateChanged.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (900, 620, 2400, 1600);
    setSize (1040, 700);
    startTimerHz (30);
}

CanopyEditor::~CanopyEditor()
{
    proc.stateChanged.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void CanopyEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* t : tabPages) t->refresh();
}

void CanopyEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (96);
    g.setColour (col::panel);
    g.fillRect (header);
    g.setColour (col::line);
    g.fillRect (header.removeFromBottom (1));

    // Output meter under the key display
    auto meter = keyLabel.getBounds().withHeight (5).withY (keyLabel.getBottom() + 18);
    g.setColour (col::line);
    g.fillRoundedRectangle (meter.toFloat(), 2.5f);
    g.setColour (meterLevel > 0.95f ? col::danger : col::sage);
    g.fillRoundedRectangle (meter.withWidth ((int) (meter.getWidth() * juce::jlimit (0.0f, 1.0f, meterLevel))).toFloat(), 2.5f);
}

void CanopyEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (96);
    tabs.setBounds (r.reduced (6, 0).withTrimmedBottom (6));

    auto h = header.reduced (16, 10);
    auto titleCol = h.removeFromLeft (150);
    title.setBounds (titleCol.removeFromTop (36));
    credit.setBounds (titleCol.removeFromTop (16).translated (2, -4));
    auto right = h.removeFromRight (300);
    auto buttons = right.removeFromTop (40);
    panicButton.setBounds (buttons.removeFromRight (90)); buttons.removeFromRight (6);
    stopButton.setBounds (buttons.removeFromRight (90)); buttons.removeFromRight (6);
    latchButton.setBounds (buttons.removeFromRight (90));
    statusLabel.setBounds (right.withTrimmedTop (6));
    auto centre = h.withSizeKeepingCentre (juce::jmin (420, h.getWidth()), h.getHeight());
    keyLabel.setBounds (centre.removeFromTop (50).translated (0, -4));
    modeLabel.setBounds (centre.removeFromTop (16));
}

void CanopyEditor::timerCallback()
{
    const int key = proc.currentKey.load();
    const int mode = (int) proc.apvts.getRawParameterValue (ID::mode)->load();
    const bool latch = proc.apvts.getRawParameterValue (ID::latch)->load() > 0.5f;
    keyLabel.setText (key >= 0 ? juce::String (kNoteNames[key]) : "-", juce::dontSendNotification);
    keyLabel.setColour (juce::Label::textColourId, key >= 0 ? col::gold : col::muted);
    juce::String m = kModeNames[juce::jlimit (0, 3, mode)];
    if (key >= 0) m += latch ? "   |   LATCHED" : "   |   HELD";
    if (proc.sustainHeld.load()) m += "   |   SUSTAIN";
    modeLabel.setText (m, juce::dontSendNotification);

    const float pk = proc.outputPeak.load();
    meterLevel = pk > meterLevel ? pk : meterLevel * 0.9f;
    repaint (keyLabel.getBounds().withHeight (10).withY (keyLabel.getBottom() + 14));

    juce::String st;
    const int lt = proc.learnTarget.load();
    const int zl = proc.zoneLearnStep.load();
    if (lt >= 0) st = "MIDI learn: move a controller or press a pedal for \"" + juce::String (kActionNames[lt]) + "\"";
    else if (zl == 1) st = "Zone learn: play the LOWEST drone key";
    else if (zl == 2) st = "Zone learn: now play the HIGHEST drone key";
    else st = juce::String (proc.activeVoices.load()) + " voices";
    statusLabel.setText (st, juce::dontSendNotification);

    for (auto* t : tabPages) t->tick();

    // Keep computer-keyboard control working: pull focus back from buttons/sliders.
    if (auto* f = juce::Component::getCurrentlyFocusedComponent())
        if (f != this && isParentOf (f) && dynamic_cast<juce::TextEditor*> (f) == nullptr && dynamic_cast<juce::ComboBox*> (f) == nullptr)
            grabKeyboardFocus();
}

bool CanopyEditor::keyPressed (const juce::KeyPress& key)
{
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    static const juce::String keyMap = "awsedftgyhuj";
    const int idx = keyMap.indexOfChar (c);
    if (idx >= 0) { proc.uiSetKey (idx); return true; }
    if (c == 'k') { proc.uiSetKey (0); return true; }
    if (key == juce::KeyPress::spaceKey) { proc.uiStop(); return true; }
    if (key == juce::KeyPress::escapeKey) { proc.uiPanic(); return true; }
    if (c == 'l') { proc.uiToggleLatch(); return true; }
    if (c == 'm') { proc.uiCycleMode(); return true; }
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey) { proc.nextItem(); return true; }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey) { proc.prevItem(); return true; }
    auto nudge = [this] (const juce::String& id, float d)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + d));
    };
    if (c == '[') { nudge (ID::swell, -0.05f); return true; }
    if (c == ']') { nudge (ID::swell, 0.05f); return true; }
    if (c == '-') { nudge (ID::master, -0.05f); return true; }
    if (c == '=' || c == '+') { nudge (ID::master, 0.05f); return true; }
    return false;
}
} // namespace canopy
