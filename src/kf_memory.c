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

#include "am_map.h"
#include "d_player.h"
#include "d_think.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_system.h"
#include "m_arena.h"
#include "m_array.h"
#include "m_random.h"
#include "p_dirty.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "s_musinfo.h"
#include "s_sound.h"
#include "st_widgets.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYFRAME_BUFFER_SIZE (256 * 1024)

typedef struct keyframe_data_s
{
    char *buffer;
    arena_copy_t *thinkers;
    arena_copy_t *msecnodes;
    arena_copy_t *activeceilings;
    arena_copy_t *activeplats;
} keyframe_data_t;

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
    uintptr_t result = 0;
    memcpy(&result, curr_p, sizeof(uintptr_t));
    curr_p += sizeof(uintptr_t);
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
            int num_visitedlevels = players[i].num_visitedlevels;
            level_t* visitedlevels = players[i].visitedlevels;

            readx(&players[i], sizeof(player_t), 1);

            players[i].num_visitedlevels = num_visitedlevels;
            players[i].visitedlevels = visitedlevels;
        }
    }
}

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        // killough 10/98: save full floor & ceiling heights, including fraction
        write32(sector->floorheight,
                sector->ceilingheight,
                sector->floor_xoffs,
                sector->floor_yoffs,
                sector->ceiling_xoffs,
                sector->ceiling_yoffs,
                sector->floor_rotation,
                sector->ceiling_rotation,
                sector->tint);

        write16(sector->floorpic,
                sector->ceilingpic,
                sector->lightlevel,
                sector->special, // needed?   yes -- transfer types
                sector->tag);    // needed?   need them -- killough 

        // Woof!
        writep(sector->soundtarget,
               sector->floordata,
               sector->ceilingdata,
               sector->thinglist,
               sector->touching_thinglist);
    }

    const line_t *line;

    int size = array_size(dirty_lines);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i].line;
        write16(line->special);
    }

    const side_t *side;

    size = array_size(dirty_sides);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i].side;

        write16(side->toptexture,
                side->bottomtexture,
                side->midtexture);

        write32(side->textureoffset,
                side->rowoffset);
    }
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        sector->floorheight = read32();
        sector->ceilingheight = read32();
        sector->floor_xoffs = read32();
        sector->floor_yoffs = read32();
        sector->ceiling_xoffs = read32();
        sector->ceiling_yoffs = read32();
        sector->floor_rotation = read32();
        sector->ceiling_rotation = read32();
        sector->tint = read32();

        sector->floorpic = read16();
        sector->ceilingpic = read16();
        sector->lightlevel = read16();
        sector->special = read16();
        sector->tag = read16();

        // Woof!
        sector->soundtarget = readp();
        sector->floordata = readp();
        sector->ceilingdata = readp();
        sector->thinglist = readp();
        sector->touching_thinglist = readp();
    }

    line_t *line;

    int oldsize = read32();
    int size = array_size(dirty_lines);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i].line;
        if (i < oldsize)
        {
            line->special = read16();
        }
        else
        {
            P_CleanLine(&dirty_lines[i]);
        }
    }

    side_t *side;
    oldsize = read32();
    size = array_size(dirty_sides);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i].side;
        if (i < oldsize)
        {
            side->toptexture = read16();
            side->bottomtexture = read16();
            side->midtexture = read16();    
            side->textureoffset = read32();
            side->rowoffset = read32(); 
        }
        else
        {
            P_CleanSide(&dirty_sides[i]);
        }
    }
}

static void ArchivePlayState(keyframe_t *keyframe)
{
    // p_tick.h
    writex(&thinkercap, sizeof(thinkercap), 1);
    writex(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    keyframe->data->thinkers = M_ArenaCopy(thinkers_arena);

    // p_map.h
    write32(floatok,
            felldown,
            tmfloorz,
            tmceilingz,
            hangsolid);

    writep(ceilingline,
           floorline,
           linetarget,
           sector_list,
           blockline);

    writex(tmbbox, sizeof(tmbbox), 1);

    writep(headsecnode);
    keyframe->data->msecnodes = M_ArenaCopy(msecnodes_arena);
    
    // p_maputil.h
    write32(opentop,
            openbottom,
            openrange,
            lowfloor);

    writex(&trace, sizeof(trace), 1);

    // p_setup.h
    writex(blocklinks, blocklinks_size, 1);

    // p_spec.h
    writep(activeceilings,
           activeplats);
    keyframe->data->activeceilings = M_ArenaCopy(activeceilings_arena);
    keyframe->data->activeplats = M_ArenaCopy(activeplats_arena);

    // music
    write32(current_musicnum);
    writex(&musinfo, sizeof(musinfo), 1);
}

static void UnArchivePlayState(const keyframe_t *keyframe)
{
    // p_tick.h
    readx(&thinkercap, sizeof(thinkercap), 1);
    readx(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    M_ArenaRestore(thinkers_arena, keyframe->data->thinkers);

    // p_map.h
    floatok = read32();
    felldown = read32();
    tmfloorz = read32();
    tmceilingz = read32();
    hangsolid = read32();

    ceilingline = readp();
    floorline = readp();
    linetarget = readp();
    sector_list = readp();
    blockline = readp();

    readx(tmbbox, sizeof(tmbbox), 1);

    headsecnode = readp();
    M_ArenaRestore(msecnodes_arena, keyframe->data->msecnodes);
    
    // p_maputil.h
    opentop = read32();
    openbottom = read32();
    openrange = read32();
    lowfloor = read32();

    readx(&trace, sizeof(trace), 1);

    // p_setup.h
    readx(blocklinks, blocklinks_size, 1);

    // p_spec.h
    activeceilings = readp();
    activeplats = readp();
    M_ArenaRestore(activeceilings_arena, keyframe->data->activeceilings);
    M_ArenaRestore(activeplats_arena, keyframe->data->activeplats);

    // music
    current_musicnum = read32();
    readx(&musinfo, sizeof(musinfo), 1);
    S_RestartMusic();
}

static void ArchiveRNG(void)
{
    writex(&rng, sizeof(rng_t), 1);
}

static void UnArchiveRNG(void)
{
    readx(&rng, sizeof(rng_t), 1);
}

static void ArchiveAutomap(void)
{
    write32(markpointnum);
    if (markpoints && markpointnum)
    {
        writex(markpoints, sizeof(*markpoints), markpointnum);
    }
}

static void UnArchiveAutomap(void)
{
    markpointnum = read32();
    if (markpoints && markpointnum)
    {
        readx(markpoints, sizeof(*markpoints), markpointnum);
    }
}

keyframe_t *P_SaveKeyframe(int tic)
{
    keyframe_t *keyframe = calloc(1, sizeof(*keyframe));
    keyframe->data = calloc(1, sizeof(*keyframe->data));

    buffer_size = KEYFRAME_BUFFER_SIZE;
    buffer = malloc(buffer_size);
    curr_p = buffer;

    write8((gametic - boom_basetic) & 255);

    write32(leveltime,
            totalleveltimes,
            playback_tic,
            playback_totaltics);

    ArchivePlayers();
    ArchiveWorld();
    ArchivePlayState(keyframe);
    ArchiveRNG();
    ArchiveAutomap();

    writep(demo_p);

    keyframe->data->buffer = buffer;
    keyframe->tic = tic;
    keyframe->episode = gameepisode;
    keyframe->map = gamemap;

    return keyframe;
}

void P_LoadKeyframe(const keyframe_t *keyframe)
{
    curr_p = keyframe->data->buffer;

    boom_basetic = gametic - read8();

    leveltime = read32();
    totalleveltimes = read32();
    playback_tic = read32();
    playback_totaltics = read32();

    P_MapStart();
    UnArchivePlayers();
    UnArchiveWorld();
    UnArchivePlayState(keyframe);
    UnArchiveRNG();
    UnArchiveAutomap();
    P_MapEnd();

    demo_p = readp();
}

void P_FreeKeyframe(keyframe_t *keyframe)
{
    keyframe_data_t *data = keyframe->data;
    free(data->buffer);
    M_ArenaFreeCopy(data->thinkers);
    M_ArenaFreeCopy(data->msecnodes);
    M_ArenaFreeCopy(data->activeceilings);
    M_ArenaFreeCopy(data->activeplats);
    free(data);
    free(keyframe);
}
