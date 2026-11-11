// rba_render: offline renderer for fitting/tests.
//   rba_render <model.txt> <in.f32> <out.f32> [gr.f32]
// in/out: raw mono float32 at the rate given in model.txt ("fs"), oversampling 1.
// model.txt: "key value..." lines; EQ bands as "bump TYPE f Q g" / "flat TYPE f Q g".
#include "../Source/RbaCore.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
using namespace rba;
static BQ bq (const std::string& t)
{
    if (t == "hp") return BQ::HP; if (t == "lp") return BQ::LP; if (t == "pk") return BQ::Peak;
    if (t == "hs") return BQ::HighShelf; if (t == "ls") return BQ::LowShelf; return BQ::HP1;
}
int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: rba_render model.txt in.f32 out.f32 [gr.f32]\n"); return 1; }
    Unit u; Settings s; double fs = 96000;
    std::ifstream mf (argv[1]); std::string line;
    while (std::getline (mf, line))
    {
        std::istringstream is (line); std::string k; if (! (is >> k) || k[0] == '#') continue;
        if (k == "bump" || k == "flat") { std::string t; EQBand b; is >> t >> b.f >> b.Q >> b.g; b.type = bq (t); (k == "bump" ? u.bump : u.flat).push_back (b); continue; }
        if (k == "attackKnots") { for (double& v : u.attackKnots) is >> v; continue; }
        if (k == "releaseKnots") { for (double& v : u.releaseKnots) is >> v; continue; }
        double v; is >> v;
        std::map<std::string, double*> m = {
            { "fs", &fs }, { "bumpGainDb", &u.bumpGainDb }, { "flatGainDb", &u.flatGainDb },
            { "col_fc", &u.colour.fc }, { "col_gamma", &u.colour.gamma }, { "col_a2", &u.colour.a2 }, { "col_a3", &u.colour.a3 }, { "col_L", &u.colour.L }, { "col_p", &u.colour.p }, { "col_asym", &u.colour.asym },
            { "drv_fc", &u.drive.fc }, { "drv_gamma", &u.drive.gamma }, { "drv_a2", &u.drive.a2 }, { "drv_a3", &u.drive.a3 }, { "drv_L", &u.drive.L }, { "drv_p", &u.drive.p }, { "drv_asym", &u.drive.asym },
            { "T0", &u.T0 }, { "slope", &u.slope }, { "knee", &u.knee }, { "detPow", &u.detPow }, { "scQ", &u.scQ }, { "scScale", &u.scScale },
            { "sqK", &u.sqK }, { "sqT", &u.sqT }, { "sqW", &u.sqW }, { "sqAtk", &u.sqAtk }, { "sqRel", &u.sqRel }, { "k3max", &u.k3max }, { "k3tau", &u.k3tau }, { "k3base", &u.k3base }, { "thump", &u.thump },
            { "inputDb", &s.inputDb }, { "threshold", &s.threshold }, { "attack", &s.attack }, { "release", &s.release },
            { "filterHz", &s.filterHz }, { "makeupDb", &s.makeupDb }, { "mix", &s.mix }, { "debump", &s.debump }, { "balance", &s.balance } };
        if (k == "power") { s.power = v > 0.5; continue; }
        if (k == "preInside") { s.preInside = v > 0.5; continue; }
        auto it = m.find (k); if (it != m.end()) *it->second = v; else std::fprintf (stderr, "unknown key %s\n", k.c_str());
    }
    FILE* fi = std::fopen (argv[2], "rb"); FILE* fo = std::fopen (argv[3], "wb"); FILE* fg = argc > 4 ? std::fopen (argv[4], "wb") : nullptr;
    if (! fi || ! fo) { std::fprintf (stderr, "file error\n"); return 1; }
    MonoModel ch; ch.prepare (u, fs); ch.update (s);
    const double mk = std::pow (10.0, s.makeupDb / 20.0);
    float buf[4096], ob[4096], gb[4096]; size_t n;
    while ((n = std::fread (buf, sizeof (float), 4096, fi)) > 0)
    {
        for (size_t i = 0; i < n; ++i)
        {
            const double y = ch.process (buf[i]) * mk;
            ob[i] = (float) (s.mix * y + (1 - s.mix) * buf[i]);
            gb[i] = (float) ch.det.gainReduction();
        }
        std::fwrite (ob, sizeof (float), n, fo); if (fg) std::fwrite (gb, sizeof (float), n, fg);
    }
    std::fclose (fi); std::fclose (fo); if (fg) std::fclose (fg);
    return 0;
}
