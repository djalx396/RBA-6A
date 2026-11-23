#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg { 0xff171513 }, panel { 0xff221f1c }, border { 0xff3a352f }, cream { 0xffede6d6 },
                       label { 0xffcfc6b4 }, sub { 0xffa69c8e }, dim { 0xff968c7f }, off { 0xff6e665c },
                       accent { 0xffe8a33d }, face { 0xfff0e6cf }, ink { 0xff2a2420 }, knob { 0xff2c2825 },
                       knobEdge { 0xff57504a }, needle { 0xffb23a1e };
}

class RbaLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
};

// a row of buttons bound to a choice or bool parameter
class Segmented : public juce::Component, private juce::Timer
{
public:
    Segmented (juce::RangedAudioParameter& p, juce::StringArray labels, juce::Array<int> values = {});
    std::function<bool()> isEnabledFn;        // greyed out and inactive when false
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setVisibleCount (int n) { visibleCount = n; repaint(); }
private:
    void timerCallback() override { repaint(); }
    int current() const;
    juce::RangedAudioParameter& param; juce::StringArray labels; juce::Array<int> values; int visibleCount = -1;
};

// one button with an LED: lit = on. Bound to a bool or choice parameter (onIndex / offIndex).
class LitToggle : public juce::Component, private juce::Timer
{
public:
    LitToggle (juce::RangedAudioParameter& p, juce::String textOn, juce::String textOff, int onIndex = 1, int offIndex = 0);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const;
private:
    void timerCallback() override { repaint(); }
    juce::RangedAudioParameter& param; juce::String textOn, textOff; int onIndex, offIndex;
};

class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& title, const juce::String& scale, int size = 56);
    std::function<juce::String()> valueText;  // overrides the parameter's own text
    std::function<juce::String()> scaleFn;    // overrides the scale line
    std::function<bool()> showEnds;           // draw SLOW (left) and FAST (right) under the scale
    std::function<bool()> isEnabledFn;
    // hardware style: scale numbers around the dial, value and name underneath
    bool hardware = false;
    std::function<juce::StringArray()> ticksFn;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::RangedAudioParameter* param = nullptr;
    juce::String title, scale; int size;
};

class GrMeter : public juce::Component
{
public:
    void setValue (float db) { value = db; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float value = 0.f;
};

class RbaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RbaEditor (RbaProcessor&);
    ~RbaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void updateUnitVisibility();
    bool isRca() const { return juce::roundToInt (proc.apvts.getRawParameterValue (ID::unit)->load()) == 1; }
    juce::RangedAudioParameter& P (const juce::String& id) { return *proc.apvts.getParameter (id); }

    RbaProcessor& proc;
    RbaLook look;

    Segmented unitSeg, bumpSeg, modeSeg, rcaRelSeg, linesSeg, threeDSeg,
              osSeg, renderOsSeg, renderModeSeg, rangeSeg;
    LitToggle powerTog, detTog, clipTog, preTog;
    Knob drive, threshold, attack, release, filter, rcaFilter, balance, crush, clipMakeup, preDrive, makeup, mix;
    GrMeter gr;
    float grSmooth = 0.f, pkSmooth[2] { 0.f, 0.f };
    juce::Rectangle<int> secInput, secComp, secClip, secPre, footer, meterArea, statusArea, rcaRelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RbaEditor)
};
