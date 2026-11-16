// rca_render: offline renderer for the Vintage RCA unit (fitting/tests).
//   rca_render <model.txt> <in.f32> <out.f32> [gr.f32]
// keys: fs, T, k, knee, atk, atkD, relS, cplS, ratS, relD, cplD, ratD, k3, k3Lf, inDb, dual, filterHz
#include "../Source/RbaCore.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
using namespace rba;
int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: rca_render model.txt in.f32 out.f32 [gr.f32]\n"); return 1; }
    RcaUnit u; double fs = 96000, inDb = 0, dual = 0, filterHz = 0;
    std::ifstream mf (argv[1]); std::string line;
    std::map<std::string, double*> m = { { "fs", &fs }, { "T", &u.T }, { "k", &u.k }, { "knee", &u.knee }, { "atk", &u.atk }, { "atkD", &u.atkD },
        { "relS", &u.relS }, { "cplS", &u.cplS }, { "ratS", &u.ratS }, { "relD", &u.relD }, { "cplD", &u.cplD }, { "ratD", &u.ratD },
        { "k3", &u.k3 }, { "hyp", &u.hyp }, { "k3Lf", &u.k3Lf }, { "inDb", &inDb }, { "dual", &dual }, { "filterHz", &filterHz } };
    while (std::getline (mf, line))
    {
        std::istringstream is (line); std::string k; double v; if (! (is >> k >> v)) continue;
        auto it = m.find (k); if (it != m.end()) *it->second = v; else std::fprintf (stderr, "unknown key %s\n", k.c_str());
    }
    FILE* fi = std::fopen (argv[2], "rb"); FILE* fo = std::fopen (argv[3], "wb"); FILE* fg = argc > 4 ? std::fopen (argv[4], "wb") : nullptr;
    if (! fi || ! fo) { std::fprintf (stderr, "file error\n"); return 1; }
    RcaMono r; r.prepare (u, fs); r.update (inDb, dual > 0.5, filterHz);
    float buf[4096], ob[4096], gb[4096]; size_t n;
    while ((n = std::fread (buf, sizeof (float), 4096, fi)) > 0)
    {
        for (size_t i = 0; i < n; ++i) { ob[i] = (float) r.process (buf[i]); gb[i] = (float) r.det.gainReduction(); }
        std::fwrite (ob, sizeof (float), n, fo); if (fg) std::fwrite (gb, sizeof (float), n, fg);
    }
    std::fclose (fi); std::fclose (fo); if (fg) std::fclose (fg);
    return 0;
}
