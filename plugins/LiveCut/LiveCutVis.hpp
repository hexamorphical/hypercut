/*
 This file is part of Livecut
 Copyright 2004 by Remy Muller.
 Adapted for DISTRHO Plugin Framework (DPF) by eventual-recluse.

 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

/*
   Shared visualisation state.

   The DSP side (audio thread) is the only writer, the UI side only reads.
   Nothing here is used to make audio decisions, so it is acceptable for the UI
   to occasionally see a slightly torn frame of waveform data. The phrase/cut
   structure, which is redrawn as a whole, is protected by a seqlock so the UI
   never sees half-written phrase data.
*/

#ifndef LIVECUT_VIS_HPP
#define LIVECUT_VIS_HPP

#include <atomic>
#include <cstdint>
#include <cstring>

namespace LiveCutVis {

static constexpr int kMaxBlocks    = 64;   // blocks per phrase
static constexpr int kMaxCuts      = 256;  // cuts per phrase
static constexpr int kBufferBins   = 4096; // capture-buffer waveform resolution (enough for zooming)
static constexpr int kScopeColumns = 8192; // scrolling output history: 4 bars (2 shown at 1x zoom)
static constexpr int kScopeBars    = 4;
static constexpr int kChromaNotes  = 128;  // Chroma levels, one per MIDI note
// Current per-slice value of each parameter, indexed by the parameter ID. Kept generously above
// LVC_CONTROL_NR so adding controls never reshapes the shared state (checked by a static_assert).
static constexpr int kMaxParams    = 160;

enum ScopeFlags : uint8_t {
    kFlagCutStart   = 0x01,
    kFlagBlockStart = 0x02,
    kFlagBeat       = 0x04,
    kFlagBar        = 0x08,
    kFlagSilent     = 0x10, // duty-cycle "off" part of a cut
    kFlagBypass     = 0x20, // fill mode: outside the fill, output is dry
};

struct Cut {
    float units;   // cut size in units (subdivisions)
    float duty;    // length / size  (0..1)
    float amp;
    float pan;
    float cents;
    float offset;  // slice start inside the block, in units (Euclid slices; 0 = block start)
    float verb;    // reverb send of this cut (0 = none)
};

struct Block {
    int16_t startUnit;
    int16_t units;
    int16_t firstCut;
    int16_t numCuts;
    uint8_t kind;     // BlockKind: what the algorithm decided for this block
    int16_t info[3];  // kind-specific details (cut length / repeats, euclid k,n,rotation, ratchet count...)
};

// Phrase structure: written whole at each block, guarded by seqlock
struct Phrase {
    int32_t totalUnits;
    int32_t subdiv;
    int32_t numBlocks;
    int32_t numCuts;
    uint32_t phraseId;
    Block blocks[kMaxBlocks];
    Cut cuts[kMaxCuts];
};

struct State {
    // ---- phrase structure (seqlock) ----
    std::atomic<uint32_t> phraseSeq { 0 };
    Phrase phrase {};

    // ---- live playhead info ----
    std::atomic<int32_t>  currentBlock { -1 };  // index into phrase.blocks
    std::atomic<int32_t>  currentCut   { -1 };  // index into phrase.cuts
    std::atomic<float>    unitPos      { 0.f }; // fractional unit position inside phrase
    std::atomic<float>    recPos       { 0.f }; // record head, 0..1 of capture buffer
    std::atomic<float>    playPos      { 0.f }; // play head, 0..1 of capture buffer
    std::atomic<float>    cutStart     { 0.f }; // current cut region in capture buffer, 0..1
    std::atomic<float>    cutEnd       { 0.f };
    std::atomic<float>    dutyEnd      { 0.f };
    std::atomic<int32_t>  cutRepeat    { 0 };   // index of current cut inside block
    std::atomic<int32_t>  cutsInBlock  { 0 };

    // ---- capture buffer waveform ----
    std::atomic<uint32_t> bufferId { 0 };       // increments at every block
    float bufferPeakL[kBufferBins] {};
    float bufferPeakR[kBufferBins] {};

    // ---- scrolling output scope ----
    std::atomic<uint32_t> scopeWrite { 0 };     // monotonically increasing column counter
    std::atomic<float>    columnsPerBar { kScopeColumns / 2.f };
    float   scopeMin[kScopeColumns] {};
    float   scopeMax[kScopeColumns] {};
    float   scopeIn[kScopeColumns] {};
    int16_t scopeBlock[kScopeColumns] {};       // block index (for colouring), -1 = none
    uint8_t scopeFlags[kScopeColumns] {};
    // envelopes, one value per scope column
    float   scopeEnv[kScopeColumns] {};         // cut envelope x amp (0..1), averaged over the column
    float   scopeBits[kScopeColumns] {};        // bitcrusher bit depth
    float   scopeRate[kScopeColumns] {};        // bitcrusher sample rate (Hz)
    float   scopeComb[kScopeColumns] {};        // comb delay (ms)
    float   scopeLP[kScopeColumns] {};          // per-slice lowpass cutoff (Hz), negative when the filter is off
    float   scopeHP[kScopeColumns] {};          // per-slice highpass cutoff (Hz), for the panel's response curve
    float   scopeVerb[kScopeColumns] {};        // reverb send level (0..1)

    // ---- currently selected value for every min/max range knob, indexed by its lower parameter ID ----
    float   paramCurrent[kMaxParams] {};
    uint8_t paramCurrentValid[kMaxParams] {};

    // ---- levels / transport ----
    std::atomic<float> levelIn  { 0.f };
    std::atomic<float> levelOut { 0.f };
    std::atomic<float> bpm      { 0.f };
    std::atomic<bool>  playing  { false };
    std::atomic<float> beatsPerBar { 4.f };
    std::atomic<float> beatUnit    { 4.f };
    std::atomic<float> limiterGR   { 0.f };  // dB of gain reduction (positive)

    // current cut envelope shape (for the capture buffer overlay)
    std::atomic<float> cutAmp      { 1.f };
    std::atomic<float> fadeFrac    { 0.f };  // fade length / cut length

    // fill mode
    std::atomic<bool>    fillMode   { false };
    std::atomic<bool>    fillActive { false };
    std::atomic<int32_t> barInCycle { 0 };
    std::atomic<int32_t> cycleBars  { 16 };
    std::atomic<int32_t> fillBars   { 4 };

    // Chroma (spectral resonator)
    float chromaDry[kChromaNotes] {};           // level of each source note in the active region
    float chromaTuned[kChromaNotes] {};         // level moved onto each destination note
    std::atomic<uint32_t> chromaMask    { 0 };  // pitch classes in use (bit 0 = C)
    std::atomic<uint32_t> chromaHeld    { 0 };  // MIDI notes held right now (pitch classes)
    std::atomic<int32_t>  chromaLatency { 0 };  // samples
    std::atomic<float>    sampleRate    { 44100.f };

    // trance gate: steps of the bar chosen last (bit 0 = step 1); lets the UI start a drawing from a random pattern
    std::atomic<uint32_t> gateMask { 0 };
    std::atomic<float>    gateEnv  { 1.f };   // gate envelope now (0 = shut .. 1 = open), drives the depth knobs' rings

    // chops: counts every audible slice that starts (the header logo sparkles on each one)
    std::atomic<uint32_t> chopCount   { 0 };
    std::atomic<float>    chopSeconds { 0.f };  // length of the latest chop's slot (sets how fast the sparks fly)
    std::atomic<float>    chopAmp     { 1.f };

    // transient shaper: largest gain change in the last block (dB, signed)
    std::atomic<float>    transientGain { 0.f };

    // writer helpers ---------------------------------------------------------
    void beginPhraseWrite() noexcept
    {
        phraseSeq.fetch_add(1, std::memory_order_acq_rel);
        std::atomic_thread_fence(std::memory_order_release);
    }

    void endPhraseWrite() noexcept
    {
        std::atomic_thread_fence(std::memory_order_release);
        phraseSeq.fetch_add(1, std::memory_order_acq_rel);
    }

    // reader helper: returns true if a consistent copy was obtained
    bool readPhrase(Phrase& out) const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const uint32_t s1 = phraseSeq.load(std::memory_order_acquire);
            if (s1 & 1u)
                continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            std::memcpy(&out, &phrase, sizeof(Phrase));
            std::atomic_thread_fence(std::memory_order_acquire);
            const uint32_t s2 = phraseSeq.load(std::memory_order_acquire);
            if (s1 == s2)
                return true;
        }
        return false;
    }
};

} // namespace LiveCutVis

#endif
