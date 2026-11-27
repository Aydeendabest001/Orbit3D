#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>
#include <vector>

// Lightweight real-time binaural renderer (spherical-head model + pinna cues).
//  * ITD        : Woodworth interaural time difference via fractional delay lines
//  * ILD        : head-shadow low-pass + gain on the far ear
//  * Front/back : rear sources lose highs and presence (~3 kHz), front gets presence
//  * Elevation  : pinna peak (7.5 kHz) + elevation-tracking notch (6..10 kHz)
//  * Distance   : inverse-distance gain + air absorption
// Listener sits at the origin facing +Z. +X is right, +Y is up.
namespace orbit
{
constexpr float kPi = 3.14159265358979f;

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setPeak (float fs, float f, float q, float gainDb)
    {
        f = std::min (f, 0.45f * fs);
        const float A     = std::pow (10.0f, gainDb / 40.0f);
        const float w0    = 2.0f * kPi * f / fs;
        const float alpha = std::sin (w0) / (2.0f * q);
        const float c     = std::cos (w0);
        const float a0    = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * c) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * c) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void copyCoeffs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0; }
};

struct SpatialSettings
{
    float distance = 0.6f;  // 0..1  inverse-distance amount
    float air      = 0.5f;  // 0..1  high-frequency loss with distance
    float backCue  = 0.8f;  // 0..1  front/back colouration
    float heightCue = 0.8f; // 0..1  elevation colouration
};

class BinauralSource
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        buffer.assign (4096, 0.0f);
        mask = 4095;
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        wp = 0;
        for (auto* e : { &earL, &earR }) { e->lp1 = e->lp2 = 0; e->peak.reset(); e->notch.reset(); e->pres.reset(); }
        first = true;
    }

    // Adds the spatialised signal for one mono source into outL / outR.
    void process (const float* in, float* outL, float* outR, int n,
                  float tx, float ty, float tz, const SpatialSettings& st)
    {
        constexpr int chunk = 16;
        int i = 0;

        if (first)
        {
            px = tx; py = ty; pz = tz;
            cur = compute (px, py, pz, st);
            first = false;
        }

        while (i < n)
        {
            const int c = std::min (chunk, n - i);
            const float k = 1.0f - std::exp (-(float) c / (sr * 0.012f));
            px += (tx - px) * k;
            py += (ty - py) * k;
            pz += (tz - pz) * k;

            const Params np = compute (px, py, pz, st);

            earL.a = lpCoef (np.fcL);
            earR.a = lpCoef (np.fcR);
            earL.peak.setPeak (sr, 7500.0f, 1.1f, np.peakDb);
            earL.notch.setPeak (sr, np.notchHz, 4.0f, np.notchDb);
            earL.pres.setPeak (sr, 3000.0f, 0.8f, np.presDb);
            earR.peak.copyCoeffs (earL.peak);
            earR.notch.copyCoeffs (earL.notch);
            earR.pres.copyCoeffs (earL.pres);

            for (int j = 0; j < c; ++j)
            {
                const float t = (float) (j + 1) / (float) c;
                const float dL = cur.dL + (np.dL - cur.dL) * t;
                const float dR = cur.dR + (np.dR - cur.dR) * t;
                const float gL = cur.gL + (np.gL - cur.gL) * t;
                const float gR = cur.gR + (np.gR - cur.gR) * t;

                buffer[(size_t) (wp & mask)] = in[i + j];

                float l = readFrac (dL);
                float r = readFrac (dR);

                earL.lp1 += earL.a * (l - earL.lp1);
                earL.lp2 += earL.a * (earL.lp1 - earL.lp2);
                earR.lp1 += earR.a * (r - earR.lp1);
                earR.lp2 += earR.a * (earR.lp1 - earR.lp2);

                l = earL.pres.process (earL.notch.process (earL.peak.process (earL.lp2)));
                r = earR.pres.process (earR.notch.process (earR.peak.process (earR.lp2)));

                wp = (wp + 1) & mask;
                outL[i + j] += l * gL;
                outR[i + j] += r * gR;
            }
            cur = np;
            i += c;
        }
    }

private:
    struct Params { float dL, dR, gL, gR, fcL, fcR, peakDb, notchHz, notchDb, presDb; };
    struct Ear { float lp1 = 0, lp2 = 0, a = 1; Biquad peak, notch, pres; };

    float lpCoef (float fc) const { return 1.0f - std::exp (-2.0f * kPi * fc / sr); }

    float readFrac (float delaySamples) const
    {
        const float pos = (float) wp - delaySamples;
        const int   i0  = (int) std::floor (pos);
        const float f   = pos - (float) i0;
        return buffer[(size_t) (i0 & mask)] * (1.0f - f)
             + buffer[(size_t) ((i0 + 1) & mask)] * f;
    }

    Params compute (float x, float y, float z, const SpatialSettings& st) const
    {
        const float d  = std::sqrt (x * x + y * y + z * z);
        const float dc = std::max (d, 0.25f);

        const float lat   = d > 1.0e-4f ? juce::jlimit (-1.0f, 1.0f, x / d) : 0.0f; // sin(lateral angle)
        const float front = d > 1.0e-4f ? juce::jlimit (-1.0f, 1.0f, z / d) : 1.0f;
        const float sinEl = d > 1.0e-4f ? juce::jlimit (-1.0f, 1.0f, y / d) : 0.0f;
        const float rear  = std::max (0.0f, -front);

        // ITD (Woodworth), head radius 8.75 cm
        const float theta = std::asin (lat);
        const float itd   = (0.0875f / 343.0f) * (theta + lat) * sr; // samples, + = source on the right
        Params p {};
        p.dL = 2.0f + std::max (0.0f, itd);
        p.dR = 2.0f + std::max (0.0f, -itd);

        // head shadow on the far ear
        const float shadowR = (1.0f - lat) * 0.5f;
        const float shadowL = (1.0f + lat) * 0.5f;

        const float rearFactor = std::pow (0.30f, rear * st.backCue);
        const float air = 1.0f / (1.0f + 0.20f * std::max (0.0f, dc - 1.0f) * st.air);

        auto cutoff = [&] (float shadow)
        {
            const float fc = 18000.0f * std::pow (1500.0f / 18000.0f, shadow) * rearFactor * air;
            return juce::jlimit (700.0f, 0.45f * sr, fc);
        };
        p.fcL = cutoff (shadowL);
        p.fcR = cutoff (shadowR);

        const float dist = std::pow (1.0f / std::max (dc, 1.0f), st.distance);
        p.gL = std::min (1.25f, dist * (1.0f - 0.40f * shadowL) / 0.8f);
        p.gR = std::min (1.25f, dist * (1.0f - 0.40f * shadowR) / 0.8f);

        // pinna / elevation
        p.peakDb  = 6.5f * sinEl * st.heightCue;
        p.notchHz = 6000.0f + 4200.0f * (sinEl + 1.0f) * 0.5f;
        p.notchDb = -8.0f * st.heightCue * (1.0f - 0.5f * std::abs (lat));

        // front/back presence: +3 dB in front, -3 dB behind
        p.presDb = 3.0f * front * st.backCue;
        return p;
    }

    float sr = 44100.0f;
    std::vector<float> buffer;
    int mask = 4095;
    int wp = 0;
    bool first = true;
    float px = 0, py = 0, pz = 1;
    Params cur {};
    Ear earL, earR;
};
}
