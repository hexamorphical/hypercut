/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

/*
   Transient shaper: level-independent, no lookahead (zero latency).

   Detection runs on the louder channel. Everything is measured in dB, so the effect does not depend on how loud the
   input is, only on its shape:

   - transient = fast envelope minus a smoothed copy of it (~12 ms, both ways). It jumps up at each onset and
     falls back to zero once the copy has caught up, whatever the sound does next.
   - sustain   = long-release envelope minus short-release envelope (same fast attack). It grows while the sound
     decays, and is zero on the hit itself.
   - pump      = a slower follower of the transient signal, minus the transient signal itself: zero during the hit,
     positive right after it, decaying over ~80 ms. Ducking by it emphasises the hit without making it louder.

   Gain (dB) = attack * transient + sustain * sustain - pump * pump, smoothed over ~1 ms and kept in a safe range.
   Attack and sustain go from -1 to +1 (cut to boost), pump from 0 to 1. Cutting the sustain is stronger than
   boosting it (a boost that strong would blow up noise floors and reverb tails; a cut that strong gates them).
*/

#ifndef LIVECUT_TRANSIENTSHAPER_H
#define LIVECUT_TRANSIENTSHAPER_H

#include <algorithm>
#include <cmath>

class TransientShaper
{
public:
    void setSampleRate(double sr)
    {
        fs = sr > 1000.0 ? sr : 44100.0;
        auto coef = [&](double ms) { return static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * ms * fs))); };
        aFast = coef(0.3);  relAtt = coef(40.0);  aSlow = coef(12.0);  // transient: fast envelope + smoothed copy
        rShort = coef(30.0); rLong = coef(300.0); aSus = coef(0.3);   // sustain pair
        pAtt = coef(4.0);   pRel = coef(80.0);                        // pump follower
        gSmooth = coef(1.0);
        gFast = coef(0.1);
        reset();
    }

    void reset()
    {
        eFast = eSlow = sShort = sLong = 0.f;
        pump = 0.f;
        gainDb = 0.f;
    }

    void setAttack(float v)  { attack = std::max(-1.f, std::min(1.f, v)); }
    void setPump(float v)    { pumpAmt = std::max(0.f, std::min(1.f, v)); }
    void setSustain(float v) { sustain = std::max(-1.f, std::min(1.f, v)); }

    // current gain change in dB (for a meter)
    float gain() const { return gainDb; }

    void process(float& l, float& r)
    {
        const float x = std::max(std::fabs(l), std::fabs(r));

        // transient pair: different attacks, same release
        eFast += (x > eFast ? aFast : relAtt) * (x - eFast);
        eSlow += aSlow * (eFast - eSlow); // catches up in both directions, so the transient always ends
        // sustain pair: same fast attack, different releases
        sShort += (x > sShort ? aSus : rShort) * (x - sShort);
        sLong  += (x > sLong  ? aSus : rLong)  * (x - sLong);

        const float transient = std::max(0.f, db(eFast) - db(eSlow));
        // a new hit ends the previous tail at once (the sustain reading would otherwise linger for a moment)
        const float tail = std::max(0.f, db(sLong) - db(sShort)) * std::max(0.f, 1.f - transient / 4.f);
        pump += (transient > pump ? pAtt : pRel) * (transient - pump);
        const float after = std::max(0.f, pump - transient);

        float target = kAttackScale * attack * std::min(transient, 30.f)
                     + (sustain >= 0.f ? kSustainScale * sustain * std::min(tail, 18.f)          // boost: gentle
                                       : kSustainCutScale * sustain * std::min(tail, 36.f))  // cut: can empty the tail
                     - kPumpScale * pumpAmt * std::min(after, 30.f);
        // nothing to shape in silence: keep the gain from running away on noise
        if (sLong < 1e-5f)
            target = 0.f;
        target = std::max(-48.f, std::min(18.f, target));
        // smooth, except on a new hit, where the shape of the previous tail must not spill into it
        gainDb += (transient > 1.f ? gFast : gSmooth) * (target - gainDb);

        const float g = std::pow(10.f, gainDb * 0.05f);
        l *= g;
        r *= g;
    }

private:
    static float db(float v) { return 20.f * std::log10(std::max(v, 1e-6f)); }

    // dB of gain per dB of detected transient / tail / post-transient, at 100 %
    static constexpr float kAttackScale  = 0.9f;
    static constexpr float kSustainScale = 0.7f;
    static constexpr float kSustainCutScale = 1.8f;
    static constexpr float kPumpScale    = 1.2f;

    double fs = 44100.0;
    float aFast = 0.f, aSlow = 0.f, relAtt = 0.f, rShort = 0.f, rLong = 0.f, aSus = 0.f, pAtt = 0.f, pRel = 0.f, gSmooth = 0.f, gFast = 0.f;
    float eFast = 0.f, eSlow = 0.f, sShort = 0.f, sLong = 0.f, pump = 0.f, gainDb = 0.f;
    float attack = 0.f, pumpAmt = 0.f, sustain = 0.f;
};

#endif
