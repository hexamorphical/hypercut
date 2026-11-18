/*
 This file is part of Livecut (DPF port).

 Stereo-linked lookahead peak limiter.

 Gain computer: the gain needed to keep each sample under the ceiling goes
 through a sliding-window minimum and then a box filter of the same length.
 With the audio delayed by (window - 1) samples, this guarantees the gain has
 fully reached its target by the time the peak comes out, with a smooth
 attack and no overshoot. Release is a one-pole recovery.

 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_LIMITER_H
#define LIVECUT_LIMITER_H

#include <algorithm>
#include <cmath>
#include <vector>

class Limiter
{
public:
    // lookahead in seconds; returns latency in samples
    unsigned setup(double sampleRate, double lookahead = 0.0015)
    {
        sr = sampleRate;
        delay = std::max<unsigned>(1u, static_cast<unsigned>(sr * lookahead));
        window = delay + 1;
        bufL.assign(delay, 0.f);
        bufR.assign(delay, 0.f);
        gainReq.assign(window, 1.f);
        boxBuf.assign(window, 1.f);
        reset();
        setRelease(releaseMs);
        return delay;
    }

    void reset()
    {
        std::fill(bufL.begin(), bufL.end(), 0.f);
        std::fill(bufR.begin(), bufR.end(), 0.f);
        std::fill(gainReq.begin(), gainReq.end(), 1.f);
        std::fill(boxBuf.begin(), boxBuf.end(), 1.f);
        boxSum = static_cast<double>(window);
        env = 1.f;
        dpos = wpos = 0;
    }

    unsigned latency() const { return delay; }

    void setCeilingDb(float db) { ceiling = std::pow(10.f, db / 20.f); }

    void setRelease(float ms)
    {
        releaseMs = ms;
        relCoef = 1.f - std::exp(-1.f / (std::max(1.f, ms) * 0.001f * static_cast<float>(sr)));
    }

    void setEnabled(bool on) { enabled = on; }

    // current gain (linear, 1 = no reduction)
    float gain() const { return env; }

    inline void tick(float& l, float& r)
    {
        // ---- gain computer on the incoming (undelayed) signal ----
        const float peak = std::max(std::fabs(l), std::fabs(r));
        const float g = (enabled && peak > ceiling) ? ceiling / peak : 1.f;

        gainReq[wpos] = g;
        float mn = 1.f;
        for (unsigned i = 0; i < window; ++i)
            mn = std::min(mn, gainReq[i]);

        boxSum += static_cast<double>(mn) - static_cast<double>(boxBuf[wpos]);
        boxBuf[wpos] = mn;
        if (++wpos >= window)
        {
            wpos = 0;
            // re-sum once per window to stop floating point drift
            double s = 0.0;
            for (unsigned i = 0; i < window; ++i)
                s += boxBuf[i];
            boxSum = s;
        }
        const float smoothed = static_cast<float>(boxSum / window);

        if (smoothed < env)
            env = smoothed;
        else
            env += relCoef * (smoothed - env);

        // ---- delay line ----
        const float dl = bufL[dpos], dr = bufR[dpos];
        bufL[dpos] = l;
        bufR[dpos] = r;
        if (++dpos >= delay)
            dpos = 0;

        if (enabled)
        {
            l = std::max(-ceiling, std::min(ceiling, dl * env));
            r = std::max(-ceiling, std::min(ceiling, dr * env));
        }
        else
        {
            l = dl;
            r = dr;
        }
    }

private:
    double sr = 44100.0;
    unsigned delay = 1, window = 2;
    unsigned dpos = 0, wpos = 0;
    std::vector<float> bufL, bufR, gainReq, boxBuf;
    double boxSum = 0.0;
    float env = 1.f;
    float ceiling = 1.f;
    float releaseMs = 120.f;
    float relCoef = 0.001f;
    bool enabled = true;
};

#endif
