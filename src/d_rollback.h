//
// Copyright(C) 2026 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Rollback netcode for multiplayer games.
//

#ifndef D_ROLLBACK_H
#define D_ROLLBACK_H

#include "d_ticcmd.h"
#include "doomtype.h"

// Rollback mode is active (see the -rollback parameter).

extern boolean rollback_enabled;

// True while already-run tics are being resimulated during a rollback.
// Code with audible or visible side effects (sounds, messages, ...)
// must check this flag.

extern boolean rollback_resim;

void RB_Init(int consoleplayer);
void RB_Shutdown(void);

// Record a confirmed set of ticcmds received from the server.

void RB_ReceiveTic(int tic, ticcmd_t *ticcmds, boolean *players_mask);

// True if the state of the given tic can no longer change: it has been
// confirmed by the server and no rollback can still reach it. Always
// true when rollback is not in use.

boolean RB_TicConfirmed(int tic);

// Whether inputs may be predicted in the current game state.

boolean RB_CanPredict(void);

// Highest unduplicated tic that may be run. Never exceeds maketic.

int RB_RunLimit(void);

// Fill cmds[] and ingame[] for the given tic with confirmed or
// predicted data and remember what was used.

void RB_PrepareTic(int tic, const ticcmd_t *local_cmd,
                   ticcmd_t *cmds, boolean *ingame);

// Save a snapshot of the game state before running the given tic,
// if a snapshot is due for it.

void RB_MaybeSaveKeyframe(int tic);

// True if a misprediction was detected and has not been resolved yet.

boolean RB_NeedsRollback(void);

// Restore the newest snapshot at or before the mispredicted tic and prepare for
// resimulation from *start_tic up to *end_tic. Returns false if the rollback
// cannot be performed yet.

boolean RB_BeginRollback(int *start_tic, int *end_tic);

#endif
