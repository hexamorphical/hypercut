/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_NOTETUNING_H
#define LIVECUT_NOTETUNING_H

#include <cmath>
#include <cstdint>

// Delay tuning: the delay time whose pitch (1 / time) is the nearest note of the scale (pcMask bit 0 = C).
// refA4 is the note grid's A4 in Hz (440 moved by the pitchmap's TUNE). A long time snaps to a sub-octave of a note,
// so it moves by at most a few percent. noteOut (optional) gets the MIDI note number (may be negative).
static inline double lvcTunedPeriod(double seconds, uint16_t pcMask, double refA4, int* noteOut = nullptr)
{
    if (! (seconds > 0.0) || (pcMask & 0x0FFF) == 0 || ! (refA4 > 0.0))
        return seconds;
    const double midi = 69.0 + 12.0 * std::log2(1.0 / (seconds * refA4));
    const int lo = static_cast<int>(std::floor(midi)) - 6, hi = static_cast<int>(std::ceil(midi)) + 6;
    int best = lo;
    double bestDist = 1e9;
    for (int m = lo; m <= hi; ++m)
    {
        if (((pcMask >> (((m % 12) + 12) % 12)) & 1u) == 0)
            continue;
        const double d = std::fabs(m - midi);
        if (d < bestDist) { bestDist = d; best = m; }
    }
    if (noteOut != nullptr)
        *noteOut = best;
    return 1.0 / (refA4 * std::exp2((best - 69) / 12.0));
}

static inline double lvcTuneRefA4(float tuneCents)
{
    return 440.0 * std::exp2(static_cast<double>(tuneCents) / 1200.0);
}

#endif
