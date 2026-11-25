// PhaseAlign: exact alignment and optional linear phase for band-limiting / oversampling filters.
// Reusable in the 396 plug-ins (FreeLA2, RBA-6A, FreeComp, Free609 ...). No JUCE.
//
// Measure the impulse response h of the LINEAR part you want to align (oversampling filters + ultrasonic
// low-pass sections), at the session rate. Then:
//   ANALOG : a fractional delay so the low-frequency delay lands exactly on a whole number of samples
//            (the filters keep their analog-style phase in the top octave).
//   LINEAR : a correction FIR that removes the filters' phase entirely (same level response), so the
//            whole chain is a pure delay in the audio band. Costs about 1 ms of latency.
#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>

namespace pa
{
constexpr double kPi = 3.14159265358979323846;
using cd = std::complex<double>;

inline void fft (std::vector<cd>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2 * kPi / (double) len * (inverse ? 1 : -1);
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const cd u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double) n;
}

inline double besselI0 (double x)
{
    double s = 1, t = 1;
    for (int k = 1; k < 50; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-12 * s) break; }
    return s;
}

// low-frequency (DC) group delay of an impulse response, in samples
inline double dcDelay (const std::vector<double>& h)
{
    double s = 0, m = 0;
    for (size_t n = 0; n < h.size(); ++n) { s += h[n]; m += (double) n * h[n]; }
    return std::abs (s) > 1e-9 ? m / s : 0.0;
}

// windowed-sinc fractional delay: delay = D + frac samples, 2D+1 taps (Kaiser, unity DC gain)
inline std::vector<double> fracDelayFir (int D, double frac)
{
    std::vector<double> c ((size_t) (2 * D + 1), 0.0);
    if (frac < 1e-6) { c[(size_t) D] = 1.0; return c; }
    const double beta = 7.0, d = D + frac, i0b = besselI0 (beta);
    double sum = 0;
    for (int n = 0; n <= 2 * D; ++n)
    {
        const double x = n - d, s = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
        const double r = (n - D) / (double) D, w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        c[(size_t) n] = s * w; sum += c[(size_t) n];
    }
    for (auto& v : c) v /= sum;
    return c;
}

// linear-phase correction for the response h (delay tau = dcDelay(h)). P = taps before the main tap (pre-response room),
// R = taps after. The corrected chain h * c is |H| with a pure delay of P + ceil(tau) samples.
inline std::vector<double> linearPhaseFir (const std::vector<double>& h, double tau, int P, int R)
{
    const int L = P + R + 1;
    size_t NF = 1; while (NF < (size_t) std::max (8 * L, (int) h.size() * 2)) NF <<= 1;
    std::vector<cd> H (NF, cd (0));
    for (size_t n = 0; n < h.size() && n < NF; ++n) H[n] = h[n];
    fft (H, false);
    const double Dtot = P + std::ceil (tau);
    std::vector<cd> C (NF);
    for (size_t k = 0; k < NF; ++k)
    {
        const double w = 2 * kPi * (double) (k <= NF / 2 ? (double) k : (double) k - (double) NF) / (double) NF;
        const double mag = std::abs (H[k]);
        C[k] = mag > 1e-5 ? std::conj (H[k]) / mag * std::polar (1.0, -w * Dtot)
                          : std::polar (1.0, -w * (Dtot - tau));      // stop band: the delay only
    }
    fft (C, true);
    std::vector<double> c ((size_t) L);
    const double alpha = 0.3;                                          // Tukey taper at both ends
    for (int n = 0; n < L; ++n)
    {
        const double x = (double) n / (L - 1);
        double w = 1.0;
        if (x < alpha / 2) w = 0.5 * (1 + std::cos (2 * kPi / alpha * (x - alpha / 2)));
        else if (x > 1 - alpha / 2) w = 0.5 * (1 + std::cos (2 * kPi / alpha * (x - 1 + alpha / 2)));
        c[(size_t) n] = C[(size_t) n].real() * w;
    }
    return c;
}

// a short FIR on a circular buffer
struct Fir
{
    std::vector<double> c, buf; int w = 0;
    void prepare (int maxLen) { buf.assign ((size_t) std::max (1, maxLen), 0.0); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0); }
    inline double process (const std::vector<double>& h, double x) noexcept
    {
        const int N = (int) buf.size();
        buf[(size_t) w] = x;
        double y = 0; int r = w;
        const int L = (int) h.size();
        for (int k = 0; k < L; ++k) { y += h[(size_t) k] * buf[(size_t) r]; r = (r == 0) ? N - 1 : r - 1; }
        w = (w + 1 == N) ? 0 : w + 1;
        return y;
    }
};
} // namespace pa
