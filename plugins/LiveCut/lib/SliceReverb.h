/*
 This file is part of Livecut (DPF port).

 Per-slice reverb throws.

 SliceReverbSends listens to the cutter: every cut rolls a chance; when it hits, that cut is sent to the
 reverb with a random send level and the tail gets a random decay time, both inside the user's min/max
 ranges. It has its own random generator so the cutter's random sequence is never affected.

 FDNReverb is an 8-line feedback delay network (Hadamard mixing) with a 4-stage allpass input diffuser,
 one-pole damping in the loop, smoothly gliding decay (so per-slice changes never click) and
 fractional, slowly gliding line lengths for the size control.

 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_SLICEREVERB_H
#define LIVECUT_SLICEREVERB_H

#include "BBCutter.h"
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------------------------------------------------

class SliceReverbSends : public BBCutListener
{
public:
    void SetOn(bool v)                         { on = v; if (!on) send = 0.f; }
    void SetChance(float v)                    { chance = v; }
    void SetSendRange(float a, float b)        { sendA = a; sendB = b; }
    void SetDecayRange(float a, float b)       { decayA = a; decayB = b; }

    float Send() const  { return send; }   // send level for the cut playing now (0 = dry)
    float Decay() const { return decay; }  // decay (RT60, seconds) of the current tail

    void OnCut(long, long) override
    {
        if (!on || rnd() >= chance)
        {
            send = 0.f;
            return;
        }
        send = sendA + (sendB - sendA) * rnd();
        const float la = std::log(decayA), lb = std::log(decayB);
        decay = std::exp(la + (lb - la) * rnd());
    }

private:
    float rnd() // xorshift32, 0..1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return float(state >> 8) / float(1u << 24);
    }

    bool on = false;
    float chance = 0.3f, sendA = 0.4f, sendB = 0.8f, decayA = 1.f, decayB = 3.f;
    float send = 0.f, decay = 2.f;
    uint32_t state = 0x2545F491u;
};

// ---------------------------------------------------------------------------------------------------------------------

// line lengths (ms, mutually prime-ish) and input allpass lengths
static const float kFdnBaseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 59.3f, 67.1f, 73.3f };
static const float kFdnApMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
static const float kFdnMaxSizeScale = 1.6f;

class FDNReverb
{
public:
    static constexpr int N = 8;

    void setSampleRate(double s)
    {
        sr = static_cast<float>(s);
        const float maxMs = kFdnBaseMs[N - 1] * kFdnMaxSizeScale + 4.f;
        bufLen = static_cast<int>(maxMs * 0.001f * sr) + 4;
        for (int i = 0; i < N; ++i)
            line[i].assign(bufLen, 0.f);
        for (int k = 0; k < 4; ++k)
        {
            apLen[k] = std::max(1, static_cast<int>(kFdnApMs[k] * 0.001f * sr));
            ap[k].assign(apLen[k], 0.f);
            apPos[k] = 0;
        }
        wpos = 0;
        reset();
        setSize(size, true);
    }

    void reset()
    {
        for (int i = 0; i < N; ++i)
        {
            std::fill(line[i].begin(), line[i].end(), 0.f);
            damp[i] = 0.f;
        }
        for (int k = 0; k < 4; ++k)
            std::fill(ap[k].begin(), ap[k].end(), 0.f);
    }

    // 0..1
    void setSize(float s, bool immediate = false)
    {
        size = s;
        const float scale = 0.45f + s * (kFdnMaxSizeScale - 0.45f);
        for (int i = 0; i < N; ++i)
        {
            lenTarget[i] = kFdnBaseMs[i] * scale * 0.001f * sr;
            if (immediate)
                len[i] = lenTarget[i];
        }
    }

    // 0..1 (0 = bright, 1 = dark)
    void setDamp(float d) { dampCoef = 0.05f + 0.8f * d; }

    // RT60 in seconds; glides there (log domain) over ~60 ms
    // (x1.13: the damping filter in the loop shortens the tail by about that much)
    void setDecay(float seconds) { decayTarget = std::log(std::max(0.05f, seconds) * 1.13f); }

    inline void process(float inL, float inR, float& outL, float& outR)
    {
        // decay + line length glide, gains refreshed every 32 samples
        if ((counter++ & 31) == 0)
        {
            decayLog += 0.04f * (decayTarget - decayLog);
            const float rt60 = std::exp(decayLog);
            for (int i = 0; i < N; ++i)
                gain[i] = std::pow(10.f, -3.f * len[i] / (rt60 * sr));
        }

        // input diffusion (mono)
        float x = 0.5f * (inL + inR);
        for (int k = 0; k < 4; ++k)
        {
            const float d = ap[k][apPos[k]];
            const float v = x + 0.62f * d;
            ap[k][apPos[k]] = v;
            x = d - 0.62f * v;
            if (++apPos[k] >= apLen[k])
                apPos[k] = 0;
        }

        // read the lines (fractional delay), damp
        float o[N];
        for (int i = 0; i < N; ++i)
        {
            // size changes glide at most 0.1 sample per sample (a brief, smooth pitch bend instead of a click)
            const float dl = lenTarget[i] - len[i];
            len[i] += dl > 0.1f ? 0.1f : (dl < -0.1f ? -0.1f : dl);

            float rp = static_cast<float>(wpos) - len[i];
            if (rp < 0.f)
                rp += static_cast<float>(bufLen);
            const int i0 = static_cast<int>(rp);
            const int i1 = (i0 + 1 >= bufLen) ? 0 : i0 + 1;
            const float fr = rp - static_cast<float>(i0);
            const float v = line[i][i0] + fr * (line[i][i1] - line[i][i0]);
            damp[i] = v + dampCoef * (damp[i] - v);
            if (std::fabs(damp[i]) < 1e-20f)
                damp[i] = 0.f;
            o[i] = damp[i];
        }

        // stereo taps (alternating signs decorrelate left and right)
        outL = 0.35f * (o[0] - o[2] + o[4] - o[6] + 0.5f * (o[1] + o[5]));
        outR = 0.35f * (o[1] - o[3] + o[5] - o[7] + 0.5f * (o[2] + o[6]));

        // Hadamard mixing (fast Walsh-Hadamard transform, normalised)
        float m[N];
        for (int i = 0; i < N; ++i)
            m[i] = o[i] * gain[i];
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = m[j], b = m[j + h];
                    m[j] = a + b;
                    m[j + h] = a - b;
                }
        const float norm = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < N; ++i)
            line[i][wpos] = m[i] * norm + x * ((i & 1) ? -0.5f : 0.5f);

        if (++wpos >= bufLen)
            wpos = 0;
    }

private:
    float sr = 44100.f;
    int bufLen = 1, wpos = 0;
    std::vector<float> line[N];
    float len[N] = {}, lenTarget[N] = {}, gain[N] = {}, damp[N] = {};
    std::vector<float> ap[4];
    int apLen[4] = { 1, 1, 1, 1 }, apPos[4] = {};
    float size = 0.6f, dampCoef = 0.45f;
    float decayLog = std::log(2.f), decayTarget = std::log(2.f);
    uint32_t counter = 0;
};

#endif
