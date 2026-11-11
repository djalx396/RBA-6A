#include "PluginEditor.h"

namespace
{
    const juce::Colour bg (0xff2b2a26), panel (0xff3a3833), cream (0xffe9e1cc), amber (0xffd9a441), dim (0xff9a9384);
}

RbaEditor::RbaEditor (RbaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    auto& lf = getLookAndFeel();
    lf.setColour (juce::Slider::rotarySliderFillColourId, amber);
    lf.setColour (juce::Slider::rotarySliderOutlineColourId, panel.brighter (0.15f));
    lf.setColour (juce::Slider::thumbColourId, cream);
    lf.setColour (juce::Slider::textBoxTextColourId, cream);
    lf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lf.setColour (juce::Label::textColourId, cream);
    lf.setColour (juce::ComboBox::backgroundColourId, panel);
    lf.setColour (juce::ComboBox::textColourId, cream);
    lf.setColour (juce::ComboBox::outlineColourId, panel.brighter (0.2f));
    lf.setColour (juce::ToggleButton::textColourId, cream);
    lf.setColour (juce::ToggleButton::tickColourId, amber);

    addKnob (input, ID::input, "INPUT");
    addKnob (threshold, ID::threshold, "THRESHOLD");
    addKnob (attack, ID::attack, "ATTACK");
    addKnob (release, ID::release, "RELEASE");
    addKnob (filter, ID::filter, "FILTER");
    addKnob (makeup, ID::makeup, "MAKE UP");
    addKnob (debump, ID::debump, "DE-BUMPIFY");
    addKnob (balance, ID::balance, "BALANCE");
    addKnob (mix, ID::mix, "DRY / WET");

    addChoice (prepos, ID::prepos, "PREAMP");
    addChoice (link, ID::link, "STEREO DETECTOR");
    addChoice (msmode, ID::msmode, "MODE");
    addChoice (oslive, ID::oslive, "OVERSAMPLING");
    addChoice (osrender, ID::osrender, "RENDER OVERSAMPLING");
    addChoice (rendermode, ID::rendermode, "USE RENDER OS");

    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ID::power, power);

    status.setJustificationType (juce::Justification::centredLeft);
    status.setColour (juce::Label::textColourId, dim);
    addAndMakeVisible (status);

    const bool stereo = proc.getTotalNumOutputChannels() > 1;
    link.box.setEnabled (stereo);
    msmode.box.setEnabled (stereo);

    setSize (820, 470);
    startTimerHz (30);
}

RbaEditor::~RbaEditor() { stopTimer(); }

void RbaEditor::addKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.label);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void RbaEditor::addChoice (Choice& c, const juce::String& id, const juce::String& name)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box.addItemList (prm->choices, 1);
    addAndMakeVisible (c.box);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c.label.setColour (juce::Label::textColourId, dim);
    addAndMakeVisible (c.label);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c.box);
}

void RbaEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto r = getLocalBounds().reduced (12);
    auto head = r.removeFromTop (44);
    g.setColour (cream);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("RBA-6A", head.removeFromLeft (150), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("push-pull vari-mu limiting amplifier", head.removeFromLeft (300), juce::Justification::centredLeft);

    // gain reduction meter (0..20 dB, like the BA-6A's)
    auto m = meterArea.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (m, 4.f);
    const float frac = juce::jlimit (0.f, 1.f, grShown / 20.f);
    g.setColour (amber);
    g.fillRoundedRectangle (m.withWidth (m.getWidth() * frac).reduced (2.f), 3.f);
    g.setColour (cream);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("GR " + juce::String (grShown, 1) + " dB", meterArea.reduced (8, 0), juce::Justification::centredRight);
    g.setColour (dim);
    for (int db : { 0, 5, 10, 15, 20 })
    {
        const float x = m.getX() + m.getWidth() * (float) db / 20.f;
        g.drawVerticalLine ((int) x, m.getBottom() + 2.f, m.getBottom() + 6.f);
    }
}

void RbaEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (44);
    meterArea = r.removeFromTop (26).withTrimmedLeft (0);
    r.removeFromTop (14);

    auto placeRow = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> ks)
    {
        const int w = row.getWidth() / (int) ks.size();
        for (auto* k : ks)
        {
            auto cell = row.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell.reduced (6, 0));
        }
    };
    placeRow (r.removeFromTop (130), { &input, &threshold, &attack, &release, &filter, &makeup });
    r.removeFromTop (8);
    auto row2 = r.removeFromTop (130);
    placeRow (row2.removeFromLeft (row2.getWidth() / 2), { &debump, &balance, &mix });

    auto right = row2.reduced (10, 4);
    auto placeChoice = [] (juce::Rectangle<int>& area, Choice& c)
    {
        auto cell = area.removeFromTop (40);
        c.label.setBounds (cell.removeFromTop (16));
        c.box.setBounds (cell.removeFromTop (22));
    };
    auto colA = right.removeFromLeft (right.getWidth() / 2).reduced (4, 0);
    auto colB = right.reduced (4, 0);
    placeChoice (colA, prepos);
    placeChoice (colA, link);
    power.setBounds (colA.removeFromTop (28));
    placeChoice (colB, msmode);

    r.removeFromTop (10);
    auto bottom = r.removeFromTop (40);
    const int w = bottom.getWidth() / 4;
    for (auto* c : { &oslive, &osrender, &rendermode })
    {
        auto cell = bottom.removeFromLeft (w).reduced (4, 0);
        c->label.setBounds (cell.removeFromTop (16));
        c->box.setBounds (cell.removeFromTop (22));
    }
    status.setBounds (bottom.reduced (4, 0));
}

void RbaEditor::timerCallback()
{
    const float gr = proc.grMeter.load();
    grShown = gr > grShown ? gr : grShown + 0.25f * (gr - grShown);
    const int f = proc.activeFactor.load();
    status.setText (juce::String ("Running ") + (f > 1 ? juce::String (f) + "x" : juce::String ("1x (off)"))
                        + (proc.rendering.load() ? "  (rendering)" : "")
                        + "\nmax at this rate: " + juce::String (proc.maxFactorForRate()) + "x",
                    juce::dontSendNotification);
    repaint (meterArea.expanded (2, 8));
}
