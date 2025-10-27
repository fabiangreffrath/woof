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
#include "g_game.h"
#include "i_timer.h"
#include "m_config.h"
#include "p_dirty.h"
#include "p_keyframe.h"

#include <stdlib.h>
#include <string.h>

static int rewind_interval;
static int rewind_depth;
static int rewind_timeout;
static boolean rewind_auto;

static boolean disable_rewind;
static int interval_tics;

typedef struct elem_s
{
    keyframe_t *keyframe;
    struct elem_s *next;
    struct elem_s *prev;
} elem_t;

typedef struct
{
    elem_t* top;
    elem_t* tail;
    int count;
} queue_t;

static queue_t queue;

static boolean IsEmpty(void)
{
    return queue.top == NULL;
}

// Add an element to the top of the queue
static void Push(keyframe_t *keyframe)
{
    // Remove oldest element if queue is full
    if (queue.count == rewind_depth)
    {
        elem_t *oldtail = queue.tail;
        if (oldtail)
        {
            queue.tail = oldtail->prev;
            if (queue.tail)
            {
                queue.tail->next = NULL;
            }
            else
            {
                // Queue becomes empty after removal
                queue.top = NULL;
            }
            P_FreeKeyframe(oldtail->keyframe);
            free(oldtail);
            --queue.count;
        }
    }

    elem_t *newelem = calloc(1, sizeof(*newelem));    
    newelem->keyframe = keyframe;
    
    if (IsEmpty())
    {
        queue.top = newelem;
        queue.tail = newelem;
    }
    else
    {
        newelem->next = queue.top;
        queue.top->prev = newelem;
        queue.top = newelem;
    }
    ++queue.count;
}

// Remove and return element from top of queue
static keyframe_t *Pop(void)
{
    if (IsEmpty())
    {
        return NULL;
    }
    
    elem_t* temp = queue.top;
    keyframe_t *keyframe = temp->keyframe;
    queue.top = temp->next;
    if (queue.top)
    {
        queue.top->prev = NULL;
    }
    else
    {
        queue.tail = NULL;
    }
    free(temp);
    --queue.count;

    return keyframe;
}

void G_SaveAutoKeyframe(void)
{
    if (!rewind_auto)
    {
        return;
    }

    interval_tics = TICRATE * rewind_interval / 1000;

    int current_tic = gametic - true_basetic;

    if (!disable_rewind && current_tic % interval_tics == 0)
    {
        int time = I_GetTimeMS();
        
        Push(P_SaveKeyframe(current_tic));

        if (rewind_timeout)
        {
            disable_rewind = (I_GetTimeMS() - time > rewind_timeout);
        }
        if (disable_rewind)
        {
            displaymsg("Slow key framing: rewind disabled");
        }
    }
}

void G_LoadAutoKeyframe(void)
{
    gameaction = ga_nothing;

    if (IsEmpty())
    {
        return;
    }

    int current_tic = gametic - true_basetic;

    // Search for the closest keyframe by interval.
    elem_t* elem = queue.top;
    while (elem)
    {
        int tic = elem->keyframe->tic;
        if (tic > 0 && current_tic - tic < interval_tics)
        {
            elem = elem->next;
        }
        else
        {
            break;
        }
    }

    if (!elem)
    {
        // No suitable keyframe found (all are too recent).
        return;
    }

    // Delete from queue skipped keyframes.
    while (queue.top != elem)
    {
        keyframe_t* skipped = Pop();
        if (skipped)
        {
            P_FreeKeyframe(skipped);
        }
    }

    keyframe_t* keyframe = Pop();
    if (keyframe)
    {
        if (keyframe->episode != gameepisode || keyframe->map != gamemap)
        {
            G_SimplifiedInitNew(keyframe->episode, keyframe->map);
            P_UnArchiveDirtyArrays(keyframe->episode, keyframe->map);
        }
        P_LoadKeyframe(keyframe);
        displaymsg("Restored key frame");

        if (IsEmpty()) // Don't delete the first keyframe.
        {
            Push(keyframe);
        }
        else
        {
            P_FreeKeyframe(keyframe);
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
    elem_t* current = queue.top;
    while (current)
    {
        elem_t* temp = current;
        current = current->next;
        P_FreeKeyframe(temp->keyframe);
        free(temp);
    }
    memset(&queue, 0, sizeof(queue));
}

void G_ResetRewind(boolean force)
{
    if (disable_rewind)
    {
        disable_rewind = false;
    }
    if (force)
    {
        FreeKeyframeQueue();
    }
}

void G_BindRewindVariables(void)
{
    BIND_NUM(rewind_interval, 1000, 100, 10000,
        "Rewind interval in miliseconds");
    BIND_NUM(rewind_depth, 60, 10, 1000,
        "Number of rewind key frames to be stored");
    BIND_NUM(rewind_timeout, 10, 0, 25,
        "Time to store a key frame, in milliseconds; if exceeded, storing "
        "will stop (0 = No limit)");
    BIND_BOOL(rewind_auto, true, "Enable storing rewind key frames");
}
