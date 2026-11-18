// UnitRca.h — the Vintage RCA unit, the Clipper and the Preamp, fitted to the Nebula BA-6A captures
// (RCA capture kit, 96 kHz). Values from fit/fit_rca.py and fit/fit_clip_pre.py.
#pragma once
#include "RbaCore.h"

namespace rba
{
inline RcaUnit makeRca()
{
    RcaUnit u;
    u.T = -7.2269864; u.k = 7.2491653; u.knee = 0.11457444;
    u.atk = 0.003660287; u.relS = 0.52932739; u.cplS = 10.224515; u.ratS = 0.69255626;
    u.atkD = 0.00037483615; u.relD = 0.17273871; u.cplD = 0.026410729; u.ratD = 13.847609;
    u.hyp = 0.86994812; u.k3 = 0; u.k3Lf = 0;
    u.inA = -14.0; u.inB = 0.93;      // Threshold 0..40 -> input gain, as the Nebula Input knob measured
    return u;
}
// Threshold knob (0..40) -> RCA input gain (dB), matched to the El Rey unit: at every Threshold the RCA
// gives the same average gain reduction as El Rey on a drum loop and on drums + noise (fit/match_thr.py),
// so switching units keeps roughly the same amount of compression.
inline double rcaInputGainDb (double threshold)
{
    static const double T[] = { 0, 10, 15, 20, 25, 30, 35, 40 };
    static const double G[] = { -4.979, -1.679, -0.329, 1.399, 4.022, 7.668, 11.517, 14.948 };
    constexpr int n = (int) (sizeof (T) / sizeof (T[0]));
    if (threshold <= T[0]) return G[0] + (threshold - T[0]) * (G[1] - G[0]) / (T[1] - T[0]);
    for (int i = 1; i < n; ++i)
        if (threshold <= T[i]) return G[i - 1] + (threshold - T[i - 1]) * (G[i] - G[i - 1]) / (T[i] - T[i - 1]);
    return G[n - 1] + (threshold - T[n - 1]) * (G[n - 1] - G[n - 2]) / (T[n - 1] - T[n - 2]);
}
inline ClipUnit makeClip()
{
    ClipUnit c; c.O = 22.046044; c.p = 1.8808637; c.s = 1; c.gamma = 1.7619359; c.fc = 574.25496;
    return c;
}
inline PreUnit makePre()
{
    PreUnit p;
    p.L = { 9.6636625, 1.0900909, -5.9999867, 27387.027, 0.0 };
    p.R = { 9.0552293, 2.2021657, -6, 28405.539, 8.0044225 };
    p.c = 0.51475848; p.Lim = 7.7995915; p.p = 2.4744828; p.asym = 0.21782795;
    p.rLeadUs = 2.222149;
    return p;
}
} // namespace rba
