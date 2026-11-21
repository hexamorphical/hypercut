/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2021 Filipe Coelho <falktx@falktx.com>
 * Copyright (C) 2020 Takamitsu Endo
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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

#include "DistrhoPlugin.hpp"
#include "LiveCutControls.hpp"
#include "LiveCutVis.hpp"
#include "BBCutter.h"
#include "BitCrusher.h"
#include "Comb.h"
#include "EchoDelay.h"
#include "DelaySlice.h"
#include "SVFilter.h"
#include "Limiter.h"
#include "SliceFilter.h"
#include "SliceReverb.h"
#include "SpectralResonator.h"
#include "TransientShaper.h"

#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

static_assert(LVC_CONTROL_NR <= LiveCutVis::kMaxParams, "LiveCutVis::kMaxParams must cover every parameter ID");

// -----------------------------------------------------------------------------------------------------------

/**
  Livecut beat-slicer audio plugin adapted for building with DISTRHO DPF. Livecut Copyright 2003 by Remy Muller.
 */
class HyperCut : public Plugin
{
    // Listens to the cutter so that the phrase/block/cut structure can be published to the UI.
    struct VisListener : public BBCutListener
    {
        HyperCut* self = nullptr;
        void OnPhrase(long, long) override { self->visOnPhrase(); }
        void OnBlock(long, long) override  { self->visOnBlock(); }
    };

public:
    LiveCutVis::State vis;

    HyperCut()
        // 1 program: keeps the VST3 parameter IDs identical to v1 (see DistrhoPluginInfo.h), 0 states
        : Plugin(LVC_CONTROL_NR, 1, 0),
          sampleRate(getSampleRate()),
          bbcutter(player),
          oldpositionInMeasure(0),
          div(8),
          lowpass(SVFilter::kLowpass),
          highpass(SVFilter::kHighpass)
    {
        for (int32_t i = 0; i < LVC_CONTROL_NR; i++)
            control[i] = LVC_DEFAULTS[i];

        sampleRateChanged(sampleRate);

        visListener.self = this;
        bbcutter.RegisterListener(&crusher);
        bbcutter.RegisterListener(&comb);
        bbcutter.RegisterListener(&delaySlice);
        bbcutter.RegisterListener(&sliceFilter);
        bbcutter.RegisterListener(&reverbSends);
        bbcutter.RegisterListener(&visListener);

        comb.SetOn(false);
        crusher.SetOn(false);

        mixSmooth = control[LVC_MIX];
        lpSmooth = std::log(control[LVC_LPFREQ]);
        hpSmooth = std::log(control[LVC_HPFREQ]);

        // constant latency from the limiter lookahead (also when the limiter is off)
        limiterLatency = limiter.setup(sampleRate);
        setLatency(limiterLatency);
    }

    // ---- visualisation hooks (called from the audio thread, inside run()) ----

    void visOnPhrase()
    {
        vis.beginPhraseWrite();
        vis.phrase.totalUnits = static_cast<int32_t>(bbcutter.GetTotalUnits());
        vis.phrase.subdiv     = static_cast<int32_t>(bbcutter.GetSubdiv());
        vis.phrase.numBlocks  = 0;
        vis.phrase.numCuts    = 0;
        vis.phrase.phraseId  += 1;
        vis.endPhraseWrite();
        visBlockIndex = -1;
        visBlockFirstCut = 0;
    }

    // the logo sparkles: count each audible slice start (muted gate steps and tied continuations don't count)
    void visCountChop(long cut)
    {
        if (gateSmooth < 0.5f || cut < 0 || cut >= player.GetNumCuts())
            return;
        const CutInfo& ci = player.GetCuts()[cut];
        if (ci.length <= 0 || ci.tiedin || ci.amp < 0.02f)
            return;
        vis.chopSeconds.store(static_cast<float>(static_cast<double>(ci.size) / std::max(1.0, static_cast<double>(sampleRate))), std::memory_order_relaxed);
        vis.chopAmp.store(ci.amp, std::memory_order_relaxed);
        vis.chopCount.fetch_add(1, std::memory_order_release);
    }

    // store whether (and how much) the cut that just started is sent to the reverb
    void visMarkReverb(long cut)
    {
        const float send = control[LVC_REVERB] > 0.5f ? reverbSends.Send() : 0.f;
        const int gi = visBlockFirstCut + static_cast<int>(cut);
        if (visBlockIndex < 0 || gi < 0 || gi >= vis.phrase.numCuts)
            return;
        if (vis.phrase.cuts[gi].verb == send)
            return;
        vis.beginPhraseWrite();
        vis.phrase.cuts[gi].verb = send;
        vis.endPhraseWrite();
    }

    void visOnBlock()
    {
        const std::vector<CutInfo>& cuts = player.GetCuts();
        const double spu = bbcutter.SamplesPerUnit();

        vis.beginPhraseWrite();
        LiveCutVis::Phrase& ph = vis.phrase;
        if (ph.numBlocks < LiveCutVis::kMaxBlocks)
        {
            LiveCutVis::Block& b = ph.blocks[ph.numBlocks];
            b.startUnit = static_cast<int16_t>(bbcutter.GetUnitsDone());
            b.units     = static_cast<int16_t>(bbcutter.GetUnitsInBlock());
            b.firstCut  = static_cast<int16_t>(ph.numCuts);
            b.numCuts   = 0;
            b.kind      = static_cast<uint8_t>(bbcutter.GetLastKind());
            const int* info = bbcutter.GetLastInfo();
            b.info[0] = static_cast<int16_t>(info[0]);
            b.info[1] = static_cast<int16_t>(info[1]);
            b.info[2] = static_cast<int16_t>(info[2]);

            for (const CutInfo& c : cuts)
            {
                if (ph.numCuts >= LiveCutVis::kMaxCuts)
                    break;
                LiveCutVis::Cut& vc = ph.cuts[ph.numCuts++];
                vc.units = spu > 0.0 ? static_cast<float>(c.size / spu) : 0.f;
                vc.duty  = c.size > 0 ? static_cast<float>(c.length) / static_cast<float>(c.size) : 1.f;
                vc.amp   = c.amp;
                vc.pan   = c.pan;
                vc.cents = c.cents;
                vc.offset = spu > 0.0 ? static_cast<float>(c.offset / spu) : 0.f;
                vc.verb = 0.f;
                ++b.numCuts;
            }

            visBlockIndex = ph.numBlocks;
            visBlockFirstCut = b.firstCut;
            ++ph.numBlocks;
        }
        vis.endPhraseWrite();

        ++blockCounter;
    }

    // Publish the value currently selected for every min/max range knob, so the UI can rotate its pointer.
    void visPublishCurrentValues()
    {
        auto put = [&](int idx, float value) {
            if (idx >= 0 && idx < LiveCutVis::kMaxParams)
            {
                vis.paramCurrent[idx] = value;
                vis.paramCurrentValid[idx] = 1;
            }
        };

        const int32_t sub = vis.phrase.subdiv > 0 ? vis.phrase.subdiv : 1;
        put(LVC_MINPHRS, static_cast<float>(vis.phrase.totalUnits) / static_cast<float>(sub));

        const int cb = vis.currentBlock.load(std::memory_order_relaxed);
        if (cb >= 0 && cb < vis.phrase.numBlocks)
            put(LVC_MINREP, static_cast<float>(vis.phrase.blocks[cb].info[1]));
        const int cc = vis.currentCut.load(std::memory_order_relaxed);
        if (cc >= 0 && cc < vis.phrase.numCuts)
        {
            put(LVC_MINAMP,   vis.phrase.cuts[cc].amp);
            put(LVC_MINPAN,   vis.phrase.cuts[cc].pan);
            put(LVC_MINPITCH, vis.phrase.cuts[cc].cents);
        }

        put(LVC_LPMIN,    std::exp(lpSmooth));
        put(LVC_HPFREQ,   sliceFilter.Highpass());
        put(LVC_MINBITS,  crusher.GetBits());
        put(LVC_MINFREQ,  crusher.GetFreq());
        put(LVC_MINDELAY, comb.GetDelayMs());
        put(LVC_RVSENDMIN,  reverbSends.Send());
        put(LVC_RVDECAYMIN, reverbSends.Decay());
        put(LVC_MINFEEDBACK, delaySlice.Feedback());
        put(LVC_MINDLYMIX,   delaySlice.Mix());

        // REGION is not randomised: mark the loudest detected note (in Hz), or the middle of the range in silence.
        float best = -1.f;
        int bestNote = -1;
        for (int n = 0; n < LiveCutVis::kChromaNotes; ++n)
        {
            const float level = vis.chromaDry[n] + vis.chromaTuned[n];
            if (level > best) { best = level; bestNote = n; }
        }
        if (best > 1e-4f && bestNote >= 0)
        {
            const double refA4 = lvcTuneRefA4(control[LVC_CHTUNE]);
            put(LVC_CHLOW, static_cast<float>(refA4 * std::pow(2.0, (bestNote - 69) / 12.0)));
        }
        else
            put(LVC_CHLOW, 0.5f * (control[LVC_CHLOW] + control[LVC_CHHIGH]));
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * Information */

    const char* getLabel() const override { return "HyperCut"; }
    const char* getDescription() const override { return "Beat-slicer"; }
    const char* getMaker() const override { return "eventual-recluse"; }
    const char* getHomePage() const override { return "https://github.com/hexamorphical/hypercut"; }
    const char* getLicense() const override { return "GPL"; }
    uint32_t getVersion() const override { return d_version(2, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('H', 'C', 'u', 't'); }

   /* --------------------------------------------------------------------------------------------------------
    * Init */

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.groupId = kPortGroupStereo;
        Plugin::initAudioPort(input, index, port);
    }

    static void setEnum(Parameter& parameter, const char* const* labels, uint32_t count)
    {
        parameter.enumValues.count = count;
        parameter.enumValues.restrictedMode = true;
        ParameterEnumerationValue* const values = new ParameterEnumerationValue[count];
        parameter.enumValues.values = values;
        for (uint32_t i = 0; i < count; ++i)
        {
            values[i].label = labels[i];
            values[i].value = static_cast<float>(i);
        }
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index >= LVC_CONTROL_NR)
            return;

        parameter.hints = kParameterIsAutomatable;
        parameter.ranges.min = controlLimits[index].first;
        parameter.ranges.max = controlLimits[index].second;
        parameter.ranges.def = LVC_DEFAULTS[index];

        // names and symbols of the original 33 parameters are kept unchanged for session compatibility
        switch (index)
        {
        case LVC_CUTPROC:
        {
            // not integer: the newer algorithms live on half steps (see algoFromParam)
            static const char* const labels[5] = { "CutProc11", "Euclid", "WarpCut", "Ratchet", "SQPusher" };
            parameter.name   = "CutProc";
            parameter.symbol = "livecut_cutproc";
            setEnum(parameter, labels, 5);
            for (uint32_t k = 0; k < 5; ++k)
                parameter.enumValues.values[k].value = 0.5f * static_cast<float>(k);
            break;
        }
        case LVC_SUBDIV:
        {
            static const char* const labels[7] = { "6", "8", "12", "16", "18", "24", "32" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "SubDiv";
            parameter.symbol = "livecut_subdiv";
            setEnum(parameter, labels, 7);
            break;
        }
        case LVC_FADE:
            parameter.name = "Fade"; parameter.symbol = "livecut_fade"; parameter.unit = "ms";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_MINAMP:   parameter.name = "Min Amp";   parameter.symbol = "livecut_minamp"; break;
        case LVC_MAXAMP:   parameter.name = "Max Amp";   parameter.symbol = "livecut_maxamp"; break;
        case LVC_MINPAN:   parameter.name = "Min Pan";   parameter.symbol = "livecut_minpan"; break;
        case LVC_MAXPAN:   parameter.name = "Max Pan";   parameter.symbol = "livecut_maxpan"; break;
        case LVC_MINPITCH: parameter.name = "Min Pitch"; parameter.symbol = "livecut_minpitch"; parameter.unit = "cents"; break;
        case LVC_MAXPITCH: parameter.name = "Max Pitch"; parameter.symbol = "livecut_maxpitch"; parameter.unit = "cents"; break;
        case LVC_DUTY:     parameter.name = "Duty";      parameter.symbol = "livecut_duty"; break;
        case LVC_FILLDUTY: parameter.name = "Fill Duty"; parameter.symbol = "livecut_fillduty"; break;
        case LVC_MAXPHRS:
            parameter.name = "Max Phrase"; parameter.symbol = "livecut_maxphrs";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_MINPHRS:
            parameter.name = "Min Phrase"; parameter.symbol = "livecut_minphrs";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_MAXREP:
            parameter.name = "CutProc11 Max Rep"; parameter.symbol = "livecut_maxrep";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_MINREP:
            parameter.name = "CutProc11 Min Rep"; parameter.symbol = "livecut_minrep";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_STUTTER:  parameter.name = "CutProc 11 Stutter"; parameter.symbol = "livecut_stutter"; break;
        case LVC_AREA:     parameter.name = "CutProc 11 Area";    parameter.symbol = "livecut_area"; break;
        case LVC_STRAIGHT: parameter.name = "WarpCut Straight";   parameter.symbol = "livecut_straight"; break;
        case LVC_REGULAR:  parameter.name = "WarpCut Regular";    parameter.symbol = "livecut_regular"; break;
        case LVC_RITARD:   parameter.name = "WarpCut Ritard";     parameter.symbol = "livecut_ritard"; break;
        case LVC_SPEED:    parameter.name = "WarpCut Speed";      parameter.symbol = "livecut_speed"; break;
        case LVC_ACTIVITY: parameter.name = "SQPusher Activity";  parameter.symbol = "livecut_activity"; break;
        case LVC_CRUSHER:
            parameter.name = "Crusher"; parameter.symbol = "livecut_crusher";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_MINBITS: parameter.name = "Crusher Min Bits"; parameter.symbol = "livecut_minbits"; break;
        case LVC_MAXBITS: parameter.name = "Crusher Max Bits"; parameter.symbol = "livecut_maxbits"; break;
        case LVC_MINFREQ:
            parameter.name = "Crusher Min Freq"; parameter.symbol = "livecut_minfreq"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_MAXFREQ:
            parameter.name = "Crusher Max Freq"; parameter.symbol = "livecut_maxfreq"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_COMB:
            parameter.name = "Comb"; parameter.symbol = "livecut_comb";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_TYPE:
        {
            static const char* const labels[2] = { "FeedFwd", "FeedBack" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Comb Type";
            parameter.symbol = "livecut_type";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_FEEDBACK: parameter.name = "Delay Feedback"; parameter.symbol = "livecut_feedback"; break;
        case LVC_MINDELAY: parameter.name = "Comb Min Delay"; parameter.symbol = "livecut_mindelay"; parameter.unit = "ms"; break;
        case LVC_MAXDELAY: parameter.name = "Comb Max Delay"; parameter.symbol = "livecut_maxdelay"; parameter.unit = "ms"; break;
        case LVC_SEED:
            parameter.name = "Seed"; parameter.symbol = "livecut_seed";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_LPFREQ:
            parameter.name = "Lowpass Max"; parameter.symbol = "livecut_lpfreq"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_HPFREQ:
            parameter.name = "Highpass Min"; parameter.symbol = "livecut_hpfreq"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_RESO:
            parameter.name = "Filter Resonance"; parameter.symbol = "livecut_reso";
            break;
        case LVC_FILTERDRIVE:
            parameter.name = "Filter Drive"; parameter.symbol = "livecut_filterdrive";
            break;
        case LVC_CRUSHMIX:
            parameter.name = "Crusher Dry/Wet"; parameter.symbol = "livecut_crushmix";
            break;
        case LVC_CRUSHLP:
            parameter.name = "Crusher Post Lowpass"; parameter.symbol = "livecut_crushlp";
            parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_MIX:
            parameter.name = "Dry/Wet"; parameter.symbol = "livecut_mix";
            break;
        case LVC_LIMITER:
            parameter.name = "Limiter"; parameter.symbol = "livecut_limiter";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_LIMCEIL:
            parameter.name = "Limiter Ceiling"; parameter.symbol = "livecut_limceil"; parameter.unit = "dB";
            break;
        case LVC_LIMREL:
            parameter.name = "Limiter Release"; parameter.symbol = "livecut_limrel"; parameter.unit = "ms";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_MINCUT:
        {
            static const char* const labels[3] = { "Any", "1/16", "1/8" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Shortest Cut";
            parameter.symbol = "livecut_mincut";
            setEnum(parameter, labels, 3);
            break;
        }
        case LVC_EUDENSITY: parameter.name = "Euclid Density"; parameter.symbol = "livecut_eudensity"; break;
        case LVC_EUCHANCE:  parameter.name = "Euclid Chance";  parameter.symbol = "livecut_euchance"; break;
        case LVC_RAINTENSITY: parameter.name = "Ratchet Intensity"; parameter.symbol = "livecut_raintensity"; break;
        case LVC_RACURVE:   parameter.name = "Ratchet Curve"; parameter.symbol = "livecut_racurve"; break;
        case LVC_FILLMODE:
            parameter.name = "Fill Mode"; parameter.symbol = "livecut_fillmode";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_FILLCYCLE:
        {
            static const char* const labels[3] = { "8 bars", "16 bars", "32 bars" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Fill Cycle";
            parameter.symbol = "livecut_fillcycle";
            setEnum(parameter, labels, 3);
            break;
        }
        case LVC_FILLBARS:
        {
            static const char* const labels[4] = { "1 bar", "2 bars", "4 bars", "8 bars" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Fill Length";
            parameter.symbol = "livecut_fillbars";
            setEnum(parameter, labels, 4);
            break;
        }
        case LVC_REVERB:
            parameter.name = "Reverb";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_RVCHANCE:   parameter.name = "Reverb Chance"; break;
        case LVC_RVSENDMIN:  parameter.name = "Reverb Send Min"; break;
        case LVC_RVSENDMAX:  parameter.name = "Reverb Send Max"; break;
        case LVC_RVDECAYMIN:
            parameter.name = "Reverb Decay Min"; parameter.unit = "s";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_RVDECAYMAX:
            parameter.name = "Reverb Decay Max"; parameter.unit = "s";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_RVSIZE:     parameter.name = "Reverb Size"; break;
        case LVC_RVDAMP:     parameter.name = "Reverb Damping"; break;
        case LVC_LPMIN:
            parameter.name = "Lowpass Min"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_HPMAX:
            parameter.name = "Highpass Max"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_FILTER:
            parameter.name = "Filter";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_EUSLICE:
        {
            static const char* const labels[5] = { "1st hit", "2nd hit", "3rd hit", "4th hit", "Random" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Euclid Slice";
            setEnum(parameter, labels, 5);
            break;
        }
        case LVC_MASTERHPFREQ:
            parameter.name = "Master Highpass"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_MASTERLPFREQ:
            parameter.name = "Master Lowpass"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_RVMODE:
        {
            static const char* const labels[2] = { "Send", "Insert" };
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Reverb Mode";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_FXORDER:
            parameter.name = "Effect Order"; parameter.hints |= kParameterIsInteger;
            break;
        case LVC_CHROMA:
            parameter.name = "Spectral Pitchmap";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_CHPOS:
            // 0 = in front of the slicer, 1..5 = slots after it, among the four other effects
            parameter.name = "Pitchmap Slot"; parameter.hints |= kParameterIsInteger;
            break;
        case LVC_CHKEY:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Pitchmap Key";
            setEnum(parameter, LVC_KEY_NAMES, 12);
            break;
        case LVC_CHSCALE:
        {
            static const char* labels[LVC_NUM_SCALES];
            for (int k = 0; k < LVC_NUM_SCALES; ++k) labels[k] = LVC_SCALES[k].name;
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Pitchmap Scale";
            setEnum(parameter, labels, LVC_NUM_SCALES);
            break;
        }
        case LVC_CHCUSTOM:
            // twelve pitch classes of the Custom scale (bit i = LVC_CHKEY + i), edited from the piano keyboard
            parameter.name = "Pitchmap Custom Scale"; parameter.hints |= kParameterIsInteger;
            break;
        case LVC_CHCOLOR:   parameter.name = "Pitchmap Color"; break;
        case LVC_CHPITCH:
            parameter.name = "Pitchmap Pitch"; parameter.unit = "st";
            parameter.hints |= kParameterIsInteger;
            break;
        case LVC_CHTUNE:    parameter.name = "Pitchmap Tune"; parameter.unit = "cents"; break;
        case LVC_CHGAIN:    parameter.name = "Pitchmap Gain"; parameter.unit = "dB"; break;
        case LVC_CHLOW:
            parameter.name = "Pitchmap Low"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_CHHIGH:
            parameter.name = "Pitchmap High"; parameter.unit = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_CHMIDI:
            parameter.name = "Pitchmap MIDI";
            parameter.hints |= kParameterIsBoolean;
           #if ! DISTRHO_PLUGIN_WANT_MIDI_INPUT
            parameter.hints |= kParameterIsHidden; // kept so the parameter list is the same in both builds
           #endif
            break;
        case LVC_CHQUALITY:
            // unused: Chroma has a single (high quality) engine; the slot stays so later parameters keep their ids
            parameter.name = "Pitchmap Quality (unused)";
            parameter.hints = kParameterIsHidden;
            break;
        case LVC_CHFFT:
        {
            static const char* const labels[2] = { "512", "1024" };
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Pitchmap FFT Size";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_CHMORPH:
            parameter.name = "Pitchmap Morph";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_CHMORPHAMT: parameter.name = "Pitchmap Morph Amount"; break;
        case LVC_CHGATE:
            parameter.name = "Pitchmap Gate";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_CHGATETHR:
            parameter.name = "Pitchmap Gate Threshold"; parameter.unit = "dB";
            break;
        case LVC_SLICER:
            parameter.name = "Slicer";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_TRANSIENT:
            parameter.name = "Transient Shaper";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_TRATTACK:  parameter.name = "Transient Attack"; break;
        case LVC_TRPUMP:    parameter.name = "Transient Pump"; break;
        case LVC_TRSUSTAIN: parameter.name = "Transient Sustain"; break;
        case LVC_TRPOS:
            parameter.name = "Transient Slot"; parameter.hints |= kParameterIsInteger;
            break;
        case LVC_MAKEUP:
            parameter.name = "Makeup Gain"; parameter.unit = "dB";
            break;
        case LVC_GATE:
            // the trance gate algorithm; when on it replaces the CutProc algorithm
            parameter.name = "Trance Gate";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_GATEMODE:
        {
            static const char* const labels[2] = { "Random", "Pattern" };
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Gate Mode";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_GATEDENSITY: parameter.name = "Gate Density"; break;
        case LVC_GATEVOL:     parameter.name = "Gate Volume Depth"; break;
        case LVC_DLYMODE:
        {
            static const char* const labels[2] = { "Comb", "Echo" };
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Delay Mode";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_DLYROUTE:
        {
            static const char* const labels[2] = { "Insert", "Send" };
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Delay Route";
            setEnum(parameter, labels, 2);
            break;
        }
        case LVC_DLYSYNC:
            parameter.name = "Delay Sync";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_DLYTIMEL: case LVC_DLYTIMER:
        {
            static const char* labels[LVC_NUM_DELAY_DIVS];
            for (int k = 0; k < LVC_NUM_DELAY_DIVS; ++k) labels[k] = LVC_DELAY_DIVS[k].name;
            parameter.hints |= kParameterIsInteger;
            parameter.name = index == LVC_DLYTIMEL ? "Delay Time L" : "Delay Time R";
            setEnum(parameter, labels, LVC_NUM_DELAY_DIVS);
            break;
        }
        case LVC_DLYMSL: case LVC_DLYMSR:
            parameter.name = index == LVC_DLYMSL ? "Delay Time L (free)" : "Delay Time R (free)";
            parameter.unit = "ms";
            parameter.hints |= kParameterIsLogarithmic;
            break;
        case LVC_DLYSTEREO:
        {
            static const char* const labels[3] = { "Mono", "Stereo", "Ping-Pong" };
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Delay Stereo";
            setEnum(parameter, labels, 3);
            break;
        }
        case LVC_DLYMIX:      parameter.name = "Delay Mix"; break;
        case LVC_DLYTONE:     parameter.name = "Delay Tone"; break;
        case LVC_DLYTUNE:
            parameter.name = "Delay Tune To Scale";
            parameter.hints |= kParameterIsBoolean;
            break;
        case LVC_MINFEEDBACK: parameter.name = "Delay Feedback Min"; break;
        case LVC_MINDLYMIX:   parameter.name = "Delay Mix Min"; break;
        case LVC_GATELP:      parameter.name = "Gate Lowpass Depth"; break;
        case LVC_GATEHP:      parameter.name = "Gate Highpass Depth"; break;
        case LVC_GATEVARY:    parameter.name = "Gate Variation"; break;
        case LVC_GATEPAT1:
            // drawn step pattern as a bit mask (bit 0 = step 1), edited from the UI's step grid
            parameter.name = "Gate Steps 1-16"; parameter.hints |= kParameterIsInteger;
            break;
        case LVC_GATEPAT2:
            parameter.name = "Gate Steps 17-32"; parameter.hints |= kParameterIsInteger;
            break;
        default:
            if (index >= LVC_GATEENV1 && index < LVC_GATEENV1 + LVC_GATE_ENV_PARAMS)
            {
                // packed attack / decay levels of two steps, edited in the UI's step sequencer
                static char names[LVC_GATE_ENV_PARAMS][32];
                const int k = static_cast<int>(index - LVC_GATEENV1);
                std::snprintf(names[k], sizeof(names[k]), "Gate Envelope %d-%d", k * 2 + 1, k * 2 + 2);
                parameter.name = names[k];
                parameter.hints |= kParameterIsInteger;
            }
            else if (index >= LVC_GATETENSION1 && index < LVC_GATETENSION1 + LVC_GATE_TENSION_PARAMS)
            {
                // envelope curve tension of two steps (signed 8 bits each), edited in the UI's step sequencer
                static char tnames[LVC_GATE_TENSION_PARAMS][32];
                const int k = static_cast<int>(index - LVC_GATETENSION1);
                std::snprintf(tnames[k], sizeof(tnames[k]), "Gate Curve %d-%d", k * 2 + 1, k * 2 + 2);
                parameter.name = tnames[k];
                parameter.hints |= kParameterIsInteger;
            }
            else if (index == LVC_GATETIE1 || index == LVC_GATETIE2)
            {
                // ties as a bit mask (bit 0 = step 1 runs into step 2), drawn by dragging across steps in the UI
                parameter.name = index == LVC_GATETIE1 ? "Gate Ties 1-16" : "Gate Ties 17-32";
                parameter.hints |= kParameterIsInteger;
            }
            break;
        case LVC_ROUTE:
            // -1 = take the order from "Effect Order" and "Pitchmap Slot" (sessions saved before this existed)
            parameter.name = "Effect Route";
            parameter.hints |= kParameterIsInteger;
            break;
        }

        // one table of symbols shared with the UI's preset files; must match the names set above
        parameter.symbol = LVC_SYMBOLS[index];
    }

    // A single placeholder program. It only exists so DPF's internal VST3 parameters keep the same count as
    // v1, which keeps every parameter ID (and so old automation) unchanged. Loading it changes nothing.
    void initProgramName(uint32_t index, String& programName) override
    {
        if (index == 0)
            programName = "Default";
    }

    void loadProgram(uint32_t) override {}

    void update_parameter(uint32_t i)
    {
        control[i] = limit<float>(control[i], controlLimits[i].first, controlLimits[i].second);

        switch (i)
        {
        case LVC_CUTPROC: case LVC_GATE: bbcutter.SetCutProc(algoFromParams(control)); break;
        case LVC_GATEMODE:    bbcutter.SetGateMode(control[i] > 0.5f ? 1 : 0); break;
        case LVC_GATEDENSITY: bbcutter.SetGateDensity(control[i]); break;
        case LVC_GATEVARY:    bbcutter.SetGateVariation(control[i]); break;
        case LVC_GATEVOL:     player.SetGateDepth(control[i]); break;
        case LVC_GATEPAT1: case LVC_GATEPAT2:
            bbcutter.SetGatePattern(gatePatternFromParams(control[LVC_GATEPAT1], control[LVC_GATEPAT2]));
            break;
        case LVC_SUBDIV:
            // applied at the start of run(): the cutter's phrase is moved to the new grid on the audio thread
            pendingDiv = LVC_SUBDIV_OPTIONS[limit<long>(static_cast<long>(control[i] + 0.5f), 0, 6)];
            break;
        case LVC_FADE:     bbcutter.SetFade(control[i]); break;
        case LVC_MINAMP:   bbcutter.SetMinAmp(control[i]); break;
        case LVC_MAXAMP:   bbcutter.SetMaxAmp(control[i]); break;
        case LVC_MINPAN:   bbcutter.SetMinPan(control[i]); break;
        case LVC_MAXPAN:   bbcutter.SetMaxPan(control[i]); break;
        case LVC_MINPITCH: bbcutter.SetMinDetune(control[i]); break;
        case LVC_MAXPITCH: bbcutter.SetMaxDetune(control[i]); break;
        case LVC_DUTY:     bbcutter.SetDutyCycle(control[i]); break;
        case LVC_FILLDUTY: bbcutter.SetFillDutyCycle(control[i]); break;
        case LVC_MAXPHRS:  bbcutter.SetMaxPhraseLength(static_cast<long>(control[i])); break;
        case LVC_MINPHRS:  bbcutter.SetMinPhraseLength(static_cast<long>(control[i])); break;
        case LVC_MAXREP:   bbcutter.SetMaxRepeats(static_cast<long>(control[i])); break;
        case LVC_MINREP:   bbcutter.SetMinRepeats(static_cast<long>(control[i])); break;
        case LVC_STUTTER:  bbcutter.SetStutterChance(control[i]); break;
        case LVC_AREA:     bbcutter.SetStutterArea(control[i]); break;
        case LVC_STRAIGHT: bbcutter.SetStraightChance(control[i]); break;
        case LVC_REGULAR:  bbcutter.SetRegularChance(control[i]); break;
        case LVC_RITARD:   bbcutter.SetRitardChance(control[i]); break;
        case LVC_SPEED:    bbcutter.SetAccel(control[i]); break;
        case LVC_ACTIVITY: bbcutter.SetActivity(control[i]); break;
        case LVC_CRUSHER:  crusher.SetOn(control[i] > 0.5f); break;
        case LVC_MINBITS:  crusher.SetMinBits(control[i]); break;
        case LVC_MAXBITS:  crusher.SetMaxBits(control[i]); break;
        case LVC_MINFREQ:  crusher.SetMinFreq(control[i]); break;
        case LVC_MAXFREQ:  crusher.SetMaxFreq(control[i]); break;
        case LVC_COMB: case LVC_DLYMODE:
            comb.SetOn(control[LVC_COMB] > 0.5f && control[LVC_DLYMODE] <= 0.5f); // the echo has its own path
            break;
        case LVC_TYPE:     comb.SetType(static_cast<long>(control[i])); break;
        case LVC_MINDELAY: comb.SetMinDelay(control[i]); break;
        case LVC_MAXDELAY: comb.SetMaxDelay(control[i]); break;
        // Feedback and amount are min/max ranges: each slice gets a random value inside them.
        case LVC_FEEDBACK: case LVC_MINFEEDBACK:
            delaySlice.SetFeedbackRange(control[LVC_MINFEEDBACK], control[LVC_FEEDBACK]);
            break;
        case LVC_DLYMIX: case LVC_MINDLYMIX:
            delaySlice.SetMixRange(control[LVC_MINDLYMIX], control[LVC_DLYMIX]);
            break;
        case LVC_DLYROUTE:
        {
            const bool insert = control[i] > 0.5f;
            if (insert != delayInsert)
            {
                echo.Restart(); // a routing switch starts from silence, not an old tail
                dlyWetSmooth = 0.f;
                dlyDrySmooth = 1.f;
                dlySendSmooth = 0.f;
                dlyOnSmooth = 0.f;
                delayInsert = insert;
            }
            break;
        }
        case LVC_SEED:     Math::randomseed(static_cast<long>(control[i])); break;
        case LVC_LIMITER:  limiter.setEnabled(control[i] > 0.5f); break;
        case LVC_LIMCEIL:  limiter.setCeilingDb(control[i]); break;
        case LVC_LIMREL:   limiter.setRelease(control[i]); break;
        case LVC_MINCUT:
            bbcutter.SetMinCutQuarters(LVC_MINCUT_QUARTERS[static_cast<int>(control[i] + 0.5f)]);
            break;
        case LVC_EUDENSITY:   bbcutter.SetEuclidDensity(control[i]); break;
        case LVC_EUCHANCE:    bbcutter.SetEuclidChance(control[i]); break;
        case LVC_RAINTENSITY: bbcutter.SetRatchetIntensity(control[i]); break;
        case LVC_RACURVE:     bbcutter.SetRatchetCurve(control[i]); break;
        case LVC_LPFREQ: case LVC_LPMIN:
            sliceFilter.SetLowpassRange(control[LVC_LPMIN], control[LVC_LPFREQ]);
            break;
        case LVC_HPFREQ: case LVC_HPMAX:
            sliceFilter.SetHighpassRange(control[LVC_HPFREQ], control[LVC_HPMAX]);
            break;
        case LVC_REVERB:
        {
            const bool on = control[i] > 0.5f;
            if (on && ! reverbSendsOn)
                reverb.reset(); // start from silence, not from an old tail
            reverbSendsOn = on;
            reverbSends.SetOn(on);
            break;
        }
        case LVC_RVMODE:
        {
            const bool insert = control[i] > 0.5f;
            if (insert != reverbInsert)
                reverb.reset(); // a mode switch starts a fresh tail
            reverbInsert = insert;
            break;
        }
        case LVC_RVCHANCE:   reverbSends.SetChance(control[i]); break;
        case LVC_RVSENDMIN: case LVC_RVSENDMAX:
            reverbSends.SetSendRange(control[LVC_RVSENDMIN], control[LVC_RVSENDMAX]);
            break;
        case LVC_RVDECAYMIN: case LVC_RVDECAYMAX:
            reverbSends.SetDecayRange(control[LVC_RVDECAYMIN], control[LVC_RVDECAYMAX]);
            break;
        case LVC_RVSIZE:     reverb.setSize(control[i]); break;
        case LVC_RVDAMP:     reverb.setDamp(control[i]); break;
        case LVC_EUSLICE:     bbcutter.SetEuclidSlice(static_cast<long>(control[i] + 0.5f)); break;
        default:
            if (i >= LVC_GATEENV1 && i < LVC_GATEENV1 + LVC_GATE_ENV_PARAMS)
                bbcutter.SetGateEnvelope(static_cast<long>(i - LVC_GATEENV1), gateEnvPacked(control[i]));
            else if (i >= LVC_GATETENSION1 && i < LVC_GATETENSION1 + LVC_GATE_TENSION_PARAMS)
                bbcutter.SetGateTensions(static_cast<long>(i - LVC_GATETENSION1), gateEnvPacked(control[i]));
            else if (i == LVC_GATETIE1 || i == LVC_GATETIE2)
                bbcutter.SetGateTies(gatePatternFromParams(control[LVC_GATETIE1], control[LVC_GATETIE2]));
            break; // filter, mix and Chroma are read directly in run()
        }
    }

   /* --------------------------------------------------------------------------------------------------------
    * Internal data */

    float getParameterValue(uint32_t index) const override
    {
        if (index >= LVC_CONTROL_NR) return 0.0f;
        return control[index];
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= LVC_CONTROL_NR) return;
        control[index] = value;
        update_parameter(index);
    }

   /* --------------------------------------------------------------------------------------------------------
    * Process */

    void activate() override
    {
        crusher.SetSampleRate(sampleRate);
        comb.SetSampleRate(sampleRate);
        lowpass.setSampleRate(sampleRate);
        highpass.setSampleRate(sampleRate);
        lowpass.reset();
        highpass.reset();
        crushLowpass.setSampleRate(sampleRate);
        crushLowpass.reset();
        masterLowpass.setSampleRate(sampleRate);
        echo.SetSampleRate(sampleRate); // allocates the delay buffers
        dlyWetSmooth = 0.f;
        dlyDrySmooth = 1.f;
        dlyOnSmooth = 0.f;
        dlySendSmooth = 0.f;
        echoIdle = true;
        delayInsert = control[LVC_DLYROUTE] > 0.5f;
        delayFbSmooth = control[LVC_FEEDBACK];
        delayMixSmooth = control[LVC_DLYMIX];
        gateLowpass.setSampleRate(sampleRate);
        gateHighpass.setSampleRate(sampleRate);
        gateLowpass.reset();
        gateHighpass.reset();
        gateEnvCoef = 1.0f - std::exp(-1.0f / (0.0005f * static_cast<float>(sampleRate))); // ~0.5 ms: follows the gate edges
        masterHighpass.setSampleRate(sampleRate);
        masterLowpass.reset();
        masterHighpass.reset();
        limiterLatency = limiter.setup(sampleRate);
        reverb.setSampleRate(sampleRate);
        reverbSendsOn = false;
        reverbInsert = control[LVC_RVMODE] > 0.5f;
        reverb.reset();

        for (uint32_t i = 0; i < LVC_CONTROL_NR; ++i)
            update_parameter(i);

        mixSmooth = control[LVC_MIX];
        slicerSmooth = control[LVC_SLICER] > 0.5f ? 1.f : 0.f;
        makeupSmooth = std::pow(10.f, control[LVC_MAKEUP] / 20.f);
        transient.setSampleRate(sampleRate);
        transOnSmooth = control[LVC_TRANSIENT] > 0.5f ? 1.f : 0.f;
        lpSmooth = lpTarget = std::log(sliceFilter.Lowpass());
        hpSmooth = hpTarget = std::log(sliceFilter.Highpass());
        filterMix = control[LVC_FILTER] > 0.5f ? 1.f : 0.f;
        filterDriveSmooth = 1.f + 8.f * control[LVC_FILTERDRIVE];
        filterDriveNorm = filterDriveSmooth > 1.001f ? 1.f / std::tanh(filterDriveSmooth) : 1.f;
        crushMixSmooth = control[LVC_CRUSHMIX];
        crushOnSmooth = control[LVC_CRUSHER] > 0.5f ? 1.f : 0.f;
        crushLpSmooth = std::log(control[LVC_CRUSHLP]);
        crushLpAmount = 0.f;
        mLpSmooth = std::log(control[LVC_MASTERLPFREQ]);
        mHpSmooth = std::log(control[LVC_MASTERHPFREQ]);
        reverb.setSize(control[LVC_RVSIZE], true);
        revOnSmooth = control[LVC_REVERB] > 0.5f ? 1.f : 0.f;
        sendSmooth = 0.f;
        smoothCoef = 1.0f - std::exp(-1.0f / (0.02f * static_cast<float>(sampleRate))); // ~20 ms
        cutoffCoef = 1.0f - std::exp(-1.0f / (0.002f * static_cast<float>(sampleRate))); // ~2 ms: follows each cut
        gateCoef = 1.0f - std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate)));   // ~6 ms
        freeRunPpq = 0.0;

        // Chroma
        chroma.setSampleRate(sampleRate);
        applyChromaSettings();
        chromaOnSmooth = control[LVC_CHROMA] > 0.5f ? 1.f : 0.f;
        chromaRunning = chromaOnSmooth > 0.f;
        chromaLatency = chromaRunning ? chroma.latency() : 0;
        std::memset(dryDelay, 0, sizeof(dryDelay));
        dryDelayPos = 0;
        std::memset(midiHeld, 0, sizeof(midiHeld));
        vis.sampleRate.store(static_cast<float>(sampleRate), std::memory_order_relaxed);
        setLatency(limiterLatency + static_cast<uint32_t>(chromaLatency));
    }

    // ---- Chroma helpers (audio thread) ----

    // all Chroma settings are pushed from run(), so a host changing parameters from another thread never
    // reconfigures the engine in the middle of a block
    void applyChromaSettings()
    {
        const int key = limit<int>(static_cast<int>(control[LVC_CHKEY] + 0.5f), 0, 11);
        const int scale = limit<int>(static_cast<int>(control[LVC_CHSCALE] + 0.5f), 0, LVC_NUM_SCALES - 1);
        const bool midi = DISTRHO_PLUGIN_WANT_MIDI_INPUT && control[LVC_CHMIDI] > 0.5f;
        if (midi && ! midiModeWas)
            midiLatched = 0; // start from the key / scale until notes arrive
        midiModeWas = midi;
        // MIDI mode: the pitch classes of the held notes; after release the last chord stays (latched)
        const uint16_t custom = static_cast<uint16_t>(control[LVC_CHCUSTOM]) & 0x0FFF;
        const uint16_t mask = (midi && midiLatched != 0) ? midiLatched : lvcScalePitchClasses(scale, key, custom);
        chroma.setPitchClasses(mask);
        chroma.setColor(control[LVC_CHCOLOR]);
        chroma.setPitch(std::round(control[LVC_CHPITCH]));
        chroma.setTune(control[LVC_CHTUNE]);
        chroma.setGainDb(control[LVC_CHGAIN]);
        chroma.setRange(control[LVC_CHLOW], control[LVC_CHHIGH]);
        chroma.setSpectral(static_cast<int>(control[LVC_CHFFT] + 0.5f), control[LVC_CHMORPH] > 0.5f,
                           control[LVC_CHMORPHAMT], control[LVC_CHGATE] > 0.5f, control[LVC_CHGATETHR]);
        vis.chromaMask.store(mask, std::memory_order_relaxed);
        // the same key / scale is the delay's note grid (also while the pitchmap itself is off)
        scaleMask = mask;
    }

   #if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void handleMidi(const MidiEvent* events, uint32_t count)
    {
        for (uint32_t e = 0; e < count; ++e)
        {
            const MidiEvent& ev = events[e];
            if (ev.size < 3 || ev.size > MidiEvent::kDataSize)
                continue;
            const uint8_t status = ev.data[0] & 0xF0;
            const uint8_t note = ev.data[1] & 0x7F;
            if (status == 0x90 && ev.data[2] > 0)
            {
                if (midiHeld[note] < 255) ++midiHeld[note];
            }
            else if (status == 0x80 || status == 0x90)
            {
                if (midiHeld[note] > 0) --midiHeld[note];
            }
            else if (status == 0xB0 && (ev.data[1] == 120 || ev.data[1] == 123))
            {
                std::memset(midiHeld, 0, sizeof(midiHeld)); // all sound / notes off (the latch stays)
            }
        }
        uint16_t held = 0;
        for (int n = 0; n < 128; ++n)
            if (midiHeld[n] > 0) held = static_cast<uint16_t>(held | (1u << (n % 12)));
        if (held != 0)
            midiLatched = held;
        vis.chromaHeld.store(held, std::memory_order_relaxed);
    }
   #endif

   #if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
   #else
    void run(const float** inputs, float** outputs, uint32_t frames) override
   #endif
    {
        const float* inL  = inputs[0];
        const float* inR  = inputs[1];
        float*       outL = outputs[0];
        float*       outR = outputs[1];

        const TimePosition& timePos(getTimePosition());

        if (pendingDiv != div)
            applySubdiv(pendingDiv);

        // ---- Chroma: settings, on/off, latency ----
       #if DISTRHO_PLUGIN_WANT_MIDI_INPUT
        handleMidi(midiEvents, midiEventCount);
       #endif
        applyChromaSettings();
        const bool chromaWanted = control[LVC_CHROMA] > 0.5f;
        if (chromaWanted && ! chromaRunning)
        {
            chroma.reset(); // start from silence, not from an old frame
            chromaRunning = true;
        }
        {
            // latency follows Chroma's frames; with a frame in the path there is nothing to crossfade with,
            // so it switches at once (the host is told about the new latency)
            const int lat = chromaWanted ? chroma.latency() : 0;
            if (lat != chromaLatency)
            {
                chromaLatency = lat;
                std::memset(dryDelay, 0, sizeof(dryDelay));
                dryDelayPos = 0;
                setLatency(limiterLatency + static_cast<uint32_t>(lat));
                chromaOnSmooth = chromaWanted ? 1.f : 0.f; // a delayed path cannot be crossfaded with an undelayed one
            }
        }
        const float chromaTarget = chromaWanted ? 1.f : 0.f;
        // routing: effects in front of the slicer process what it records, the rest process its output
        int route[kFlowItems];
        flowFromParams(control, route);
        int preFx[kFlowItems], postFx[kFlowItems], numPre = 0, numPost = 0;
        {
            bool afterSlicer = false;
            for (int k = 0; k < kFlowItems; ++k)
            {
                if (route[k] == kRouteSlice) afterSlicer = true;
                else if (afterSlicer) postFx[numPost++] = route[k];
                else preFx[numPre++] = route[k];
            }
        }
        bool chromaPre = false;
        for (int k = 0; k < numPre; ++k)
            chromaPre = chromaPre || preFx[k] == kFxChroma;


        double bpm, beats_per_bar, beat_unit, ppqPos;

        if (timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0 && timePos.bbt.beatsPerBar > 0.0f)
        {
            bpm = timePos.bbt.beatsPerMinute;
            beats_per_bar = timePos.bbt.beatsPerBar;
            beat_unit = timePos.bbt.beatType > 0.0f ? timePos.bbt.beatType : 4.0;
            // In DPF the first bar / beat == 1, our calculations require 0
            const double bar  = timePos.bbt.bar - 1;
            const double beat = timePos.bbt.beat - 1;
            const double beatFraction = timePos.bbt.ticksPerBeat > 0.0 ? timePos.bbt.tick / timePos.bbt.ticksPerBeat : 0.0;
            ppqPos = (bar * beats_per_bar) + beat + beatFraction;
            freeRunPpq = ppqPos;
        }
        else
        {
            // no host tempo information (e.g. standalone without transport): free-run at 120 bpm 4/4
            bpm = 120.0;
            beats_per_bar = 4.0;
            beat_unit = 4.0;
            ppqPos = freeRunPpq;
            freeRunPpq += (static_cast<double>(frames) / sampleRate) * (bpm / 60.0);
        }

        // Chroma in front of the slicer delays the audio it records by a frame: run the slicer's grid as late,
        // so cuts still fall on the beat of what it hears (the reported latency puts it back in time)
        if (chromaPre && chromaLatency > 0)
            ppqPos -= (static_cast<double>(chromaLatency) / sampleRate) * (bpm / 60.0);

        const double ppqduration = (static_cast<double>(frames) / sampleRate) * (bpm / 60.0);
        const double divPerSample = frames > 0
            ? (static_cast<double>(div) / beats_per_bar) * ppqduration * (beat_unit / 4.0) / static_cast<double>(frames)
            : 0.0;
        double position = (static_cast<double>(div) / beats_per_bar) * ppqPos * (beat_unit / 4.0);
        const double ref = 0.0;

        bbcutter.SetTimeInfos(bpm, beats_per_bar, beat_unit, sampleRate);

        long oldmeasure = static_cast<long>(std::floor((position - ref) / div));
        auto unitInMeasure = [&](double pos) {
            const double d = static_cast<double>(div);
            return static_cast<long>(std::floor(pos - d * std::floor(pos / d))); // 0 .. div-1, also for pos < 0
        };
        long positionInMeasure = unitInMeasure(position - ref);

        // ---- fill mode: slice only during the last N bars of every cycle ----
        const bool fillOn = control[LVC_FILLMODE] > 0.5f;
        const long fillCycle = LVC_FILLCYCLE_OPTIONS[limit<int>(static_cast<int>(control[LVC_FILLCYCLE] + 0.5f), 0, 2)];
        const long fillBars = std::min(fillCycle, LVC_FILLBARS_OPTIONS[limit<int>(static_cast<int>(control[LVC_FILLBARS] + 0.5f), 0, 3)]);
        auto barInCycle = [&](long m) { return ((m % fillCycle) + fillCycle) % fillCycle; };
        auto inFill = [&](long m) { return barInCycle(m) >= fillCycle - fillBars; };

        if (fillOn != fillWasOn || fillCycle != lastFillCycle || fillBars != lastFillBars)
        {
            // mode switched or reconfigured mid-bar: adopt the current state without resetting the phrase
            fillWasOn = fillOn;
            lastFillCycle = fillCycle;
            lastFillBars = fillBars;
            fillZoneActive = fillOn ? inFill(oldmeasure) : true;
            bbcutter.SetPhraseLimit(fillOn && fillZoneActive ? fillCycle - barInCycle(oldmeasure) : 0);
        }

        // advances the cutter, starting a fresh (length-capped) phrase when a fill begins
        auto advance = [&](long measure, long pim) {
            if (fillOn && measure != lastMeasureSeen)
            {
                const bool zone = inFill(measure);
                if (zone && ! fillZoneActive && pim == 0)
                    bbcutter.ResetPhrase();
                fillZoneActive = zone;
                bbcutter.SetPhraseLimit(zone ? fillCycle - barInCycle(measure) : 0);
            }
            lastMeasureSeen = measure;
            bbcutter.SetPosition(measure, pim);
        };

        if (positionInMeasure != oldpositionInMeasure)
        {
            oldpositionInMeasure = positionInMeasure;
            advance(oldmeasure, oldpositionInMeasure);
        }

        gateApplyPattern();

        // ---- visualisation setup for this block ----
        const double samplesPerBar = sampleRate * 60.0 / bpm * beats_per_bar * 4.0 / beat_unit;
        const uint32_t samplesPerColumn = std::max<uint32_t>(1u,
            static_cast<uint32_t>(samplesPerBar * LiveCutVis::kScopeBars / LiveCutVis::kScopeColumns + 0.5));
        vis.columnsPerBar.store(static_cast<float>(samplesPerBar / samplesPerColumn), std::memory_order_relaxed);
        vis.bpm.store(static_cast<float>(bpm), std::memory_order_relaxed);
        vis.beatsPerBar.store(static_cast<float>(beats_per_bar), std::memory_order_relaxed);
        vis.beatUnit.store(static_cast<float>(beat_unit), std::memory_order_relaxed);
        vis.playing.store(timePos.playing, std::memory_order_relaxed);
        const double unitsPerBeat = static_cast<double>(div) / beats_per_bar;

        // ---- echo (delay module, ECHO mode): times from the tempo or in ms, mix crossfaded ----
        const bool echoMode = control[LVC_DLYMODE] > 0.5f;
        const bool echoOn = echoMode && control[LVC_COMB] > 0.5f;
        {
            const double quartersPerBar = beats_per_bar * 4.0 / beat_unit;
            double tl, tr;
            if (control[LVC_DLYSYNC] > 0.5f)
            {
                tl = delayDivSeconds(delayDiv(control[LVC_DLYTIMEL]), bpm, quartersPerBar);
                tr = delayDivSeconds(delayDiv(control[LVC_DLYTIMER]), bpm, quartersPerBar);
            }
            else
            {
                tl = control[LVC_DLYMSL] * 0.001;
                tr = control[LVC_DLYMSR] * 0.001;
            }
            // TUNE: the delay times snap to note periods of the pitchmap's key / scale. The echo only tunes its
            // free times (synced times stay on the beat); the comb tunes each slice's delay.
            const bool tuned = control[LVC_DLYTUNE] > 0.5f;
            const double refA4 = lvcTuneRefA4(control[LVC_CHTUNE]);
            comb.SetTuning(tuned ? scaleMask : 0, refA4);
            if (tuned && control[LVC_DLYSYNC] <= 0.5f)
            {
                echo.SetTone(control[LVC_DLYTONE]); // the loop latency depends on the tone
                const double pl = lvcTunedPeriod(tl, scaleMask, refA4), pr = lvcTunedPeriod(tr, scaleMask, refA4);
                echo.SetTimes(pl * sampleRate - echo.LoopLatency(1.0 / pl), pr * sampleRate - echo.LoopLatency(1.0 / pr));
            }
            else
                echo.SetTimes(tl * sampleRate, tr * sampleRate);
            echo.SetMode(limit<int>(static_cast<int>(control[LVC_DLYSTEREO] + 0.5f), 0, 2));
            echo.SetTone(control[LVC_DLYTONE]);
            if (echoOn && echoIdle)
            {
                echo.Restart(); // starts from silence, never from what was left in the buffer
                echoIdle = false;
            }
        }
        // ---- parameter targets ----
        const bool gateOn = control[LVC_GATE] > 0.5f;
        gateLpTarget = gateOn ? control[LVC_GATELP] : 0.f;
        gateHpTarget = gateOn ? control[LVC_GATEHP] : 0.f;
        if (gateOn && ! gateWasOn)
        {
            gateLowpass.reset();
            gateHighpass.reset();
            gateLpSmooth = gateLpTarget;
            gateHpSmooth = gateHpTarget;
        }
        gateWasOn = gateOn;
        const float mixTarget = control[LVC_MIX];
        const float slicerTarget = control[LVC_SLICER] > 0.5f ? 1.f : 0.f;
        const float makeupTarget = std::pow(10.f, control[LVC_MAKEUP] / 20.f);
        const float transTarget = control[LVC_TRANSIENT] > 0.5f ? 1.f : 0.f;
        transient.setAttack(control[LVC_TRATTACK]);
        transient.setPump(control[LVC_TRPUMP]);
        transient.setSustain(control[LVC_TRSUSTAIN]);
        if (transTarget > 0.f && transOnSmooth <= 1e-4f)
            transient.reset(); // start from silence, not from an old envelope
        transGainPeak = 0.f;

        const float mLpTarget = std::log(control[LVC_MASTERLPFREQ]);
        const float mHpTarget = std::log(control[LVC_MASTERHPFREQ]);
        // the last part of each knob's travel fades the filter out; at the extreme it is fully bypassed
        const float mLpOpenLog = std::log(20000.0f), mLpFadeLog = std::log(17000.0f);
        const float mHpOpenLog = std::log(20.0f),    mHpFadeLog = std::log(24.0f);

        const float reso      = control[LVC_RESO];
        const float driveTarget = 1.f + 8.f * control[LVC_FILTERDRIVE];
        const float filterTarget = control[LVC_FILTER] > 0.5f ? 1.f : 0.f;
        const float crushMixTarget = control[LVC_CRUSHMIX];
        const float crushOnTarget = control[LVC_CRUSHER] > 0.5f ? 1.f : 0.f;
        const float crushLpTarget = std::log(control[LVC_CRUSHLP]);
        const float crushLpOpenLog = std::log(20000.0f), crushLpFadeLog = std::log(17000.0f);
        const float revTarget = control[LVC_REVERB] > 0.5f ? 1.f : 0.f;
        const float lpOff = std::log(19000.0f), lpMaxLog = std::log(20000.0f);
        const float hpOff = std::log(25.0f),    hpMinLog = std::log(20.0f);


        float peakIn = 0.f, peakOut = 0.f;
        float minLimGain = 1.f;

        for (uint32_t i = 0; i < frames; ++i)
        {
            positionInMeasure = unitInMeasure(position - ref);
            const long measure = static_cast<long>(std::floor((position - ref) / div));

            if (positionInMeasure != oldpositionInMeasure)
            {
                advance(measure, positionInMeasure);

                if (measure != oldmeasure)
                    scopeFlags |= LiveCutVis::kFlagBar;
                const long beatIdx = static_cast<long>(std::floor(position / unitsPerBeat));
                if (beatIdx != lastBeatIdx)
                {
                    lastBeatIdx = beatIdx;
                    scopeFlags |= LiveCutVis::kFlagBeat;
                }
            }

            // a new block was chosen: reset the capture buffer display
            if (blockCounter != lastBlockCounter)
            {
                lastBlockCounter = blockCounter;
                captureSize = player.GetBufferSize();
                std::memset(vis.bufferPeakL, 0, sizeof(vis.bufferPeakL));
                std::memset(vis.bufferPeakR, 0, sizeof(vis.bufferPeakR));
                vis.bufferId.fetch_add(1, std::memory_order_relaxed);
                scopeFlags |= LiveCutVis::kFlagBlockStart | LiveCutVis::kFlagCutStart;
                lastCutIdx = 0;
                visMarkReverb(0);
                visCountChop(0);
            }

            const long recIndex = player.GetInputIndex();

            const float dryL = inL[i];
            const float dryR = inR[i];

            chromaOnSmooth += smoothCoef * (chromaTarget - chromaOnSmooth);

            // dry for the final mix: always the untouched input, delayed by the pitchmap's frames like the wet path
            float mixDryL = dryL, mixDryR = dryR;
            if (chromaLatency > 0)
            {
                const int rd = (dryDelayPos - chromaLatency + kDryDelaySize) & (kDryDelaySize - 1);
                dryDelay[0][dryDelayPos] = dryL;
                dryDelay[1][dryDelayPos] = dryR;
                mixDryL = dryDelay[0][rd];
                mixDryR = dryDelay[1][rd];
                dryDelayPos = (dryDelayPos + 1) & (kDryDelaySize - 1);
            }

            // ---- smoothed settings ----
            mixSmooth += smoothCoef * (mixTarget - mixSmooth);
            filterMix += smoothCoef * (filterTarget - filterMix);
            filterDriveSmooth += smoothCoef * (driveTarget - filterDriveSmooth);
            crushMixSmooth += smoothCoef * (crushMixTarget - crushMixSmooth);
            crushOnSmooth += smoothCoef * (crushOnTarget - crushOnSmooth);
            crushLpSmooth += cutoffCoef * (crushLpTarget - crushLpSmooth);
            mLpSmooth += smoothCoef * (mLpTarget - mLpSmooth);
            mHpSmooth += smoothCoef * (mHpTarget - mHpSmooth);
            // per-slice cutoffs (picked by SliceFilter at each block / cut), lightly smoothed against zipper noise
            const float lpHz = sliceFilter.Lowpass(), hpHz = sliceFilter.Highpass();
            if (lpHz != lastLpHz) { lastLpHz = lpHz; lpTarget = std::log(lpHz); }
            if (hpHz != lastHpHz) { lastHpHz = hpHz; hpTarget = std::log(hpHz); }
            lpSmooth  += cutoffCoef * (lpTarget - lpSmooth);
            hpSmooth  += cutoffCoef * (hpTarget - hpSmooth);

            if ((filterCounter++ & 15) == 0)
            {
                lowpass.set(std::exp(lpSmooth), reso);
                highpass.set(std::exp(hpSmooth), reso);
                // fade filters fully out when parked at their extremes, so they are transparent by default
                lpAmount = limit<float>((lpMaxLog - lpSmooth) / (lpMaxLog - lpOff), 0.f, 1.f);
                hpAmount = limit<float>((hpSmooth - hpMinLog) / (hpOff - hpMinLog), 0.f, 1.f);
                filterDriveNorm = filterDriveSmooth > 1.001f ? 1.f / std::tanh(filterDriveSmooth) : 1.f;
                const float nyq = 0.45f * static_cast<float>(sampleRate);
                crushLowpass.set(std::min(nyq, std::exp(crushLpSmooth)), 0.f);
                crushLpAmount = limit<float>((crushLpOpenLog - crushLpSmooth) / (crushLpOpenLog - crushLpFadeLog), 0.f, 1.f);
                masterLowpass.set(std::exp(mLpSmooth), 0.f);  // flat response (Butterworth)
                masterHighpass.set(std::exp(mHpSmooth), 0.f);
            }

            // ---- reverb state (shared by send and insert modes) ----
            revOnSmooth += smoothCoef * (revTarget - revOnSmooth);
            float revL = 0.f, revR = 0.f, revSendL = 0.f, revSendR = 0.f;
            if (revOnSmooth > 1e-4f)
            {
                sendSmooth += cutoffCoef * (reverbSends.Send() - sendSmooth);
                const float decay = reverbSends.Decay();
                if (decay != lastDecay) { lastDecay = decay; reverb.setDecay(decay); }
            }

            transOnSmooth += smoothCoef * (transTarget - transOnSmooth);
            // Feedback and amount follow the value chosen for the current slice, lightly smoothed.
            delayFbSmooth  += smoothCoef * (delaySlice.Feedback() - delayFbSmooth);
            delayMixSmooth += smoothCoef * (delaySlice.Mix() - delayMixSmooth);
            dlyOnSmooth += smoothCoef * ((control[LVC_COMB] > 0.5f ? 1.f : 0.f) - dlyOnSmooth);
            // SEND: AMOUNT is the level fed into a parallel tail. INSERT: AMOUNT is the dry/wet crossfade.
            dlySendSmooth += smoothCoef * (((control[LVC_COMB] > 0.5f && ! delayInsert) ? delayMixSmooth : 0.f) - dlySendSmooth);
            float dlySendL = 0.f, dlySendR = 0.f, dlyWetL = 0.f, dlyWetR = 0.f;
            // mix law: the dry stays full up to 50 %, the wet reaches full at 50 %
            const float dlyMixNow = limit<float>(delayMixSmooth, 0.f, 1.f);
            const float dlyInsertWet = std::min(1.f, 2.f * dlyMixNow);
            const float dlyInsertDry = std::min(1.f, 2.f * (1.f - dlyMixNow));
            const float dlyWetTarget = delayInsert ? dlyOnSmooth * dlyInsertWet : 0.f;
            const float dlyDryTarget = delayInsert ? 1.f - dlyOnSmooth * (1.f - dlyInsertDry) : 1.f;
            dlyWetSmooth += smoothCoef * (dlyWetTarget - dlyWetSmooth);
            dlyDrySmooth += smoothCoef * (dlyDryTarget - dlyDrySmooth);

            // fill gate: outside the fill the output is dry (smoothed to avoid clicks)
            gateSmooth += gateCoef * ((fillOn && ! fillZoneActive ? 0.f : 1.f) - gateSmooth);

            // ---- effects: the same code wherever an effect sits in the route ----
            auto applyFx = [&](int fx, float& l, float& r) {
                switch (fx)
                {
                case kFxChroma:
                {
                    if (! chromaRunning)
                        break;
                    float cl = l, cr = r;
                    chroma.process(cl, cr);
                    l += chromaOnSmooth * (cl - l);
                    r += chromaOnSmooth * (cr - r);
                    break;
                }
                case kFxTransient:
                {
                    if (transOnSmooth <= 1e-4f)
                        break;
                    float tl = l, tr = r;
                    transient.process(tl, tr);
                    l += transOnSmooth * (tl - l);
                    r += transOnSmooth * (tr - r);
                    const float gdb = transient.gain() * transOnSmooth;
                    if (std::fabs(gdb) > std::fabs(transGainPeak)) transGainPeak = gdb;
                    break;
                }
                case kFxCrusher:
                {
                    float cl = l, cr = r;
                    crusher.tick(cl, cr, l, r);
                    // post lowpass tames the digital edges of the bit/sample-rate reduction
                    float fl = cl, fr = cr;
                    crushLowpass.tick(fl, fr);
                    cl += crushLpAmount * (fl - cl);
                    cr += crushLpAmount * (fr - cr);
                    const float amt = crushMixSmooth * crushOnSmooth;
                    l += amt * (cl - l);
                    r += amt * (cr - r);
                    break;
                }
                case kFxComb:
                {
                    if (delayInsert)
                    {
                        // INSERT: the delay sits in the slice path, crossfaded by the AMOUNT slider.
                        if (echoMode)
                        {
                            if (echoOn || dlyWetSmooth > 1e-5f || dlyDrySmooth < 0.99999f)
                            {
                                echo.SetFeedback(delayFbSmooth);
                                echo.process(l, r, dlyDrySmooth, dlyWetSmooth);
                            }
                        }
                        else
                        {
                            const float inL = l, inR = r;
                            comb.SetFeedBack(delayFbSmooth);
                            comb.tick(l, r, l, r);
                            if (dlyWetSmooth > 1e-5f || dlyDrySmooth < 0.99999f)
                            {
                                l = inL * dlyDrySmooth + l * dlyWetSmooth;
                                r = inR * dlyDrySmooth + r * dlyWetSmooth;
                            }
                        }
                    }
                    else if (dlyOnSmooth > 1e-4f)
                    {
                        // SEND: tap the slice into a parallel delay and leave the dry path untouched.
                        // The tail is added after the global mix, so it can ring over the dry part.
                        const float sg = dlySendSmooth * gateSmooth;
                        dlySendL = l * sg;
                        dlySendR = r * sg;
                        if (echoMode)
                        {
                            echo.SetFeedback(delayFbSmooth);
                            echo.process(dlySendL, dlySendR, 0.f, 1.f);
                            dlyWetL = dlySendL;
                            dlyWetR = dlySendR;
                        }
                        else
                            comb.tick(dlyWetL, dlyWetR, dlySendL, dlySendR);
                    }
                    break;
                }
                case kFxFilter:
                {
                    // saturate the signal going into the filter: the normalisation keeps full-scale peaks
                    // roughly where they were while low-level material gets the harmonic density of the drive
                    float fl = l, fr = r;
                    if (filterDriveSmooth > 1.001f)
                    {
                        fl = std::tanh(l * filterDriveSmooth) * filterDriveNorm;
                        fr = std::tanh(r * filterDriveSmooth) * filterDriveNorm;
                    }
                    lowpass.tick(fl, fr);
                    l += lpAmount * filterMix * (fl - l);
                    r += lpAmount * filterMix * (fr - r);
                    fl = l; fr = r;
                    highpass.tick(fl, fr);
                    l += hpAmount * filterMix * (fl - l);
                    r += hpAmount * filterMix * (fr - r);
                    break;
                }
                case kFxVerb:
                {
                    if (revOnSmooth <= 1e-4f)
                        break;
                    if (reverbInsert)
                    {
                        reverb.process(l, r, revL, revR);
                        const float amt = sendSmooth * revOnSmooth;
                        l += amt * (revL - l);
                        r += amt * (revR - r);
                        if (amt > scopeVerbAcc) scopeVerbAcc = amt;
                    }
                    else
                    {
                        const float sg = sendSmooth * gateSmooth * revOnSmooth;
                        revSendL = l * sg;
                        revSendR = r * sg;
                        if (sg > scopeVerbAcc) scopeVerbAcc = sg;
                    }
                    break;
                }
                }
            };

            // in front of the slicer: these process what it records
            float srcL = dryL, srcR = dryR;
            for (int k = 0; k < numPre; ++k)
                applyFx(preFx[k], srcL, srcR);

            float l = 0.f;
            float r = 0.f;
            player.tick(l, r, srcL, srcR);

            // trance gate filter: the gate envelope pulls a lowpass down / a highpass up (open when the step is open)
            if (gateOn)
            {
                gateLpSmooth += smoothCoef * (gateLpTarget - gateLpSmooth);
                gateHpSmooth += smoothCoef * (gateHpTarget - gateHpSmooth);
                gateEnvSmooth += gateEnvCoef * (player.GetGateEnv() - gateEnvSmooth);
                if (gateLpSmooth > 1e-4f || gateHpSmooth > 1e-4f)
                {
                    if ((gateFilterCounter++ & 3) == 0)
                    {
                        const float nyq = 0.45f * static_cast<float>(sampleRate);
                        gateLowpass.set(std::min(nyq, gateLowpassHz(gateLpSmooth, gateEnvSmooth)), 0.0);
                        gateHighpass.set(std::min(nyq, gateHighpassHz(gateHpSmooth, gateEnvSmooth)), 0.0);
                    }
                    if (gateLpSmooth > 1e-4f) gateLowpass.tick(l, r);
                    if (gateHpSmooth > 1e-4f) gateHighpass.tick(l, r);
                }
            }

            // slicer off: the input runs straight into the effects (the cutter keeps its timing in the background,
            // so switching back on lands on the grid)
            slicerSmooth += smoothCoef * (slicerTarget - slicerSmooth);
            if (slicerSmooth < 0.9999f)
            {
                l = srcL + slicerSmooth * (l - srcL);
                r = srcR + slicerSmooth * (r - srcR);
            }

            // record the capture buffer waveform as it is being filled
            if (captureSize > 0 && recIndex < captureSize)
            {
                const int bin = static_cast<int>((static_cast<int64_t>(recIndex) * LiveCutVis::kBufferBins) / captureSize);
                const float al = std::fabs(srcL), ar = std::fabs(srcR);
                if (al > vis.bufferPeakL[bin]) vis.bufferPeakL[bin] = al;
                if (ar > vis.bufferPeakR[bin]) vis.bufferPeakR[bin] = ar;
            }

            const long cutIdx = player.GetCurrentCut();
            if (cutIdx != lastCutIdx)
            {
                lastCutIdx = cutIdx;
                scopeFlags |= LiveCutVis::kFlagCutStart;
                visMarkReverb(cutIdx);
                visCountChop(cutIdx);
                // insert mode: the reverb sits in the slice path, so a new slice cuts its tail
                if (reverbInsert)
                    reverb.reset();
            }

            // after the slicer
            for (int k = 0; k < numPost; ++k)
                applyFx(postFx[k], l, r);

            const float wet = mixSmooth * gateSmooth;
            float oL = mixDryL + wet * (l - mixDryL);
            float oR = mixDryR + wet * (r - mixDryR);

            // send-mode tails are added after the gate so they can ring over the dry part
            if (! reverbInsert && revOnSmooth > 1e-4f)
            {
                reverb.process(revSendL, revSendR, revL, revR);
                oL += mixSmooth * revOnSmooth * revL;
                oR += mixSmooth * revOnSmooth * revR;
            }
            if (! delayInsert && dlyOnSmooth > 1e-4f)
            {
                oL += mixSmooth * dlyOnSmooth * dlyWetL;
                oR += mixSmooth * dlyOnSmooth * dlyWetR;
            }

            // master tone shaping: the filters always run, so moving the knob never jumps their state.
            // The last stretch of the knob crossfades each filter out; at the open extreme it is bypassed.
            const float mLpAmount = limit<float>((mLpOpenLog - mLpSmooth) / (mLpOpenLog - mLpFadeLog), 0.f, 1.f);
            const float mHpAmount = limit<float>((mHpSmooth - mHpOpenLog) / (mHpFadeLog - mHpOpenLog), 0.f, 1.f);
            float fL = oL, fR = oR;
            masterLowpass.tick(fL, fR);
            oL += mLpAmount * (fL - oL);
            oR += mLpAmount * (fR - oR);
            fL = oL; fR = oR;
            masterHighpass.tick(fL, fR);
            oL += mHpAmount * (fL - oL);
            oR += mHpAmount * (fR - oR);

            limiter.tick(oL, oR);
            if (limiter.gain() < minLimGain) minLimGain = limiter.gain();

            // makeup gain after the limiter (a boost here can go past the ceiling)
            makeupSmooth += smoothCoef * (makeupTarget - makeupSmooth);
            oL *= makeupSmooth;
            oR *= makeupSmooth;

            outL[i] = oL;
            outR[i] = oR;

            // ---- scope accumulation ----
            const float mono = 0.5f * (oL + oR);
            const float inAbs = std::max(std::fabs(dryL), std::fabs(dryR));
            if (mono < scopeMinAcc) scopeMinAcc = mono;
            if (mono > scopeMaxAcc) scopeMaxAcc = mono;
            if (inAbs > scopeInAcc) scopeInAcc = inAbs;
            scopeEnvAcc += player.GetLastGain();
            peakIn = std::max(peakIn, inAbs);
            peakOut = std::max(peakOut, std::max(std::fabs(oL), std::fabs(oR)));

            if (++scopeCount >= samplesPerColumn)
            {
                visPublishCurrentValues();
                const uint32_t w = vis.scopeWrite.load(std::memory_order_relaxed);
                const uint32_t c = w % LiveCutVis::kScopeColumns;
                vis.scopeMin[c] = scopeMinAcc;
                vis.scopeMax[c] = scopeMaxAcc;
                vis.scopeIn[c]  = scopeInAcc;
                const bool hasCuts = player.GetNumCuts() > 0 && cutIdx < player.GetNumCuts();
                vis.scopeBlock[c] = hasCuts ? static_cast<int16_t>(visBlockIndex) : -1;
                uint8_t flags = scopeFlags;
                if (hasCuts && player.GetReadIndex() >= player.GetCuts()[cutIdx].length)
                    flags |= LiveCutVis::kFlagSilent;
                if (gateSmooth < 0.5f)
                    flags |= LiveCutVis::kFlagBypass;
                vis.scopeFlags[c] = flags;
                vis.scopeEnv[c]  = scopeEnvAcc / static_cast<float>(samplesPerColumn);
                vis.scopeBits[c] = crusher.GetBits();
                vis.scopeRate[c] = crusher.GetFreq();
                vis.scopeComb[c] = echoMode ? 0.f : comb.GetDelayMs(); // the lane shows the comb's per-slice delay
                vis.scopeLP[c]   = std::exp(lpSmooth) * (filterMix > 0.5f ? 1.f : -1.f); // negative = filter off
                vis.scopeHP[c]   = sliceFilter.Highpass(); // current per-slice highpass, for the filter panel's curve
                vis.scopeVerb[c] = scopeVerbAcc;
                scopeVerbAcc = 0.f;
                scopeEnvAcc = 0.f;
                vis.scopeWrite.store(w + 1, std::memory_order_release);

                scopeCount = 0;
                scopeFlags = 0;
                scopeMinAcc = 1e9f;
                scopeMaxAcc = -1e9f;
                scopeInAcc = 0.f;
            }

            oldpositionInMeasure = positionInMeasure;
            oldmeasure = measure;
            position += divPerSample;
        }

        // echo fully faded out: the next switch-on restarts it from silence
        if (! echoOn && dlyWetSmooth < 1e-5f && dlyDrySmooth > 0.99999f)
        {
            dlyWetSmooth = 0.f;
            dlyDrySmooth = 1.f;
            echoIdle = true;
        }

        publishLiveState(position);

        // Chroma: stop once faded out; publish its levels
        if (! chromaWanted && chromaOnSmooth < 1e-4f && chromaLatency == 0)
        {
            chromaRunning = false;
            chromaOnSmooth = 0.f;
        }
        if (chromaRunning)
        {
            std::memcpy(vis.chromaDry, chroma.dryLevel, sizeof(vis.chromaDry));
            std::memcpy(vis.chromaTuned, chroma.tunedLevel, sizeof(vis.chromaTuned));
        }
        else
        {
            std::memset(vis.chromaDry, 0, sizeof(vis.chromaDry));
            std::memset(vis.chromaTuned, 0, sizeof(vis.chromaTuned));
        }
        vis.chromaLatency.store(chromaLatency, std::memory_order_relaxed);
        vis.transientGain.store(transGainPeak, std::memory_order_relaxed);

        vis.gateMask.store(static_cast<uint32_t>(bbcutter.GetGateMask()), std::memory_order_relaxed);
        vis.gateEnv.store(gateEnvSmooth, std::memory_order_relaxed);

        vis.fillMode.store(fillOn, std::memory_order_relaxed);
        vis.fillActive.store(! fillOn || fillZoneActive, std::memory_order_relaxed);
        vis.barInCycle.store(static_cast<int32_t>(barInCycle(oldmeasure)), std::memory_order_relaxed);
        vis.cycleBars.store(static_cast<int32_t>(fillCycle), std::memory_order_relaxed);
        vis.fillBars.store(static_cast<int32_t>(fillBars), std::memory_order_relaxed);

        // simple peak meters with release
        visLevelIn  = std::max(peakIn,  visLevelIn  * 0.85f);
        visLevelOut = std::max(peakOut, visLevelOut * 0.85f);
        vis.levelIn.store(visLevelIn, std::memory_order_relaxed);
        vis.levelOut.store(visLevelOut, std::memory_order_relaxed);

        // limiter gain reduction meter: instant attack, slow visual release
        const float grDb = -20.f * std::log10(std::max(1e-6f, minLimGain));
        visGR = std::max(grDb, visGR * 0.9f);
        vis.limiterGR.store(visGR, std::memory_order_relaxed);
    }

    // new subdivision: move the cutter and the published phrase map to the new grid
    void applySubdiv(long newDiv)
    {
        const long oldDiv = div;
        div = newDiv;
        bbcutter.SetSubdiv(div);
        if (oldDiv <= 0)
            return;
        const float f = static_cast<float>(newDiv) / static_cast<float>(oldDiv);
        vis.beginPhraseWrite();
        LiveCutVis::Phrase& ph = vis.phrase;
        ph.totalUnits = static_cast<int32_t>(bbcutter.GetTotalUnits());
        ph.subdiv = static_cast<int32_t>(newDiv);
        for (int bi = 0; bi < ph.numBlocks; ++bi)
        {
            LiveCutVis::Block& b = ph.blocks[bi];
            const int end = static_cast<int>(std::lround((b.startUnit + b.units) * f));
            b.startUnit = static_cast<int16_t>(std::lround(b.startUnit * f));
            b.units = static_cast<int16_t>(std::max(0, end - b.startUnit));
        }
        for (int ci = 0; ci < ph.numCuts; ++ci)
        {
            ph.cuts[ci].units *= f;
            ph.cuts[ci].offset *= f;
        }
        vis.endPhraseWrite();
    }

    // Trance gate, pattern mode: edits of the drawn pattern (and of the duty) reach the steps still to come in the
    // bar that is playing, instead of waiting for the next bar. The step being played is left alone (no clicks).
    void gateApplyPattern()
    {
        if (control[LVC_GATE] <= 0.5f)
            return;
        const bool drawn = control[LVC_GATEMODE] > 0.5f;
        const uint32_t mask = gatePatternFromParams(control[LVC_GATEPAT1], control[LVC_GATEPAT2]);
        const uint32_t ties = gatePatternFromParams(control[LVC_GATETIE1], control[LVC_GATETIE2]);
        const int steps = static_cast<int>(std::min<long>(div, LVC_GATE_MAX_STEPS));
        const double duty = limit<float>(control[LVC_DUTY], 0.f, 1.f);
        auto isOn = [&](long st) { return st >= 0 && st < steps && ((mask >> st) & 1u) != 0; };
        auto tiedOut = [&](long st) { return st + 1 < steps && isOn(st) && isOn(st + 1) && ((ties >> st) & 1u) != 0; };
        const std::vector<CutInfo>& cuts = player.GetCuts();
        const long first = player.GetCurrentCut() + 1;
        bool writing = false;
        for (long j = first; j < static_cast<long>(cuts.size()); ++j)
        {
            const CutInfo& c = cuts[j];
            if (c.step < 0 || c.step >= LVC_GATE_MAX_STEPS)
                continue; // not a gate block
            // step envelopes follow edits in both modes
            const int st = static_cast<int>(c.step);
            const float att = GateEnvFraction(gateEnvLevel(control, st, false));
            const float dec = GateEnvFraction(gateEnvLevel(control, st, true));
            const float ten = gateTension(control, st);
            if (att != c.envattack || dec != c.envdecay || ten != c.envtension)
                player.SetCutEnvelope(j, att, dec, ten);
            // open / muted and ties only in pattern mode (random mode keeps its own steps)
            if (! drawn)
                continue;
            const bool on = isOn(c.step);
            const bool tout = tiedOut(c.step);
            // a slice only continues from the previous cut if that one really runs into it (it may be playing already)
            const bool tin = j > 0 ? cuts[j - 1].tiedout && on : tiedOut(c.step - 1);
            if (tin != c.tiedin || tout != c.tiedout)
                player.SetCutTies(j, tin, tout);
            const long want = on ? (tout ? c.size : static_cast<long>(static_cast<double>(c.size) * duty)) : 0;
            if (want == c.length)
                continue;
            player.SetCutLength(j, want);
            const int gi = visBlockFirstCut + static_cast<int>(j);
            if (visBlockIndex >= 0 && gi >= 0 && gi < vis.phrase.numCuts)
            {
                if (! writing)
                {
                    vis.beginPhraseWrite();
                    writing = true;
                }
                vis.phrase.cuts[gi].duty = c.size > 0 ? static_cast<float>(want) / static_cast<float>(c.size) : 1.f;
            }
        }
        if (writing)
            vis.endPhraseWrite();
    }

    void publishLiveState(double position)
    {
        const long numCuts = player.GetNumCuts();
        const long cut = player.GetCurrentCut();

        vis.currentBlock.store(visBlockIndex, std::memory_order_relaxed);

        const double frac = position - std::floor(position);
        const float unitPos = static_cast<float>(std::max(0.0, static_cast<double>(bbcutter.GetUnitsDone() - 1) + frac));
        vis.unitPos.store(unitPos, std::memory_order_relaxed);

        if (numCuts <= 0 || cut >= numCuts || captureSize <= 0)
        {
            vis.currentCut.store(-1, std::memory_order_relaxed);
            vis.cutRepeat.store(0, std::memory_order_relaxed);
            vis.cutsInBlock.store(0, std::memory_order_relaxed);
            vis.playPos.store(-1.f, std::memory_order_relaxed);
            vis.recPos.store(-1.f, std::memory_order_relaxed);
            return;
        }

        const CutInfo& ci = player.GetCuts()[cut];
        const double ratio = std::fabs(ci.cents) > 1e-10 ? std::pow(2.0, ci.cents / 1200.0) : 1.0;
        const double bufSize = static_cast<double>(captureSize);

        vis.currentCut.store(visBlockFirstCut + static_cast<int32_t>(cut), std::memory_order_relaxed);
        vis.cutRepeat.store(static_cast<int32_t>(cut), std::memory_order_relaxed);
        vis.cutsInBlock.store(static_cast<int32_t>(numCuts), std::memory_order_relaxed);
        const double off = static_cast<double>(ci.offset);
        vis.recPos.store(static_cast<float>(std::min(1.0, player.GetInputIndex() / bufSize)), std::memory_order_relaxed);
        vis.playPos.store(static_cast<float>(std::min(1.0, (off + player.GetReadIndex() * ratio) / bufSize)), std::memory_order_relaxed);
        vis.cutStart.store(static_cast<float>(std::min(1.0, off / bufSize)), std::memory_order_relaxed);
        vis.cutEnd.store(static_cast<float>(std::min(1.0, (off + ci.size * ratio) / bufSize)), std::memory_order_relaxed);
        vis.dutyEnd.store(static_cast<float>(std::min(1.0, (off + ci.length * ratio) / bufSize)), std::memory_order_relaxed);
        vis.cutAmp.store(ci.amp, std::memory_order_relaxed);
        vis.fadeFrac.store(ci.length > 0 ? static_cast<float>(player.GetFade()) / static_cast<float>(ci.length) : 0.f,
                           std::memory_order_relaxed);
    }

   /* --------------------------------------------------------------------------------------------------------
    * Callbacks (optional) */

    void sampleRateChanged(double newSampleRate) override
    {
        sampleRate = newSampleRate;
    }

    // -------------------------------------------------------------------------------------------------------

private:
    float sampleRate;

    // Parameters.
    float control[LVC_CONTROL_NR];

    // Livecut components
    LivePlayer player;
    BitCrusher crusher;
    SVFilter crushLowpass{SVFilter::kLowpass};
    float crushMixSmooth = 1.f, crushOnSmooth = 0.f;
    float crushLpSmooth = 0.f, crushLpAmount = 0.f;
    Comb comb;
    BBCutter bbcutter;
    VisListener visListener;

    // Variables used by the run() method which are stored or updated outside of the run() method
    long oldpositionInMeasure;
    long div;
    long pendingDiv = 8;
    double freeRunPpq = 0.0;

    // master section
    SVFilter lowpass{SVFilter::kLowpass}, highpass{SVFilter::kHighpass};
    Limiter limiter;
    float mixSmooth = 1.f, lpSmooth = 0.f, hpSmooth = 0.f;
    float slicerSmooth = 1.f;
    float makeupSmooth = 1.f;

    // transient shaper
    TransientShaper transient;
    float transOnSmooth = 0.f, transGainPeak = 0.f;
    float lpAmount = 0.f, hpAmount = 0.f;
    float filterMix = 1.f;
    float filterDriveSmooth = 1.f, filterDriveNorm = 1.f;
    float lpTarget = 0.f, hpTarget = 0.f, lastLpHz = -1.f, lastHpHz = -1.f;
    float cutoffCoef = 0.01f;
    SliceFilter sliceFilter;

    // delay module: per-slice feedback / amount, the routing switch, and the echo delay line
    SliceDelay delaySlice;
    float delayFbSmooth = 0.5f, delayMixSmooth = 0.35f;
    bool delayInsert = true; // 0 = SEND (parallel tail), 1 = INSERT (in the slice path)
    float dlyOnSmooth = 0.f, dlySendSmooth = 0.f;
    EchoDelay echo;
    float dlyWetSmooth = 0.f, dlyDrySmooth = 1.f; // insert crossfade (both comb and echo)
    bool echoIdle = true;

    // trance gate filter (right after the slicer, follows the gate envelope)
    SVFilter gateLowpass{SVFilter::kLowpass}, gateHighpass{SVFilter::kHighpass};
    float gateLpTarget = 0.f, gateHpTarget = 0.f, gateLpSmooth = 0.f, gateHpSmooth = 0.f;
    float gateEnvSmooth = 1.f, gateEnvCoef = 0.05f;
    uint32_t gateFilterCounter = 0;
    bool gateWasOn = false;

    // master tone filters (after dry/wet and reverb, before the limiter)
    SVFilter masterLowpass{SVFilter::kLowpass}, masterHighpass{SVFilter::kHighpass};
    float mLpSmooth = 0.f, mHpSmooth = 0.f;

    // per-slice reverb
    SliceReverbSends reverbSends;
    FDNReverb reverb;
    bool reverbSendsOn = false;
    bool reverbInsert = false;
    float revOnSmooth = 0.f, sendSmooth = 0.f, lastDecay = -1.f, scopeVerbAcc = 0.f;
    float smoothCoef = 0.001f;
    uint32_t filterCounter = 0;

    // Chroma (spectral resonator)
    SpectralResonator chroma;
    static constexpr int kDryDelaySize = 16384; // power of two, > largest Chroma latency (8192 + 4096 at 192 kHz)
    float dryDelay[2][kDryDelaySize] {};
    int dryDelayPos = 0;
    int chromaLatency = 0;
    uint32_t limiterLatency = 0;
    float chromaOnSmooth = 0.f;
    bool chromaRunning = false;
    uint8_t midiHeld[128] {};
    uint16_t midiLatched = 0;
    bool midiModeWas = false;

    // fill mode
    float gateSmooth = 1.f, gateCoef = 0.004f;
    bool fillWasOn = false, fillZoneActive = true;
    long lastFillCycle = 0, lastFillBars = 0;
    long lastMeasureSeen = -1000000;

    // visualisation bookkeeping (audio thread only)
    int32_t visBlockIndex = -1;
    int32_t visBlockFirstCut = 0;
    uint32_t blockCounter = 0, lastBlockCounter = 0;
    long captureSize = 0;
    long lastCutIdx = 0;
    uint16_t scaleMask = 0x0FFF; // pitch classes of the pitchmap's key / scale (the delay's TUNE grid)
    long lastBeatIdx = 0;
    uint32_t scopeCount = 0;
    uint8_t scopeFlags = 0;
    float scopeMinAcc = 1e9f, scopeMaxAcc = -1e9f, scopeInAcc = 0.f, scopeEnvAcc = 0.f;
    float visLevelIn = 0.f, visLevelOut = 0.f, visGR = 0.f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HyperCut)
};

/* ------------------------------------------------------------------------------------------------------------
 * Accessor used by the UI (direct access, same binary) */

LiveCutVis::State* getLiveCutVisState(void* pluginInstance)
{
    if (pluginInstance == nullptr)
        return nullptr;
    return &static_cast<HyperCut*>(static_cast<Plugin*>(pluginInstance))->vis;
}

/* ------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance. */

Plugin* createPlugin()
{
    return new HyperCut();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
