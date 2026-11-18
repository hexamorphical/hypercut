/*
 This file is part of Livecut (DPF port).

 Stereo lowpass -> highpass filter pair based on the topology-preserving
 transform state-variable filter (Andrew Simper / Cytomic, "SvfLinearTrapOptimised2").

 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef SVFILTER_H
#define SVFILTER_H

#include <cmath>

class SVFilter
{
public:
    enum Mode { kLowpass = 0, kHighpass };

    explicit SVFilter(Mode m = kLowpass) : mode(m) { reset(); }

    void reset()
    {
        ic1eqL = ic2eqL = ic1eqR = ic2eqR = 0.0;
    }

    void setSampleRate(double s) { sr = s; dirty = true; }

    // cutoff in Hz, res 0..1
    void set(double cutoff, double res)
    {
        if (cutoff == fc && res == q)
            return;
        fc = cutoff;
        q = res;
        dirty = true;
    }

    inline void tick(float& l, float& r)
    {
        if (dirty)
            update();

        process(l, ic1eqL, ic2eqL);
        process(r, ic1eqR, ic2eqR);
    }

private:
    inline void process(float& x, double& ic1eq, double& ic2eq) const
    {
        const double v0 = x;
        const double v3 = v0 - ic2eq;
        const double v1 = a1 * ic1eq + a2 * v3;
        const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        if (mode == kLowpass)
            x = static_cast<float>(v2);
        else
            x = static_cast<float>(v0 - k * v1 - v2);

        // flush denormals
        if (std::fabs(ic1eq) < 1e-20) ic1eq = 0.0;
        if (std::fabs(ic2eq) < 1e-20) ic2eq = 0.0;
    }

    void update()
    {
        dirty = false;
        const double nyq = sr * 0.49;
        const double f = fc < 10.0 ? 10.0 : (fc > nyq ? nyq : fc);
        // res 0 -> Q 0.707 (butterworth, flat), res 1 -> Q ~ 6 (+15.6 dB peak)
        const double Q = 0.7071 * std::pow(8.5, q);
        const double g = std::tan(M_PI * f / sr);
        k = 1.0 / Q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    Mode mode;
    double sr = 44100.0;
    double fc = 1000.0, q = 0.0;
    double k = 1.4142, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1eqL, ic2eqL, ic1eqR, ic2eqR;
    bool dirty = true;
};

#endif
