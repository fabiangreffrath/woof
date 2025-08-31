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
#include "dsdhacked.h"
#include "g_game.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_random.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_musinfo.h"
#include "s_sound.h"
#include "st_widgets.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct keyframe_s
{
    int tic;
    char *buffer;
    arena_copy_t *thinkers;
    arena_copy_t *msecnodes;
    arena_copy_t *activeceilings;
    arena_copy_t *activeplats;
};

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
                sec->ceiling_rotation,
                sec->tint);

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
        sec->tint = read32();

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
    // p_tick.h
    writex(&thinkercap, sizeof(thinkercap), 1);
    writex(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    keyframe->thinkers = M_ArenaCopy(thinkers_arena);

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

    write32(numspechit);
    writex(spechit, sizeof(*spechit), numspechit);

    writex(tmbbox, sizeof(tmbbox), 1);

    writep(headsecnode);
    keyframe->msecnodes = M_ArenaCopy(msecnodes_arena);
    
    // p_maputil.h
    write32(opentop,
            openbottom,
            openrange,
            lowfloor);

    write32(num_intercepts);
    writex(intercepts, sizeof(*intercepts), num_intercepts);

    writex(&trace, sizeof(trace), 1);

    // p_setup.h
    writex(blocklinks, blocklinks_size, 1);

    // p_spec.h
    writep(activeceilings,
           activeplats);
    keyframe->activeceilings = M_ArenaCopy(activeceilings_arena);
    keyframe->activeplats = M_ArenaCopy(activeplats_arena);

    // music
    write32(current_musicnum);
    writex(&musinfo, sizeof(musinfo), 1);
}

static void UnArchivePlayState(const keyframe_t *keyframe)
{
    // p_tick.h
    readx(&thinkercap, sizeof(thinkercap), 1);
    readx(thinkerclasscap, sizeof(thinker_t), NUMTHCLASS);
    M_ArenaRestore(thinkers_arena, keyframe->thinkers);

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

    numspechit = read32();
    readx(spechit, sizeof(*spechit), numspechit);

    readx(tmbbox, sizeof(tmbbox), 1);

    headsecnode = readp();
    M_ArenaRestore(msecnodes_arena, keyframe->msecnodes);
    
    // p_maputil.h
    opentop = read32();
    openbottom = read32();
    openrange = read32();
    lowfloor = read32();

    num_intercepts = read32();
    readx(intercepts, sizeof(*intercepts), num_intercepts);

    readx(&trace, sizeof(trace), 1);

    // p_setup.h
    readx(blocklinks, blocklinks_size, 1);

    // p_spec.h
    activeceilings = readp();
    activeplats = readp();
    M_ArenaRestore(activeceilings_arena, keyframe->activeceilings);
    M_ArenaRestore(activeplats_arena, keyframe->activeplats);

    setmobjstate_recursion = 0;
    memset(seenstate_tab, 0, sizeof(statenum_t) * num_states);

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
    ArchiveAutomap();

    if (demoplayback || demorecording)
    {
        writep(demo_p);
    }

    keyframe->buffer = buffer;
    keyframe->tic = tic;

    return keyframe;
}

void P_LoadKeyframe(const keyframe_t *keyframe)
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
    UnArchiveAutomap();
    P_MapEnd();

    if (demoplayback || demorecording)
    {
        demo_p = readp();
    }
}

void P_FreeKeyframe(keyframe_t *keyframe)
{
    free(keyframe->buffer);
    M_ArenaFreeCopy(keyframe->thinkers);
    M_ArenaFreeCopy(keyframe->msecnodes);
    M_ArenaFreeCopy(keyframe->activeceilings);
    M_ArenaFreeCopy(keyframe->activeplats);
    free(keyframe);
}

int P_GetKeyframeTic(const keyframe_t *keyframe)
{
    return keyframe->tic;
}
