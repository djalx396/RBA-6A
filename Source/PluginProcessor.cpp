#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UnitElRey.h"
#include "UnitRca.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;

juce::AudioProcessorValueTreeState::ParameterLayout RbaProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto db  = [] (float x, int) { return juce::String (x, 1) + " dB"; };
    auto pc  = [] (float x, int) { return juce::String (juce::roundToInt (x)) + " %"; };
    auto one = [] (float x, int) { return juce::String (x, 1); };
    auto f = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float step, float def,
                  std::function<juce::String (float, int)> fn)
    {
        v.push_back (std::make_unique<APF> (juce::ParameterID { id, 2 }, name, juce::NormalisableRange<float> (lo, hi, step), def,
                                            juce::AudioParameterFloatAttributes().withStringFromValueFunction (fn)));
    };
    auto c = [&] (const juce::String& id, const juce::String& name, juce::StringArray items, int def, bool autom = true)
    {
        v.push_back (std::make_unique<APC> (juce::ParameterID { id, 2 }, name, items, def,
                                            juce::AudioParameterChoiceAttributes().withAutomatable (autom)));
    };
    auto b = [&] (const juce::String& id, const juce::String& name, bool def)
    {
        v.push_back (std::make_unique<APB> (juce::ParameterID { id, 2 }, name, def));
    };

    c (ID::unit, "Unit", { "El Rey", "Vintage RCA" }, 0);
    b (ID::power, "Power", true);
    f (ID::input, "Drive", -18.f, 18.f, 0.01f, 0.f, db);
    c (ID::bump, "Bump", { "Off", "Pre", "Post" }, 1);
    f (ID::threshold, "Threshold", 0.f, 40.f, 0.01f, 20.f, one);
    f (ID::attack, "Attack (El Rey)", 1.f, 3.f, 0.01f, 2.f, [] (float x, int) { return juce::String (x, 2); });
    f (ID::release, "Release (El Rey)", 1.f, 5.f, 0.01f, 3.f, [] (float x, int) { return juce::String (x, 2); });
    c (ID::rcarel, "Release (RCA)", { "Single", "Dual" }, 0);
    c (ID::rcahot, "Range (RCA)", { "Normal", "+8 dB" }, 0);
    f (ID::filter, "SC Filter (El Rey)", 0.f, 250.f, 0.1f, 0.f,
       [] (float x, int) { return x < 1.f ? juce::String ("Off") : juce::String (juce::roundToInt (x)) + " Hz"; });
    f (ID::rcafilter, "SC Filter (RCA)", 0.f, 500.f, 0.1f, 0.f,
       [] (float x, int) { return x < 100.f ? juce::String ("Off") : juce::String (juce::roundToInt (x)) + " Hz"; });
    f (ID::balance, "P-P Balance", -100.f, 100.f, 0.1f, 0.f, pc);
    c (ID::link, "Detector", { "Linked", "Dual mono" }, 0);
    c (ID::msmode, "Mode", { "L/R", "M/S" }, 0);
    b (ID::clip, "Clipper", false);
    f (ID::crush, "Crush", 0.f, 40.f, 0.01f, 25.f, one);
    f (ID::clipmakeup, "Clip Make Up", 0.f, 10.f, 0.01f, 0.f, db);
    b (ID::pre, "Preamp", false);
    f (ID::predrive, "Preamp Drive", -30.f, 30.f, 0.01f, 0.f, one);
    c (ID::prelines, "Preamp Lines", { "Mono", "Stereo" }, 1);
    b (ID::threed, "3D Bump", false);
    f (ID::makeup, "Make Up", 0.f, 24.f, 0.01f, 0.f, db);
    f (ID::mix, "Dry/Wet", 0.f, 100.f, 0.1f, 100.f, pc);
    c (ID::oslive, "Oversampling", { "Off", "2x", "4x", "8x" }, 1, false);
    c (ID::osrender, "Render oversampling", { "Off", "2x", "4x", "8x" }, 2, false);
    c (ID::rendermode, "Use render oversampling", { "Auto (when rendering)", "Always", "Never" }, 0, false);
    return { v.begin(), v.end() };
}

RbaProcessor::RbaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RBA6A", createLayout())
{
    const juce::String ids[pCount] = { ID::unit, ID::power, ID::input, ID::bump, ID::threshold, ID::attack, ID::release,
        ID::rcarel, ID::filter, ID::rcafilter, ID::balance, ID::link, ID::msmode, ID::clip, ID::crush, ID::clipmakeup,
        ID::pre, ID::predrive, ID::prelines, ID::threed, ID::makeup, ID::mix, ID::oslive, ID::osrender, ID::rendermode, ID::rcahot };
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
    s.debump    = 0;
    s.makeupDb  = p[pMakeup]->load();
    s.mix       = p[pMix]->load() / 100.0;
    s.power     = p[pPower]->load() > 0.5f;
    s.preInside = false;
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
    const auto rca = rba::makeRca();
    const auto clipU = rba::makeClip();
    const auto preU = rba::makePre();
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
        const double fo = fs * set.factor;
        for (int c = 0; c < 2; ++c)
        {
            set.ch[c].prepare (unit, fs, set.factor);
            set.ch[c].update (s);
            set.det[c].prepare (unit, fo);
            set.det[c].update (s, true);
            set.rca[c].prepare (rca, fo);
            set.rdet[c].prepare (rca, fo);
            set.post[c].prepare (unit, fo, false);
            set.clip[c].prepare (clipU, fo);
            set.pre[c].prepare (preU);
        }
        fixedLatency = juce::jmax (fixedLatency, set.latency);
    }
    preEq[0].prepare (preU.L, fs); preEq[1].prepare (preU.R, fs);
    lastStereoLines = false;
    for (int c = 0; c < 2; ++c)
    {
        preDel[c].set (1.0); preDel[c].reset();
        dryDelay[c].prepare (fixedLatency + 2);
        wetDelay[c].prepare (fixedLatency + 2);
    }
    fixedLatency += 1;                          // the preamp line delay: 1 sample (+ a fraction on the left line in Stereo)
    dryBuf.setSize (2, samplesPerBlock);
    makeupSm.reset (fs, 0.02); makeupSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) s.makeupDb));
    mixSm.reset (fs, 0.02);    mixSm.setCurrentAndTargetValue ((float) s.mix);
    setLatencySamples (fixedLatency);           // same in every oversampling mode
    latencyNow = fixedLatency;

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

    const bool offline = isNonRealtime();
    rendering = offline;
    const int f = wantedFactor (offline);
    if (f != kFactors[activeSet])
    {
        for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
        auto& set = sets[(size_t) activeSet];
        if (set.os) set.os->reset();
        for (int c = 0; c < 2; ++c)
        {
            set.ch[c].reset(); set.det[c].reset(); set.rca[c].reset(); set.rdet[c].reset();
            set.post[c].reset(); set.clip[c].reset(); wetDelay[c].clear();
        }
    }
    activeFactor = f;
    auto& set = sets[(size_t) activeSet];

    // ---- settings
    const auto s = readSettings();
    const bool rcaUnit  = juce::roundToInt (p[pUnit]->load()) == 1;
    const int  bump     = juce::roundToInt (p[pBump]->load());          // 0 Off, 1 Pre, 2 Post
    const bool linked   = nc == 2 && juce::roundToInt (p[pLink]->load()) == 0;
    const bool ms       = nc == 2 && juce::roundToInt (p[pMs]->load()) == 1;
    const bool clipOn   = p[pClip]->load() > 0.5f;
    const bool preOn    = p[pPre]->load() > 0.5f;
    const bool stereoLn = nc == 2 && juce::roundToInt (p[pPreLines]->load()) == 1;
    const bool d3       = nc == 2 && preOn && stereoLn && bump != 0 && p[p3D]->load() > 0.5f;
    const bool dual     = juce::roundToInt (p[pRcaRel]->load()) == 1;
    const double rcaFilt = p[pRcaFilter]->load() >= 100.f ? p[pRcaFilter]->load() : 0.0;
    const auto rcaU = rba::makeRca();
    // matched to the El Rey unit; Range +8 dB drives the limiter 8 dB harder (the v0.2 range at the top of the knob)
    const double rcaGdb = rba::rcaInputGainDb (s.threshold) + (juce::roundToInt (p[pRcaHot]->load()) == 1 ? 8.0 : 0.0);
    const double rcaG = std::pow (10.0, rcaGdb / 20.0), rcaInv = 1.0 / rcaG;
    const double preDriveDb = p[pPreDrive]->load();
    const auto preU = rba::makePre();

    for (int c = 0; c < nc; ++c)
    {
        set.ch[c].update (s); set.det[c].update (s);
        set.ch[c].set3D (d3 && c == 1);
        set.post[c].set3D (d3 && c == 1);
        set.rdet[c].update (dual, s.power, rcaFilt);
        set.clip[c].update (p[pCrush]->load(), p[pClipMk]->load());
        set.pre[c].update (preDriveDb, (stereoLn && c == 1) ? preU.R.extraDb : 0.0);
    }
    const bool lineDiff = preOn && stereoLn;
    if (lineDiff != lastStereoLines)
    {
        // Preamp Stereo: the right line leads the left by about 2.2 us -> delay the left a little more
        const double lead = preU.rLeadUs * 1e-6 * fs;
        preDel[0].set (lineDiff ? 1.0 + lead : 1.0); preDel[1].set (1.0);
        preEq[1].prepare (lineDiff ? preU.R : preU.L, fs);
        lastStereoLines = lineDiff;
    }
    makeupSm.setTargetValue (juce::Decibels::decibelsToGain ((float) s.makeupDb));
    mixSm.setTargetValue ((float) s.mix);

    for (int c = 0; c < nc; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    // ---- base rate: input EQ (bump path for Bump = Pre, else the flat path) and Drive gain
    for (int c = 0; c < nc; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) d[i] = (float) set.ch[c].frontMode (d[i], bump == 1 ? 1 : 0);
    }

    // ---- oversampled: drive, compressor (M/S optional), post bump, clipper, preamp drive
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
    juce::dsp::AudioBlock<float> up = set.os ? set.os->processSamplesUp (block) : block;
    const int nu = (int) up.getNumSamples();
    float grMax = 0.f;
    float* ch[2] = { up.getChannelPointer (0), nc > 1 ? up.getChannelPointer (1) : nullptr };
    const double gIn = set.ch[0].inputGain();
    for (int i = 0; i < nu; ++i)
    {
        double x[2] = { ch[0][i], nc > 1 ? ch[1][i] : 0.0 };
        if (ms) { const double m = 0.5 * (x[0] + x[1]), sd = 0.5 * (x[0] - x[1]); x[0] = m; x[1] = sd; }
        double y[2] = { 0, 0 };
        if (! rcaUnit)
        {
            if (linked)
            {
                const auto& d = set.det[0];
                for (int c = 0; c < nc; ++c) y[c] = set.ch[c].pushPull (x[c], d.gain(), d.gainReduction(), d.level());
                set.det[0].feed (y, nc);
                grMax = juce::jmax (grMax, (float) set.det[0].gainReduction());
            }
            else
                for (int c = 0; c < nc; ++c)
                {
                    auto& d = set.det[c];
                    y[c] = set.ch[c].pushPull (x[c], d.gain(), d.gainReduction(), d.level());
                    d.feed (&y[c], 1);
                    grMax = juce::jmax (grMax, (float) d.gainReduction());
                }
            for (int c = 0; c < nc; ++c) y[c] /= gIn;
        }
        else
        {
            double u[2];
            for (int c = 0; c < nc; ++c) u[c] = set.ch[c].driveOnly (x[c]) * rcaG;
            if (linked)
            {
                const auto& d = set.rdet[0];
                for (int c = 0; c < nc; ++c) y[c] = set.rca[c].process (u[c], d.gain(), d.gainReduction(), d.level());
                set.rdet[0].feed (y, nc);
                grMax = juce::jmax (grMax, (float) set.rdet[0].gainReduction());
            }
            else
                for (int c = 0; c < nc; ++c)
                {
                    auto& d = set.rdet[c];
                    y[c] = set.rca[c].process (u[c], d.gain(), d.gainReduction(), d.level());
                    d.feed (&y[c], 1);
                    grMax = juce::jmax (grMax, (float) d.gainReduction());
                }
            for (int c = 0; c < nc; ++c) y[c] *= rcaInv / gIn;
        }
        if (ms) { const double m = y[0], sd = y[1]; y[0] = m + sd; y[1] = m - sd; }
        for (int c = 0; c < nc; ++c)
        {
            double v = y[c];
            if (bump == 2) v = set.post[c].process (v);
            if (clipOn)    v = set.clip[c].process (v);
            if (preOn)     v = set.pre[c].process (v);
            ch[c][i] = (float) v;
        }
    }
    if (set.os) set.os->processSamplesDown (block);

    // ---- base rate: preamp EQ + line delay, latency alignment, Make Up, Dry/Wet, meters
    const int wetPad = fixedLatency - 1 - set.latency;
    float pk[2] = { 0.f, 0.f };
    for (int i = 0; i < n; ++i)
    {
        const float mk = makeupSm.getNextValue(), mx = mixSm.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            double v = d[i];
            if (preOn) v = preEq[c].process (v);
            v = preDel[c].process (v);
            const float wet = wetDelay[c].process ((float) v, wetPad) * mk;
            const float dry = dryDelay[c].process (dryBuf.getSample (c, i), fixedLatency);
            d[i] = mx * wet + (1.f - mx) * dry;
            pk[c] = juce::jmax (pk[c], std::abs (d[i]));
        }
    }
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    grMeter = grMax;
    outPeak[0] = pk[0]; outPeak[1] = nc > 1 ? pk[1] : pk[0];
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
