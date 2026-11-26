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

    c (ID::unit, "Unit", { "Modern", "Vintage" }, 0);
    b (ID::power, "Power", true);
    f (ID::input, "Drive", -18.f, 18.f, 0.01f, 0.f, db);
    c (ID::bump, "Bump", { "Off", "Pre", "Post" }, 1);
    f (ID::threshold, "Threshold", 0.f, 40.f, 0.01f, 20.f, one);
    f (ID::attack, "Attack (Modern)", 1.f, 3.f, 0.01f, 2.f,
       [] (float x, int) { return juce::String (x, 2) + (x < 1.25f ? " slow" : (x > 2.75f ? " fast" : "")); });
    f (ID::release, "Release (Modern)", 1.f, 5.f, 0.01f, 3.f,
       [] (float x, int) { return juce::String (x, 2) + (x < 1.5f ? " slow" : (x > 4.5f ? " fast" : "")); });
    c (ID::rcarel, "Release (Vintage)", { "Single", "Dual" }, 0);
    c (ID::rcahot, "Range", { "Normal", "+9 dB" }, 0);
    f (ID::filter, "SC Filter (Modern)", 0.f, 250.f, 0.1f, 0.f,
       [] (float x, int) { return x < 1.f ? juce::String ("Off") : juce::String (juce::roundToInt (x)) + " Hz"; });
    f (ID::rcafilter, "SC Filter (Vintage)", 0.f, 500.f, 0.1f, 0.f,
       [] (float x, int) { return x < 100.f ? juce::String ("Off") : juce::String (juce::roundToInt (x)) + " Hz"; });
    f (ID::balance, "P-P Balance", -100.f, 100.f, 0.1f, 0.f, pc);
    c (ID::link, "Detector", { "Linked", "Dual mono" }, 0);
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
    c (ID::bandlimit, "Filter", { "OFF", "CLEAN", "AIR" }, 1, false);
    c (ID::phase, "Filter Phase", { "ANALOG", "LINEAR" }, 0, false);
    c (ID::meter, "Output Meter", { "Peak", "Average" }, 0, false);
    return { v.begin(), v.end() };
}

RbaProcessor::RbaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RBA6A", createLayout())
{
    const juce::String ids[pCount] = { ID::unit, ID::power, ID::input, ID::bump, ID::threshold, ID::attack, ID::release,
        ID::rcarel, ID::filter, ID::rcafilter, ID::balance, ID::link, ID::clip, ID::crush, ID::clipmakeup,
        ID::pre, ID::predrive, ID::prelines, ID::threed, ID::makeup, ID::mix, ID::oslive, ID::osrender, ID::rendermode, ID::rcahot,
        ID::bandlimit, ID::phase, ID::meter };
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
    s.threshold = p[pThreshold]->load()                                   // Range +9: 9 dB more compression (Modern)
                + (juce::roundToInt (p[pRcaHot]->load()) == 1 ? 9.0 : 0.0);
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

// band-limit layouts: slot 0 = D (before the compressor), 1 = C (after it), 2 = B (after the Clipper), 3 = A (after the Preamp)
// CLEAN: a complete 25 kHz filter (Airwindows Hypersonic, 14th order) after each stage.
// AIR:   an 8th-order 25 kHz Butterworth split in four (D, C, B, A from gentlest to most resonant), then a Hypersonic at the end.
static const char* const* layoutSpecs (int mode)
{
    static const char* const clean[4] = { "", "HYP", "HYP", "HYP" };
    static const char* const air[4]   = { "B4:D", "B4:C", "B4:B", "B4:A HYP" };
    return mode == 1 ? clean : (mode == 2 ? air : nullptr);
}

// impulse response (session rate) of the linear part the output alignment corrects:
// up-sampling -> every band-limit section of the mode in series -> down-sampling
void RbaProcessor::measureLinear (OsSet& set, int mode, std::vector<double>& h)
{
    const double fo = fs * set.factor;
    fbl::BandLimit<4> bl;
    if (mode > 0 && fo >= 88000.0) bl.configure (layoutSpecs (mode), fo); else bl.off();
    int len = 4096; while (len < (int) std::ceil (0.04 * fs)) len <<= 1;      // >= 40 ms
    h.assign ((size_t) len, 0.0);
    juce::AudioBuffer<float> tmp (numCh, blockSize);
    if (set.os) set.os->reset();
    for (int pos = 0; pos < len; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, len - pos);
        tmp.clear();
        if (pos == 0) tmp.setSample (0, 0, 1.0f);
        juce::dsp::AudioBlock<float> block (tmp.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        juce::dsp::AudioBlock<float> up = set.os ? set.os->processSamplesUp (block) : block;
        float* u = up.getChannelPointer (0);
        for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = (float) bl.processDry (u[i]);
        if (set.os) set.os->processSamplesDown (block);
        for (int i = 0; i < n; ++i) h[(size_t) (pos + i)] = tmp.getSample (0, i);
    }
    if (set.os) set.os->reset();
}

void RbaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    blockSize = juce::jmax (1, samplesPerBlock);
    numCh = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));
    maxFactor = fs < 60000 ? 8 : (fs < 120000 ? 4 : 2);      // 2/4/8x at 44.1/48, 2/4x at 88.2/96, 2x at 176.4/192
    const auto unit = rba::makeElRey();
    const auto rca = rba::makeRca();
    const auto clipU = rba::makeClip();
    const auto preU = rba::makePre();
    const auto s = readSettings();

    std::vector<double> hs[4][3];
    for (int i = 0; i < 4; ++i)
    {
        auto& set = sets[(size_t) i];
        set.factor = kFactors[i];
        set.os.reset(); set.dryOs.reset();
        set.latency = 0;
        if (set.factor > maxFactor) continue;
        if (set.factor > 1)
        {
            for (auto* o : { &set.os, &set.dryOs })
            {
                *o = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, (size_t) (i),
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
                (*o)->initProcessing ((size_t) blockSize);
            }
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
            for (int m = 0; m < 3; ++m)
            {
                for (auto* b : { &set.bl[m][c], &set.dbl[m][c] })
                {
                    if (m > 0 && fo >= 88000.0) b->configure (layoutSpecs (m), fo); else b->off();
                    b->reset();
                }
            }
        }
        for (int m = 0; m < 3; ++m) { measureLinear (set, m, hs[i][m]); set.tau[m] = pa::dcDelay (hs[i][m]); }
    }
    // ---- output alignment: the same total latency in every OVERSAMPLE x FILTER setting
    // ANALOG: a fractional delay lands the low-frequency delay on whole samples (88.2 kHz and up; below: rounded)
    // LINEAR: a correction FIR removes the filters' phase (1 ms before the main tap, 0.3 ms after)
    const int dfd = fs >= 60000.0 ? 8 : 0;
    const int P = (int) std::lround (1.0e-3 * fs), R = (int) std::lround (0.3e-3 * fs);
    int Ta = 0, Tl = 0;
    for (int i = 0; i < 4; ++i)
        for (int m = 0; m < 3; ++m)
        {
            if (sets[(size_t) i].factor > maxFactor) continue;
            const double t = sets[(size_t) i].tau[m];
            Ta = juce::jmax (Ta, dfd > 0 ? (int) std::ceil (t - 1e-9) + dfd : (int) std::lround (t));
            Tl = juce::jmax (Tl, P + (int) std::ceil (t - 1e-9));
        }
    int maxLen = 1;
    for (int i = 0; i < 4; ++i)
    {
        auto& set = sets[(size_t) i];
        if (set.factor > maxFactor) continue;
        for (int m = 0; m < 3; ++m)
        {
            const double t = set.tau[m];
            if (dfd > 0)
            {
                const double d = Ta - t; const int fl = (int) std::floor (d + 1e-9);
                set.fir[0][m] = pa::fracDelayFir (dfd, juce::jmax (0.0, d - fl)); set.pad[0][m] = fl - dfd;
            }
            else { set.fir[0][m] = { 1.0 }; set.pad[0][m] = Ta - (int) std::lround (t); }
            set.fir[1][m] = pa::linearPhaseFir (hs[i][m], t, P, R);
            set.pad[1][m] = Tl - (P + (int) std::ceil (t - 1e-9));
            maxLen = juce::jmax (maxLen, (int) set.fir[1][m].size(), (int) set.fir[0][m].size());
        }
    }
    fixedLat[0] = Ta + 1; fixedLat[1] = Tl + 1;      // + the preamp line delay (1 sample, + a fraction on the left line in Stereo)

    preEq[0].prepare (preU.L, fs); preEq[1].prepare (preU.R, fs);
    lastStereoLines = false;
    for (int c = 0; c < 2; ++c)
    {
        preDel[c].set (1.0); preDel[c].reset();
        outFir[c].prepare (maxLen + 1); dryFir[c].prepare (maxLen + 1);
        wetPadD[c].prepare (juce::jmax (Ta, Tl) + 2); dryPadD[c].prepare (juce::jmax (Ta, Tl) + 2);
        dryOne[c].prepare (2);
    }
    dryBuf.setSize (2, blockSize);
    makeupSm.reset (fs, 0.02); makeupSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) s.makeupDb));
    mixSm.reset (fs, 0.02);    mixSm.setCurrentAndTargetValue ((float) s.mix);
    dryRunning = false; lastMode = -1; lastPhase = -1;
    msCoef = 1.0 - std::exp (-1.0 / (0.3 * fs));
    for (int c = 0; c < 2; ++c) msIn[c] = msOut[c] = 0;
    updateLatency();

    const int f = wantedFactor (isNonRealtime());
    for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
    activeFactor = f;
}

void RbaProcessor::updateLatency()
{
    const int ph = juce::jlimit (0, 1, juce::roundToInt (p[pPhase]->load()));
    if (ph == lastPhase) return;
    lastPhase = ph;
    fixedLatency = fixedLat[ph];
    latencyNow = fixedLatency;
    setLatencySamples (fixedLatency);                // the same in every oversampling and filter setting
}

void RbaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (numCh, buffer.getNumChannels());
    if (n == 0 || nc == 0) return;

    if (n > dryBuf.getNumSamples()) dryBuf.setSize (2, n, false, false, true);    // safety: host sent a bigger block

    updateLatency();
    const bool offline = isNonRealtime();
    rendering = offline;
    const int f = wantedFactor (offline);
    const int mode = juce::jlimit (0, 2, juce::roundToInt (p[pBandLimit]->load()));
    if (f != kFactors[activeSet] || mode != lastMode)
    {
        for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
        auto& set = sets[(size_t) activeSet];
        if (set.os) set.os->reset();
        if (set.dryOs) set.dryOs->reset();
        for (int c = 0; c < 2; ++c)
        {
            set.ch[c].reset(); set.det[c].reset(); set.rca[c].reset(); set.rdet[c].reset();
            set.post[c].reset(); set.clip[c].reset(); set.bl[mode][c].reset(); set.dbl[mode][c].reset();
            outFir[c].clear(); dryFir[c].clear(); wetPadD[c].clear(); dryPadD[c].clear(); dryOne[c].clear();
        }
        lastMode = mode; dryRunning = false;
    }
    activeFactor = f;
    auto& set = sets[(size_t) activeSet];

    // ---- settings
    const auto s = readSettings();
    const bool rcaUnit  = juce::roundToInt (p[pUnit]->load()) == 1;
    const int  bump     = juce::roundToInt (p[pBump]->load());          // 0 Off, 1 Pre, 2 Post
    const bool linked   = nc == 2 && juce::roundToInt (p[pLink]->load()) == 0;
    const bool clipOn   = p[pClip]->load() > 0.5f;
    const bool preOn    = p[pPre]->load() > 0.5f;
    const bool stereoLn = nc == 2 && juce::roundToInt (p[pPreLines]->load()) == 1;
    const bool d3       = nc == 2 && preOn && stereoLn && bump != 0 && p[p3D]->load() > 0.5f;
    const bool dual     = juce::roundToInt (p[pRcaRel]->load()) == 1;
    const double rcaFilt = p[pRcaFilter]->load() >= 100.f ? p[pRcaFilter]->load() : 0.0;
    const auto rcaU = rba::makeRca();
    // matched to the Modern unit; Range +9 drives the limiter 9 dB harder
    const double rcaGdb = rba::rcaInputGainDb (p[pThreshold]->load()) + (juce::roundToInt (p[pRcaHot]->load()) == 1 ? 9.0 : 0.0);
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
    auto& bl = set.bl[mode];
    float ipk[2] = { 0.f, 0.f };
    for (int c = 0; c < nc; ++c)
    {
        const float* d = dryBuf.getReadPointer (c);
        for (int i = 0; i < n; ++i) { ipk[c] = juce::jmax (ipk[c], std::abs (d[i])); msIn[c] += msCoef * ((double) d[i] * d[i] - msIn[c]); }
    }

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
        for (int c = 0; c < nc; ++c) x[c] = bl[c].process (0, x[c]);                 // D
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
        for (int c = 0; c < nc; ++c)
        {
            double v = bl[c].process (1, y[c]);                                       // C
            if (bump == 2) v = set.post[c].process (v);
            if (clipOn)    v = set.clip[c].process (v);
            v = bl[c].process (2, v);                                                  // B
            if (preOn)     v = set.pre[c].process (v);
            ch[c][i] = (float) bl[c].process (3, v);                                   // A
        }
    }
    if (set.os) set.os->processSamplesDown (block);

    // ---- the dry signal (for Dry/Wet) takes the same linear path: oversampling + every band-limit section,
    //      so dry and wet line up exactly. Runs only while the mix uses it.
    const bool needDry = mixSm.isSmoothing() || mixSm.getTargetValue() < 0.9999f;
    if (needDry)
    {
        if (! dryRunning)
        {
            if (set.dryOs) set.dryOs->reset();
            for (int c = 0; c < 2; ++c) { dryFir[c].clear(); dryPadD[c].clear(); dryOne[c].clear(); }
            for (int c = 0; c < nc; ++c) set.dbl[mode][c].reset();
            dryRunning = true;
        }
        juce::dsp::AudioBlock<float> db (dryBuf.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
        juce::dsp::AudioBlock<float> dup = set.dryOs ? set.dryOs->processSamplesUp (db) : db;
        for (int c = 0; c < nc; ++c)
        {
            float* u = dup.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < dup.getNumSamples(); ++i) u[i] = (float) set.dbl[mode][c].processDry (u[i]);
        }
        if (set.dryOs) set.dryOs->processSamplesDown (db);
    }
    else dryRunning = false;

    // ---- base rate: preamp EQ + line delay, alignment FIR (ANALOG or LINEAR) + pad to the fixed latency, Make Up, Dry/Wet
    const int ph = juce::jlimit (0, 1, lastPhase);
    const auto& fir = set.fir[ph][mode];
    const int pad = juce::jmax (0, set.pad[ph][mode]);
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
            const float wet = wetPadD[c].process ((float) outFir[c].process (fir, v), pad) * mk;
            float dry = 0.f;
            if (dryRunning) dry = dryOne[c].process (dryPadD[c].process ((float) dryFir[c].process (fir, dryBuf.getSample (c, i)), pad), 1);
            d[i] = mx * wet + (1.f - mx) * dry;
            pk[c] = juce::jmax (pk[c], std::abs (d[i]));
            msOut[c] += msCoef * ((double) d[i] * d[i] - msOut[c]);
        }
    }
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    grMeter = grMax;
    outPeak[0] = pk[0]; outPeak[1] = nc > 1 ? pk[1] : pk[0];
    inPeak[0] = ipk[0]; inPeak[1] = nc > 1 ? ipk[1] : ipk[0];
    for (int c = 0; c < 2; ++c)
    {
        const int k = nc > 1 ? c : 0;
        outRms[c] = (float) std::sqrt (msOut[k]); inRms[c] = (float) std::sqrt (msIn[k]);
    }
    if (clipOn)
    {
        double r = 0; for (int c = 0; c < nc; ++c) r = juce::jmax (r, set.clip[c].takeReductionDb());
        if (r > clipRed.load()) clipRed = (float) r;          // the editor reads and resets it
    }
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
