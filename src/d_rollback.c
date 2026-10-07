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

#include <string.h>

#include "d_event.h"
#include "d_rollback.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_argv.h"
#include "nw_client.h"
#include "nw_defs.h"
#include "p_keyframe.h"

// How many tics we are allowed to run ahead of the confirmed data.

#define RB_MAX_PREDICT 8

// A state snapshot is saved every this many tics.

#define RB_KEYFRAME_INTERVAL 2

boolean rollback_enabled = false;
boolean rollback_resim = false;

static int rb_localplayer;

// The next tic expected from the server; all tics below it are
// confirmed.

static int rb_recvtic;

// The earliest tic whose confirmed data did not match the prediction,
// or -1 if no rollback is pending.

static int rollback_pending = -1;

// Set when a rollback was needed but could not be performed. While
// stalled, no prediction happens until we have caught up with the
// confirmed data.

static boolean rb_stalled = false;

static keyframe_t *keyframes[BACKUPTICS];

// The ticcmd sets that were actually used to run each tic.

static ticcmd_t used_cmds[BACKUPTICS][NW_MAXPLAYERS];
static boolean used_ingame[BACKUPTICS][NW_MAXPLAYERS];
static boolean used_predicted[BACKUPTICS][NW_MAXPLAYERS];
static int used_tic[BACKUPTICS];

// Confirmed ticcmd sets received from the server.

static ticcmd_t confirmed_cmds[BACKUPTICS][NW_MAXPLAYERS];
static boolean confirmed_ingame[BACKUPTICS][NW_MAXPLAYERS];
static int confirmed_tic[BACKUPTICS];

static void FreeSlotKeyframe(int slot)
{
    if (keyframes[slot])
    {
        P_DeleteKeyframe(keyframes[slot]);
        keyframes[slot] = NULL;
    }
}

void RB_Shutdown(void)
{
    for (int i = 0; i < BACKUPTICS; ++i)
    {
        FreeSlotKeyframe(i);
    }

    rollback_enabled = false;
    rollback_resim = false;
    rollback_pending = -1;
    rb_stalled = false;
}

void RB_Init(int consoleplayer, int num_players)
{
    RB_Shutdown();

    //!
    // @category net
    // @help
    //
    // Disable rollback netcode.
    //

    if (M_ParmExists("-norollback"))
    {
        return;
    }

    rb_localplayer = consoleplayer;
    rb_recvtic = 0;

    for (int i = 0; i < BACKUPTICS; ++i)
    {
        used_tic[i] = -1;
        confirmed_tic[i] = -1;
    }

    if (!nw_client_connected || drone)
    {
        return;
    }

    if (ticdup != 1)
    {
        I_Printf(VB_WARNING, "Rollback requires -dup 1, using lockstep instead");
        return;
    }

    rollback_enabled = true;

    I_Printf(VB_INFO, "Rollback netcode enabled, predicting up to %d tics",
             RB_MAX_PREDICT);
}

static void FreeOldKeyframes(void)
{
    // A misprediction can only be detected for a tic at or above
    // rb_recvtic - 1, when its confirmed data arrives. A pending
    // rollback targets such a tic, and its keyframe search reaches
    // RB_KEYFRAME_INTERVAL + RB_MAX_PREDICT + 1 tics below the target.
    //
    // The horizon must therefore respect a pending rollback: a burst of
    // confirmed tics (common right after a packet-loss stall) advances
    // rb_recvtic in a single frame, and without this the keyframes the
    // pending rollback is about to use would be freed first, stalling
    // the rollback and leaving the divergence uncorrected.

    int horizon = rb_recvtic;

    if (rollback_pending >= 0 && rollback_pending < horizon)
    {
        horizon = rollback_pending;
    }

    const int oldest = horizon - RB_MAX_PREDICT - RB_KEYFRAME_INTERVAL - 2;

    for (int i = 0; i < BACKUPTICS; ++i)
    {
        if (keyframes[i] && keyframes[i]->tic < oldest)
        {
            FreeSlotKeyframe(i);
        }
    }
}

void RB_ReceiveTic(int tic, ticcmd_t *ticcmds, boolean *players_mask)
{
    const int slot = tic % BACKUPTICS;

    confirmed_tic[slot] = tic;

    for (int i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (i == rb_localplayer)
        {
            // Our own ticcmd is never sent back to us expanded; the
            // server's copy is irrelevant.

            continue;
        }

        confirmed_cmds[slot][i] = ticcmds[i];
        confirmed_ingame[slot][i] = players_mask[i];

        // If this tic was already simulated using a prediction and the
        // confirmed data differs, schedule a rollback to it.

        if (i < MAXPLAYERS && used_tic[slot] == tic && used_predicted[slot][i])
        {
            ticcmd_t *pred = &used_cmds[slot][i];
            ticcmd_t *conf = &ticcmds[i];
            boolean differs;

            differs = pred->forwardmove != conf->forwardmove
                   || pred->sidemove != conf->sidemove
                   || pred->angleturn != conf->angleturn
                   || pred->buttons != conf->buttons
                   || used_ingame[slot][i] != players_mask[i];

            if (differs)
            {
                if (rollback_pending < 0 || tic < rollback_pending)
                {
                    rollback_pending = tic;
                }
            }
        }
    }

    rb_recvtic = tic + 1;

    FreeOldKeyframes();
}

boolean RB_TicConfirmed(int tic)
{
    if (!rollback_enabled)
    {
        return true;
    }

    // RB_ReceiveTic() can only schedule a rollback for the tic it is
    // confirming right now, that is, for rb_recvtic - 1. Everything
    // below that has been confirmed and matched the prediction, so its
    // state is final.

    return tic < rb_recvtic - 1;
}

boolean RB_CanPredict(void)
{
    if (!rollback_enabled || gamestate != GS_LEVEL || gameaction != ga_nothing)
    {
        return false;
    }

    // Prediction requires at least one snapshot to roll back to.

    for (int i = 0; i < BACKUPTICS; ++i)
    {
        if (keyframes[i])
        {
            return true;
        }
    }

    return false;
}

int RB_RunLimit(void)
{
    int limit;

    // Once we have caught up with the confirmed data, prediction may
    // resume.

    if (rb_stalled && gametic / ticdup <= rb_recvtic)
    {
        rb_stalled = false;
    }

    limit = rb_recvtic + ((!rb_stalled && RB_CanPredict()) ? RB_MAX_PREDICT : 0);

    // We cannot run a tic before the local input for it is built.

    if (limit > maketic)
    {
        limit = maketic;
    }

    return limit;
}

// Predict the input of a remote player for the given tic by repeating
// their latest known input.

static void Predict(int tic, int player, ticcmd_t *cmd, boolean *ingame)
{
    ticcmd_t last_cmd;

    memset(&last_cmd, 0, sizeof(last_cmd));
    *ingame = playeringame[player];

    for (int t = tic - 1; t >= 0 && tic - t <= RB_MAX_PREDICT + 4; --t)
    {
        const int slot = t % BACKUPTICS;

        if (confirmed_tic[slot] == t)
        {
            last_cmd = confirmed_cmds[slot][player];
            *ingame = confirmed_ingame[slot][player];
            break;
        }

        if (used_tic[slot] == t)
        {
            last_cmd = used_cmds[slot][player];
            *ingame = used_ingame[slot][player];
            break;
        }
    }

    // One-shot events must never be repeated.

    if (last_cmd.buttons & BT_SPECIAL)
    {
        last_cmd.buttons = 0;
    }

    last_cmd.chatchar = 0;

    *cmd = last_cmd;
}

void RB_PrepareTic(int tic, const ticcmd_t *local_cmd, ticcmd_t *cmds,
                   boolean *ingame)
{
    const int slot = tic % BACKUPTICS;

    used_tic[slot] = tic;

    for (int i = 0; i < NW_MAXPLAYERS; ++i)
    {
        boolean predicted = false;

        if (i >= MAXPLAYERS)
        {
            memset(&cmds[i], 0, sizeof(cmds[i]));
            ingame[i] = false;
        }
        else if (i == rb_localplayer)
        {
            cmds[i] = *local_cmd;
            ingame[i] = true;
        }
        else if (confirmed_tic[slot] == tic)
        {
            cmds[i] = confirmed_cmds[slot][i];
            ingame[i] = confirmed_ingame[slot][i];
        }
        else
        {
            Predict(tic, i, &cmds[i], &ingame[i]);
            predicted = true;
        }

        used_cmds[slot][i] = cmds[i];
        used_ingame[slot][i] = ingame[i];
        used_predicted[slot][i] = predicted;
    }
}

void RB_MaybeSaveKeyframe(int tic)
{
    if (!rollback_enabled || tic % RB_KEYFRAME_INTERVAL)
    {
        return;
    }

    if (gamestate != GS_LEVEL || gameaction != ga_nothing)
    {
        return;
    }

    const int slot = tic % BACKUPTICS;

    FreeSlotKeyframe(slot);

    keyframes[slot] = P_SaveKeyframe(tic);
}

boolean RB_NeedsRollback(void)
{
    return rollback_enabled && rollback_pending >= 0;
}

boolean RB_BeginRollback(int *start_tic, int *end_tic)
{
    const int target = rollback_pending;

    rollback_pending = -1;

    // Find the newest snapshot at or before the mispredicted tic.
    // Snapshots are saved at a fixed interval while tics are run, so
    // one is always nearby.

    keyframe_t *keyframe = NULL;
    int keyframe_tic = -1;

    for (int t = target;
         t >= 0 && t >= target - RB_KEYFRAME_INTERVAL - RB_MAX_PREDICT - 1;
         --t)
    {
        const int slot = t % BACKUPTICS;

        if (keyframes[slot] && keyframes[slot]->tic == t)
        {
            keyframe = keyframes[slot];
            keyframe_tic = t;
            break;
        }
    }

    if (!keyframe)
    {
        // Nothing to roll back to. Stall until we have caught up with
        // the confirmed data. The divergence at `target` is left
        // uncorrected, so this must never happen in practice; if it
        // does, the client is now permanently out of sync with its
        // peers and a lockstep peer will fail its consistency check.

        I_Printf(VB_WARNING,
                 "rollback stalled at tic %d: no keyframe in [%d, %d]",
                 target, target - RB_KEYFRAME_INTERVAL - RB_MAX_PREDICT - 1,
                 target);

        rb_stalled = true;

        return false;
    }

    *start_tic = keyframe_tic;
    *end_tic = gametic / ticdup;

    // P_LoadKeyframe() derives boom_basetic from the current gametic,
    // so rewind gametic before restoring.

    gametic = keyframe_tic * ticdup;

    P_LoadKeyframe(keyframe);

    // The keyframes of the diverged tics are about to be replaced.

    for (int t = keyframe_tic + 1; t <= *end_tic; ++t)
    {
        FreeSlotKeyframe(t % BACKUPTICS);
    }

    return true;
}
