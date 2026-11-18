// Local test of the real RbaProcessor (not shipped): renders through the plug-in wrapper and
// checks it against the core model, the fixed latency across oversampling factors, and dry/wet.
#include "../Source/PluginProcessor.h"
#include "../Source/UnitElRey.h"
#include "../Source/UnitRca.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>

static std::vector<float> runPlugin (RbaProcessor& p, const std::vector<float>& x, double fs, int block)
{
    p.setPlayConfigDetails (2, 2, fs, block);
    p.prepareToPlay (fs, block);
    std::vector<float> out (x.size());
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    for (size_t i = 0; i < x.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, x.size() - i);
        buf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c) for (int k = 0; k < n; ++k) buf.setSample (c, k, x[i + (size_t) k]);
        p.processBlock (buf, midi);
        for (int k = 0; k < n; ++k) out[i + (size_t) k] = buf.getSample (0, k);
    }
    return out;
}
static void setP (RbaProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    if (auto* snap = std::getenv ("RBA_SNAPSHOT"))       // render the editor to PNG files and exit
    {
        for (int u = 0; u <= 1; ++u)
        {
            RbaProcessor p; setP (p, "unit", (float) u); p.setPlayConfigDetails (2, 2, 96000, 512); p.prepareToPlay (96000, 512);
            if (u == 1) { setP (p, "clip", 1); setP (p, "pre", 1); setP (p, "bump", 0); }
            p.grMeter = u ? 6.0f : 4.5f; p.outPeak[0] = 0.55f; p.outPeak[1] = 0.5f;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (1120, 620);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::String (snap) + (u ? "_rca.png" : "_elrey.png"));
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
            std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        }
        return 0;
    }
    const double fs = 96000; const int N = (int) fs * 3;
    std::vector<float> x ((size_t) N);
    for (int i = 0; i < N; ++i)                       // 1 kHz tone with a +20 dB step at 1 s, back at 2 s
    {
        const double t = i / fs; const double a = (t > 1.0 && t < 2.0) ? 0.5 : 0.05;
        x[(size_t) i] = (float) (a * std::sin (2 * rba::kPi * 1000 * t));
    }
    int fails = 0;
    // 1) plug-in (oversampling Off) vs core model
    {
        RbaProcessor p; setP (p, "threshold", 25.4f); setP (p, "oslive", 0); setP (p, "rendermode", 2);
        auto y = runPlugin (p, x, fs, 512);
        rba::MonoModel m; m.prepare (rba::makeElRey(), fs); rba::Settings s; s.threshold = 25.4; m.update (s);
        const int L = p.getLatencySamples();
        double maxd = 0;
        int at = -1;
        for (int i = 0; i + L < N; ++i)
        {
            const double dd = std::abs ((double) y[(size_t) (i + L)] - m.process (x[(size_t) i]));
            if (dd > maxd) { maxd = dd; at = i; }
        }
        std::printf ("  (largest difference at %.4f s)\n", at / fs);
        std::printf ("plug-in (OS off) vs model, allowing for the %d-sample fixed latency: max abs diff %.2e  -> %s\n", L, maxd, maxd < 1e-4 ? "OK" : "FAIL");
        fails += maxd >= 1e-4;
        std::printf ("reported latency with OS off: %d samples\n", p.getLatencySamples());
    }
    // 2) latency is the same for every oversampling setting, and the output lines up
    int lat0 = -1; std::vector<float> ref;
    for (int os = 0; os <= 2; ++os)
    {
        RbaProcessor p; setP (p, "threshold", 25.4f); setP (p, "oslive", (float) os); setP (p, "rendermode", 2);
        auto y = runPlugin (p, x, fs, 480);
        const int lat = p.getLatencySamples();
        if (lat0 < 0) { lat0 = lat; ref = y; }
        // cross-correlate the step region against the first render to find any timing offset
        int best = 0; double bv = -1e9;
        for (int d = -8; d <= 8; ++d)
        {
            double acc = 0; for (int i = (int) fs; i < (int) (1.1 * fs); ++i) acc += ref[(size_t) i] * y[(size_t) (i + d)];
            if (acc > bv) { bv = acc; best = d; }
        }
        std::printf ("oversampling %dx: latency %d, timing offset vs OS off %+d samples -> %s\n", 1 << os, lat, best,
                     (lat == lat0 && best == 0) ? "OK" : "FAIL");
        fails += ! (lat == lat0 && best == 0);
    }
    // 3) Dry/Wet 0 % = the input, delayed by the reported latency
    {
        RbaProcessor p; setP (p, "mix", 0.f); setP (p, "oslive", 2); setP (p, "rendermode", 2);
        auto y = runPlugin (p, x, fs, 333); const int L = p.getLatencySamples();
        double maxd = 0; for (int i = L; i < N; ++i) maxd = std::max (maxd, std::abs ((double) y[(size_t) i] - x[(size_t) (i - L)]));
        std::printf ("dry/wet 0%% = input delayed by %d: max diff %.2e -> %s\n", L, maxd, maxd < 1e-6 ? "OK" : "FAIL"); fails += maxd >= 1e-6;
    }
    // 4) Linked vs dual-mono with a one-sided signal
    {
        RbaProcessor p; setP (p, "threshold", 30.f); setP (p, "oslive", 0); setP (p, "rendermode", 2);
        p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer midi; double eL = 0, eR = 0, eRin = 0;
        for (int blk = 0; blk < 300; ++blk)
        {
            for (int k = 0; k < 512; ++k)
            {
                const double t = (blk * 512 + k) / fs;
                b.setSample (0, k, (float) (0.5 * std::sin (2 * rba::kPi * 1000 * t)));
                b.setSample (1, k, (float) (0.01 * std::sin (2 * rba::kPi * 440 * t)));
            }
            p.processBlock (b, midi);
            if (blk > 200) for (int k = 0; k < 512; ++k) { eL += b.getSample (0, k) * b.getSample (0, k); eR += b.getSample (1, k) * b.getSample (1, k); eRin += 0.01 * 0.01 * 0.5; }
        }
        const double grR_linked = 10 * std::log10 (eR / eRin);
        setP (p, "link", 1); eR = 0; eRin = 0;
        for (int blk = 0; blk < 300; ++blk)
        {
            for (int k = 0; k < 512; ++k)
            {
                const double t = (blk * 512 + k) / fs;
                b.setSample (0, k, (float) (0.5 * std::sin (2 * rba::kPi * 1000 * t)));
                b.setSample (1, k, (float) (0.01 * std::sin (2 * rba::kPi * 440 * t)));
            }
            p.processBlock (b, midi);
            if (blk > 200) for (int k = 0; k < 512; ++k) { eR += b.getSample (1, k) * b.getSample (1, k); eRin += 0.01 * 0.01 * 0.5; }
        }
        const double grR_dual = 10 * std::log10 (eR / eRin);
        std::printf ("quiet right channel with a loud left: linked %.2f dB, dual mono %.2f dB -> %s\n", grR_linked, grR_dual,
                     (grR_linked < grR_dual - 3) ? "OK (linked compresses it, dual mono doesn't)" : "FAIL");
        fails += ! (grR_linked < grR_dual - 3);
    }
    // 5) Vintage RCA unit: 1 kHz at -10 dBFS with the input gain of the RCA capture (+12.47 dB) -> about 8.1 dB GR
    {
        float thr = 0; for (float t = 0; t <= 40.f; t += 0.01f) if (rba::rcaInputGainDb (t) <= 12.47) thr = t;   // knob position for +12.47 dB
        RbaProcessor p; setP (p, "unit", 1); setP (p, "bump", 0); setP (p, "threshold", thr);
        setP (p, "oslive", 1); setP (p, "rendermode", 2);
        std::vector<float> t ((size_t) fs); for (int i = 0; i < (int) fs; ++i) t[(size_t) i] = (float) (0.3162 * std::sin (2 * rba::kPi * 1000 * i / fs));
        runPlugin (p, t, fs, 512);
        const float g = p.grMeter.load();
        std::printf ("RCA unit, -10 dBFS 1 kHz at the capture's input gain: GR %.2f dB (capture: 8.1) -> %s\n", g, std::abs (g - 8.1f) < 1.0f ? "OK" : "FAIL");
        fails += std::abs (g - 8.1f) >= 1.0f;
    }
    // 6) every stage on (Clipper, Preamp Stereo, 3D bump, M/S, both units): finite output, sensible level, L != R
    for (int u = 0; u <= 1; ++u)
    {
        RbaProcessor p; setP (p, "unit", (float) u); setP (p, "threshold", 25.f); setP (p, "clip", 1); setP (p, "crush", 30.f);
        setP (p, "pre", 1); setP (p, "predrive", 20.f); setP (p, "prelines", 1); setP (p, "threed", 1); setP (p, "bump", 2);
        setP (p, "msmode", 1); setP (p, "oslive", 2); setP (p, "rendermode", 2);
        p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer midi; juce::Random rnd (3); bool finite = true; double e = 0, d = 0;
        for (int blk = 0; blk < 400; ++blk)
        {
            for (int k = 0; k < 512; ++k) { const float v = 0.4f * (rnd.nextFloat() * 2 - 1); b.setSample (0, k, v); b.setSample (1, k, v); }
            p.processBlock (b, midi);
            for (int k = 0; k < 512; ++k)
            {
                const float l = b.getSample (0, k), r = b.getSample (1, k);
                finite = finite && std::isfinite (l) && std::isfinite (r) && std::abs (l) < 4.f;
                if (blk > 100) { e += l * l; d += (l - r) * (l - r); }
            }
        }
        const double lvl = 10 * std::log10 (e / (300.0 * 512)), side = 10 * std::log10 (d / e + 1e-30);
        const bool ok = finite && lvl > -30 && lvl < 0 && side > -60 && side < -10;
        std::printf ("all stages on, unit %s: finite %s, level %.1f dBFS, L-R difference %.1f dB below the signal -> %s\n",
                     u ? "RCA" : "El Rey", finite ? "yes" : "NO", lvl, -side, ok ? "OK" : "FAIL");
        fails += ! ok;
    }
    // 7) CPU: 20 s of stereo at 96 kHz per oversampling setting
    for (int os = 0; os <= 2; ++os)
    {
        RbaProcessor p; setP (p, "threshold", 25.4f); setP (p, "oslive", (float) os); setP (p, "rendermode", 2);
        setP (p, "clip", 1); setP (p, "pre", 1); setP (p, "prelines", 1);
        p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer midi; juce::Random rnd (1);
        const int blocks = (int) (20 * fs / 512);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int c = 0; c < 2; ++c) for (int k = 0; k < 512; ++k) b.setSample (c, k, 0.3f * (rnd.nextFloat() * 2 - 1));
            p.processBlock (b, midi);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("CPU (El Rey + Clipper + Preamp), stereo 96 kHz, oversampling %dx: %.2f%% of one core (20 s of audio in %.0f ms)\n", 1 << os, 100.0 * ms / 20000.0, ms);
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails;
}
