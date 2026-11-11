#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UnitElRey.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;

enum PIdx { pInput, pThreshold, pAttack, pRelease, pFilter, pDebump, pMakeup, pMix, pPower, pPrepos, pBalance,
            pLink, pMs, pOsLive, pOsRender, pRenderMode, pCount };

juce::AudioProcessorValueTreeState::ParameterLayout RbaProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto db = [] (float x, int) { return juce::String (x, 1) + " dB"; };
    auto pc = [] (float x, int) { return juce::String (juce::roundToInt (x)) + " %"; };

    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::input, 1 }, "Input",
        juce::NormalisableRange<float> (-18.f, 18.f, 0.01f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::threshold, 1 }, "Threshold",
        juce::NormalisableRange<float> (0.f, 40.f, 0.01f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return juce::String (x, 1); })));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::attack, 1 }, "Attack",
        juce::NormalisableRange<float> (1.f, 3.f, 0.01f), 2.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return juce::String (x, 2) + (x < 1.5f ? " (slow)" : x > 2.5f ? " (fast)" : ""); })));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::release, 1 }, "Release",
        juce::NormalisableRange<float> (1.f, 5.f, 0.01f), 3.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return juce::String (x, 2) + (x < 1.5f ? " (slow)" : x > 4.5f ? " (fast)" : ""); })));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::filter, 1 }, "Filter",
        juce::NormalisableRange<float> (0.f, 250.f, 0.1f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return x < 1.f ? juce::String ("Off") : juce::String (juce::roundToInt (x)) + " Hz"; })));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::debump, 1 }, "De-Bumpify",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pc)));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::makeup, 1 }, "Make Up",
        juce::NormalisableRange<float> (0.f, 24.f, 0.01f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::mix, 1 }, "Dry/Wet",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pc)));
    v.push_back (std::make_unique<APB> (juce::ParameterID { ID::power, 1 }, "Power", true));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::prepos, 1 }, "Preamp position",
        juce::StringArray { "Before push-pull", "Inside push-pull" }, 0));
    v.push_back (std::make_unique<APF> (juce::ParameterID { ID::balance, 1 }, "Balance",
        juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pc)));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::link, 1 }, "Stereo detector",
        juce::StringArray { "Linked", "Dual mono" }, 0));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::msmode, 1 }, "Mode",
        juce::StringArray { "L/R", "M/S" }, 0));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::oslive, 1 }, "Oversampling",
        juce::StringArray { "Off", "2x", "4x", "8x" }, 1, juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::osrender, 1 }, "Render oversampling",
        juce::StringArray { "Off", "2x", "4x", "8x" }, 3, juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    v.push_back (std::make_unique<APC> (juce::ParameterID { ID::rendermode, 1 }, "Use render oversampling",
        juce::StringArray { "Auto (when rendering)", "Always", "Never" }, 0, juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    return { v.begin(), v.end() };
}

RbaProcessor::RbaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RBA6A", createLayout())
{
    const juce::String ids[pCount] = { ID::input, ID::threshold, ID::attack, ID::release, ID::filter, ID::debump, ID::makeup,
                                       ID::mix, ID::power, ID::prepos, ID::balance, ID::link, ID::msmode, ID::oslive,
                                       ID::osrender, ID::rendermode };
    for (int i = 0; i < pCount; ++i) p[i] = apvts.getRawParameterValue (ids[i]);
}

bool RbaProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

rba::Settings RbaProcessor::readSettings() const
{
    rba::Settings s;
    s.inputDb   = p[pInput]->load();
    s.threshold = p[pThreshold]->load();
    s.attack    = p[pAttack]->load();
    s.release   = p[pRelease]->load();
    s.filterHz  = p[pFilter]->load();
    s.debump    = p[pDebump]->load() / 100.0;
    s.makeupDb  = p[pMakeup]->load();
    s.mix       = p[pMix]->load() / 100.0;
    s.power     = p[pPower]->load() > 0.5f;
    s.preInside = juce::roundToInt (p[pPrepos]->load()) == 1;
    s.balance   = p[pBalance]->load() / 100.0;
    return s;
}

int RbaProcessor::wantedFactor (bool offline) const
{
    const int mode = juce::roundToInt (p[pRenderMode]->load());     // 0 Auto, 1 Always, 2 Never
    const bool useRender = mode == 1 || (mode == 0 && offline);
    const int idx = juce::roundToInt ((useRender ? p[pOsRender] : p[pOsLive])->load());
    return juce::jmin (kFactors[juce::jlimit (0, 3, idx)], maxFactor);
}

void RbaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    numCh = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));
    maxFactor = fs < 60000 ? 8 : (fs < 120000 ? 4 : 2);      // 2/4/8x at 44.1/48, 2/4x at 88.2/96, 2x at 176.4/192
    const auto unit = rba::makeElRey();
    const auto s = readSettings();

    fixedLatency = 0;
    for (int i = 0; i < 4; ++i)
    {
        auto& set = sets[(size_t) i];
        set.factor = kFactors[i];
        set.os.reset();
        set.latency = 0;
        if (set.factor > maxFactor) continue;
        if (set.factor > 1)
        {
            set.os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, (size_t) (i),
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            set.os->initProcessing ((size_t) samplesPerBlock);
            set.latency = juce::roundToInt (set.os->getLatencyInSamples());
        }
        for (int c = 0; c < 2; ++c)
        {
            set.ch[c].prepare (unit, fs, set.factor);
            set.ch[c].update (s);
            set.det[c].prepare (unit, fs * set.factor);
            set.det[c].update (s, true);
        }
        fixedLatency = juce::jmax (fixedLatency, set.latency);
    }
    for (int c = 0; c < 2; ++c)
    {
        dryDelay[c].prepare (fixedLatency + 1);
        wetDelay[c].prepare (fixedLatency + 1);
    }
    dryBuf.setSize (2, samplesPerBlock);
    makeupSm.reset (fs, 0.02); makeupSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) s.makeupDb));
    mixSm.reset (fs, 0.02);    mixSm.setCurrentAndTargetValue ((float) s.mix);
    setLatencySamples (fixedLatency);     // same in every oversampling mode

    const int f = wantedFactor (isNonRealtime());
    for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
    activeFactor = f;
}

void RbaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (numCh, buffer.getNumChannels());
    if (n == 0 || nc == 0) return;

    // oversampling choice (render detection); switching resets the chosen set's state
    const bool offline = isNonRealtime();
    rendering = offline;
    const int f = wantedFactor (offline);
    if (f != kFactors[activeSet])
    {
        for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
        auto& set = sets[(size_t) activeSet];
        if (set.os) set.os->reset();
        for (int c = 0; c < 2; ++c) { set.ch[c].reset(); set.det[c].reset(); }
        for (int c = 0; c < 2; ++c) wetDelay[c].clear();
    }
    activeFactor = f;
    auto& set = sets[(size_t) activeSet];

    const auto s = readSettings();
    const bool linked = nc == 2 && juce::roundToInt (p[pLink]->load()) == 0;
    const bool ms = nc == 2 && juce::roundToInt (p[pMs]->load()) == 1;
    for (int c = 0; c < nc; ++c) { set.ch[c].update (s); set.det[c].update (s); }
    makeupSm.setTargetValue (juce::Decibels::decibelsToGain ((float) s.makeupDb));
    mixSm.setTargetValue ((float) s.mix);

    // dry copy
    for (int c = 0; c < nc; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    // base rate: preamp EQ / colour, De-Bumpify, Input gain; then M/S encode if wanted
    for (int c = 0; c < nc; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) d[i] = (float) set.ch[c].front (d[i]);
    }
    if (ms)
    {
        auto* l = buffer.getWritePointer (0); auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]); l[i] = m; r[i] = sd; }
    }

    // push-pull section at the oversampled rate
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
    juce::dsp::AudioBlock<float> up = set.os ? set.os->processSamplesUp (block) : block;
    const int nu = (int) up.getNumSamples();
    float grMax = 0.f;
    float* ch[2] = { up.getChannelPointer (0), nc > 1 ? up.getChannelPointer (1) : nullptr };
    const double gIn = set.ch[0].inputGain();
    for (int i = 0; i < nu; ++i)
    {
        double y[2];
        if (linked)
        {
            const auto& d = set.det[0];
            for (int c = 0; c < nc; ++c) y[c] = set.ch[c].pushPull (ch[c][i], d.gain(), d.gainReduction(), d.level());
            set.det[0].feed (y, nc);
            grMax = juce::jmax (grMax, (float) set.det[0].gainReduction());
        }
        else
        {
            for (int c = 0; c < nc; ++c)
            {
                auto& d = set.det[c];
                y[c] = set.ch[c].pushPull (ch[c][i], d.gain(), d.gainReduction(), d.level());
                d.feed (&y[c], 1);
                grMax = juce::jmax (grMax, (float) d.gainReduction());
            }
        }
        for (int c = 0; c < nc; ++c) ch[c][i] = (float) (y[c] / gIn);
    }
    if (set.os) set.os->processSamplesDown (block);

    // M/S decode, latency alignment, Make Up, Dry/Wet
    if (ms)
    {
        auto* l = buffer.getWritePointer (0); auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i) { const float m = l[i], sd = r[i]; l[i] = m + sd; r[i] = m - sd; }
    }
    const int wetPad = fixedLatency - set.latency;
    for (int i = 0; i < n; ++i)
    {
        const float mk = makeupSm.getNextValue(), mx = mixSm.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float wet = wetDelay[c].process (d[i], wetPad) * mk;
            const float dry = dryDelay[c].process (dryBuf.getSample (c, i), fixedLatency);
            d[i] = mx * wet + (1.f - mx) * dry;
        }
    }
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    grMeter = grMax;
}

void RbaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RbaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RbaProcessor::createEditor() { return new RbaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RbaProcessor(); }
