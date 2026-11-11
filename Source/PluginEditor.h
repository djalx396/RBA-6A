#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class RbaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RbaEditor (RbaProcessor&);
    ~RbaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& name);
    void addChoice (Choice&, const juce::String& id, const juce::String& name);

    RbaProcessor& proc;
    Knob input, threshold, attack, release, filter, makeup, debump, balance, mix;
    Choice prepos, link, msmode, oslive, osrender, rendermode;
    juce::ToggleButton power { "Power" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    juce::Label status;
    float grShown = 0.f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RbaEditor)
};
