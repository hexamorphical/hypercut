/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_GATEENVELOPE_H
#define LIVECUT_GATEENVELOPE_H

// Trance gate step envelope, shared by the DSP and the UI so the drawing matches the sound.
// Positions are fractions of the step: t = 0..1 through the step, a = attack, d = decay (0 = none).
// Attack fades the step in, decay then falls from the peak to silence.
// tension -1..1 bends both like an automation curve: 0 = straight, > 0 bulges up (fast rise / holds before the
// drop), < 0 sags down (slow rise / drops fast with a long tail).
#include <cmath>

static inline float GateCurveExponent(float tension)
{
  const float k = tension < 0.f ? -tension : tension;
  return 1.f + 5.f * k;
}

static inline float GateEnvelope(float t, float a, float d, float tension = 0.f)
{
  const float e = GateCurveExponent(tension);
  if(a > 0.f && t < a)
  {
    const float x = t / a;
    if(tension == 0.f) return x;
    return tension > 0.f ? 1.f - std::pow(1.f - x, e) : std::pow(x, e);
  }
  if(d > 0.f)
  {
    const float x = (t - a) / d;
    if(x >= 1.f) return 0.f;
    if(tension == 0.f) return 1.f - x;
    return tension > 0.f ? 1.f - std::pow(x, e) : std::pow(1.f - x, e);
  }
  return 1.f;
}

// Stored levels 0..15 -> fraction of the step. Squared, so the short (punchy) times have the finest steps.
static const int kGateEnvLevels = 16;

static inline float GateEnvFraction(int level)
{
  if(level <= 0) return 0.f;
  if(level >= kGateEnvLevels - 1) return 1.f;
  const float x = float(level) / float(kGateEnvLevels - 1);
  return x * x;
}

#endif
