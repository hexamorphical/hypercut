/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

#ifndef LIVECUT_BLOCKKINDS_H
#define LIVECUT_BLOCKKINDS_H

// What a cut procedure decided for its last block (read by the UI to show what the algorithm is doing)
enum BlockKind
{
  kBlockNormal = 0,  // CutProc11: repeated cut
  kBlockStutter,     // CutProc11: end-of-phrase stutter
  kBlockStraight,    // WarpCut / Euclid: block played straight
  kBlockRoll,        // WarpCut: regular roll
  kBlockAccel,       // WarpCut: accelerating roll
  kBlockRitard,      // WarpCut: decelerating roll
  kBlockQuavers,     // SQPusher
  kBlockSemiquavers, // SQPusher
  kBlockFill,        // SQPusher: pre-baked fill
  kBlockEuclid,      // Euclid pattern
  kBlockRatchet,     // Ratchet retriggers
  kBlockGate,        // Trance gate: one bar of open / muted steps
  kNumBlockKinds
};

#endif
