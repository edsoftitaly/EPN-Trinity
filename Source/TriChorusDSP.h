// EPN Audio - Trinity Stereo Chorus
// Header-only DSP engine (no JUCE dependency, so it can be unit tested offline).
//
// Model of an early-80s rack tri-chorus:
//   mono input -> (-12 dB pad) -> 3 bucket-brigade delay lines (L / C / R)
//   each line swept by the same LFO set, phase-shifted 0 / 120 / 240 degrees
//   LFO "Preset"  = fixed slow sine sweep + small fast "vibrato" sine
//   LFO "Manual"  = sine, Rate 0.03-7.45 Hz, depth per line = Intensity I/II/III
//   Both LFO modes can be on together (their sweeps add up).
//   Stereo mode: Left out = dry + L + C,  Right out = dry + C + R
//   Mono mode  : both outs = dry + L + C + R
//
// BBD voice model: 4-pole anti-alias filter, soft clipping at the BBD input,
// low-level hiss, cubic-interpolated modulated delay, 4-pole reconstruction
// filter whose cutoff follows the virtual BBD clock (longer delay = darker),
// and a gentle low cut on the wet path.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace epn
{

constexpr double kTwoPi = 6.283185307179586;

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void setLowpass (double fs, double fc, double q)
    {
        fc = std::clamp (fc, 20.0, fs * 0.45);
        const double w0 = kTwoPi * fc / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cs) * 0.5) / a0);
        b1 = (float) ((1.0 - cs) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cs) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void reset() { z1 = z2 = 0.0f; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// 4th-order Butterworth low-pass as two cascaded biquads
struct Lowpass4
{
    Biquad s1, s2;
    void set (double fs, double fc)
    {
        s1.setLowpass (fs, fc, 0.54119610);
        s2.setLowpass (fs, fc, 1.30656296);
    }
    void reset() { s1.reset(); s2.reset(); }
    inline float process (float x) { return s2.process (s1.process (x)); }
};

struct OnePoleHP
{
    float a = 0.0f, xPrev = 0.0f, yPrev = 0.0f;
    void set (double fs, double fc) { a = (float) std::exp (-kTwoPi * fc / fs); }
    void reset() { xPrev = yPrev = 0.0f; }
    inline float process (float x)
    {
        const float y = a * (yPrev + x - xPrev);
        xPrev = x;
        yPrev = y;
        return y;
    }
};

struct Smoother
{
    float current = 0.0f, target = 0.0f, coeff = 0.0f;
    void setTime (double fs, double seconds) { coeff = (float) std::exp (-1.0 / (fs * seconds)); }
    void snap (float v) { current = target = v; }
    inline float next() { current = target + (current - target) * coeff; return current; }
};

//==============================================================================
class ModDelay
{
public:
    void prepare (double fs, double maxMs)
    {
        const int needed = (int) std::ceil (fs * maxMs * 0.001) + 8;
        int size = 1;
        while (size < needed) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

    inline void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // delay in samples, measured from the most recently pushed sample
    inline float read (float delaySamples) const
    {
        delaySamples = std::clamp (delaySamples, 2.0f, (float) (mask - 4));
        const float pos = (float) (writePos - 1) - delaySamples;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;

        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) (i & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];

        // 4-point, 3rd-order Hermite
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

//==============================================================================
struct TriChorusParams
{
    bool  power      = true;
    bool  effectOn   = true;
    bool  pad        = false;   // -12 dB input pad
    bool  stereo     = true;    // Chorus Mode
    bool  lfoPreset  = true;
    bool  lfoManual  = false;
    float intensity[3] { 0.5f, 0.5f, 0.5f }; // 0..1, manual LFO depth per line
    float rate       = 0.45f;   // 0..1 -> 0.03..7.45 Hz (log)
};

//==============================================================================
class TriChorusEngine
{
public:
    // ---- voicing constants (tweak here) ------------------------------------
    static constexpr double kBaseDelayMs[3]   { 7.6, 8.0, 8.4 };
    static constexpr double kManualMaxMs      = 3.5;   // manual sweep at Intensity max (+/-)
    static constexpr double kPresetSlowHz     = 0.52;
    static constexpr double kPresetSlowMs     = 1.6;
    static constexpr double kPresetVibHz      = 5.8;
    static constexpr double kPresetVibMs      = 0.17;
    static constexpr double kManualMinHz      = 0.03;
    static constexpr double kManualMaxHz      = 7.45;
    static constexpr double kBbdStages        = 1024.0; // MN3207-style
    static constexpr double kFilterCeilingHz  = 9000.0;
    static constexpr float  kWetGain          = 0.5f;
    static constexpr float  kNoiseLevel       = 6.0e-5f; // ~ -84 dBFS at BBD level
    static constexpr float  kPeakThreshold    = 0.7f;    // input level that lights PEAK

    static float rateToHz (float norm)
    {
        return (float) (kManualMinHz * std::pow (kManualMaxHz / kManualMinHz, (double) std::clamp (norm, 0.0f, 1.0f)));
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voices)
        {
            v.line.prepare (fs, 40.0);
            v.hp.set (fs, 70.0);
            v.inLp.set (fs, kFilterCeilingHz);
            v.outLp.set (fs, kFilterCeilingHz);
        }
        for (auto* s : { &presetAmt, &manualAmt, &stereoAmt, &activeAmt, &padAmt })
            s->setTime (fs, 0.02);
        for (auto& s : intensitySm) s.setTime (fs, 0.03);
        rateSm.setTime (fs, 0.05);
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
        {
            v.line.reset();
            v.inLp.reset();
            v.outLp.reset();
            v.hp.reset();
        }
        phSlow = phVib = phManual = 0.0;
        sampleCounter = 0;
        presetAmt.snap (params.lfoPreset ? 1.0f : 0.0f);
        manualAmt.snap (params.lfoManual ? 1.0f : 0.0f);
        stereoAmt.snap (params.stereo ? 1.0f : 0.0f);
        activeAmt.snap (params.power && params.effectOn ? 1.0f : 0.0f);
        padAmt.snap (params.pad ? 1.0f : 0.0f);
        for (int i = 0; i < 3; ++i) intensitySm[(size_t) i].snap (params.intensity[i]);
        rateSm.snap (rateToHz (params.rate));
    }

    void setParams (const TriChorusParams& p) { params = p; }

    // in/out may alias. inR may equal inL for mono input.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        presetAmt.target = params.lfoPreset ? 1.0f : 0.0f;
        manualAmt.target = params.lfoManual ? 1.0f : 0.0f;
        stereoAmt.target = params.stereo ? 1.0f : 0.0f;
        activeAmt.target = (params.power && params.effectOn) ? 1.0f : 0.0f;
        padAmt.target    = params.pad ? 1.0f : 0.0f;
        for (int i = 0; i < 3; ++i) intensitySm[(size_t) i].target = params.intensity[i];
        rateSm.target = rateToHz (params.rate);

        const double msToSamples = fs * 0.001;
        const double incSlow = kPresetSlowHz / fs;
        const double incVib  = kPresetVibHz / fs;
        static constexpr double third = 1.0 / 3.0;

        for (int n = 0; n < numSamples; ++n)
        {
            const float l = inL[n];
            const float r = inR[n];
            const float mono = 0.5f * (l + r);

            const float pAmt = presetAmt.next();
            const float mAmt = manualAmt.next();
            const float sAmt = stereoAmt.next();
            const float act  = activeAmt.next();
            const float pad  = padAmt.next();
            const float rateHz = rateSm.next();

            // pad: -12 dB into the BBD, +12 dB make-up after (headroom vs. noise trade-off)
            const float padIn  = 1.0f - 0.75f * pad;
            const float padOut = 1.0f / padIn;

            const float bbdIn = mono * padIn;
            if (std::abs (bbdIn) > kPeakThreshold) peakFlag = true;

            // LFOs
            phSlow   += incSlow;          if (phSlow   >= 1.0) phSlow   -= 1.0;
            phVib    += incVib;           if (phVib    >= 1.0) phVib    -= 1.0;
            phManual += rateHz / fs;      if (phManual >= 1.0) phManual -= 1.0;

            const bool updateFilters = (sampleCounter++ & 31) == 0;

            float wet[3];
            for (int v = 0; v < 3; ++v)
            {
                auto& vc = voices[(size_t) v];
                const double off = v * third;
                const double depthM = intensitySm[(size_t) v].next() * kManualMaxMs;

                double dMs = kBaseDelayMs[v];
                dMs += pAmt * (kPresetSlowMs * std::sin (kTwoPi * (phSlow + off))
                             + kPresetVibMs  * std::sin (kTwoPi * (phVib  + off)));
                dMs += mAmt * depthM * std::sin (kTwoPi * (phManual + off));

                if (updateFilters)
                {
                    // virtual BBD clock: f_clk = N / (2 * delay)
                    const double fclk = kBbdStages / (2.0 * dMs * 0.001);
                    const double fc = std::min (kFilterCeilingHz, 0.22 * fclk);
                    vc.inLp.set (fs, fc);
                    vc.outLp.set (fs, fc);
                }

                // BBD input: anti-alias filter, soft clip, hiss
                float x = vc.inLp.process (bbdIn);
                x = softClip (x) + kNoiseLevel * noise();
                vc.line.push (x);

                float y = vc.line.read ((float) (dMs * msToSamples));
                y = vc.outLp.process (y);
                y = vc.hp.process (y);
                wet[v] = y * padOut;
            }

            const float cMix = 0.70710678f;
            const float wetLst = wet[0] + cMix * wet[1];
            const float wetRst = wet[2] + cMix * wet[1];
            const float wetMono = 0.62f * (wet[0] + wet[1] + wet[2]);

            const float wL = kWetGain * (sAmt * wetLst + (1.0f - sAmt) * wetMono);
            const float wR = kWetGain * (sAmt * wetRst + (1.0f - sAmt) * wetMono);

            // effect on: mono dry + wet ; effect off / power off: untouched input
            outL[n] = act * (mono + wL) + (1.0f - act) * l;
            outR[n] = act * (mono + wR) + (1.0f - act) * r;
        }
    }

    bool consumePeak() { const bool p = peakFlag; peakFlag = false; return p; }

private:
    struct Voice
    {
        ModDelay line;
        Lowpass4 inLp, outLp;
        OnePoleHP hp;
    };

    static inline float softClip (float x)
    {
        // BBD + op-amp headroom: clean up to ~0.5, smooth limit around 1.0
        const float a = std::abs (x);
        if (a < 0.5f) return x;
        const float s = x > 0 ? 1.0f : -1.0f;
        return s * (0.5f + 0.5f * std::tanh ((a - 0.5f) * 2.0f));
    }

    inline float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    double fs = 48000.0;
    TriChorusParams params;
    std::array<Voice, 3> voices;
    Smoother presetAmt, manualAmt, stereoAmt, activeAmt, padAmt, rateSm;
    std::array<Smoother, 3> intensitySm;
    double phSlow = 0.0, phVib = 0.0, phManual = 0.0;
    uint32_t sampleCounter = 0;
    uint32_t rng = 0x1234567u;
    bool peakFlag = false;
};

} // namespace epn
