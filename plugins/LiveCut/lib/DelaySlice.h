/*
  This file is part of Livecut (DPF port).

  Per-slice delay feedback and echo amount.

  The comb's TIME already draws a random start/end inside a min/max range at each block and
  glides between them over the block's cuts. FEEDBACK and the echo's amount/MIX work the same
  way here: two range sliders set the bounds, every block rolls a fresh pair of end points,
  and each cut gets a value interpolated between them.

  It uses its own random generator so it never changes the cutter's or the other effects'
  random sequences.

  Livecut can be redistributed and/or modified under the terms of the
  GNU General Public License, as published by the Free Software Foundation;
  either version 2 of the License, or (at your option) any later version.
  */

#ifndef LIVECUT_DELAY_SLICE_H
#define LIVECUT_DELAY_SLICE_H

#include "BBCutter.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

class SliceDelay : public BBCutListener
{
public:
    void SetFeedbackRange(float a, float b) { fbA = a; fbB = b; }
    void SetMixRange(float a, float b)      { mixA = a; mixB = b; }

    // Values chosen for the cut currently playing (before the caller's own smoothing).
    float Feedback() const { return feedback; }
    float Mix() const      { return mix; }

    void OnBlock(long, long) override
    {
        startFb  = pick(fbA, fbB);
        endFb    = pick(fbA, fbB);
        startMix = pick(mixA, mixB);
        endMix   = pick(mixA, mixB);
        feedback = startFb;
        mix      = startMix;
    }

    void OnCut(long cut, long numcuts) override
    {
        const float t = numcuts > 1 ? static_cast<float>(cut) / static_cast<float>(numcuts - 1) : 0.f;
        feedback = startFb + (endFb - startFb) * t;
        mix      = startMix + (endMix - startMix) * t;
    }

private:
    float pick(float a, float b)
    {
        const float lo = std::min(a, b), hi = std::max(a, b);
        return lo + (hi - lo) * rnd();
    }

    float rnd() // xorshift32, 0..1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
    }

    float fbA = 0.5f, fbB = 0.5f, mixA = 0.35f, mixB = 0.35f;
    float startFb = 0.5f, endFb = 0.5f, startMix = 0.35f, endMix = 0.35f;
    float feedback = 0.5f, mix = 0.35f;
    uint32_t state = 0x1A2B3C4Du;
};

#endif
