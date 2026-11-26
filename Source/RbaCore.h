// RbaCore.h — RBA-6A signal model (one channel). Header-only, no JUCE dependency, so the same
// code runs in the plug-in and in the offline fitting / test tools.
//
// Chain (per channel):
//   x -> preamp colour  [EQ bump path + fixed colour, blended to the flat path by De-Bumpify]
//     -> Input gain
//     -> drive saturator (single-ended)            <- default position ("preamp > push")
//     -> PUSH  (two antiphase halves)
//          [drive saturator per half]              <- "preamp in the push-pull" position
//          vari-mu gain (common-mode) + GR-dependent odd distortion
//     -> PULL  (difference; Balance leaks even terms and control-voltage thump)
//     -> detector tap (feedback compressor: the detector hears the pulled output)
//     -> 1 / Input gain -> Make Up -> Dry/Wet
//
// The nonlinear section (drive saturator .. pull, plus the detector loop) runs at the
// oversampled rate; the preamp EQ runs at the base rate (it is linear).

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace rba
{
constexpr double kPi = 3.14159265358979323846;

// ------------------------------------------------------------------------------- filters
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

enum class BQ { HP, LP, Peak, HighShelf, LowShelf, HP1 };

inline Biquad design (BQ t, double f0, double Q, double gdb, double fs)
{
    Biquad q;
    f0 = std::min (f0, 0.49 * fs);
    const double A = std::pow (10.0, gdb / 40.0), w = 2 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w),
                 al = s / (2 * Q);
    double b0, b1, b2, a0, a1, a2;
    switch (t)
    {
        case BQ::HP:   b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case BQ::LP:   b0 = (1 - c) / 2; b1 = 1 - c;    b2 = (1 - c) / 2; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case BQ::Peak: b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; break;
        case BQ::HighShelf:
        {
            const double sq = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) + (A - 1) * c + sq); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - sq);
            a0 = (A + 1) - (A - 1) * c + sq; a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - sq; break;
        }
        case BQ::LowShelf:
        {
            const double sq = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) - (A - 1) * c + sq); b1 = 2 * A * ((A - 1) - (A + 1) * c); b2 = A * ((A + 1) - (A - 1) * c - sq);
            a0 = (A + 1) + (A - 1) * c + sq; a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - sq; break;
        }
        case BQ::HP1: default:
        {
            const double K = std::tan (w / 2);
            q.b0 = 1 / (1 + K); q.b1 = -q.b0; q.b2 = 0; q.a1 = (K - 1) / (K + 1); q.a2 = 0; return q;
        }
    }
    q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
    return q;
}

struct EQBand { BQ type; double f, Q, g; };

struct EQChain
{
    double gain = 1;
    std::vector<Biquad> s;
    void setup (double gainDb, const std::vector<EQBand>& bands, double fs)
    {
        gain = std::pow (10.0, gainDb / 20.0);
        s.clear();
        for (auto& b : bands) s.push_back (design (b.type, b.f, b.Q, b.g, fs));
    }
    inline double process (double x) noexcept { x *= gain; for (auto& b : s) x = b.process (x); return x; }
    void reset() { for (auto& b : s) b.reset(); }
};

// ------------------------------------------------------------------- saturator (one block)
// Low-end emphasis (one-pole low shelf, boost 1+gamma below fc) -> static curve -> exact inverse
// shelf. The linear part cancels, so the block only adds distortion, more of it in the low end
// (transformer-like). Static curve: polynomial (H2/H3 at low level) into a soft limiter whose
// hardness p sets how sharply higher harmonics appear near the limit L.
struct SatShape { double fc = 200, gamma = 0, a2 = 0, a3 = 0, L = 4, p = 4, asym = 0, fastBelow = 0; };

struct Saturator
{
    SatShape sh;
    double a = 0, lpIn = 0, lpOut = 0;
    void setup (const SatShape& s, double fs)
    {
        sh = s; a = 1 - std::exp (-2 * kPi * s.fc / fs);
        sh.fastBelow = std::pow (1e-7 * s.p, 1 / s.p);                  // r^p / p < 1e-7 below this
    }
    void reset() { lpIn = lpOut = 0; }

    static inline double curve (double z, const SatShape& s) noexcept
    {
        const double v = z + s.a2 * z * z + s.a3 * z * z * z;
        const double Lh = s.L * (v >= 0 ? 1 + s.asym : 1 - s.asym);     // lopsided knee: even harmonics
        const double r = std::abs (v / Lh);
        if (r < s.fastBelow) return v;                                   // far below the knee: exact to ~1e-7
        return v / std::pow (1 + std::pow (r, s.p), 1 / s.p);
    }
    inline double process (double u) noexcept
    {
        lpIn += a * (u - lpIn);
        const double e = u + sh.gamma * lpIn;                       // pre-emphasis
        const double v = curve (e, sh);
        // de-emphasis: y + gamma * LP(y) = v, solved for this sample
        const double y = (v - sh.gamma * (1 - a) * lpOut) / (1 + sh.gamma * a);
        lpOut += a * (y - lpOut);
        return y;
    }
};

// ------------------------------------------------------------------------- model constants
// Everything fitted to the El Rey 2 captures (96 kHz, HYP, OVS 32x). See tools/fit_*.py.
struct Unit
{
    // preamp EQ: bump path (De-Bumpify 0) and flat path (De-Bumpify 100); designed per sample rate
    double bumpGainDb = 0;  std::vector<EQBand> bump;
    double flatGainDb = 0;  std::vector<EQBand> flat;
    SatShape colour;          // fixed colour on the bump path (not driven by Input)
    SatShape drive;           // Input-driven saturator
    // Input-driven preamp "squash": smooth level-dependent gain (envelope-controlled, no instant
    // waveshaping), as the sampled preamp loses level before it makes many harmonics
    double sqK = 0.0;         // dB of gain loss per dB above the knee (0 = off)
    double sqT = 0.0;         // knee (dBFS of the driven signal)
    double sqW = 6.0;         // knee width (dB)
    double sqAtk = 0.002, sqRel = 0.05;   // envelope times (s)
    // compressor
    double T0 = -10;          // threshold (dB, detector scale) at Threshold = 0
    double slope = 1.0;       // feedback slope k (GR dB per dB of detector level above the knee)
    double knee = 6.0;        // softplus width (dB)
    double detPow = 1.0;      // 1 = average of |y|, 2 = mean square
    double attackKnots[5]  = { 0.06, 0.04, 0.012, 0.010, 0.008 };            // s, Attack 1,1.5,2,2.5,3
    double releaseKnots[9] = { 0.30, 0.22, 0.15, 0.11, 0.075, 0.05, 0.02, 0.01, 0.005 };  // s, Release 1..5 / 0.5
    // GR smoothing after the detector (one-pole in dB, attack / release), per setting; 0 = off.
    // Separates the detector's ripple/static behaviour from the audible attack and release.
    double smAtkKnots[5] = { 0, 0, 0, 0, 0 };
    double smRelKnots[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    double scQ = 0.707;       // (unused since the 1st-order sidechain filter)
    double scScale = 1.0;     // sidechain corner = Filter (Hz) x scScale
    // vari-mu odd distortion: H3 ratio = k3max * (1 - exp(-GR / k3tau)) + k3base (as a fraction)
    double k3max = 1.3e-3, k3tau = 3.0, k3base = 0.0;
    double thump = 0.02;      // control-voltage leak scale at full imbalance
};

// --------------------------------------------------------------------------- user settings
struct Settings
{
    double inputDb = 0;       // -18 .. +18 (gain-compensated drive)
    double threshold = 0;     // 0 .. 40 (higher = more compression)
    double attack = 2;        // 1 .. 3 (continuous)
    double release = 3;       // 1 .. 5 (continuous)
    double filterHz = 0;      // 0 = off, 0 .. 250
    double makeupDb = 0;      // 0 .. 24
    double mix = 1;           // 0 .. 1 (wet)
    double debump = 0;        // 0 .. 1
    double balance = 0;       // -1 .. 1 (push-pull imbalance)
    bool   power = true;      // compressor on
    bool   preInside = false; // drive saturator inside the push-pull pair
};

inline double interpLog (const double* knots, int n, double pos01)
{
    const double x = std::clamp (pos01, 0.0, 1.0) * (n - 1);
    const int i = std::min ((int) x, n - 2);
    const double f = x - i;
    return std::exp (std::log (knots[i]) * (1 - f) + std::log (knots[i + 1]) * f);
}

// ------------------------------------------------------------------------------- detector
// Feedback detector. It hears the pulled output of one or two channels (Linked = one shared
// envelope from both; the plug-in uses one Detector per channel for Dual mono) and decides the
// gain used from the next sample on.
class Detector
{
public:
    void prepare (const Unit& u, double fsOS_) { unit = u; fsOS = fsOS_; lastFilterHz = -1; reset(); }
    void reset() { env = 0; g = 1; gr = 0; sc[0].reset(); sc[1].reset(); }
    void update (const Settings& s, bool force = false)
    {
        power = s.power;
        T = unit.T0 - s.threshold;
        {
            const double floorDb = T - 14.0 * unit.knee;               // softplus(z) < 1e-6 below this
            envFloor = std::pow (10.0, floorDb / (unit.detPow > 1.5 ? 10.0 : 20.0));
        }
        const double tA = interpLog (unit.attackKnots, 5, (s.attack - 1) / 2);
        const double tR = interpLog (unit.releaseKnots, 9, (s.release - 1) / 4);
        aAtk = 1 - std::exp (-1 / (tA * fsOS));
        aRel = 1 - std::exp (-1 / (tR * fsOS));
        const double sA = unit.smAtkKnots[0] > 0 ? interpLog (unit.smAtkKnots, 5, (s.attack - 1) / 2) : 0;
        const double sR = unit.smRelKnots[0] > 0 ? interpLog (unit.smRelKnots, 9, (s.release - 1) / 4) : 0;
        bAtk = sA > 0 ? 1 - std::exp (-1 / (sA * fsOS)) : 1.0;
        bRel = sR > 0 ? 1 - std::exp (-1 / (sR * fsOS)) : 1.0;
        if (force || s.filterHz != lastFilterHz)
        {
            scOn = s.filterHz > 1.0;
            // El Rey's Filter measures as a gentle 6 dB/oct high-pass at about 1.25 x the knob value
            if (scOn) sc[0] = sc[1] = design (BQ::HP1, s.filterHz * unit.scScale, 0.707, 0, fsOS);
            lastFilterHz = s.filterHz;
        }
    }
    // feed one sample per channel (n = 1 or 2); returns nothing, read gain()/gainReduction()
    inline void feed (const double* y, int n) noexcept
    {
        if (! power) { g = 1; gr = 0; return; }
        double r = 0;
        for (int c = 0; c < n; ++c)
        {
            const double d = scOn ? sc[c].process (y[c]) : y[c];
            r += unit.detPow > 1.5 ? d * d : std::abs (d);
        }
        r /= n;
        env += (r > env ? aAtk : aRel) * (r - env);
        double grT = 0;
        if (env >= envFloor)                                              // GR < ~1e-6 dB below the floor
        {
            const double p = unit.detPow > 1.5 ? env : env * env;
            const double L = 10.0 * std::log10 (std::max (p, 1e-24));
            const double z = (L - T) / unit.knee;
            const double soft = z > 30 ? z : std::log1p (std::exp (z));
            grT = unit.slope * unit.knee * soft;
        }
        else if (gr < 1e-7) { gr = 0; g = 1; return; }
        gr += (grT > gr ? bAtk : bRel) * (grT - gr);
        g = std::exp (-gr * 0.11512925464970229);          // 10^(-gr/20)
    }
    double gain() const noexcept { return g; }
    double gainReduction() const noexcept { return gr; }
    double level() const noexcept { return unit.detPow > 1.5 ? std::sqrt (std::max (env, 0.0)) : env; }
private:
    Unit unit; double fsOS = 96000, T = -10, aAtk = 0.01, aRel = 0.001, bAtk = 1, bRel = 1, env = 0, g = 1, gr = 0, envFloor = 0;
    bool power = true, scOn = false; double lastFilterHz = -1; Biquad sc[2];
};

// ----------------------------------------------------------------------------- one channel
// Base-rate front end (preamp EQ / colour, Input gain) and the oversampled push-pull section
// are separate so the plug-in can put its up/down-sampler between them.
class Channel
{
public:
    // the "3D" bump of the right channel: the bump's low sections a little lower (about 6 %), the
    // subsonic cut identical in shape, and a touch more air (+0.4 dB shelf at 15 kHz)
    static std::vector<EQBand> bump3D (const std::vector<EQBand>& b)
    {
        std::vector<EQBand> o = b;
        for (auto& e : o) if (e.f < 300) e.f *= 0.94;
        o.push_back ({ BQ::HighShelf, 15000, 0.707, 0.4 });
        return o;
    }
    void set3D (bool on)
    {
        if (on == is3D) return;
        is3D = on;
        eqBump.setup (unit.bumpGainDb, on ? bump3D (unit.bump) : unit.bump, fsBase);
    }
    void prepare (const Unit& u, double baseRate, int osFactor)
    {
        unit = u; fsBase = baseRate; fsOS = baseRate * osFactor; is3D = false;
        eqBump.setup (u.bumpGainDb, u.bump, fsBase);
        eqFlat.setup (u.flatGainDb, u.flat, fsBase);
        colour.setup (u.colour, fsBase);
        drivePos.setup (u.drive, fsOS); driveNeg.setup (u.drive, fsOS);
        dcBlock = design (BQ::HP1, 5.0, 0.707, 0, fsOS);
        sqA = 1 - std::exp (-1 / (u.sqAtk * fsOS)); sqR = 1 - std::exp (-1 / (u.sqRel * fsOS));
        sqFloor = std::pow (10.0, (u.sqT - 18.0 * u.sqW) / 20.0);       // softplus < 1.5e-8 below this
        reset();
    }
    void reset()
    {
        eqBump.reset(); eqFlat.reset(); colour.reset(); drivePos.reset(); driveNeg.reset();
        dcBlock.reset(); gThump = 1; sqEnv = 0;
    }
    void update (const Settings& s) { st = s; gIn = std::pow (10.0, s.inputDb / 20.0); }

    // base rate: preamp EQ and fixed colour, De-Bumpify blend, Input gain
    inline double front (double x) noexcept
    {
        const double b = colour.process (eqBump.process (x));
        const double f = eqFlat.process (x);
        return gIn * ((1 - st.debump) * b + st.debump * f);
    }
    // v0.2 front: 0 = flat path (Bump Off / Post), 1 = bump path (Bump Pre)
    inline double frontMode (double x, int mode) noexcept
    {
        const double b = colour.process (eqBump.process (x));
        const double f = eqFlat.process (x);
        return gIn * (mode == 1 ? b : f);
    }

    // oversampled: drive, push, vari-mu (gain g / reduction gr from the detector), pull.
    // Returns the pulled signal at drive level (the detector hears this); divide by inputGain().
    // Input Drive alone (squash + drive saturator), single-ended: used in front of the RCA unit
    inline double driveOnly (double u) noexcept
    {
        u = squash (u);
        return drivePos.process (u);
    }
    inline double squash (double u) noexcept
    {
        if (unit.sqK > 0)
        {
            const double r = std::abs (u);
            sqEnv += (r > sqEnv ? sqA : sqR) * (r - sqEnv);
            if (sqEnv > sqFloor)
            {
                const double L = 20.0 * std::log10 (std::max (sqEnv, 1e-12));
                const double z = (L - unit.sqT) / unit.sqW;
                const double soft = z > 30 ? z : std::log1p (std::exp (z));
                u *= std::exp (-unit.sqK * unit.sqW * soft * 0.11512925464970229);
            }
        }
        return u;
    }
    inline double pushPull (double u, double g, double gr, double detLevel) noexcept
    {
        if (unit.sqK > 0)                               // preamp squash (envelope-controlled gain)
        {
            const double r = std::abs (u);
            sqEnv += (r > sqEnv ? sqA : sqR) * (r - sqEnv);
            if (sqEnv > sqFloor)
            {
            const double L = 20.0 * std::log10 (std::max (sqEnv, 1e-12));
            const double z = (L - unit.sqT) / unit.sqW;
            const double soft = z > 30 ? z : std::log1p (std::exp (z));
            u *= std::exp (-unit.sqK * unit.sqW * soft * 0.11512925464970229);
            }
        }
        double hp, hn;                                  // the two halves (normalised to full scale)
        if (st.preInside) { hp = drivePos.process (u); hn = -driveNeg.process (-u); }
        else              { hp = hn = drivePos.process (u); }

        // vari-mu per half: common-mode gain plus odd distortion growing with GR, normalised by the
        // pre-gain level so the H3 ratio follows GR rather than signal level
        // normalised by the level *before* the gain stage (detector level / gain), so the H3 ratio
        // follows the amount of GR, not the output level
        const double k3 = st.power ? (unit.k3max * (1 - std::exp (-gr / unit.k3tau)) + unit.k3base) * 4.0 : 0.0;
        const double pre = detLevel / std::max (g, 1e-4);
        const double norm = 1.0 / (2.0 * pre * pre + 1e-9);
        auto stage = [&] (double h) { return g * (h + k3 * h * h * h * norm); };
        const double sp = stage (hp), sn = stage (hn);

        // pull: Balance leaks a share of the even part and the common-mode control voltage (thump)
        const double eps = 0.5 * st.balance;
        double y = 0.5 * ((1 + eps) * sp + (1 - eps) * sn);
        if (st.power && eps != 0) { y += eps * unit.thump * (g - gThump); gThump += 0.001 * (g - gThump); }
        return dcBlock.process (y);
    }
    double inputGain() const noexcept { return gIn; }

private:
    Unit unit; Settings st; bool is3D = false;
    double fsBase = 96000, fsOS = 96000, gIn = 1, gThump = 1, sqEnv = 0, sqA = 0.01, sqR = 0.001, sqFloor = 0;
    EQChain eqBump, eqFlat; Saturator colour, drivePos, driveNeg; Biquad dcBlock;
};

// ================================================================== Vintage RCA unit
// Fitted to the Nebula BA-6A captures (RCA capture kit, 96 kHz): a feedback PEAK limiter with an
// output-referred hard knee, and the RCA timing network (C1 || R11, plus R37 + C18 in Dual).
struct RcaUnit
{
    double T = -6.5;          // limiting starts at this output peak level (dBFS, detector scale)
    double k = 4.9;           // GR dB per dB of output above T (static ratio 1 + k)
    double knee = 0.3;        // softplus width (dB), small = hard knee
    double atk = 0.0008;      // charge time constant of C1 through the rectifier (s)
    // discharge (R11 C1), coupling to the second capacitor (R37 C1) and C18 / C1, per mode
    double relS = 0.12, cplS = 1.0, ratS = 0.2;      // Single
    double relD = 0.33, cplD = 0.33, ratD = 2.7;     // Dual
    double atkD = 0.0008;     // Dual charge time constant
    double hyp = 0.55;        // release of the gain reduction itself: d(GR)/dt = -GR^2 / hyp (dB*s); 0 = off
    double k3 = 0.0;          // odd distortion while limiting (fraction, normalised)
    double k3Lf = 0.0;        // extra odd distortion below ~200 Hz while limiting
    double inA = -14.0, inB = 0.93;                  // Threshold knob (0..40) -> input gain dB = inA + inB * T
};

class RcaDetector
{
public:
    void prepare (const RcaUnit& u, double fs) { unit = u; fsOS = fs; lastHz = -1; reset(); }
    void reset() { v1 = v2 = 0; g = 1; gr = 0; sc[0][0].reset(); sc[0][1].reset(); sc[1][0].reset(); sc[1][1].reset(); }
    void update (bool dual, bool power_, double filterHz)
    {
        power = power_;
        const double ta = dual ? unit.atkD : unit.atk;
        const double tr = dual ? unit.relD : unit.relS, tc = dual ? unit.cplD : unit.cplS;
        rat = dual ? unit.ratD : unit.ratS;
        aA = 1 - std::exp (-1 / (ta * fsOS));
        aR = 1 - std::exp (-1 / (tr * fsOS));
        aC = 1 / (tc * fsOS);
        hypK = unit.hyp > 0 ? 1.0 / (unit.hyp * fsOS) : 0.0;
        if (filterHz != lastHz)
        {
            scOn = filterHz > 1.0;
            // measured: a gentle 6 dB/oct high-pass at about 1.33 x the knob value (100..500 Hz)
            if (scOn) for (auto& c : sc) { c[0] = design (BQ::HP1, filterHz * 1.33, 0.7071, 0, fsOS); }
            lastHz = filterHz;
        }
    }
    // The RC network works on the control voltage, taken as proportional to GR in dB (vari-mu):
    // the rectified output peak gives a target GR; C1 charges towards it through the rectifier,
    // discharges through R11 (plus a depth-dependent term), and shares charge with C18 via R37.
    inline void feed (const double* y, int n) noexcept
    {
        if (! power) { g = 1; gr = 0; v1 = v2 = 0; return; }
        double d = 0;
        for (int c = 0; c < n; ++c)
        {
            double v = y[c];
            if (scOn) v = sc[c][0].process (v);
            d = std::max (d, std::abs (v));
        }
        levelEst += (d > levelEst ? 1.0 : 0.0005) * (d - levelEst);
        double grS = 0;
        if (d > 1e-9)
        {
            const double z = (20.0 * std::log10 (d) - unit.T) / unit.knee;
            if (z > -40) grS = unit.k * unit.knee * (z > 30 ? z : std::log1p (std::exp (z)));
        }
        if (grS > v1) v1 += aA * (grS - v1);
        v1 -= aR * v1 + hypK * v1 * v1;
        const double i = aC * (v1 - v2);                 // R37: C1 <-> C18
        v1 -= i; v2 += i / rat;
        if (v1 < 1e-7 && v2 < 1e-7) { v1 = v2 = 0; gr = 0; g = 1; return; }
        gr = std::max (0.0, v1);
        g = std::exp (-gr * 0.11512925464970229);
    }
    double gain() const noexcept { return g; }
    double gainReduction() const noexcept { return gr; }
    double level() const noexcept { return levelEst; }
private:
    RcaUnit unit; double levelEst = 0; double fsOS = 96000, v1 = 0, v2 = 0, g = 1, gr = 0, aA = 0.01, aR = 0.001, aC = 0.0001, rat = 1, hypK = 0;
    bool power = true, scOn = false; double lastHz = -1; Biquad sc[2][2];
};

// the RCA gain stage: push-pull vari-mu (even terms cancel), odd distortion while limiting
class RcaStage
{
public:
    void prepare (const RcaUnit& u, double fs)
    {
        unit = u; lf = 1 - std::exp (-2 * kPi * 200.0 / fs); dc = design (BQ::HP1, 5.0, 0.707, 0, fs); reset();
    }
    void reset() { lfz = 0; dc.reset(); }
    inline double process (double u, double g, double gr, double detLevel) noexcept
    {
        lfz += lf * (u - lfz);                           // low band (< ~200 Hz)
        const double on = std::min (1.0, gr);            // distortion comes in with the first dB of GR
        const double pre = detLevel / std::max (g, 1e-4);
        const double norm = 1.0 / (pre * pre + 1e-12);
        const double c3 = on * unit.k3 * norm, c3l = on * unit.k3Lf * norm;
        const double y = g * (u + c3 * u * u * u + c3l * lfz * lfz * lfz);
        return dc.process (y);
    }
private:
    RcaUnit unit; double lf = 0.01, lfz = 0; Biquad dc;
};

// ===================================================================== output stages (v0.2)
// Clipper, fitted to the Nebula BA-6A clipper: low-end emphasis (lows clip harder), soft limiting
// curve, exact de-emphasis, level compensated below the onset (about 22 - Crush dBFS). Odd harmonics.
struct ClipUnit { double O = 22.05, p = 1.881, s = 1.0, gamma = 1.762, fc = 574.3; };
class Clipper
{
public:
    void prepare (const ClipUnit& u, double fs) { cu = u; a = 1 - std::exp (-2 * kPi * u.fc / fs); reset(); }
    void reset() { lpIn = lpOut = 0; eMax = 0; }
    void update (double crush, double makeupDb)
    {
        G = std::pow (10.0, (crush - cu.O) / 20.0); invG = 1 / G;
        mk = std::pow (10.0, makeupDb / 20.0);
    }
    inline double process (double u) noexcept
    {
        lpIn += a * (u - lpIn);
        const double e = (u + cu.gamma * lpIn) * G;
        const double ae = std::abs (e);
        if (ae > eMax) eMax = ae;
        const double v = e * std::pow (1 + std::pow (ae, cu.p), -cu.s / cu.p) * invG;
        const double y = (v - cu.gamma * (1 - a) * lpOut) / (1 + cu.gamma * a);
        lpOut += a * (y - lpOut);
        return y * mk;
    }
    // largest peak reduction (dB) since the last call
    double takeReductionDb() noexcept
    {
        const double r = 20.0 * cu.s / cu.p * std::log10 (1 + std::pow (eMax, cu.p));
        eMax = 0; return r;
    }
private:
    ClipUnit cu; double a = 0.01, lpIn = 0, lpOut = 0, G = 1, invG = 1, mk = 1, eMax = 0;
};

// Preamp (the RCA output stage), fitted to the Nebula BA-6A pre: gentle EQ and a drive curve
// (gain-compensated, asymmetric soft limiter). The Stereo program's right line differs a little
// (EQ, about 8 dB more drive, and it leads the left line by about 2.2 microseconds).
struct PreLine { double lsF = 9.66, lsQ = 1.09, lsG = -6.0, lpF = 27387; double extraDb = 0; };
struct PreUnit { PreLine L, R { 9.06, 2.20, -6.0, 28406, 8.0 }; double c = 0.5148, Lim = 7.80, p = 2.474, asym = 0.2178; double rLeadUs = 2.22; };
class PreDrive       // oversampled part
{
public:
    void prepare (const PreUnit& u) { pu = u; }
    void update (double driveDb, double extraDb) { G = std::pow (10.0, (pu.c * driveDb + extraDb) / 20.0); invG = 1 / G; }
    inline double process (double x) noexcept
    {
        const double z = G * x;
        const double Lh = pu.Lim * (z >= 0 ? 1 + pu.asym : 1 - pu.asym);
        return z / std::pow (1 + std::pow (std::abs (z / Lh), pu.p), 1 / pu.p) * invG;
    }
private:
    PreUnit pu; double G = 1, invG = 1;
};
// Bump path on its own (Bump = Post: after the compressor)
class BumpPath
{
public:
    void prepare (const Unit& u, double fs, bool d3) { unit = u; rate = fs; is3D = d3; build(); }
    void set3D (bool on) { if (on != is3D) { is3D = on; build(); } }
    inline double process (double x) noexcept { return colour.process (eq.process (x)); }
    void reset() { eq.reset(); colour.reset(); }
private:
    void build()
    {
        std::vector<EQBand> b = unit.bump;
        if (is3D) { for (auto& e : b) if (e.f < 300) e.f *= 0.94; b.push_back ({ BQ::HighShelf, 15000, 0.707, 0.4 }); }
        eq.setup (unit.bumpGainDb, b, rate); colour.setup (unit.colour, rate);
    }
    Unit unit; double rate = 96000; bool is3D = false; EQChain eq; Saturator colour;
};
// first-order Thiran allpass: a fractional delay of D samples (0.5 .. 1.5) with flat magnitude
struct FracDelay
{
    double a = 0, z = 0;
    void set (double D) { a = (1 - D) / (1 + D); }
    inline double process (double x) noexcept { const double y = a * x + z; z = x - a * y; return y; }
    void reset() { z = 0; }
};
class PreEQ          // base-rate part (linear)
{
public:
    void prepare (const PreLine& l, double fs)
    {
        ls = design (BQ::LowShelf, l.lsF, l.lsQ, l.lsG, fs);
        const double K = std::tan (kPi * std::min (l.lpF, 0.45 * fs) / fs);
        lp.b0 = K / (1 + K); lp.b1 = lp.b0; lp.b2 = 0; lp.a1 = (K - 1) / (K + 1); lp.a2 = 0;
    }
    inline double process (double x) noexcept { return lp.process (ls.process (x)); }
    void reset() { ls.reset(); lp.reset(); }
private:
    Biquad ls, lp;
};

// ------------------------------------------------------------------- mono convenience wrapper
// One channel + its own detector, oversampling 1 (used by the offline tools).
struct RcaMono
{
    RcaStage st; RcaDetector det; double gin = 1;
    void prepare (const RcaUnit& u, double fs) { st.prepare (u, fs); det.prepare (u, fs); }
    void update (double inDb, bool dual, double filterHz) { gin = std::pow (10.0, inDb / 20.0); det.update (dual, true, filterHz); }
    inline double process (double x) noexcept
    {
        const double y = st.process (gin * x, det.gain(), det.gainReduction(), det.level());
        det.feed (&y, 1);
        return y;
    }
};

struct MonoModel
{
    Channel ch; Detector det;
    void prepare (const Unit& u, double fs) { ch.prepare (u, fs, 1); det.prepare (u, fs); }
    void update (const Settings& s) { ch.update (s); det.update (s, true); }
    inline double process (double x) noexcept
    {
        const double y = ch.pushPull (ch.front (x), det.gain(), det.gainReduction(), det.level());
        det.feed (&y, 1);
        return y / ch.inputGain();
    }
};

} // namespace rba
