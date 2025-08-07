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

#include "d_player.h"
#include "d_think.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dsdhacked.h"
#include "g_game.h"
#include "i_system.h"
#include "i_timer.h"
#include "info.h"
#include "m_arena.h"
#include "m_config.h"
#include "m_random.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"
#include "st_widgets.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int rewind_interval;
static int rewind_depth;
static int rewind_timeout;

static int keyframe_tic;
static boolean disable_rewind;

typedef struct
{
    int tic;
    char *buffer;
    arena_copy_t *thinkers;
    arena_copy_t *msecnodes;
    arena_copy_t *activeceilings;
    arena_copy_t *activeplats;
} keyframe_t;

static char *buffer, *curr_p;
static size_t buffer_size;

inline static void check_buffer(size_t size)
{
    ptrdiff_t offset = curr_p - buffer;

    if (offset + size <= buffer_size)
    {
        return;
    }

    while (offset + size > buffer_size)
    {
        buffer_size *= 2;
    }

    buffer = I_Realloc(buffer, buffer_size);
    curr_p = buffer + offset;
}

inline static void write8_internal(const int8_t data[], size_t size)
{
    check_buffer(size);
    memcpy(curr_p, data, size);
    curr_p += size;
}

inline static void write16_internal(const int16_t data[], size_t size)
{
    check_buffer(size);
    memcpy(curr_p, data, size);
    curr_p += size;
}

inline static void write32_internal(const int32_t data[], size_t size)
{
    check_buffer(size);
    memcpy(curr_p, data, size);
    curr_p += size;
}

inline static void writep_internal(const void *data[], size_t size)
{
    check_buffer(size);
    memcpy(curr_p, data, size);
    curr_p += size;
}

inline static void writex(const void *ptr, size_t size, int count)
{
    size_t offset = size * count;
    check_buffer(offset);
    memcpy(curr_p, ptr, offset);
    curr_p += offset;
}

#define write8(...)                                \
    write8_internal((const int8_t[]){__VA_ARGS__}, \
                    sizeof((const int8_t[]){__VA_ARGS__}))

#define write16(...)                                 \
    write16_internal((const int16_t[]){__VA_ARGS__}, \
                     sizeof((const int16_t[]){__VA_ARGS__}))

#define write32(...)                                 \
    write32_internal((const int32_t[]){__VA_ARGS__}, \
                     sizeof((const int32_t[]){__VA_ARGS__}))

#define writep(...)                                \
    writep_internal((const void *[]){__VA_ARGS__}, \
                     sizeof((const void *[]){__VA_ARGS__}))

inline static int8_t read8(void)
{
    int8_t result = *((int8_t *)curr_p);
    curr_p += sizeof(int8_t);
    return result;
}

inline static int16_t read16(void)
{
    int16_t result = 0;
    memcpy(&result, curr_p, sizeof(int16_t));
    curr_p += sizeof(int16_t);
    return result;
}

inline static int32_t read32(void)
{
    int32_t result = 0;
    memcpy(&result, curr_p, sizeof(int32_t));
    curr_p += sizeof(int32_t);
    return result;
}

inline static void *readp(void)
{
    intptr_t result = 0;
    memcpy(&result, curr_p, sizeof(intptr_t));
    curr_p += sizeof(intptr_t);
    return (void *)result;
}

inline static void readx(void *ptr, size_t size, int count)
{
    size_t offset = size * count;
    memcpy(ptr, curr_p, offset);
    curr_p += offset;
}

static void ArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            writex(&players[i], sizeof(player_t), 1);
        }
    }
}

static void UnArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            readx(&players[i], sizeof(player_t), 1);
        }
    }
}

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sec;
    const line_t *li;
    const side_t *si;

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        // killough 10/98: save full floor & ceiling heights, including fraction
        write32(sec->floorheight,
                sec->ceilingheight,
                sec->floor_xoffs,
                sec->floor_yoffs,
                sec->ceiling_xoffs,
                sec->ceiling_yoffs,
                sec->floor_rotation,
                sec->ceiling_rotation);

        write16(sec->floorpic,
                sec->ceilingpic,
                sec->lightlevel,
                sec->special, // needed?   yes -- transfer types
                sec->tag);    // needed?   need them -- killough 

        // Woof!
        writep(sec->soundtarget,
               sec->thinglist,
               sec->floordata,
               sec->ceilingdata,
               sec->lightingdata,
               sec->touching_thinglist);
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        write16(li->flags,
                li->special,
                li->tag);

        write32(li->angle,
                li->frontmusic,
                li->backmusic);

        // Woof!
        writep(li->frontsector,
               li->backsector);

        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] != NO_INDEX)
            {
                si = &sides[li->sidenum[j]];

                write16(si->toptexture,
                        si->bottomtexture,
                        si->midtexture);

                write32(si->textureoffset,
                        si->rowoffset);
            }
        }
    }
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sec;
    line_t *li;
    side_t *si;

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        sec->floorheight = read32();
        sec->ceilingheight = read32();
        sec->floor_xoffs = read32();
        sec->floor_yoffs = read32();
        sec->ceiling_xoffs = read32();
        sec->ceiling_yoffs = read32();
        sec->floor_rotation = read32();
        sec->ceiling_rotation = read32();

        sec->floorpic = read16();
        sec->ceilingpic = read16();
        sec->lightlevel = read16();
        sec->special = read16();
        sec->tag = read16();

        // Woof!
        sec->soundtarget = readp();
        sec->thinglist = readp();
        sec->floordata = readp();
        sec->ceilingdata = readp();
        sec->lightingdata = readp();
        sec->touching_thinglist = readp();
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        li->flags = read16();
        li->special = read16();
        li->tag = read16();
        
        li->angle = read32();
        li->frontmusic = read32();
        li->backmusic = read32();

        // Woof!
        li->frontsector = readp();
        li->backsector = readp();

        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] != NO_INDEX)
            {
                si = &sides[li->sidenum[j]];
              
                si->toptexture = read16();
                si->bottomtexture = read16();
                si->midtexture = read16();
                
                si->textureoffset = read32(); 
                si->rowoffset = read32(); 
                si->oldtextureoffset = si->textureoffset;
                si->oldrowoffset = si->rowoffset;
            }
        }
    }
}

static void ArchivePlayState(keyframe_t *keyframe)
{
    writex(&thinkercap, sizeof(thinkercap), 1);
    writex(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    keyframe->thinkers = M_CopyArena(thinkers_arena);

    writep(headsecnode);
    keyframe->msecnodes = M_CopyArena(msecnodes_arena);

    writep(activeceilings);
    keyframe->activeceilings = M_CopyArena(activeceilings_arena);

    writep(activeplats);
    keyframe->activeplats = M_CopyArena(activeplats_arena);

    writex(blocklinks, blocklinks_size, 1);

    write32(floatok,
            felldown,
            tmfloorz,
            tmceilingz);
    writex(tmbbox, sizeof(tmbbox), 1);
    writep(ceilingline,
           floorline,
           linetarget,
           sector_list,
           blockline);

    write32(hangsolid);

    write32(numspechit);
    writex(spechit, sizeof(*spechit), numspechit);
    
    write32(opentop,
            openbottom,
            openrange,
            lowfloor);

    writex(&trace, sizeof(trace), 1);
    write32(num_intercepts);
    writex(intercepts, sizeof(*intercepts), num_intercepts);
}

static void UnArchivePlayState(keyframe_t *keyframe)
{
    readx(&thinkercap, sizeof(thinkercap), 1);
    readx(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    M_RestoreArena(thinkers_arena, keyframe->thinkers);

    headsecnode = readp();
    M_RestoreArena(msecnodes_arena, keyframe->msecnodes);

    activeceilings = readp();
    M_RestoreArena(activeceilings_arena, keyframe->activeceilings);

    activeplats = readp();
    M_RestoreArena(activeplats_arena, keyframe->activeplats);

    readx(blocklinks, blocklinks_size, 1);

    floatok = read32();
    felldown = read32();
    tmfloorz = read32();
    tmceilingz = read32();
    readx(tmbbox, sizeof(tmbbox), 1);

    ceilingline = readp();
    floorline = readp();
    linetarget = readp();
    sector_list = readp();
    blockline = readp();
    hangsolid = read32();

    numspechit = read32();
    readx(spechit, sizeof(*spechit), numspechit);

    opentop = read32();
    openbottom = read32();
    openrange = read32();
    lowfloor = read32();

    readx(&trace, sizeof(trace), 1);
    num_intercepts = read32();
    readx(intercepts, sizeof(*intercepts), num_intercepts);

    setmobjstate_recursion = 0;
    memset(seenstate_tab, 0, sizeof(statenum_t) * num_states);
}

static void ArchiveRNG(void)
{
    writex(&rng, sizeof(rng_t), 1);
}

static void UnArchiveRNG(void)
{
    readx(&rng, sizeof(rng_t), 1);
}

static keyframe_t *SaveKeyframe(void)
{
    keyframe_t *keyframe = calloc(1, sizeof(*keyframe));

    buffer_size = 512 * 1024;
    buffer = malloc(buffer_size);
    curr_p = buffer;

    write8((gametic - basetic) & 255);

    write32(leveltime,
            totalleveltimes,
            playback_tic,
            playback_totaltics);

    ArchivePlayers();
    ArchiveWorld();
    ArchivePlayState(keyframe);
    ArchiveRNG();

    if (demoplayback || demorecording)
    {
        writep(demo_p);
    }

    keyframe->buffer = buffer;
    keyframe->tic = keyframe_tic;

    return keyframe;
}

static void LoadKeyframe(keyframe_t *keyframe)
{
    curr_p = keyframe->buffer;

    basetic = gametic - read8();

    leveltime = read32();
    totalleveltimes = read32();
    playback_tic = read32();
    playback_totaltics = read32();

    P_MapStart();
    UnArchivePlayers();
    UnArchiveWorld();
    UnArchivePlayState(keyframe);
    UnArchiveRNG();
    P_MapEnd();

    if (demoplayback || demorecording)
    {
        demo_p = readp();
    }
}

static void FreeKeyframe(keyframe_t *keyframe)
{
    free(keyframe->buffer);
    M_FreeArenaCopy(keyframe->thinkers);
    M_FreeArenaCopy(keyframe->msecnodes);
    M_FreeArenaCopy(keyframe->activeceilings);
    M_FreeArenaCopy(keyframe->activeplats);
    free(keyframe);
}

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
            FreeKeyframe(oldtail->keyframe);
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
        FreeKeyframe(temp->keyframe);
        free(temp);
    }
    memset(&queue, 0, sizeof(queue));
}

void P_SaveKeyframe(void)
{
    int interval_tics = TICRATE * rewind_interval / 1000;

    if (!disable_rewind && keyframe_tic % interval_tics == 0)
    {
        int time = I_GetTimeMS();
        
        Push(SaveKeyframe());

        disable_rewind = (I_GetTimeMS() - time > rewind_timeout);
        if (disable_rewind)
        {
            displaymsg("Slow key framing: rewind disabled");
        }
    }

    ++keyframe_tic;
}

void P_LoadKeyframe(void)
{
    int interval_tics = TICRATE * rewind_interval / 1000;

    while (1)
    {
        keyframe_t *keyframe = Pop();
        
        if (!keyframe)
        {
            break;
        }

        if (keyframe->tic > 0 && keyframe_tic - keyframe->tic < interval_tics)
        {
            FreeKeyframe(keyframe);
            continue;
        }

        LoadKeyframe(keyframe);
        displaymsg("Restored key frame %d", queue.count + 1);

        if (keyframe->tic == 0) // don't delete first keyframe
        {
            Push(keyframe);
        }
        else
        {
            FreeKeyframe(keyframe);
        }

        G_ClearInput();
        break;
    }
}

void P_ResetKeyframes(void)
{
    FreeKeyframeQueue();
    keyframe_tic = 0;
    disable_rewind = false;
}

void P_BindKeyframeVariables(void)
{
    BIND_NUM(rewind_interval, 1000, 100, 10000,
        "Rewind interval in miliseconds");
    BIND_NUM(rewind_depth, 60, 10, 1000,
        "Number of rewind keyframes to be stored");
    BIND_NUM(rewind_timeout, 10, 0, 25,
        "Time to store a key frame, in milliseconds; if exceeded, storing "
        "will stop (0 = No limit)");
}
