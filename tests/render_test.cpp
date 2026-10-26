// Offline sanity test for the Trinity DSP engine.
// Build: g++ -O2 -std=c++17 -I../Source render_test.cpp -o render_test
#include "TriChorusDSP.h"
#include <cstdio>
#include <cstring>
#include <string>

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size();
    const uint32_t dataBytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::clamp (L[i] * 32767.0f, -32768.0f, 32767.0f);
        int16_t b = (int16_t) std::clamp (R[i] * 32767.0f, -32768.0f, 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

// plucked-ish guitar arpeggio
static std::vector<float> makeInput (int fs, double seconds)
{
    const int n = (int) (fs * seconds);
    std::vector<float> x ((size_t) n, 0.0f);
    const double notes[] = { 164.81, 246.94, 329.63, 392.0, 493.88, 392.0, 329.63, 246.94 };
    const int step = fs / 4;
    for (int k = 0; k * step < n; ++k)
    {
        const double f0 = notes[k % 8];
        for (int i = k * step; i < n; ++i)
        {
            const double t = (i - k * step) / (double) fs;
            const double env = std::exp (-t * 2.5);
            double s = 0;
            for (int h = 1; h <= 8; ++h) s += std::sin (epn::kTwoPi * f0 * h * t) / h;
            x[(size_t) i] += (float) (0.12 * env * s);
        }
    }
    return x;
}

struct Stats { float peak = 0; bool finite = true; double corr = 1; double maxLRdiff = 0; };

static Stats run (epn::TriChorusParams p, const std::vector<float>& in, int fs,
                  std::vector<float>& L, std::vector<float>& R)
{
    epn::TriChorusEngine e;
    e.setParams (p);
    e.prepare (fs);
    L.assign (in.size(), 0); R.assign (in.size(), 0);
    const int block = 256;
    for (size_t i = 0; i < in.size(); i += block)
    {
        const int nb = (int) std::min<size_t> (block, in.size() - i);
        e.process (&in[i], &in[i], &L[i], &R[i], nb);
    }
    Stats s;
    double sll = 0, srr = 0, slr = 0;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) s.finite = false;
        s.peak = std::max ({ s.peak, std::abs (L[i]), std::abs (R[i]) });
        s.maxLRdiff = std::max (s.maxLRdiff, (double) std::abs (L[i] - R[i]));
        sll += L[i] * L[i]; srr += R[i] * R[i]; slr += L[i] * R[i];
    }
    s.corr = slr / std::sqrt (sll * srr + 1e-30);
    return s;
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what); if (! ok) ++fails; };

    for (int fs : { 44100, 48000, 96000 })
    {
        auto in = makeInput (fs, 6.0);
        std::vector<float> L, R;
        std::printf ("--- fs = %d\n", fs);

        epn::TriChorusParams p; // defaults: stereo, preset LFO
        auto s = run (p, in, fs, L, R);
        std::printf ("    preset/stereo: peak %.3f corr %.3f\n", s.peak, s.corr);
        check (s.finite, "preset stereo finite");
        check (s.corr < 0.97 && s.corr > 0.3, "preset stereo is wide but not out of phase");
        if (fs == 48000) writeWav (outDir + "/preset_stereo.wav", L, R, fs);

        p.stereo = false;
        s = run (p, in, fs, L, R);
        check (s.maxLRdiff < 1e-6, "mono mode: L == R");

        p.stereo = true; p.lfoPreset = false; p.lfoManual = true;
        p.rate = 1.0f; p.intensity[0] = p.intensity[1] = p.intensity[2] = 1.0f;
        s = run (p, in, fs, L, R);
        std::printf ("    manual max: peak %.3f corr %.3f\n", s.peak, s.corr);
        check (s.finite && s.peak < 1.5f, "manual max rate/intensity stays sane");
        if (fs == 48000) writeWav (outDir + "/manual_fast.wav", L, R, fs);

        p.lfoPreset = true; p.rate = 0.75f; p.intensity[0] = 0.3f; p.intensity[1] = 0.6f; p.intensity[2] = 0.9f;
        s = run (p, in, fs, L, R);
        check (s.finite, "preset + manual together finite");
        if (fs == 48000) writeWav (outDir + "/preset_plus_manual.wav", L, R, fs);

        p.effectOn = false;
        s = run (p, in, fs, L, R);
        double err = 0; for (size_t i = 0; i < in.size(); ++i) err = std::max (err, (double) std::abs (L[i] - in[i]));
        check (err < 1e-6, "effect off = clean bypass");

        // hot input: pad off clips more than pad on
        std::vector<float> hot (in.size()); for (size_t i = 0; i < in.size(); ++i) hot[i] = in[i] * 4.0f;
        epn::TriChorusParams h; h.lfoPreset = false; h.stereo = false;
        auto sNoPad = run (h, hot, fs, L, R); auto ref = L;
        h.pad = true; auto sPad = run (h, hot, fs, L, R);
        std::printf ("    hot in: peak no-pad %.3f  pad %.3f\n", sNoPad.peak, sPad.peak);
        check (sPad.peak > sNoPad.peak, "pad gives more headroom on hot input");

        // silence: noise floor
        std::vector<float> zero (in.size(), 0.0f);
        epn::TriChorusParams z;
        run (z, zero, fs, L, R);
        double rms = 0; for (auto v : L) rms += v * v; rms = std::sqrt (rms / L.size());
        std::printf ("    noise floor %.1f dBFS\n", 20 * std::log10 (rms + 1e-12));
        check (rms < 1e-3, "noise floor below -60 dBFS");
    }

    std::printf (fails ? "\n%d FAILURES\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}
