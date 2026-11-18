/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

/*
   Stereo echo with long delay times (the delay module's ECHO mode).

   - Mono: both channels use the left time (stereo image kept). Stereo: own time per side. Ping-pong: the input
     (summed) enters the left line, each repeat crosses to the other side.
   - Time changes glide like tape (smoothed fractional read position) instead of jumping.
   - Feedback goes through a one-pole lowpass (TONE: each repeat darker) and a soft clipper, so high feedback
     saturates (above full scale only) instead of exploding.
   - Switching on never replays old audio: taps older than what was written since restart() read as silence,
     so the buffer never has to be cleared on the audio thread.
*/

#ifndef LIVECUT_ECHODELAY_H
#define LIVECUT_ECHODELAY_H

#include <algorithm>
#include <cmath>
#include <vector>

class EchoDelay
{
public:
  enum Mode { kMono = 0, kStereo, kPingPong };
  static constexpr double kMaxSeconds = 8.0; // 2 bars at 60 bpm

  // allocates: call outside the audio thread (activate)
  void SetSampleRate(double v)
  {
    sr = v > 0.0 ? v : 44100.0;
    long n = 1;
    while(n < long(sr * kMaxSeconds) + 8) n <<= 1;
    bufL.assign(size_t(n), 0.f);
    bufR.assign(size_t(n), 0.f);
    mask = n - 1;
    maxDelay = double(n - 4);
    glide = float(1.0 - std::exp(-1.0 / (0.08 * sr))); // ~80 ms tape glide
    SetTone(tone);
    Restart();
  }

  // forget what is in the buffer (it is ignored until overwritten) and jump straight to the target times
  void Restart()
  {
    fresh = 0;
    dL = targetL;
    dR = targetR;
    lpL = lpR = 0.f;
  }

  void SetTimes(double samplesL, double samplesR)
  {
    targetL = clampDelay(samplesL);
    targetR = clampDelay(samplesR);
  }

  void SetMode(int v) { mode = v; }
  void SetFeedback(float v) { feedback = v < 0.f ? 0.f : (v > 0.95f ? 0.95f : v); }

  // 0 = bright (~18 kHz) .. 1 = dark (~1.2 kHz)
  void SetTone(float v)
  {
    tone = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    const double fc = 18000.0 * std::pow(1200.0 / 18000.0, double(tone));
    damp = float(1.0 - std::exp(-2.0 * 3.14159265358979 * std::min(fc, 0.45 * sr) / sr));
  }

  // dryGain / wetGain: the mix (smoothed by the caller)
  inline void process(float &l, float &r, float dryGain, float wetGain)
  {
    dL += glide * (targetL - dL);
    dR += glide * (targetR - dR);
    const double readL = dL;                         // left time (also mono and ping-pong)
    const double readR = mode == kStereo ? dR : dL;  // right has its own time in stereo mode only

    const float tl = tap(bufL, readL);
    const float tr = tap(bufR, readR);
    lpL += damp * (tl - lpL);
    lpR += damp * (tr - lpR);

    float wl, wr;
    if(mode == kPingPong)
    {
      wl = 0.5f * (l + r) + feedback * lpR; // bounce: right repeat comes back on the left
      wr = feedback * lpL;                  // left repeat goes to the right
    }
    else
    {
      wl = l + feedback * lpL;
      wr = r + feedback * lpR;
    }
    bufL[size_t(w)] = soft(wl);
    bufR[size_t(w)] = soft(wr);
    w = (w + 1) & mask;
    if(fresh <= mask) ++fresh;

    l = l * dryGain + lpL * wetGain;
    r = r * dryGain + lpR * wetGain;
  }

  inline double MaxDelaySamples() const { return maxDelay; }
  // the TONE lowpass delays each repeat a little: its phase delay at hz, in samples (subtract it to tune the loop)
  inline double LoopLatency(double hz) const
  {
    const double k = 1.0 - double(damp);
    const double w = 2.0 * 3.14159265358979 * std::max(1.0, hz) / sr;
    return std::atan2(k * std::sin(w), 1.0 - k * std::cos(w)) / w;
  }

private:
  double clampDelay(double d) const { return d < 1.0 ? 1.0 : (d > maxDelay ? maxDelay : d); }

  // linear-interpolated read d samples back; silence for anything not written since Restart()
  inline float tap(const std::vector<float> &b, double d) const
  {
    if(double(fresh) < std::ceil(d)) // the oldest sample needed was written before Restart()
      return 0.f;
    const double pos = double(w) - d;
    const double fl = std::floor(pos);
    const long i0 = long(fl);
    const float frac = float(pos - fl);
    const float a = b[size_t(i0 & mask)];
    const float c = b[size_t((i0 + 1) & mask)];
    return a + frac * (c - a);
  }

  // clean up to full scale, then a soft knee towards +-2 (only runaway feedback gets squashed)
  static inline float soft(float x)
  {
    const float a = std::fabs(x);
    if(a <= 1.f)
      return x;
    const float over = a - 1.f;
    const float y = 1.f + over / (1.f + over);
    return x < 0.f ? -y : y;
  }

  std::vector<float> bufL, bufR;
  long mask = 0, w = 0, fresh = 0;
  double sr = 44100.0, maxDelay = 1.0;
  double targetL = 1000.0, targetR = 1000.0, dL = 1000.0, dR = 1000.0;
  float glide = 0.001f, damp = 0.5f, tone = 0.3f, feedback = 0.5f;
  float lpL = 0.f, lpR = 0.f;
  int mode = kPingPong;
};

#endif
