/*
 This file is part of Livecut
 Copyright 2004 by Remy Muller.
 Adapted for DISTRHO Plugin Framework (DPF) by eventual-recluse.
 
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 
 Livecut is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Livecut; if not, visit www.gnu.org/licenses or write to the
 Free Software Foundation, Inc., 59 Temple Place, Suite 330, 
 Boston, MA 02111-1307 USA
 */

/* Control information required by LiveCut.cpp and LiveCutUI.cpp */

#ifndef LIVECUT_CONTROLS_HPP
#define LIVECUT_CONTROLS_HPP

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "lib/NoteTuning.h"

template <class T>
T limit (const T x, const T min, const T max)
{
    return (x < min ? min : (x > max ? max : x));
}

enum LiveCutControls{
    LVC_CUTPROC       = 0,
    LVC_SUBDIV        = 1,
    LVC_FADE          = 2,
    LVC_MINAMP        = 3,
    LVC_MAXAMP        = 4,
    LVC_MINPAN        = 5,
    LVC_MAXPAN        = 6,
    LVC_MINPITCH      = 7,
    LVC_MAXPITCH      = 8,
    LVC_DUTY          = 9,
    LVC_FILLDUTY      = 10,
    LVC_MAXPHRS       = 11,
    LVC_MINPHRS       = 12,
    LVC_MAXREP        = 13,
    LVC_MINREP        = 14,
    LVC_STUTTER       = 15,
    LVC_AREA          = 16,
    LVC_STRAIGHT      = 17,
    LVC_REGULAR       = 18,
    LVC_RITARD        = 19,
    LVC_SPEED         = 20,
    LVC_ACTIVITY      = 21,
    LVC_CRUSHER       = 22,
    LVC_MINBITS       = 23,
    LVC_MAXBITS       = 24,
    LVC_MINFREQ       = 25,
    LVC_MAXFREQ       = 26,
    LVC_COMB          = 27,
    LVC_TYPE          = 28,
    LVC_FEEDBACK      = 29,
    LVC_MINDELAY      = 30,
    LVC_MAXDELAY      = 31,
    LVC_SEED          = 32,
    LVC_LPFREQ        = 33,
    LVC_HPFREQ        = 34,
    LVC_RESO          = 35,
    LVC_MIX           = 36,
    LVC_LIMITER       = 37,
    LVC_LIMCEIL       = 38,
    LVC_LIMREL        = 39,
    LVC_MINCUT        = 40,
    LVC_EUDENSITY     = 41,
    LVC_EUCHANCE      = 42,
    LVC_RAINTENSITY   = 43,
    LVC_RACURVE       = 44,
    LVC_FILLMODE      = 45,
    LVC_FILLCYCLE     = 46,
    LVC_FILLBARS      = 47,
    LVC_EUSLICE       = 48,
    LVC_FILTER        = 49,
    LVC_LPMIN         = 50,  // lowpass range: LVC_LPMIN .. LVC_LPFREQ (max)
    LVC_HPMAX         = 51,  // highpass range: LVC_HPFREQ (min) .. LVC_HPMAX
    LVC_REVERB        = 52,  // per-slice reverb throws
    LVC_RVCHANCE      = 53,
    LVC_RVSENDMIN     = 54,
    LVC_RVSENDMAX     = 55,
    LVC_RVDECAYMIN    = 56,
    LVC_RVDECAYMAX    = 57,
    LVC_RVSIZE        = 58,
    LVC_RVDAMP        = 59,
    LVC_MASTERHPFREQ  = 60,  // master highpass cutoff (open at minimum)
    LVC_MASTERLPFREQ  = 61,  // master lowpass cutoff (open at maximum)
    LVC_RVMODE        = 62,  // 0 = send, 1 = insert
    LVC_FXORDER       = 63,  // encoded order of the four reorderable effects
    // Chroma: spectral resonator (key / scale tuning of the active frequency region)
    LVC_CHROMA        = 64,  // on / off
    LVC_CHPOS         = 65,  // 0 = in front of the slicer, 1 = first effect after it .. 5 = last
    LVC_CHKEY         = 66,  // root pitch class, 0 = C
    LVC_CHSCALE       = 67,  // index into LVC_SCALES
    LVC_CHCOLOR       = 68,  // 0..1 dry/wet of the tuning, 1..2 adds in-key resonators
    LVC_CHPITCH       = 69,  // semitones, applied before the scale is enforced
    LVC_CHTUNE        = 70,  // cents: reference of the note grid (A4 = 440 Hz + tune)
    LVC_CHGAIN        = 71,  // dB, gain of the processed region
    LVC_CHLOW         = 72,  // active region, Hz
    LVC_CHHIGH        = 73,
    LVC_CHMIDI        = 74,  // held MIDI notes choose the pitch classes
    LVC_CHQUALITY     = 75,  // unused (single engine), kept so the later ids do not move
    LVC_CHFFT         = 76,  // spectral stage FFT size: 0 = 512, 1 = 1024
    LVC_CHMORPH       = 77,  // spectral morph on / off
    LVC_CHMORPHAMT    = 78,  // 0 = wet magnitudes, 1 = dry magnitudes
    LVC_CHGATE        = 79,  // spectral gate on / off
    LVC_CHGATETHR     = 80,  // dB
    LVC_SLICER        = 81,  // the auto slicer on / off (off: the input runs straight through the effects)
    LVC_ROUTE         = 82,  // full order of slicer + five effects (permutation code), -1 = from LVC_FXORDER / LVC_CHPOS
    LVC_TRANSIENT     = 83,  // transient shaper on / off
    LVC_TRATTACK      = 84,  // -1..1: cut .. boost the hit
    LVC_TRPUMP        = 85,  // 0..1: dip right after the hit
    LVC_TRSUSTAIN     = 86,  // -1..1: cut .. boost the tail
    LVC_TRPOS         = 87,  // position of the shaper in the signal-flow row (0 = first .. 6 = last)
    LVC_MAKEUP        = 88,  // dB, output gain after the limiter
    // Trance gate: a cutting algorithm that keeps the audio in place and only mutes steps.
    // Its own switch (not a value of LVC_CUTPROC) so the CutProc parameter keeps its range and steps for old sessions.
    LVC_GATE          = 89,  // on: the trance gate replaces the LVC_CUTPROC algorithm
    LVC_GATEMODE      = 90,  // 0 = random patterns, 1 = the drawn pattern (LVC_GATEPAT1 / 2)
    LVC_GATEDENSITY   = 91,  // random mode: share of open steps
    LVC_GATEVARY      = 92,  // random mode: chance for each step to be re-rolled at every bar
    LVC_GATEPAT1      = 93,  // drawn pattern, steps 1..16 (bit 0 = step 1)
    LVC_GATEPAT2      = 94,  // drawn pattern, steps 17..32
    LVC_GATEENV1        = 95,  // trance gate step envelopes: steps 1-2 (bits 0-3 attack, 4-7 decay of step 1; 8-15 step 2)
    LVC_GATEENV2        = 96,
    LVC_GATEENV3        = 97,
    LVC_GATEENV4        = 98,
    LVC_GATEENV5        = 99,
    LVC_GATEENV6        = 100,
    LVC_GATEENV7        = 101,
    LVC_GATEENV8        = 102,
    LVC_GATEENV9        = 103,
    LVC_GATEENV10       = 104,
    LVC_GATEENV11       = 105,
    LVC_GATEENV12       = 106,
    LVC_GATEENV13       = 107,
    LVC_GATEENV14       = 108,
    LVC_GATEENV15       = 109,
    LVC_GATEENV16       = 110,
    LVC_GATETENSION1  = 111,  // trance gate envelope curve tension, signed 8 bits per step (0 = straight): steps 1-2
    LVC_GATETENSION2  = 112,
    LVC_GATETENSION3  = 113,
    LVC_GATETENSION4  = 114,
    LVC_GATETENSION5  = 115,
    LVC_GATETENSION6  = 116,
    LVC_GATETENSION7  = 117,
    LVC_GATETENSION8  = 118,
    LVC_GATETENSION9  = 119,
    LVC_GATETENSION10 = 120,
    LVC_GATETENSION11 = 121,
    LVC_GATETENSION12 = 122,
    LVC_GATETENSION13 = 123,
    LVC_GATETENSION14 = 124,
    LVC_GATETENSION15 = 125,
    LVC_GATETENSION16 = 126,
    LVC_GATETIE1      = 127, // trance gate ties: bit i = step i+1 runs into the next step as one slice (steps 1-16)
    LVC_GATETIE2      = 128, // steps 17-32
    LVC_GATEVOL       = 129, // how deeply the gate envelope shapes the level (1 = muted steps are silent)
    LVC_GATELP        = 130, // gate envelope -> lowpass: octaves closed (x8) when the envelope is down
    LVC_GATEHP        = 131, // gate envelope -> highpass: octaves raised (x8) when the envelope is down
    // Delay module: the original per-slice comb, or a tempo-synced stereo echo (LVC_COMB switches either on)
    LVC_DLYMODE       = 132, // 0 = comb (per slice), 1 = echo
    LVC_DLYSYNC       = 133, // echo: 1 = note values (LVC_DLYTIMEL / R), 0 = milliseconds (LVC_DLYMSL / R)
    LVC_DLYTIMEL      = 134, // echo: index into LVC_DELAY_DIVS, left (and mono / ping-pong) time
    LVC_DLYTIMER      = 135, // echo: right time (stereo mode)
    LVC_DLYMSL        = 136, // echo: free time, ms
    LVC_DLYMSR        = 137,
    LVC_DLYSTEREO     = 138, // echo: 0 = mono (same time both sides), 1 = stereo (own times), 2 = ping-pong
    LVC_DLYMIX        = 139, // echo: dry / wet
    LVC_DLYTONE       = 140, // echo: damping in the feedback loop (each repeat darker)
    LVC_DLYTUNE       = 141, // delay times snap to note periods of the pitchmap's key / scale (echo: free times only)
    LVC_CONTROL_NR    = 142
};

static const std::array<std::pair<float, float>, LVC_CONTROL_NR> controlLimits =
{{
    {0.0f, 2.0f},        // LVC_CUTPROC (half steps, see algoFromParam)
    {0.0f, 6.0f},        // LVC_SUBDIV
    {0.0f, 100.0f},      // LVC_FADE
    {0.0f, 1.0f},        // LVC_MINAMP
    {0.0f, 1.0f},        // LVC_MAXAMP
    {-1.0f, 1.0f},       // LVC_MINPAN
    {-1.0f, 1.0f},       // LVC_MAXPAN
    {-2400.0f, 2400.0f}, // LVC_MINPITCH
    {-2400.0f, 2400.0f}, // LVC_MAXPITCH
    {0.0f, 1.0f},        // LVC_DUTY
    {0.0f, 1.0f},        // LVC_FILLDUTY
    {1.0f, 8.0f},        // LVC_MAXPHRS
    {1.0f, 8.0f},        // LVC_MINPHRS
    {0.0f, 4.0f},        // LVC_MAXREP
    {0.0f, 4.0f},        // LVC_MINREP
    {0.0f, 1.0f},        // LVC_STUTTER
    {0.0f, 1.0f},        // LVC_AREA
    {0.0f, 1.0f},        // LVC_STRAIGHT
    {0.0f, 1.0f},        // LVC_REGULAR
    {0.0f, 1.0f},        // LVC_RITARD
    {0.5f, 0.999f},      // LVC_SPEED
    {0.0f, 1.0f},        // LVC_ACTIVITY
    {0.0f, 1.0f},        // LVC_CRUSHER
    {1.0f, 32.0f},       // LVC_MINBITS
    {1.0f, 32.0f},       // LVC_MAXBITS
    {441.0f, 44100.0f},  // LVC_MINFREQ
    {441.0f, 44100.0f},  // LVC_MAXFREQ
    {0.0f, 1.0f},        // LVC_COMB
    {0.0f, 1.0f},        // LVC_TYPE
    {0.0f, 0.9f},        // LVC_FEEDBACK
    {1.0f, 50.0f},       // LVC_MINDELAY
    {1.0f, 50.0f},       // LVC_MAXDELAY
    {1.0f, 16.0f},       // LVC_SEED
    {20.0f, 20000.0f},   // LVC_LPFREQ
    {20.0f, 20000.0f},   // LVC_HPFREQ
    {0.0f, 1.0f},        // LVC_RESO
    {0.0f, 1.0f},        // LVC_MIX
    {0.0f, 1.0f},        // LVC_LIMITER
    {-12.0f, 0.0f},      // LVC_LIMCEIL
    {10.0f, 1000.0f},    // LVC_LIMREL
    {0.0f, 2.0f},        // LVC_MINCUT
    {0.0f, 1.0f},        // LVC_EUDENSITY
    {0.0f, 1.0f},        // LVC_EUCHANCE
    {0.0f, 1.0f},        // LVC_RAINTENSITY
    {0.0f, 1.0f},        // LVC_RACURVE
    {0.0f, 1.0f},        // LVC_FILLMODE
    {0.0f, 2.0f},        // LVC_FILLCYCLE
    {0.0f, 3.0f},        // LVC_FILLBARS
    {0.0f, 4.0f},        // LVC_EUSLICE
    {0.0f, 1.0f},        // LVC_FILTER
    {20.0f, 20000.0f},   // LVC_LPMIN
    {20.0f, 20000.0f},   // LVC_HPMAX
    {0.0f, 1.0f},        // LVC_REVERB
    {0.0f, 1.0f},        // LVC_RVCHANCE
    {0.0f, 1.0f},        // LVC_RVSENDMIN
    {0.0f, 1.0f},        // LVC_RVSENDMAX
    {0.2f, 12.0f},       // LVC_RVDECAYMIN (s)
    {0.2f, 12.0f},       // LVC_RVDECAYMAX (s)
    {0.0f, 1.0f},        // LVC_RVSIZE
    {0.0f, 1.0f},        // LVC_RVDAMP
    {20.0f, 20000.0f},   // LVC_MASTERHPFREQ
    {20.0f, 20000.0f},   // LVC_MASTERLPFREQ
    {0.0f, 1.0f},        // LVC_RVMODE
    {0.0f, 23.0f},       // LVC_FXORDER (0 = crusher, comb, filter, verb)
    {0.0f, 1.0f},        // LVC_CHROMA
    {0.0f, 5.0f},        // LVC_CHPOS
    {0.0f, 11.0f},       // LVC_CHKEY
    {0.0f, 23.0f},       // LVC_CHSCALE (LVC_NUM_SCALES - 1)
    {0.0f, 2.0f},        // LVC_CHCOLOR
    {-24.0f, 24.0f},     // LVC_CHPITCH (semitones)
    {-50.0f, 50.0f},     // LVC_CHTUNE (cents)
    {-24.0f, 12.0f},     // LVC_CHGAIN (dB)
    {20.0f, 20000.0f},   // LVC_CHLOW
    {20.0f, 20000.0f},   // LVC_CHHIGH
    {0.0f, 1.0f},        // LVC_CHMIDI
    {0.0f, 1.0f},        // LVC_CHQUALITY
    {0.0f, 1.0f},        // LVC_CHFFT
    {0.0f, 1.0f},        // LVC_CHMORPH
    {0.0f, 1.0f},        // LVC_CHMORPHAMT
    {0.0f, 1.0f},        // LVC_CHGATE
    {-90.0f, -10.0f},    // LVC_CHGATETHR (dB)
    {0.0f, 1.0f},        // LVC_SLICER
    {-1.0f, 719.0f},     // LVC_ROUTE
    {0.0f, 1.0f},        // LVC_TRANSIENT
    {-1.0f, 1.0f},       // LVC_TRATTACK
    {0.0f, 1.0f},        // LVC_TRPUMP
    {-1.0f, 1.0f},       // LVC_TRSUSTAIN
    {0.0f, 6.0f},        // LVC_TRPOS
    {-12.0f, 24.0f},     // LVC_MAKEUP (dB)
    {0.0f, 1.0f},        // LVC_GATE
    {0.0f, 1.0f},        // LVC_GATEMODE
    {0.0f, 1.0f},        // LVC_GATEDENSITY
    {0.0f, 1.0f},        // LVC_GATEVARY
    {0.0f, 65535.0f},    // LVC_GATEPAT1 (16-bit step mask)
    {0.0f, 65535.0f},    // LVC_GATEPAT2
    {0.0f, 65535.0f},    // LVC_GATEENV1
    {0.0f, 65535.0f},    // LVC_GATEENV2
    {0.0f, 65535.0f},    // LVC_GATEENV3
    {0.0f, 65535.0f},    // LVC_GATEENV4
    {0.0f, 65535.0f},    // LVC_GATEENV5
    {0.0f, 65535.0f},    // LVC_GATEENV6
    {0.0f, 65535.0f},    // LVC_GATEENV7
    {0.0f, 65535.0f},    // LVC_GATEENV8
    {0.0f, 65535.0f},    // LVC_GATEENV9
    {0.0f, 65535.0f},    // LVC_GATEENV10
    {0.0f, 65535.0f},    // LVC_GATEENV11
    {0.0f, 65535.0f},    // LVC_GATEENV12
    {0.0f, 65535.0f},    // LVC_GATEENV13
    {0.0f, 65535.0f},    // LVC_GATEENV14
    {0.0f, 65535.0f},    // LVC_GATEENV15
    {0.0f, 65535.0f},    // LVC_GATEENV16
    {0.0f, 65535.0f},    // LVC_GATETENSION1
    {0.0f, 65535.0f},    // LVC_GATETENSION2
    {0.0f, 65535.0f},    // LVC_GATETENSION3
    {0.0f, 65535.0f},    // LVC_GATETENSION4
    {0.0f, 65535.0f},    // LVC_GATETENSION5
    {0.0f, 65535.0f},    // LVC_GATETENSION6
    {0.0f, 65535.0f},    // LVC_GATETENSION7
    {0.0f, 65535.0f},    // LVC_GATETENSION8
    {0.0f, 65535.0f},    // LVC_GATETENSION9
    {0.0f, 65535.0f},    // LVC_GATETENSION10
    {0.0f, 65535.0f},    // LVC_GATETENSION11
    {0.0f, 65535.0f},    // LVC_GATETENSION12
    {0.0f, 65535.0f},    // LVC_GATETENSION13
    {0.0f, 65535.0f},    // LVC_GATETENSION14
    {0.0f, 65535.0f},    // LVC_GATETENSION15
    {0.0f, 65535.0f},    // LVC_GATETENSION16
    {0.0f, 65535.0f},    // LVC_GATETIE1
    {0.0f, 65535.0f},    // LVC_GATETIE2
    {0.0f, 1.0f},        // LVC_GATEVOL
    {0.0f, 1.0f},        // LVC_GATELP
    {0.0f, 1.0f},        // LVC_GATEHP
    {0.0f, 1.0f},        // LVC_DLYMODE
    {0.0f, 1.0f},        // LVC_DLYSYNC
    {0.0f, 14.0f},       // LVC_DLYTIMEL (LVC_NUM_DELAY_DIVS - 1)
    {0.0f, 14.0f},       // LVC_DLYTIMER
    {1.0f, 4000.0f},     // LVC_DLYMSL (ms)
    {1.0f, 4000.0f},     // LVC_DLYMSR (ms)
    {0.0f, 2.0f},        // LVC_DLYSTEREO
    {0.0f, 1.0f},        // LVC_DLYMIX
    {0.0f, 1.0f},        // LVC_DLYTONE
    {0.0f, 1.0f}         // LVC_DLYTUNE
}};

static const float LVC_DEFAULTS[LVC_CONTROL_NR] = {
    0.0f, // LVC_CUTPROC
    1.0f, // LVC_SUBDIV
    0.01f, // LVC_FADE
    1.0f, // LVC_MINAMP
    1.0f, // LVC_MAXAMP
    -0.2f, // LVC_MINPAN
    0.2f,  // LVC_MAXPAN
    0.0f, // LVC_MINPITCH
    0.0f, // LVC_MAXPITCH
    1.0f, // LVC_DUTY
    1.0f, // LVC_FILLDUTY
    4.0f, // LVC_MAXPHRS
    1.0f, // LVC_MINPHRS
    1.0f, // LVC_MAXREP
    0.0f, // LVC_MINREP
    0.8f, // LVC_STUTTER
    0.5f, // LVC_AREA
    0.3f, // LVC_STRAIGHT
    0.5f, // LVC_REGULAR
    0.5f, // LVC_RITARD
    0.9f, // LVC_SPEED
    0.5f, // LVC_ACTIVITY
    0.0f, // LVC_CRUSHER
    32.0f, // LVC_MINBITS
    32.0f, // LVC_MAXBITS
    44100.0f, // LVC_MINFREQ
    44100.0f, // LVC_MAXFREQ
    0.0f, // LVC_COMB
    0.0f, // LVC_TYPE
    0.5f, // LVC_FEEDBACK
    10.0f, // LVC_MINDELAY
    10.0f, //  LVC_MAXDELAY
    1.0f, //  LVC_SEED
    20000.0f, // LVC_LPFREQ
    20.0f,    // LVC_HPFREQ
    0.0f,     // LVC_RESO
    1.0f,     // LVC_MIX
    0.0f,     // LVC_LIMITER (off, so old sessions sound exactly as before)
    -0.3f,    // LVC_LIMCEIL
    120.0f,   // LVC_LIMREL
    0.0f,     // LVC_MINCUT (0 = any, 1 = no 1/32, 2 = no 1/16 or 1/32)
    0.45f,    // LVC_EUDENSITY
    0.8f,     // LVC_EUCHANCE
    0.6f,     // LVC_RAINTENSITY
    0.5f,     // LVC_RACURVE
    0.0f,     // LVC_FILLMODE (off: slice all the time)
    1.0f,     // LVC_FILLCYCLE (16 bars)
    2.0f,     // LVC_FILLBARS (last 4 bars)
    0.0f,     // LVC_EUSLICE (1st hit = classic behaviour)
    1.0f,     // LVC_FILTER (on: the cutoffs alone decide, so earlier sessions sound the same)
    20000.0f, // LVC_LPMIN (open)
    20.0f,    // LVC_HPMAX (open)
    0.0f,     // LVC_REVERB (off)
    0.3f,     // LVC_RVCHANCE
    0.4f,     // LVC_RVSENDMIN
    0.8f,     // LVC_RVSENDMAX
    1.0f,     // LVC_RVDECAYMIN
    3.0f,     // LVC_RVDECAYMAX
    0.6f,     // LVC_RVSIZE
    0.5f,     // LVC_RVDAMP
    20.0f,    // LVC_MASTERHPFREQ (open = bypassed)
    20000.0f, // LVC_MASTERLPFREQ (open = bypassed)
    0.0f,     // LVC_RVMODE (send, as before)
    0.0f,     // LVC_FXORDER (original order)
    0.0f,     // LVC_CHROMA (off: old sessions sound the same)
    4.0f,     // LVC_CHPOS (after the filter, before the reverb in the original order)
    0.0f,     // LVC_CHKEY (C)
    0.0f,     // LVC_CHSCALE (major)
    1.0f,     // LVC_CHCOLOR (100 %: fully tuned, no extra resonance)
    0.0f,     // LVC_CHPITCH
    0.0f,     // LVC_CHTUNE
    0.0f,     // LVC_CHGAIN
    100.0f,   // LVC_CHLOW
    6000.0f,  // LVC_CHHIGH
    0.0f,     // LVC_CHMIDI
    1.0f,     // LVC_CHQUALITY (unused)
    1.0f,     // LVC_CHFFT (1024)
    0.0f,     // LVC_CHMORPH
    0.5f,     // LVC_CHMORPHAMT
    0.0f,     // LVC_CHGATE
    -50.0f,   // LVC_CHGATETHR
    1.0f,     // LVC_SLICER (on: old sessions slice as before)
    532.0f,   // LVC_ROUTE: pitchmap, filter, crush, SLICE, comb (delay), reverb (with the shaper first: see LVC_TRPOS)
    0.0f,     // LVC_TRANSIENT (off)
    0.0f,     // LVC_TRATTACK
    0.0f,     // LVC_TRPUMP
    0.0f,     // LVC_TRSUSTAIN
    0.0f,     // LVC_TRPOS (first: IN > TRANSIENT > PITCHMAP > FILTER > CRUSH > SLICE > DELAY > REVERB)
    0.0f,     // LVC_MAKEUP (unity)
    0.0f,     // LVC_GATE (off: old sessions keep their algorithm)
    0.0f,     // LVC_GATEMODE (random)
    0.6f,     // LVC_GATEDENSITY
    0.25f,    // LVC_GATEVARY
    21845.0f, // LVC_GATEPAT1 (0x5555: every other step open)
    21845.0f, // LVC_GATEPAT2
    0.0f,     // LVC_GATEENV1 (no envelope)
    0.0f,     // LVC_GATEENV2 (no envelope)
    0.0f,     // LVC_GATEENV3 (no envelope)
    0.0f,     // LVC_GATEENV4 (no envelope)
    0.0f,     // LVC_GATEENV5 (no envelope)
    0.0f,     // LVC_GATEENV6 (no envelope)
    0.0f,     // LVC_GATEENV7 (no envelope)
    0.0f,     // LVC_GATEENV8 (no envelope)
    0.0f,     // LVC_GATEENV9 (no envelope)
    0.0f,     // LVC_GATEENV10 (no envelope)
    0.0f,     // LVC_GATEENV11 (no envelope)
    0.0f,     // LVC_GATEENV12 (no envelope)
    0.0f,     // LVC_GATEENV13 (no envelope)
    0.0f,     // LVC_GATEENV14 (no envelope)
    0.0f,     // LVC_GATEENV15 (no envelope)
    0.0f,     // LVC_GATEENV16 (no envelope)
    0.0f,     // LVC_GATETENSION1 (straight)
    0.0f,     // LVC_GATETENSION2
    0.0f,     // LVC_GATETENSION3
    0.0f,     // LVC_GATETENSION4
    0.0f,     // LVC_GATETENSION5
    0.0f,     // LVC_GATETENSION6
    0.0f,     // LVC_GATETENSION7
    0.0f,     // LVC_GATETENSION8
    0.0f,     // LVC_GATETENSION9
    0.0f,     // LVC_GATETENSION10
    0.0f,     // LVC_GATETENSION11
    0.0f,     // LVC_GATETENSION12
    0.0f,     // LVC_GATETENSION13
    0.0f,     // LVC_GATETENSION14
    0.0f,     // LVC_GATETENSION15
    0.0f,     // LVC_GATETENSION16
    0.0f,     // LVC_GATETIE1 (no ties)
    0.0f,     // LVC_GATETIE2
    1.0f,     // LVC_GATEVOL (full: the gate mutes, as before)
    0.0f,     // LVC_GATELP (off)
    0.0f,     // LVC_GATEHP (off)
    0.0f,     // LVC_DLYMODE (comb: old sessions sound the same)
    1.0f,     // LVC_DLYSYNC (note values)
    6.0f,     // LVC_DLYTIMEL (1/8 dotted)
    8.0f,     // LVC_DLYTIMER (1/4)
    375.0f,   // LVC_DLYMSL
    500.0f,   // LVC_DLYMSR
    2.0f,     // LVC_DLYSTEREO (ping-pong)
    0.35f,    // LVC_DLYMIX
    0.3f,     // LVC_DLYTONE
    0.0f,     // LVC_DLYTUNE (off: old sessions sound the same)
};

// Parameter symbols (stable identifiers used by hosts, sessions and preset files). Never reorder or rename.
static const char* const LVC_SYMBOLS[LVC_CONTROL_NR] = {
    "livecut_cutproc",
    "livecut_subdiv",
    "livecut_fade",
    "livecut_minamp",
    "livecut_maxamp",
    "livecut_minpan",
    "livecut_maxpan",
    "livecut_minpitch",
    "livecut_maxpitch",
    "livecut_duty",
    "livecut_fillduty",
    "livecut_maxphrs",
    "livecut_minphrs",
    "livecut_maxrep",
    "livecut_minrep",
    "livecut_stutter",
    "livecut_area",
    "livecut_straight",
    "livecut_regular",
    "livecut_ritard",
    "livecut_speed",
    "livecut_activity",
    "livecut_crusher",
    "livecut_minbits",
    "livecut_maxbits",
    "livecut_minfreq",
    "livecut_maxfreq",
    "livecut_comb",
    "livecut_type",
    "livecut_feedback",
    "livecut_mindelay",
    "livecut_maxdelay",
    "livecut_seed",
    "livecut_lpfreq",
    "livecut_hpfreq",
    "livecut_reso",
    "livecut_mix",
    "livecut_limiter",
    "livecut_limceil",
    "livecut_limrel",
    "livecut_mincut",
    "livecut_eudensity",
    "livecut_euchance",
    "livecut_raintensity",
    "livecut_racurve",
    "livecut_fillmode",
    "livecut_fillcycle",
    "livecut_fillbars",
    "livecut_euslice",
    "livecut_filter",
    "livecut_lpmin",
    "livecut_hpmax",
    "livecut_reverb",
    "livecut_rvchance",
    "livecut_rvsendmin",
    "livecut_rvsendmax",
    "livecut_rvdecaymin",
    "livecut_rvdecaymax",
    "livecut_rvsize",
    "livecut_rvdamp",
    "livecut_masterhpfreq",
    "livecut_masterlpfreq",
    "livecut_rvmode",
    "livecut_fxorder",
    "livecut_chroma",
    "livecut_chpos",
    "livecut_chkey",
    "livecut_chscale",
    "livecut_chcolor",
    "livecut_chpitch",
    "livecut_chtune",
    "livecut_chgain",
    "livecut_chlow",
    "livecut_chhigh",
    "livecut_chmidi",
    "livecut_chquality",
    "livecut_chfft",
    "livecut_chmorph",
    "livecut_chmorphamt",
    "livecut_chgate",
    "livecut_chgatethr",
    "livecut_slicer",
    "livecut_route",
    "livecut_transient",
    "livecut_trattack",
    "livecut_trpump",
    "livecut_trsustain",
    "livecut_trpos",
    "livecut_makeup",
    "livecut_gate",
    "livecut_gatemode",
    "livecut_gatedensity",
    "livecut_gatevary",
    "livecut_gatepat1",
    "livecut_gatepat2",
    "livecut_gateenv1",
    "livecut_gateenv2",
    "livecut_gateenv3",
    "livecut_gateenv4",
    "livecut_gateenv5",
    "livecut_gateenv6",
    "livecut_gateenv7",
    "livecut_gateenv8",
    "livecut_gateenv9",
    "livecut_gateenv10",
    "livecut_gateenv11",
    "livecut_gateenv12",
    "livecut_gateenv13",
    "livecut_gateenv14",
    "livecut_gateenv15",
    "livecut_gateenv16",
    "livecut_gatetension1",
    "livecut_gatetension2",
    "livecut_gatetension3",
    "livecut_gatetension4",
    "livecut_gatetension5",
    "livecut_gatetension6",
    "livecut_gatetension7",
    "livecut_gatetension8",
    "livecut_gatetension9",
    "livecut_gatetension10",
    "livecut_gatetension11",
    "livecut_gatetension12",
    "livecut_gatetension13",
    "livecut_gatetension14",
    "livecut_gatetension15",
    "livecut_gatetension16",
    "livecut_gatetie1",
    "livecut_gatetie2",
    "livecut_gatevol",
    "livecut_gatelp",
    "livecut_gatehp",
    "livecut_dlymode",
    "livecut_dlysync",
    "livecut_dlytimel",
    "livecut_dlytimer",
    "livecut_dlymsl",
    "livecut_dlymsr",
    "livecut_dlystereo",
    "livecut_dlymix",
    "livecut_dlytone",
    "livecut_dlytune",
};

// Chroma scales: bit i set = the note i semitones above the root belongs to the scale.
struct LvcScale { const char* name; uint16_t mask; int relative; int relShift; };
static inline uint16_t lvcMask(std::initializer_list<int> steps)
{
    uint16_t m = 0;
    for (int st : steps) m = static_cast<uint16_t>(m | (1u << st));
    return m;
}
// relative: index of the relative scale (same notes, other root) or -1; relShift: root change to get there
static const LvcScale LVC_SCALES[] = {
    { "Major",             lvcMask({0, 2, 4, 5, 7, 9, 11}),  1, -3 },
    { "Minor",             lvcMask({0, 2, 3, 5, 7, 8, 10}),  0,  3 },
    { "Harmonic Minor",    lvcMask({0, 2, 3, 5, 7, 8, 11}), -1,  0 },
    { "Melodic Minor",     lvcMask({0, 2, 3, 5, 7, 9, 11}), -1,  0 },
    { "Dorian",            lvcMask({0, 2, 3, 5, 7, 9, 10}), -1,  0 },
    { "Phrygian",          lvcMask({0, 1, 3, 5, 7, 8, 10}), -1,  0 },
    { "Lydian",            lvcMask({0, 2, 4, 6, 7, 9, 11}), -1,  0 },
    { "Mixolydian",        lvcMask({0, 2, 4, 5, 7, 9, 10}), -1,  0 },
    { "Locrian",           lvcMask({0, 1, 3, 5, 6, 8, 10}), -1,  0 },
    { "Major Pentatonic",  lvcMask({0, 2, 4, 7, 9}),        10, -3 },
    { "Minor Pentatonic",  lvcMask({0, 3, 5, 7, 10}),        9,  3 },
    { "Blues",             lvcMask({0, 3, 5, 6, 7, 10}),    -1,  0 },
    { "Phrygian Dominant", lvcMask({0, 1, 4, 5, 7, 8, 10}), -1,  0 },
    { "Hungarian Minor",   lvcMask({0, 2, 3, 6, 7, 8, 11}), -1,  0 },
    { "Hirajoshi",         lvcMask({0, 2, 3, 7, 8}),        -1,  0 },
    { "Whole Tone",        lvcMask({0, 2, 4, 6, 8, 10}),    -1,  0 },
    { "Major Triad",       lvcMask({0, 4, 7}),              -1,  0 },
    { "Minor Triad",       lvcMask({0, 3, 7}),              -1,  0 },
    { "Major 7th",         lvcMask({0, 4, 7, 11}),          -1,  0 },
    { "Minor 7th",         lvcMask({0, 3, 7, 10}),          -1,  0 },
    { "Dominant 7th",      lvcMask({0, 4, 7, 10}),          -1,  0 },
    { "Fifths",            lvcMask({0, 7}),                 -1,  0 },
    { "Root Only",         lvcMask({0}),                    -1,  0 },
    { "Chromatic",         0x0FFF,                          -1,  0 },
};
static constexpr int LVC_NUM_SCALES = static_cast<int>(sizeof(LVC_SCALES) / sizeof(LVC_SCALES[0]));
static_assert(LVC_NUM_SCALES == 24, "LVC_CHSCALE range must match the scale table");

static const char* const LVC_KEY_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// absolute pitch classes (bit 0 = C) of a scale on a root
static inline uint16_t lvcScalePitchClasses(int scale, int root)
{
    scale = scale < 0 ? 0 : (scale >= LVC_NUM_SCALES ? LVC_NUM_SCALES - 1 : scale);
    root = ((root % 12) + 12) % 12;
    const uint32_t m = LVC_SCALES[scale].mask;
    return static_cast<uint16_t>(((m << root) | (m >> (12 - root))) & 0x0FFF);
}

// Chroma sits at slot LVC_CHPOS: 0 = in front of the slicer, 1..5 = among the four reorderable effects after it (its
// own parameter, so that LVC_FXORDER keeps its meaning for old sessions)
static constexpr int kFxChroma = 4;

// Reorderable effects. LVC_FXORDER packs a permutation of these four into 0..23 (Lehmer code).
enum LiveCutFx { kFxCrusher = 0, kFxComb, kFxFilter, kFxVerb, kFxCount };

static inline void decodeFxOrder(int code, int out[kFxCount])
{
    code = limit<int>(code, 0, 23);
    int digits[kFxCount];
    for (int i = kFxCount - 1; i >= 0; --i)
    {
        const int base = kFxCount - i;
        digits[i] = code % base;
        code /= base;
    }
    int avail[kFxCount] = { kFxCrusher, kFxComb, kFxFilter, kFxVerb };
    int n = kFxCount;
    for (int i = 0; i < kFxCount; ++i)
    {
        const int d = limit<int>(digits[i], 0, n - 1);
        out[i] = avail[d];
        for (int j = d; j < n - 1; ++j) avail[j] = avail[j + 1];
        --n;
    }
}

static inline int encodeFxOrder(const int order[kFxCount])
{
    int avail[kFxCount] = { kFxCrusher, kFxComb, kFxFilter, kFxVerb };
    int code = 0, n = kFxCount;
    for (int i = 0; i < kFxCount; ++i)
    {
        int d = 0;
        while (d < n && avail[d] != order[i]) ++d;
        d = limit<int>(d, 0, n - 1);
        code = code * (kFxCount - i) + d;
        for (int j = d; j < n - 1; ++j) avail[j] = avail[j + 1];
        --n;
    }
    return code;
}

// Full routing: the slicer and the five effects in any order (effects in front of the slicer process what it
// records). LVC_ROUTE holds the permutation as a Lehmer code; -1 means "derive it from LVC_FXORDER and LVC_CHPOS",
// which is what sessions saved before LVC_ROUTE existed contain.
static constexpr int kRouteSlice = kFxChroma + 1;   // id of the slicer in a route
static constexpr int kRouteItems = kFxCount + 2;    // slicer + four effects + pitchmap

static inline void decodePerm(int code, int n, int* out)
{
    int digits[kRouteItems], avail[kRouteItems];
    for (int i = n - 1; i >= 0; --i) { digits[i] = code % (n - i); code /= (n - i); }
    for (int i = 0; i < n; ++i) avail[i] = i;
    int left = n;
    for (int i = 0; i < n; ++i)
    {
        const int d = limit<int>(digits[i], 0, left - 1);
        out[i] = avail[d];
        for (int j = d; j < left - 1; ++j) avail[j] = avail[j + 1];
        --left;
    }
}

static inline int encodePerm(const int* order, int n)
{
    int avail[kRouteItems];
    for (int i = 0; i < n; ++i) avail[i] = i;
    int code = 0, left = n;
    for (int i = 0; i < n; ++i)
    {
        int d = 0;
        while (d < left && avail[d] != order[i]) ++d;
        d = limit<int>(d, 0, left - 1);
        code = code * (n - i) + d;
        for (int j = d; j < left - 1; ++j) avail[j] = avail[j + 1];
        --left;
    }
    return code;
}

// the route in effect: LVC_ROUTE, or the older effect order + pitchmap slot
static inline void routeFromParams(float route, float fxOrder, float chPos, int out[kRouteItems])
{
    const int code = static_cast<int>(std::floor(route + 0.5f));
    if (code >= 0)
    {
        decodePerm(limit<int>(code, 0, 719), kRouteItems, out);
        return;
    }
    int four[kFxCount];
    decodeFxOrder(static_cast<int>(fxOrder + 0.5f), four);
    const int slot = limit<int>(static_cast<int>(chPos + 0.5f), 0, kFxCount + 1);
    int k = 0;
    if (slot == 0) out[k++] = kFxChroma;
    out[k++] = kRouteSlice;
    for (int p = 0, j = 0; p <= kFxCount && k < kRouteItems; ++p)
    {
        if (slot != 0 && p == slot - 1) out[k++] = kFxChroma;
        else if (j < kFxCount) out[k++] = four[j++];
    }
}

// The signal-flow row: the six items of the route plus the transient shaper, which has its own position parameter
// (LVC_TRPOS) so that LVC_ROUTE keeps its meaning.
static constexpr int kFxTransient = kRouteSlice + 1;
static constexpr int kFlowItems   = kRouteItems + 1;

static inline void flowFromParams(const float* control, int out[kFlowItems])
{
    int six[kRouteItems];
    routeFromParams(control[LVC_ROUTE], control[LVC_FXORDER], control[LVC_CHPOS], six);
    const int tp = limit<int>(static_cast<int>(control[LVC_TRPOS] + 0.5f), 0, kFlowItems - 1);
    for (int k = 0, j = 0; k < kFlowItems; ++k)
        out[k] = (k == tp) ? kFxTransient : six[j++];
}

// fill mode: cycle length and active bars at the end of each cycle
static const long LVC_FILLCYCLE_OPTIONS[3] = { 8, 16, 32 };
static const long LVC_FILLBARS_OPTIONS[4] = { 1, 2, 4, 8 };

// shortest allowed cut, in quarter notes, for each LVC_MINCUT option
static const double LVC_MINCUT_QUARTERS[3] = { 0.0, 0.25, 0.5 };

// Algorithms. LVC_CUTPROC keeps its original 0..2 range so that old sessions (plain values 0 / 1 / 2) and
// old host automation (normalised 0 / 0.5 / 1) still select the same algorithm. The two newer algorithms
// sit on the half steps: 0 CutProc11, 0.5 Euclid, 1 WarpCut, 1.5 Ratchet, 2 SQPusher.
// The trance gate has its own switch (LVC_GATE) that overrides LVC_CUTPROC: adding a sixth value to LVC_CUTPROC
// would change its step count, and with it what old normalised automation selects.
// The values match the cutter's CutId.
enum LiveCutAlgo { kAlgoCutProc11 = 0, kAlgoWarpCut, kAlgoSQPusher, kAlgoEuclid, kAlgoRatchet, kAlgoGate, kAlgoCount };

static inline int algoFromParam(float v)
{
    static const int slotToAlgo[5] = { kAlgoCutProc11, kAlgoEuclid, kAlgoWarpCut, kAlgoRatchet, kAlgoSQPusher };
    const int slot = static_cast<int>(v * 2.0f + 0.5f);
    return slotToAlgo[slot < 0 ? 0 : (slot > 4 ? 4 : slot)];
}

// LVC_CUTPROC value of an algorithm (the gate keeps whatever LVC_CUTPROC holds: 0 = CutProc11)
static inline float paramFromAlgo(int algo)
{
    static const float values[kAlgoCount] = { 0.0f, 1.0f, 2.0f, 0.5f, 1.5f, 0.0f };
    return values[algo < 0 ? 0 : (algo >= kAlgoCount ? kAlgoCount - 1 : algo)];
}

// the algorithm in effect, from a full parameter set
static inline int algoFromParams(const float* control)
{
    return control[LVC_GATE] > 0.5f ? kAlgoGate : algoFromParam(control[LVC_CUTPROC]);
}

// trance gate pattern: bit i = step i + 1 is open
static constexpr int LVC_GATE_MAX_STEPS = 32;

// step envelopes: two steps per parameter, 4-bit attack + 4-bit decay level each (0 = none, 15 = whole step)
static constexpr int LVC_GATE_ENV_PARAMS = LVC_GATE_MAX_STEPS / 2;

static inline uint32_t gateEnvPacked(float v)
{
    return static_cast<uint32_t>(limit<float>(v, 0.f, 65535.f) + 0.5f) & 0xFFFFu;
}

static inline int gateEnvLevel(const float* control, int step, bool decay)
{
    if (step < 0 || step >= LVC_GATE_MAX_STEPS) return 0;
    const uint32_t v = gateEnvPacked(control[LVC_GATEENV1 + step / 2]);
    return static_cast<int>((v >> ((step % 2) * 8 + (decay ? 4 : 0))) & 15u);
}

// the parameter value with one level replaced
static inline float gateEnvWithLevel(float paramValue, int step, bool decay, int level)
{
    const int shift = (step % 2) * 8 + (decay ? 4 : 0);
    uint32_t v = gateEnvPacked(paramValue);
    v = (v & ~(15u << shift)) | (static_cast<uint32_t>(limit<int>(level, 0, 15)) << shift);
    return static_cast<float>(v);
}

// envelope curve tension of each step: signed 8 bits (-127..127 = -1..1, 0 = straight line), two steps per parameter
static constexpr int LVC_GATE_TENSION_PARAMS = LVC_GATE_MAX_STEPS / 2;

static inline float gateTensionFromPacked(uint32_t packed, int step)
{
    const int raw = static_cast<int>((packed >> ((step % 2) * 8)) & 255u);
    const int v = raw >= 128 ? raw - 256 : raw;
    return static_cast<float>(limit<int>(v, -127, 127)) / 127.f;
}

static inline float gateTension(const float* control, int step)
{
    if (step < 0 || step >= LVC_GATE_MAX_STEPS) return 0.f;
    return gateTensionFromPacked(gateEnvPacked(control[LVC_GATETENSION1 + step / 2]), step);
}

static inline float gateTensionWith(float paramValue, int step, float tension)
{
    const int shift = (step % 2) * 8;
    const int v = limit<int>(static_cast<int>(std::lround(tension * 127.f)), -127, 127);
    uint32_t p = gateEnvPacked(paramValue);
    p = (p & ~(255u << shift)) | ((static_cast<uint32_t>(v) & 255u) << shift);
    return static_cast<float>(p);
}

// gate envelope -> filter: the cutoffs for an envelope value (1 = step fully open: both filters out of the way)
static constexpr float LVC_GATE_FILTER_OCTAVES = 8.f;

static inline float gateLowpassHz(float depth, float env)
{
    return 20000.f * std::exp2(-LVC_GATE_FILTER_OCTAVES * depth * (1.f - env));
}

static inline float gateHighpassHz(float depth, float env)
{
    return 20.f * std::exp2(LVC_GATE_FILTER_OCTAVES * depth * (1.f - env));
}

// echo note values: length in quarter notes, or in bars (follows the time signature)
struct LvcDelayDiv { const char* name; float length; bool bars; };
static const LvcDelayDiv LVC_DELAY_DIVS[] = {
    { "1/32", 0.125f, false }, { "1/16T", 1.f / 6.f, false }, { "1/16", 0.25f, false }, { "1/16D", 0.375f, false },
    { "1/8T", 1.f / 3.f, false }, { "1/8", 0.5f, false }, { "1/8D", 0.75f, false }, { "1/4T", 2.f / 3.f, false },
    { "1/4", 1.f, false }, { "1/4D", 1.5f, false }, { "1/2T", 4.f / 3.f, false }, { "1/2", 2.f, false },
    { "1/2D", 3.f, false }, { "1 BAR", 1.f, true }, { "2 BARS", 2.f, true },
};
static constexpr int LVC_NUM_DELAY_DIVS = static_cast<int>(sizeof(LVC_DELAY_DIVS) / sizeof(LVC_DELAY_DIVS[0]));
static_assert(LVC_NUM_DELAY_DIVS == 15, "LVC_DLYTIMEL / R range must match the note value table");

static inline const LvcDelayDiv& delayDiv(float v)
{
    return LVC_DELAY_DIVS[limit<int>(static_cast<int>(v + 0.5f), 0, LVC_NUM_DELAY_DIVS - 1)];
}

// echo time in seconds for a note value at a tempo (quarters per bar from the time signature)
static inline double delayDivSeconds(const LvcDelayDiv& d, double bpm, double quartersPerBar)
{
    const double quarters = d.bars ? d.length * quartersPerBar : d.length;
    return quarters * 60.0 / (bpm > 1.0 ? bpm : 120.0);
}

static inline uint32_t gatePatternFromParams(float lo, float hi)
{
    const uint32_t a = static_cast<uint32_t>(limit<float>(lo, 0.f, 65535.f) + 0.5f) & 0xFFFFu;
    const uint32_t b = static_cast<uint32_t>(limit<float>(hi, 0.f, 65535.f) + 0.5f) & 0xFFFFu;
    return a | (b << 16);
}

static const long LVC_SUBDIV_OPTIONS[7] = {
    6, 8, 12, 16, 18, 24, 32
};

#endif
