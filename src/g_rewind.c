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

#include "p_keyframe.h"

#include "doomtype.h"
#include "g_game.h"
#include "i_timer.h"
#include "m_config.h"

#include <stdlib.h>
#include <string.h>

static int rewind_interval;
static int rewind_depth;
static int rewind_timeout;
static boolean rewind_auto;

static boolean disable_rewind;
static int current_tic;

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

void G_SaveAutoKeyframe(void)
{
    if (!rewind_auto)
    {
        return;
    }

    int interval_tics = TICRATE * rewind_interval / 1000;

    if (!disable_rewind && current_tic % interval_tics == 0)
    {
        int time = I_GetTimeMS();
        
        Push(P_SaveKeyframe(current_tic));

        disable_rewind = (I_GetTimeMS() - time > rewind_timeout);
        if (disable_rewind)
        {
            displaymsg("Slow key framing: rewind disabled");
        }
    }

    ++current_tic;
}

void G_LoadAutoKeyframe(void)
{
    int interval_tics = TICRATE * rewind_interval / 1000;

    while (1)
    {
        keyframe_t *keyframe = Pop();
        
        if (!keyframe)
        {
            break;
        }

        int tic = P_GetKeyframeTic(keyframe);
        if (tic > 0 && current_tic - tic < interval_tics)
        {
            P_FreeKeyframe(keyframe);
            continue;
        }

        P_LoadKeyframe(keyframe);
        displaymsg("Restored key frame");

        if (tic == 0) // don't delete first keyframe
        {
            Push(keyframe);
        }
        else
        {
            P_FreeKeyframe(keyframe);
        }

        G_ClearInput();
        break;
    }
}

void G_ResetRewind(void)
{
    FreeKeyframeQueue();
    current_tic = 0;
    disable_rewind = false;
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
