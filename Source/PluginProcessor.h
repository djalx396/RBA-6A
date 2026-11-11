#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "RbaCore.h"

namespace ID
{
    static const juce::String input { "input" }, threshold { "threshold" }, attack { "attack" }, release { "release" },
                              filter { "filter" }, debump { "debump" }, makeup { "makeup" }, mix { "mix" },
                              power { "power" }, prepos { "prepos" }, balance { "balance" }, link { "link" },
                              msmode { "msmode" }, oslive { "oslive" }, osrender { "osrender" }, rendermode { "rendermode" };
}

class RbaProcessor : public juce::AudioProcessor
{
public:
    RbaProcessor();
    ~RbaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RBA-6A"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // for the editor
    std::atomic<float> grMeter { 0.f };        // dB, max over the last block
    std::atomic<int>   activeFactor { 1 };     // oversampling factor in use
    std::atomic<bool>  rendering { false };
    int maxFactorForRate() const { return maxFactor; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    rba::Settings readSettings() const;
    int  wantedFactor (bool offline) const;

    static constexpr int kFactors[4] = { 1, 2, 4, 8 };
    struct OsSet
    {
        int factor = 1;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;    // null for factor 1
        int latency = 0;                                        // base-rate samples
        rba::Channel ch[2];
        rba::Detector det[2];
    };
    std::array<OsSet, 4> sets;
    int activeSet = 0, maxFactor = 8, fixedLatency = 0, numCh = 2;
    double fs = 48000;

    // fixed-latency alignment: wet gets (fixedLatency - set latency), dry gets fixedLatency
    struct Delay
    {
        std::vector<float> buf; int w = 0;
        void prepare (int maxDelay) { buf.assign ((size_t) maxDelay + 1, 0.f); w = 0; }
        inline float process (float x, int d) noexcept
        {
            if (d <= 0 || buf.empty()) return x;
            buf[(size_t) w] = x;
            int r = w - d; if (r < 0) r += (int) buf.size();
            w = (w + 1) % (int) buf.size();
            return buf[(size_t) r];
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    };
    Delay dryDelay[2], wetDelay[2];
    juce::AudioBuffer<float> dryBuf;
    juce::SmoothedValue<float> makeupSm, mixSm;

    std::atomic<float>* p[16] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RbaProcessor)
};
