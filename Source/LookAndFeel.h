// Canopy Pads - ambient drone pads for church musicians
// Copyright (c) 2026 John Carl Tagbor (Tagbor Audio). MIT License.

#pragma once
#include <JuceHeader.h>

namespace canopy
{
namespace col
{
    const juce::Colour bg       (0xff11151b);
    const juce::Colour panel    (0xff1a2029);
    const juce::Colour panel2   (0xff222a35);
    const juce::Colour line     (0xff2e3845);
    const juce::Colour text     (0xffece7dd);
    const juce::Colour muted    (0xff8a94a3);
    const juce::Colour gold     (0xffe2a85c);
    const juce::Colour sage     (0xff7fb8a4);
    const juce::Colour danger   (0xffd9644a);
}

class CanopyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CanopyLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, col::gold);
        setColour (juce::Slider::rotarySliderOutlineColourId, col::line);
        setColour (juce::Slider::trackColourId, col::gold);
        setColour (juce::Slider::backgroundColourId, col::line);
        setColour (juce::Slider::thumbColourId, col::text);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::buttonOnColourId, col::gold);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::muted);
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::gold);
        setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
        setColour (juce::ToggleButton::textColourId, col::text);
        setColour (juce::ToggleButton::tickColourId, col::gold);
        setColour (juce::ToggleButton::tickDisabledColourId, col::muted);
        setColour (juce::ListBox::backgroundColourId, col::panel);
        setColour (juce::ListBox::outlineColourId, col::line);
        setColour (juce::TextEditor::backgroundColourId, col::panel2);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::gold);
        setColour (juce::TextEditor::highlightColourId, col::gold.withAlpha (0.4f));
        setColour (juce::TabbedComponent::backgroundColourId, col::bg);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::tabTextColourId, col::muted);
        setColour (juce::TabbedButtonBar::frontTextColourId, col::gold);
        setColour (juce::ScrollBar::thumbColourId, col::line);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::AlertWindow::outlineColourId, col::line);
        setColour (juce::ProgressBar::backgroundColourId, col::line);
        setColour (juce::ProgressBar::foregroundColourId, col::gold);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float lw = juce::jmax (3.0f, radius * 0.16f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - lw / 2, radius - lw / 2, 0.0f, startAngle, endAngle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, radius - lw / 2, radius - lw / 2, 0.0f, startAngle, angle, true);
        g.setColour (s.isEnabled() ? s.findColour (juce::Slider::rotarySliderFillColourId) : col::muted);
        g.strokePath (fill, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto tip = centre.getPointOnCircumference (radius - lw * 1.8f, angle);
        g.setColour (col::text);
        g.fillEllipse (juce::Rectangle<float> (lw * 0.9f, lw * 0.9f).withCentre (tip));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const bool horiz = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
        const float thick = horiz ? juce::jmin ((float) h * 0.5f, 14.0f) : juce::jmin ((float) w * 0.5f, 14.0f);
        juce::Rectangle<float> track = horiz ? juce::Rectangle<float> ((float) x, (float) y + h / 2.0f - thick / 2, (float) w, thick)
                                             : juce::Rectangle<float> ((float) x + w / 2.0f - thick / 2, (float) y, thick, (float) h);
        g.setColour (s.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (track, thick / 2);
        juce::Rectangle<float> fill = horiz ? track.withWidth (sliderPos - (float) x)
                                            : track.withTop (sliderPos);
        g.setColour (s.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (fill, thick / 2);
        const float r = thick * 0.95f;
        juce::Point<float> c = horiz ? juce::Point<float> (sliderPos, track.getCentreY()) : juce::Point<float> (track.getCentreX(), sliderPos);
        g.setColour (col::text);
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgIn, bool over, bool down) override
    {
        auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
        auto c = bgIn;
        if (down) c = c.brighter (0.15f); else if (over) c = c.brighter (0.07f);
        g.setColour (c);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (b.getToggleState() ? col::gold.brighter (0.2f) : col::line);
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions ((float) juce::jmin (16, h / 2 + 2), juce::Font::bold));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path p;
        const float ax = (float) w - 16.0f, ay = (float) h / 2.0f;
        p.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }

    void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool front = b.isFrontTab();
        g.setColour (front ? col::panel2 : (over ? col::panel.brighter (0.05f) : col::bg));
        g.fillRoundedRectangle (r.reduced (2.0f, 3.0f), 7.0f);
        g.setColour (front ? col::gold : col::muted);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
    }

    int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 120; }
};
} // namespace canopy
