//
//  Copyright(C) 2021 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//      DSDHacked support

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "d_think.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"

//
//   States
//

state_t *states = NULL;
int num_states;
byte *defined_codeptr_args = NULL;
statenum_t *seenstate_tab = NULL;
actionf_t *deh_codeptr = NULL;

static void InitStates(void)
{
    states = original_states;
    num_states = NUMSTATES;

    array_grow(seenstate_tab, num_states);
    memset(seenstate_tab, 0, num_states * sizeof(*seenstate_tab));

    array_grow(deh_codeptr, num_states);
    for (int i = 0; i < num_states; i++)
    {
        deh_codeptr[i] = states[i].action;
    }

    array_grow(defined_codeptr_args, num_states);
    memset(defined_codeptr_args, 0, num_states * sizeof(*defined_codeptr_args));
}

static void FreeStates(void)
{
    array_free(defined_codeptr_args);
    array_free(deh_codeptr);
}

void dsdh_EnsureStatesCapacity(int limit)
{
    if (limit < num_states)
    {
        return;
    }

    const int old_num_states = num_states;

    static boolean first_allocation = true;
    if (first_allocation)
    {
        states = NULL;
        array_grow(states, old_num_states + limit);
        memcpy(states, original_states, old_num_states * sizeof(*states));
        first_allocation = false;
    }
    else
    {
        array_grow(states, limit);
    }

    num_states = array_capacity(states);
    const int size_delta = num_states - old_num_states;
    memset(states + old_num_states, 0, size_delta * sizeof(*states));

    array_grow(deh_codeptr, size_delta);
    memset(deh_codeptr + old_num_states, 0, size_delta * sizeof(*deh_codeptr));

    array_grow(defined_codeptr_args, size_delta);
    memset(defined_codeptr_args + old_num_states, 0,
           size_delta * sizeof(*defined_codeptr_args));

    array_grow(seenstate_tab, size_delta);
    memset(seenstate_tab + old_num_states, 0,
           size_delta * sizeof(*seenstate_tab));

    for (int i = old_num_states; i < num_states; ++i)
    {
        states[i].sprite = SPR_TNT1;
        states[i].tics = -1;
        states[i].nextstate = i;
    }
}

//
//   Sprites
//

char **sprnames = NULL;
int num_sprites;
static char **deh_spritenames = NULL;
static byte *sprnames_state = NULL;

static void InitSprites(void)
{
    sprnames = original_sprnames;
    num_sprites = NUMSPRITES;

    array_grow(deh_spritenames, num_sprites);
    for (int i = 0; i < num_sprites; i++)
    {
        deh_spritenames[i] = strdup(sprnames[i]);
    }

    array_grow(sprnames_state, num_sprites);
    memset(sprnames_state, 0, num_sprites * sizeof(*sprnames_state));
}

static void EnsureSpritesCapacity(int limit)
{
    if (limit < num_sprites)
    {
        return;
    }

    const int old_num_sprites = num_sprites;

    static boolean first_allocation = true;
    if (first_allocation)
    {
        sprnames = NULL;
        array_grow(sprnames, old_num_sprites + limit);
        memcpy(sprnames, original_sprnames,
               old_num_sprites * sizeof(*sprnames));
        first_allocation = false;
    }
    else
    {
        array_grow(sprnames, limit);
    }

    num_sprites = array_capacity(sprnames);
    const int size_delta = num_sprites - old_num_sprites;
    memset(sprnames + old_num_sprites, 0, size_delta * sizeof(*sprnames));

    array_grow(sprnames_state, size_delta);
    memset(sprnames_state + old_num_sprites, 0,
           size_delta * sizeof(*sprnames_state));
}

static void FreeSprites(void)
{
    for (int i = 0; i < array_capacity(deh_spritenames); i++)
    {
        if (deh_spritenames[i])
        {
            free(deh_spritenames[i]);
        }
    }
    array_free(deh_spritenames);
    array_free(sprnames_state);
}

int dsdh_GetDehSpriteIndex(const char *key)
{
    for (int i = 0; i < num_sprites; ++i)
    {
        if (sprnames[i] && !strncasecmp(sprnames[i], key, 4)
            && !sprnames_state[i])
        {
            sprnames_state[i] = true; // sprite has been edited
            return i;
        }
    }

    return -1;
}

int dsdh_GetOriginalSpriteIndex(const char *key)
{
    int i;
    const char *c;

    for (i = 0; i < array_capacity(deh_spritenames); ++i)
    {
        if (deh_spritenames[i] && !strncasecmp(deh_spritenames[i], key, 4))
        {
            return i;
        }
    }

    // is it a number?
    for (c = key; *c; c++)
    {
        if (!isdigit(*c))
        {
            return -1;
        }
    }

    i = atoi(key);
    EnsureSpritesCapacity(i);

    return i;
}

//
//   SFX
//
#include "sounds.h"

sfxinfo_t *S_sfx = NULL;
int num_sfx;
static char **deh_soundnames = NULL;
static byte *sfx_state = NULL;

static void InitSFX(void)
{
    S_sfx = original_S_sfx;
    num_sfx = NUMSFX;

    array_grow(deh_soundnames, num_sfx);
    for (int i = 1; i < num_sfx; i++)
    {
        deh_soundnames[i] = S_sfx[i].name ? strdup(S_sfx[i].name) : NULL;
    }

    array_grow(sfx_state, num_sfx);
    memset(sfx_state, 0, num_sfx * sizeof(*sfx_state));
}

static void FreeSFX(void)
{
    for (int i = 1; i < array_capacity(deh_soundnames); i++)
    {
        if (deh_soundnames[i])
        {
            free(deh_soundnames[i]);
        }
    }
    array_free(deh_soundnames);
    array_free(sfx_state);
}

void dsdh_EnsureSFXCapacity(int limit)
{
    if (limit < num_sfx)
    {
        return;
    }

    const int old_num_sfx = num_sfx;

    static int first_allocation = true;
    if (first_allocation)
    {
        S_sfx = NULL;
        array_grow(S_sfx, old_num_sfx + limit);
        memcpy(S_sfx, original_S_sfx, old_num_sfx * sizeof(*S_sfx));
        first_allocation = false;
    }
    else
    {
        array_grow(S_sfx, limit);
    }

    num_sfx = array_capacity(S_sfx);
    const int size_delta = num_sfx - old_num_sfx;
    memset(S_sfx + old_num_sfx, 0, size_delta * sizeof(*S_sfx));

    array_grow(sfx_state, size_delta);
    memset(sfx_state + old_num_sfx, 0, size_delta * sizeof(*sfx_state));

    for (int i = old_num_sfx; i < num_sfx; ++i)
    {
        S_sfx[i].priority = 127;
        S_sfx[i].lumpnum = -1;
    }
}

int dsdh_GetDehSFXIndex(const char *key, size_t length)
{
    for (int i = 1; i < num_sfx; ++i)
    {
        if (S_sfx[i].name && strlen(S_sfx[i].name) == length
            && !strncasecmp(S_sfx[i].name, key, length) && !sfx_state[i])
        {
            sfx_state[i] = true; // sfx has been edited
            return i;
        }
    }

    return -1;
}

int dsdh_GetOriginalSFXIndex(const char *key)
{
    int i;
    const char *c;

    for (i = 1; i < array_capacity(deh_soundnames); ++i)
    {
        if (deh_soundnames[i] && !strncasecmp(deh_soundnames[i], key, 6))
        {
            return i;
        }
    }

    // is it a number?
    for (c = key; *c; c++)
    {
        if (!isdigit(*c))
        {
            return -1;
        }
    }

    i = atoi(key);
    dsdh_EnsureSFXCapacity(i);

    return i;
}

//
//   Music
//

musicinfo_t *S_music = NULL;
int num_music;
static byte *music_state = NULL;

static void InitMusic(void)
{
    S_music = original_S_music;
    num_music = NUMMUSIC;

    array_grow(music_state, num_music);
    memset(music_state, 0, num_music * sizeof(*music_state));
}

int dsdh_GetDehMusicIndex(const char *key, int length)
{
    for (int i = 1; i < num_music; ++i)
    {
        if (S_music[i].name && strlen(S_music[i].name) == length
            && !strncasecmp(S_music[i].name, key, length) && !music_state[i])
        {
            music_state[i] = true; // music has been edited
            return i;
        }
    }

    return -1;
}

static void FreeMusic(void)
{
    array_free(music_state);
}

//
//  Things
//
#include "p_map.h" // MELEERANGE

mobjinfo_t *mobjinfo = NULL;
int num_mobj_types;

static void InitMobjInfo(void)
{
    mobjinfo = original_mobjinfo;
    num_mobj_types = NUMMOBJTYPES;
}

void dsdh_EnsureMobjInfoCapacity(int limit)
{
    if (limit < num_mobj_types)
    {
        return;
    }

    const int old_num_mobj_types = num_mobj_types;

    static boolean first_allocation = true;
    if (first_allocation)
    {
        mobjinfo = NULL;
        array_grow(mobjinfo, old_num_mobj_types + limit);
        memcpy(mobjinfo, original_mobjinfo,
               old_num_mobj_types * sizeof(*mobjinfo));
        first_allocation = false;
    }
    else
    {
        array_grow(mobjinfo, limit);
    }

    num_mobj_types = array_capacity(mobjinfo);
    memset(mobjinfo + old_num_mobj_types, 0,
           (num_mobj_types - old_num_mobj_types) * sizeof(*mobjinfo));

    for (int i = old_num_mobj_types; i < num_mobj_types; ++i)
    {
        mobjinfo[i].droppeditem = MT_NULL;
        mobjinfo[i].infighting_group = IG_DEFAULT;
        mobjinfo[i].projectile_group = PG_DEFAULT;
        mobjinfo[i].splash_group = SG_DEFAULT;
        mobjinfo[i].altspeed = NO_ALTSPEED;
        mobjinfo[i].meleerange = MELEERANGE;
    }
}

void dsdh_InitTables(void)
{
    InitStates();
    InitSprites();
    InitSFX();
    InitMusic();
    InitMobjInfo();
}

void dsdh_FreeTables(void)
{
    FreeStates();
    FreeSprites();
    FreeSFX();
    FreeMusic();
}
