/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

/*
   Spectral resonator ("Chroma"): pulls the active frequency region onto a key / scale.

   Every partial of the region is assigned to its nearest grid note (A4 = 440 Hz + tune). The note is moved, with
   the pitch offset applied, to the nearest note of the scale by a frequency shift of (target - source) Hz, so
   in-key material is left alone and the small detunings of a performance survive.

   Engine: a peak-locked phase vocoder (Laroche & Dolson). Spectral peaks are found in each frame, their true
   frequency comes from the phase advance between frames, and each peak's region of bins is moved by its own shift.
   The phase rotation of a shift is accumulated per source note, so it stays continuous from frame to frame.
   Partials only need to be a few bins apart, so chords work. Latency: one frame (2048 samples at 44.1 / 48 kHz).
   (A zero-latency filter-bank engine was tried and dropped: one-pole bands spread each partial over many bands,
   so chords could not be moved cleanly.)

   Color 0..1 crossfades the moved material in place of the original; 1..2 adds narrow resonators on the in-key
   notes of the region, ringing longer as the knob goes up. Gain scales the region.

   Spectral stage (optional, latency N = 512 or 1024): STFT of the dry signal and of the output. Morph moves the
   output magnitudes toward the dry ones, keeping the output phase. The gate silences each output bin whose dry bin
   is below the threshold, which shortens ringing and tails.
*/

#ifndef LIVECUT_SPECTRALRESONATOR_H
#define LIVECUT_SPECTRALRESONATOR_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// In-place radix-2 complex FFT (unscaled both ways). Tables are built in setup(), never on the audio thread.
class ChromaFFT
{
public:
    void setup(int n)
    {
        N = n;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        cosT.assign(static_cast<size_t>(n / 2), 0.f);
        sinT.assign(static_cast<size_t>(n / 2), 0.f);
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = 2.0 * M_PI * i / n;
            cosT[static_cast<size_t>(i)] = static_cast<float>(std::cos(a));
            sinT[static_cast<size_t>(i)] = static_cast<float>(std::sin(a));
        }
        rev.assign(static_cast<size_t>(n), 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[static_cast<size_t>(i)] = r;
        }
    }

    int size() const { return N; }

    void transform(float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < N; ++i)
        {
            const int j = rev[static_cast<size_t>(i)];
            if (j > i)
            {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        const float sgn = inverse ? 1.f : -1.f;
        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len >> 1, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[static_cast<size_t>(k * step)];
                    const float wi = sgn * sinT[static_cast<size_t>(k * step)];
                    const int a = i + k, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                }
        }
    }

private:
    int N = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

class SpectralResonator
{
public:
    static constexpr int kNotes    = 128;    // one entry per MIDI note
    static constexpr int kBlock    = 32;     // decision / visualisation rate
    static constexpr int kNumFFT   = 5;      // 512 .. 8192
    static constexpr int kMaxN     = 8192;   // largest frame (high quality at 192 kHz)
    static constexpr int kMaxBins  = kMaxN / 2 + 1;

    SpectralResonator()
    {
        for (int i = 0; i < kNumFFT; ++i)
        {
            const int n = 512 << i;
            fft[i].setup(n);
            win[i].assign(static_cast<size_t>(n), 0.f);
            for (int k = 0; k < n; ++k)
                win[i][static_cast<size_t>(k)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * k / n)); // periodic Hann
        }
        setSampleRate(44100.0);
    }

    // ---- setup (not real-time) ------------------------------------------------------------------------------------

    void setSampleRate(double sr)
    {
        fs = sr > 1000.0 ? sr : 44100.0;
        srShift = fs > 100000.0 ? 2 : (fs > 50000.0 ? 1 : 0); // frames grow with the sample rate
        smoothCoef = 1.0 - std::exp(-1.0 / (0.02 * fs));        // ~20 ms parameter smoothing
        visDecay = static_cast<float>(std::exp(-kBlock / (0.12 * fs)));
        dirtyBands = dirtyMap = true;
        reset();
    }

    void reset()
    {
        resetPV();
        resetSpectral();
        std::memset(rzr, 0, sizeof(rzr));
        std::memset(rzi, 0, sizeof(rzi));
        resRunning = false;
        std::memset(dryLevel, 0, sizeof(dryLevel));
        std::memset(tunedLevel, 0, sizeof(tunedLevel));
        blockPos = 0;
        c1s = std::min(1.f, color);
        c2s = std::max(0.f, color - 1.f);
        gs = gainTarget;
        dirtyBands = dirtyMap = true;
    }

    // ---- settings (audio thread, between samples) -------------------------------------------------------------------

    // absolute pitch classes, bit 0 = C (0 = no notes: treated as chromatic, nothing moves)
    void setPitchClasses(uint16_t mask)
    {
        mask &= 0x0FFF;
        if (mask != pcMask) { pcMask = mask; dirtyMap = true; }
    }
    void setColor(float c) { color = std::max(0.f, std::min(2.f, c)); }
    void setPitch(float semis)
    {
        if (semis != pitch) { pitch = semis; dirtyMap = true; }
    }
    void setTune(float cents)
    {
        if (cents != tune) { tune = cents; dirtyBands = dirtyMap = true; }
    }
    void setGainDb(float db) { gainTarget = static_cast<float>(std::pow(10.0, db / 20.0)); }
    void setRange(float lo, float hi)
    {
        if (lo != rangeLo || hi != rangeHi) { rangeLo = lo; rangeHi = hi; dirtyBands = dirtyMap = true; }
    }
    // fftIndex 0 = 512, 1 = 1024 (doubled at 88.2 / 96 kHz, quadrupled above)
    void setSpectral(int fftIndex, bool morphOn, float morph, bool gateOn, float gateDb)
    {
        fftIndex = fftIndex > 0 ? 1 : 0;
        const bool on = morphOn || gateOn;
        if (on != specOn || fftIndex != specSel)
        {
            specOn = on;
            specSel = fftIndex;
            resetSpectral();
        }
        doMorph = morphOn;
        morphAmt = std::max(0.f, std::min(1.f, morph));
        doGate = gateOn;
        gateThr = static_cast<float>(std::pow(10.0, gateDb / 20.0));
    }

    // total latency in samples: phase vocoder frame + spectral stage frame
    int latency() const { return pvSize() + (specOn ? specSize() : 0); }

    uint16_t pitchClasses() const { return pcMask; }
    int regionLow() const { return noteLo; }
    int regionHigh() const { return noteHi; }

    // ---- processing -------------------------------------------------------------------------------------------------

    void process(float& l, float& r)
    {
        if (blockPos == 0)
            beginBlock();

        c1s += static_cast<float>(smoothCoef) * (std::min(1.f, color) - c1s);
        c2s += static_cast<float>(smoothCoef) * (std::max(0.f, color - 1.f) - c2s);
        gs  += static_cast<float>(smoothCoef) * (gainTarget - gs);

        const float in[2] = { l, r };
        float out[2], dry[2];
        pvTick(in, out, dry);

        if (resRunning)
            resonate(out);

        if (specOn)
            spectralTick(dry, out);

        l = out[0];
        r = out[1];

        if (++blockPos >= kBlock)
        {
            blockPos = 0;
            endBlock();
        }
    }

    // ---- visualisation: per-note levels (linear amplitude), refreshed at block / frame rate -------------------------
    float dryLevel[kNotes];   // what the region holds, by source note
    float tunedLevel[kNotes]; // what was moved, at its destination note

private:
    int pvSize() const { return 2048 << srShift; }
    int pvIndex() const { return 2 + srShift; }
    int specSize() const { return 512 << (specSel + srShift); }
    int specIndex() const { return specSel + srShift; }

    double noteHz(double note) const { return refHz * std::pow(2.0, (note - 69.0) / 12.0); }

    // ---- block-rate housekeeping --------------------------------------------------------------------------------

    void beginBlock()
    {
        if (dirtyBands) rebuildBands();
        if (dirtyMap) rebuildMap();
        updateResonators();
    }

    void endBlock()
    {
        for (int m = 0; m < kNotes; ++m)
        {
            dryLevel[m] *= visDecay;
            tunedLevel[m] *= visDecay;
        }

        // resonator tails: stop iterating notes that have died out
        if (resRunning)
        {
            double energy = 0.0;
            for (int i = 0; i < numRes; ++i)
            {
                const int n = resNotes[i];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double e = rzr[ch][n] * rzr[ch][n] + rzi[ch][n] * rzi[ch][n];
                    if (e < 1e-24) rzr[ch][n] = rzi[ch][n] = 0.0;
                    energy += e;
                }
            }
            if (c2s < 1e-5f && energy < 1e-18)
            {
                resRunning = false;
                std::memset(rzr, 0, sizeof(rzr));
                std::memset(rzi, 0, sizeof(rzi));
            }
        }
    }

    void rebuildBands()
    {
        dirtyBands = false;
        refHz = 440.0 * std::pow(2.0, tune / 1200.0);
        const double lo = std::max(10.0, static_cast<double>(std::min(rangeLo, rangeHi)));
        const double hi = std::min(0.45 * fs, static_cast<double>(std::max(rangeLo, rangeHi)));
        int nlo = static_cast<int>(std::ceil(69.0 + 12.0 * std::log2(lo / refHz)));
        int nhi = static_cast<int>(std::floor(69.0 + 12.0 * std::log2(hi / refHz)));
        nlo = std::max(0, std::min(kNotes - 1, nlo));
        nhi = std::max(-1, std::min(kNotes - 1, nhi));
        if (hi <= lo) nhi = nlo - 1;
        regionLoHz = lo;
        regionHiHz = hi;

        for (int n = 0; n < kNotes; ++n)
            omega[n] = 2.0 * M_PI * noteHz(n) / fs;
        noteLo = nlo;
        noteHi = nhi;
    }

    // nearest note of the pitch-class mask to a (fractional) note; ties go down
    static int nearestInMask(double v, uint16_t mask)
    {
        const int base = static_cast<int>(std::floor(v));
        int best = base;
        double bestD = 1e9;
        for (int d = -7; d <= 8; ++d)
        {
            const int n = base + d;
            const int pc = ((n % 12) + 12) % 12;
            if ((mask >> pc) & 1u)
            {
                const double dist = std::fabs(n - v);
                if (dist < bestD - 1e-9) { bestD = dist; best = n; }
            }
        }
        return best;
    }

    void rebuildMap()
    {
        dirtyMap = false;
        const uint16_t mask = pcMask != 0 ? pcMask : 0x0FFF;
        const double maxHz = 0.47 * fs;
        numMoving = 0;
        for (int m = 0; m < kNotes; ++m)
        {
            int t = nearestInMask(m + pitch, mask);
            while (t > kNotes - 1 || (t > m && noteHz(t) > maxHz)) t -= 12;
            while (t < 0) t += 12;
            target[m] = t;
            dOmega[m] = omega[t] - omega[m];
            isMoving[m] = t != m;
            if (isMoving[m])
                moving[numMoving++] = m;
        }
        // in-key resonators sit on the scale notes inside the region
        for (int n = 0; n < kNotes; ++n)
            resFeed[n] = pcMask != 0 && n >= noteLo && n <= noteHi && ((pcMask >> (n % 12)) & 1u);
    }

    // ---- engine: peak-locked phase vocoder -----------------------------------------------------------

    void resetPV()
    {
        std::memset(pvIn, 0, sizeof(pvIn));
        std::memset(pvOut, 0, sizeof(pvOut));
        std::memset(pvAcc, 0, sizeof(pvAcc));
        std::memset(pvPrevR, 0, sizeof(pvPrevR));
        std::memset(pvPrevI, 0, sizeof(pvPrevI));
        std::memset(pvDry, 0, sizeof(pvDry));
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < kMaxBins; ++k)
                pvLast[0][ch][k] = pvLast[1][ch][k] = -1;
        pvFlip = 0;
        std::memset(theta, 0, sizeof(theta));
        const int n = pvSize();
        pvRover = n - n / 4;
        pvDryPos = 0;
    }

    void pvTick(const float* in, float* out, float* dry)
    {
        const int n = pvSize();
        const int lat0 = n - n / 4;
        for (int ch = 0; ch < 2; ++ch)
        {
            pvIn[ch][pvRover] = in[ch];
            out[ch] = pvOut[ch][pvRover - lat0];
            // dry delayed by one frame, aligned with the output
            dry[ch] = pvDry[ch][pvDryPos];
            pvDry[ch][pvDryPos] = in[ch];
        }
        if (++pvDryPos >= n)
            pvDryPos = 0;
        if (++pvRover >= n)
        {
            pvRover = lat0;
            pvFrame();
        }
    }

    void pvFrame()
    {
        const int idx = pvIndex();
        const ChromaFFT& f = fft[idx];
        const float* w = win[idx].data();
        const int n = f.size(), hop = n / 4, half = n / 2;
        const double twoPi = 2.0 * M_PI;

        // shift phases advance one hop for every moving note, whether or not it sounds right now
        for (int i = 0; i < numMoving; ++i)
        {
            const int m = moving[i];
            theta[m] = std::fmod(theta[m] + dOmega[m] * hop, twoPi);
        }

        // both channels in one FFT: L in the real part, R in the imaginary part
        for (int k = 0; k < n; ++k)
        {
            bufR[k] = w[k] * pvIn[0][k];
            bufI[k] = w[k] * pvIn[1][k];
        }
        f.transform(bufR, bufI, false);
        for (int ch = 0; ch < 2; ++ch)
            std::memmove(pvIn[ch], pvIn[ch] + hop, sizeof(float) * static_cast<size_t>(n - hop));
        for (int k = 0; k <= half; ++k)
        {
            const int nk = (n - k) & (n - 1);
            xr[0][k] = 0.5f * (bufR[k] + bufR[nk]);
            xi[0][k] = 0.5f * (bufI[k] - bufI[nk]);
            xr[1][k] = 0.5f * (bufI[k] + bufI[nk]);
            xi[1][k] = -0.5f * (bufR[k] - bufR[nk]);
        }

        const int kLo = std::max(2, static_cast<int>(std::ceil(regionLoHz * n / fs)));
        const int kHi = std::min(half - 2, static_cast<int>(std::floor(regionHiHz * n / fs)));
        const float c1 = c1s, g = gs;
        const float levelScale = 4.f / n; // full-scale sine peak bin -> 1
        const double toNote = fs / (twoPi * refHz);
        const int cur = pvFlip, prev = pvFlip ^ 1;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* Xr = xr[ch];
            const float* Xi = xi[ch];
            float* Yr = yr[ch];
            float* Yi = yi[ch];
            int* lastPrev = pvLast[prev][ch];
            int* lastCur = pvLast[cur][ch];

            float peakMax = 0.f;
            for (int k = 0; k <= half; ++k)
            {
                mag2[k] = Xr[k] * Xr[k] + Xi[k] * Xi[k];
                Yr[k] = Xr[k];
                Yi[k] = Xi[k];
                lastCur[k] = -1;
                if (k >= kLo && k <= kHi && mag2[k] > peakMax) peakMax = mag2[k];
            }
            const float floor2 = std::max(peakMax * 1e-8f, 1e-12f * n * n); // -80 dB below the loudest, absolute floor

            // peaks of the region
            int numPeaks = 0;
            for (int k = kLo; k <= kHi; ++k)
            {
                const float m = mag2[k];
                if (m > floor2 && m > mag2[k - 1] && m >= mag2[k + 1] && m > mag2[k - 2] && m >= mag2[k + 2])
                    peaks[numPeaks++] = k;
            }

            for (int i = 0; i < numPeaks; ++i)
            {
                const int p = peaks[i];

                // region of influence: from the valley below to the valley above
                int lo = p, hi = p;
                if (i == 0)
                    while (lo > 1 && lo > p - 16 && mag2[lo - 1] < mag2[lo]) --lo;
                else
                {
                    lo = peaks[i - 1] + 1;
                    for (int k = lo; k < p; ++k)
                        if (mag2[k] < mag2[lo]) lo = k;
                    lo += 1; // the valley bin itself belongs to the peak below
                    if (lo > p) lo = p;
                }
                if (i == numPeaks - 1)
                    while (hi < half - 1 && hi < p + 16 && mag2[hi + 1] < mag2[hi]) ++hi;
                else
                {
                    hi = p;
                    for (int k = p + 1; k < peaks[i + 1]; ++k)
                        if (mag2[k] < mag2[hi]) hi = k;
                }

                // true frequency from the phase advance since the previous frame
                const double cr = Xr[p] * pvPrevR[ch][p] + Xi[p] * pvPrevI[ch][p];
                const double ci = Xi[p] * pvPrevR[ch][p] - Xr[p] * pvPrevI[ch][p];
                double dphi = std::atan2(ci, cr) - twoPi * p * hop / n;
                dphi -= twoPi * std::floor(dphi / twoPi + 0.5);
                const double wTrue = twoPi * p / n + dphi / hop;
                if (wTrue <= 0.0)
                    continue;
                const double note = 69.0 + 12.0 * std::log2(wTrue * toNote);

                // hysteresis: keep the note this partial had in the previous frame (it may have moved a bin)
                int last = lastPrev[p];
                if (last < 0) last = lastPrev[p - 1];
                if (last < 0) last = lastPrev[p + 1];
                int s = (last >= 0 && std::fabs(note - last) < 0.5 + kHysteresis)
                      ? last : static_cast<int>(std::lround(note));
                s = std::max(0, std::min(kNotes - 1, s));
                lastCur[p - 1] = lastCur[p] = lastCur[p + 1] = s;

                const float level = std::sqrt(mag2[p]) * levelScale;
                if (level > dryLevel[s]) dryLevel[s] = level;

                if (! isMoving[s])
                    continue;
                if (level * c1 > tunedLevel[target[s]]) tunedLevel[target[s]] = level * c1;

                // move the whole region: integer bin shift, exact frequency through the accumulated phase
                const int db = static_cast<int>(std::lround(dOmega[s] * n / twoPi));
                const float rr = static_cast<float>(std::cos(theta[s]));
                const float ri = static_cast<float>(std::sin(theta[s]));
                for (int k = lo; k <= hi; ++k)
                {
                    Yr[k] -= Xr[k];
                    Yi[k] -= Xi[k];
                    const int kk = k + db;
                    if (kk >= 1 && kk < half)
                    {
                        Yr[kk] += Xr[k] * rr - Xi[k] * ri;
                        Yi[kk] += Xr[k] * ri + Xi[k] * rr;
                    }
                }
            }

            // color crossfade, region gain; remember this frame for the next phase advance
            for (int k = 0; k <= half; ++k)
            {
                float vr = Xr[k] + c1 * (Yr[k] - Xr[k]);
                float vi = Xi[k] + c1 * (Yi[k] - Xi[k]);
                if (k >= kLo && k <= kHi) { vr *= g; vi *= g; }
                Yr[k] = vr;
                Yi[k] = vi;
                pvPrevR[ch][k] = Xr[k];
                pvPrevI[ch][k] = Xi[k];
            }
        }
        pvFlip ^= 1;

        synthesize(f, w, yr, yi, pvAcc, pvOut);
    }

    // inverse FFT of two hermitian spectra at once (Y = L + jR), overlap-add, hop of finished samples to `out`
    void synthesize(const ChromaFFT& f, const float* w, float (*sr)[kMaxBins], float (*si)[kMaxBins],
                    float (*acc)[kMaxN * 2], float (*out)[kMaxN])
    {
        const int n = f.size(), hop = n / 4, half = n / 2;
        for (int k = 0; k < n; ++k)
        {
            if (k <= half)
            {
                bufR[k] = sr[0][k] - si[1][k];
                bufI[k] = si[0][k] + sr[1][k];
            }
            else
            {
                const int m = n - k; // conj(L[m]) + j conj(R[m])
                bufR[k] = sr[0][m] + si[1][m];
                bufI[k] = -si[0][m] + sr[1][m];
            }
        }
        f.transform(bufR, bufI, true);

        const float ola = 1.f / (static_cast<float>(n) * 1.5f); // 1/N and the Hann^2 overlap sum at 75 %
        for (int k = 0; k < n; ++k)
        {
            acc[0][k] += w[k] * bufR[k] * ola;
            acc[1][k] += w[k] * bufI[k] * ola;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            std::memcpy(out[ch], acc[ch], sizeof(float) * static_cast<size_t>(hop));
            std::memmove(acc[ch], acc[ch] + hop, sizeof(float) * static_cast<size_t>(n));
            std::memset(acc[ch] + n, 0, sizeof(float) * static_cast<size_t>(hop));
        }
    }

    // ---- resonators (color above 100 %) --------------------------------------------------------------------------

    void updateResonators()
    {
        if (! resRunning && c2s < 1e-5f && color <= 1.f)
            return;
        resRunning = true;
        const double t60 = 0.1 * std::pow(25.0, static_cast<double>(std::min(1.f, c2s)));
        const double rr = std::exp(-6.907755 / (t60 * fs));
        resGain = 1.0 - rr; // peak gain 1 at the note
        numRes = 0;
        for (int n = 0; n < kNotes; ++n)
        {
            const bool inside = n >= noteLo && n <= noteHi;
            if (! inside)
            {
                for (int ch = 0; ch < 2; ++ch)
                    rzr[ch][n] = rzi[ch][n] = 0.0; // notes that left the region keep no tail
                continue;
            }
            const bool ringing = rzr[0][n] != 0.0 || rzi[0][n] != 0.0 || rzr[1][n] != 0.0 || rzi[1][n] != 0.0;
            if (! resFeed[n] && ! ringing)
                continue;
            resQr[n] = rr * std::cos(omega[n]);
            resQi[n] = rr * std::sin(omega[n]);
            resNotes[numRes++] = n;
        }
    }

    void resonate(float* io)
    {
        const double amt = static_cast<double>(gs) * c2s;
        for (int ch = 0; ch < 2; ++ch)
        {
            const double feed = io[ch];
            double R = 0.0;
            for (int i = 0; i < numRes; ++i)
            {
                const int n = resNotes[i];
                double& sr = rzr[ch][n];
                double& si = rzi[ch][n];
                const double x = resFeed[n] ? feed : 0.0;
                const double nr = resQr[n] * sr - resQi[n] * si + resGain * x;
                const double ni = resQr[n] * si + resQi[n] * sr;
                sr = nr;
                si = ni;
                R += 2.0 * nr;
            }
            io[ch] = static_cast<float>(feed + amt * R);
        }
    }

    // ---- spectral stage: morph + gate -----------------------------------------------------------------------------

    void resetSpectral()
    {
        std::memset(specInD, 0, sizeof(specInD));
        std::memset(specInW, 0, sizeof(specInW));
        std::memset(specOut, 0, sizeof(specOut));
        std::memset(specAcc, 0, sizeof(specAcc));
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < kMaxBins; ++k)
                gateGain[ch][k] = 1.f;
        const int n = specSize();
        specRover = n - n / 4;
    }

    void spectralTick(const float* dry, float* io)
    {
        const int n = specSize();
        const int lat0 = n - n / 4;
        for (int ch = 0; ch < 2; ++ch)
        {
            specInD[ch][specRover] = dry[ch];
            specInW[ch][specRover] = io[ch];
            io[ch] = specOut[ch][specRover - lat0];
        }
        if (++specRover >= n)
        {
            specRover = lat0;
            spectralFrame();
        }
    }

    void spectralFrame()
    {
        const int idx = specIndex();
        const ChromaFFT& f = fft[idx];
        const float* w = win[idx].data();
        const int n = f.size(), hop = n / 4, half = n / 2;
        const float magScale = 4.f / n; // full-scale sine peak bin -> 1
        static constexpr float kGateRelease = 0.55f;

        for (int ch = 0; ch < 2; ++ch)
        {
            // dry in the real part, wet in the imaginary part: one FFT for both
            for (int k = 0; k < n; ++k)
            {
                bufR[k] = w[k] * specInD[ch][k];
                bufI[k] = w[k] * specInW[ch][k];
            }
            f.transform(bufR, bufI, false);

            for (int k = 0; k <= half; ++k)
            {
                const int nk = (n - k) & (n - 1);
                const float dr = 0.5f * (bufR[k] + bufR[nk]);
                const float di = 0.5f * (bufI[k] - bufI[nk]);
                float wr = 0.5f * (bufI[k] + bufI[nk]);
                float wi = -0.5f * (bufR[k] - bufR[nk]);

                const float dm = std::sqrt(dr * dr + di * di);
                const float wm = std::sqrt(wr * wr + wi * wi);

                if (doMorph)
                {
                    const float m = wm + morphAmt * (dm - wm);
                    if (wm > 1e-12f)
                    {
                        const float sc = m / wm;
                        wr *= sc; wi *= sc;
                    }
                    else if (dm > 1e-12f)
                    {
                        const float sc = m / dm;
                        wr = dr * sc; wi = di * sc;
                    }
                }
                if (doGate)
                {
                    const float tgt = dm * magScale >= gateThr ? 1.f : 0.f;
                    float& g = gateGain[ch][k];
                    g = tgt >= g ? tgt : g * kGateRelease;
                    wr *= g; wi *= g;
                }
                yr[ch][k] = wr;
                yi[ch][k] = wi;
            }
        }

        synthesize(f, w, yr, yi, specAcc, specOut);

        for (int ch = 0; ch < 2; ++ch)
        {
            std::memmove(specInD[ch], specInD[ch] + hop, sizeof(float) * static_cast<size_t>(n - hop));
            std::memmove(specInW[ch], specInW[ch] + hop, sizeof(float) * static_cast<size_t>(n - hop));
        }
    }

    // ---- constants ------------------------------------------------------------------------------------------------
    static constexpr double kHysteresis = 0.12; // semitones beyond the boundary before a partial changes note

    // ---- settings ---------------------------------------------------------------------------------------------------
    double fs = 44100.0;
    int srShift = 0;
    uint16_t pcMask = 0;
    float color = 1.f, pitch = 0.f, tune = 0.f, gainTarget = 1.f;
    float rangeLo = 100.f, rangeHi = 6000.f;
    bool dirtyBands = true, dirtyMap = true;

    double smoothCoef = 0.001;
    float c1s = 1.f, c2s = 0.f, gs = 1.f;
    float visDecay = 0.99f;
    int blockPos = 0;

    // ---- note grid + mapping ----------------------------------------------------------------------------------------
    double refHz = 440.0, regionLoHz = 100.0, regionHiHz = 6000.0;
    int noteLo = 0, noteHi = -1;
    double omega[kNotes] {};
    int target[kNotes] {};
    double dOmega[kNotes] {};
    bool isMoving[kNotes] {};
    int moving[kNotes] {};
    int numMoving = 0;

    // ---- phase vocoder ----------------------------------------------------------------------------------------------
    float pvIn[2][kMaxN] {}, pvOut[2][kMaxN] {}, pvAcc[2][kMaxN * 2] {}, pvDry[2][kMaxN] {};
    float pvPrevR[2][kMaxBins] {}, pvPrevI[2][kMaxBins] {};
    int pvLast[2][2][kMaxBins] {}; // [frame flip][channel][bin]: note of the partial at that bin, -1 = none
    int pvFlip = 0, pvRover = 0, pvDryPos = 0;
    double theta[kNotes] {};
    float mag2[kMaxBins] {};
    int peaks[kMaxBins] {};

    // ---- resonators ---------------------------------------------------------------------------------------------
    bool resFeed[kNotes] {};
    double resQr[kNotes] {}, resQi[kNotes] {}, resGain = 0.0;
    double rzr[2][kNotes] {}, rzi[2][kNotes] {};
    int resNotes[kNotes] {};
    int numRes = 0;
    bool resRunning = false;

    // ---- spectral stage ---------------------------------------------------------------------------------------------
    bool specOn = false, doMorph = false, doGate = false;
    int specSel = 1, specRover = 0;
    float morphAmt = 0.5f, gateThr = 0.003f;
    float specInD[2][kMaxN] {}, specInW[2][kMaxN] {}, specOut[2][kMaxN] {}, specAcc[2][kMaxN * 2] {};
    float gateGain[2][kMaxBins] {};

    // ---- shared FFT scratch -----------------------------------------------------------------------------------------
    ChromaFFT fft[kNumFFT];
    std::vector<float> win[kNumFFT];
    float bufR[kMaxN] {}, bufI[kMaxN] {};
    float xr[2][kMaxBins] {}, xi[2][kMaxBins] {}, yr[2][kMaxBins] {}, yi[2][kMaxBins] {};
};

#endif
