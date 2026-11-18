/*
 This file is part of Livecut (DPF port).

 Slice-synchronous filter modulation, in the spirit of the BitCrusher and Comb effects:
 at every block a random start and end cutoff are drawn (log-uniform) inside the user's
 min/max ranges, and each cut of the block gets a cutoff interpolated between the two.

 It uses its own random generator so it never changes the cutter's random sequence, and it
 draws nothing at all while a range is collapsed (min == max), e.g. at the default settings.

 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_SLICEFILTER_H
#define LIVECUT_SLICEFILTER_H

#include "BBCutter.h"
#include <cmath>
#include <cstdint>

class SliceFilter : public BBCutListener
{
public:
    void SetLowpassRange(float a, float b)  { lpA = a; lpB = b; }
    void SetHighpassRange(float a, float b) { hpA = a; hpB = b; }

    // cutoffs for the cut currently playing
    float Lowpass() const  { return lpCur; }
    float Highpass() const { return hpCur; }

    void OnBlock(long, long) override
    {
        pick(lpA, lpB, lpStart, lpEnd);
        pick(hpA, hpB, hpStart, hpEnd);
        lpCur = lpStart;
        hpCur = hpStart;
    }

    void OnCut(long cut, long numcuts) override
    {
        const float t = numcuts > 1 ? float(cut) / float(numcuts - 1) : 0.f;
        lpCur = std::exp(std::log(lpStart) + t * (std::log(lpEnd) - std::log(lpStart)));
        hpCur = std::exp(std::log(hpStart) + t * (std::log(hpEnd) - std::log(hpStart)));
    }

private:
    void pick(float a, float b, float& start, float& end)
    {
        if (std::fabs(a - b) < 1e-3f)
        {
            start = end = a;
            return;
        }
        const float la = std::log(a), lb = std::log(b);
        start = std::exp(la + (lb - la) * rnd());
        end   = std::exp(la + (lb - la) * rnd());
    }

    float rnd() // xorshift32, 0..1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return float(state >> 8) / float(1u << 24);
    }

    float lpA = 20000.f, lpB = 20000.f, hpA = 20.f, hpB = 20.f;
    float lpStart = 20000.f, lpEnd = 20000.f, hpStart = 20.f, hpEnd = 20.f;
    float lpCur = 20000.f, hpCur = 20.f;
    uint32_t state = 0x9E3779B9u;
};

#endif
