// FreeBandLimit: reusable ultrasonic band-limiting for the 396 plug-ins (FreeLA2, RBA-6A, FreeComp, Free609 ...).
//
// A Butterworth low-pass split into its 2-pole sections, which can be placed anywhere in a processing chain
// ("slots"). The linear result is the full filter; where each section sits changes what each nonlinear stage
// sees (gentle sections early darken what a stage is driven with; resonant sections late restore the air).
//
// Families:
//   HYP  Airwindows Hypersonic / HypersonX: 7 sections at 25 kHz (14th-order Butterworth),
//        section Qs and cutoff from the Airwindows source, Copyright (c) airwindows / Chris Johnson, MIT licence.
//   B4   4 sections at 25 kHz (8th-order Butterworth).
//   B5   5 sections at 21 kHz (10th-order Butterworth, the Airwindows UltrasonX / Ultrasonic Qs).
// Sections are named A (most resonant) .. last (gentlest).
//
// Slot spec: tokens separated by spaces. "HYP" = a complete Hypersonic set; "B4:D" = section D of B4;
// "HYP:G" = section G of Hypersonic; "B4" = a complete B4 set. Empty string = nothing.
// The dry path for a dry/wet mix is every section of every slot in one chain (same linear response).
#pragma once
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

namespace fbl
{
constexpr double kPi = 3.14159265358979323846;

struct Family { const char* name; int n; const double* q; double hz; };
inline const Family* families (int& count)
{
    static const double hyp[7] = { 4.46570214, 1.51387132, 0.93979296, 0.70710678, 0.59051105, 0.52972649, 0.50316379 };
    static const double b4[4]  = { 2.56291545, 0.89997622, 0.60134489, 0.50979558 };
    static const double b5[5]  = { 3.19622661, 1.10134463, 0.70710678, 0.56116312, 0.50623256 };
    static const Family f[3] = { { "HYP", 7, hyp, 25000.0 }, { "B4", 4, b4, 25000.0 }, { "B5", 5, b5, 21000.0 } };
    count = 3; return f;
}
inline const Family* findFamily (const std::string& name)
{
    int n = 0; const Family* f = families (n);
    for (int i = 0; i < n; ++i) if (name == f[i].name) return &f[i];
    return nullptr;
}

struct Section
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0, q = 0.7071, hz = 25000;
    void design (double fs)     // Airwindows' bilinear low-pass (cutoff clamped to 0.49 fs)
    {
        const double K = std::tan (kPi * std::min (hz / fs, 0.49)), n = 1.0 / (1.0 + K / q + K * K);
        b0 = K * K * n; b1 = 2 * b0; b2 = b0; a1 = 2 * (K * K - 1) * n; a2 = (1 - K / q + K * K) * n;
    }
    inline double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0; }
    double groupDelaySec() const { return 1.0 / (q * 2.0 * kPi * hz); }   // low-frequency group delay
};

class Chain
{
public:
    void clear() { s.clear(); }
    void add (double q, double hz, double fs) { Section x; x.q = q; x.hz = hz; x.design (fs); s.push_back (x); }
    void append (const Chain& o) { for (auto x : o.s) { x.reset(); s.push_back (x); } }
    inline double process (double x) noexcept { for (auto& b : s) x = b.process (x); return x; }
    void reset() { for (auto& b : s) b.reset(); }
    bool empty() const { return s.empty(); }
    double groupDelaySec() const { double g = 0; for (auto& b : s) g += b.groupDelaySec(); return g; }
    // parse a slot spec (see top of file); unknown tokens are ignored
    void parse (const std::string& spec, double fs)
    {
        std::istringstream in (spec); std::string tok;
        while (in >> tok)
        {
            const auto c = tok.find (':');
            const Family* f = findFamily (tok.substr (0, c));
            if (! f) continue;
            if (c == std::string::npos) { for (int i = 0; i < f->n; ++i) add (f->q[i], f->hz, fs); continue; }
            for (size_t k = c + 1; k < tok.size(); ++k)
            {
                const int i = tok[k] - 'A';
                if (i >= 0 && i < f->n) add (f->q[i], f->hz, fs);
            }
        }
    }
private:
    std::vector<Section> s;
};

// one channel of band-limiting with N slots plus the matching dry chain
template <int N>
class BandLimit
{
public:
    void configure (const char* const* specs, double fs)
    {
        dry.clear();
        for (int k = 0; k < N; ++k)
        {
            slot[k].clear();
            if (specs) slot[k].parse (specs[k] ? specs[k] : "", fs);
            dry.append (slot[k]);
        }
    }
    void off() { for (auto& c : slot) c.clear(); dry.clear(); }
    inline double process (int k, double x) noexcept { return slot[k].process (x); }
    inline double processDry (double x) noexcept { return dry.process (x); }
    void reset() { for (auto& c : slot) c.reset(); dry.reset(); }
    double groupDelaySec() const { return dry.groupDelaySec(); }
    bool active() const { return ! dry.empty(); }
private:
    Chain slot[(size_t) N], dry;
};
} // namespace fbl
