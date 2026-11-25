//
// Copyright(C) 2025 Roman Fomin
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

#include "d_event.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_message.h"
#include "g_game.h"
#include "i_printf.h"
#include "i_timer.h"
#include "m_config.h"
#include "p_dirty.h"
#include "p_keyframe.h"

#define REWIND_MAX_DEPTH 1000
#define REWIND_SLOW_LIMIT 3

static int rewind_interval;
static int rewind_depth;
static int rewind_timeout;
static boolean rewind_auto;

static boolean disable_rewind;
static int slow_keyframes;
static boolean skip_timeout = true;

static keyframe_t *rewind_queue[REWIND_MAX_DEPTH];
static int rewind_head;
static int rewind_count;

static boolean IsEmpty(void)
{
    return rewind_count == 0;
}

static int OldestIndex(void)
{
    return (rewind_head - rewind_count + 1 + REWIND_MAX_DEPTH)
           % REWIND_MAX_DEPTH;
}

static void DropOldest(void)
{
    if (IsEmpty())
    {
        return;
    }

    const int index = OldestIndex();
    P_DeleteKeyframe(rewind_queue[index]);
    rewind_queue[index] = NULL;
    --rewind_count;
}

// Add a keyframe as the newest element, dropping the oldest ones when
// the queue is over the configured depth.

static void Push(keyframe_t *keyframe)
{
    int depth = rewind_depth;
    if (depth < 1)
    {
        depth = 1;
    }
    if (depth > REWIND_MAX_DEPTH)
    {
        depth = REWIND_MAX_DEPTH;
    }

    while (rewind_count >= depth)
    {
        DropOldest();
    }

    if (IsEmpty())
    {
        rewind_head = 0;
    }
    else
    {
        rewind_head = (rewind_head + 1) % REWIND_MAX_DEPTH;
    }

    rewind_queue[rewind_head] = keyframe;
    ++rewind_count;
}

// Remove and return the newest element.

static keyframe_t *Pop(void)
{
    if (IsEmpty())
    {
        return NULL;
    }

    keyframe_t *keyframe = rewind_queue[rewind_head];
    rewind_queue[rewind_head] = NULL;
    rewind_head = (rewind_head - 1 + REWIND_MAX_DEPTH) % REWIND_MAX_DEPTH;
    --rewind_count;

    return keyframe;
}

static int RewindIntervalTics(void)
{
    const int tics = TICRATE * rewind_interval / 1000;
    return tics > 0 ? tics : 1;
}

void G_SaveAutoKeyframe(void)
{
    if (!rewind_auto)
    {
        return;
    }

    const int interval_tics = RewindIntervalTics();
    const int current_tic = gametic - true_basetic;

    if (!disable_rewind && current_tic % interval_tics == 0)
    {
        const uint64_t start = I_GetTimeUS();

        Push(P_SaveKeyframe(current_tic));

        if (rewind_timeout && !skip_timeout)
        {
            const uint64_t elapsed = I_GetTimeUS() - start;

            if (elapsed > (uint64_t)rewind_timeout * 1000)
            {
                if (++slow_keyframes >= REWIND_SLOW_LIMIT)
                {
                    disable_rewind = true;
                    displaymsg("Slow key framing: rewind disabled");
                }
            }
            else
            {
                slow_keyframes = 0;
            }
        }

        // The first save after a reset or a level load includes cold
        // allocations; do not use it for the timeout decision.

        skip_timeout = false;
    }
}

void G_LoadAutoKeyframe(void)
{
    gameaction = ga_nothing;

    const int current_tic = gametic - true_basetic;
    const int interval_tics = RewindIntervalTics();

    // Search for the newest keyframe at least one interval old.

    int chosen = -1;
    for (int i = 0; i < rewind_count; ++i)
    {
        const int index =
            (rewind_head - i + REWIND_MAX_DEPTH) % REWIND_MAX_DEPTH;
        const keyframe_t *keyframe = rewind_queue[index];

        if (keyframe->tic > 0
            && current_tic - keyframe->tic < interval_tics)
        {
            continue;
        }

        chosen = index;
        break;
    }

    if (chosen < 0)
    {
        // All stored keyframes are too recent (or there are none).
        displaymsg("No keyframe available");
        return;
    }

    // Delete skipped keyframes.

    while (rewind_head != chosen)
    {
        keyframe_t *skipped = Pop();
        if (skipped)
        {
            P_DeleteKeyframe(skipped);
        }
    }

    keyframe_t *keyframe = Pop();
    if (keyframe)
    {
        if (keyframe->episode != gameepisode || keyframe->map != gamemap)
        {
            G_SimplifiedInitNew(keyframe->episode, keyframe->map);

            if (!P_UnArchiveDirtyArrays(keyframe->episode, keyframe->map))
            {
                I_Printf(VB_WARNING,
                         "No dirty arrays for episode %d map %d",
                         keyframe->episode, keyframe->map);
            }
        }
        P_LoadKeyframe(keyframe);
        displaymsg("Restored key frame");

        if (IsEmpty()) // Don't delete the first keyframe.
        {
            Push(keyframe);
        }
        else
        {
            P_DeleteKeyframe(keyframe);
        }

        G_ClearInput();
        if (!freelook)
        {
            players[consoleplayer].pitch = 0;
        }
        gamestate = GS_LEVEL;
    }
}

static void FreeKeyframeQueue(void)
{
    for (int i = 0; i < rewind_count; ++i)
    {
        const int index =
            (rewind_head - i + REWIND_MAX_DEPTH) % REWIND_MAX_DEPTH;
        P_DeleteKeyframe(rewind_queue[index]);
        rewind_queue[index] = NULL;
    }

    rewind_head = 0;
    rewind_count = 0;
}

void G_ResetRewind(boolean force)
{
    if (disable_rewind)
    {
        disable_rewind = false;
    }

    slow_keyframes = 0;
    skip_timeout = true;

    if (force)
    {
        FreeKeyframeQueue();
        P_FreeKeyframePool();
    }
}

void G_BindRewindVariables(void)
{
    BIND_NUM(rewind_interval, 1000, 100, 10000,
        "Rewind interval in milliseconds");
    BIND_NUM(rewind_depth, 60, 10, REWIND_MAX_DEPTH,
        "Number of rewind key frames to be stored");
    BIND_NUM(rewind_timeout, 10, 0, 25,
        "Time to store a key frame [ms]; if exceeded, storing "
        "will stop (0 = No limit)");
    BIND_BOOL(rewind_auto, true, "Enable storing rewind key frames");
}
