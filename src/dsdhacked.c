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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "info.h"
#include "i_system.h" // I_Realloc

//
//   States
//

state_t* states;
int num_states;
byte* defined_codeptr_args;
statenum_t* seenstate_tab;
actionf_t* deh_codeptr;

static void InitStates(void)
{
  int i;

  num_states = NUMSTATES;

  states = original_states;

  seenstate_tab = calloc(num_states, sizeof(*seenstate_tab));

  deh_codeptr = malloc(num_states * sizeof(*deh_codeptr));
  for (i = 0; i < num_states; i++)
    deh_codeptr[i] = states[i].action;

  defined_codeptr_args = calloc(num_states, sizeof(*defined_codeptr_args));
}

static void FreeStates(void)
{
  free(defined_codeptr_args);
  free(deh_codeptr);
}

void dsdh_EnsureStatesCapacity(int limit)
{
  int i;
  static boolean first_allocation = true;

  while (limit >= num_states)
  {
    int old_num_states = num_states;

    num_states *= 2;

    if (first_allocation)
    {
       first_allocation = false;
       states = malloc(num_states * sizeof(*states));
       memcpy(states, original_states, old_num_states * sizeof(*states));
    }
    else
    {
      states = I_Realloc(states, num_states * sizeof(*states));
    }
    memset(states + old_num_states, 0, (num_states - old_num_states) * sizeof(*states));

    deh_codeptr = I_Realloc(deh_codeptr, num_states * sizeof(*deh_codeptr));
    memset(deh_codeptr + old_num_states, 0, (num_states - old_num_states) * sizeof(*deh_codeptr));

    defined_codeptr_args =
      I_Realloc(defined_codeptr_args, num_states * sizeof(*defined_codeptr_args));
    memset(defined_codeptr_args + old_num_states, 0,
      (num_states - old_num_states) * sizeof(*defined_codeptr_args));

    seenstate_tab = I_Realloc(seenstate_tab, num_states * sizeof(*seenstate_tab));
    memset(seenstate_tab + old_num_states, 0,
      (num_states - old_num_states) * sizeof(*seenstate_tab));

    for (i = old_num_states; i < num_states; ++i)
    {
      states[i].sprite = SPR_TNT1;
      states[i].tics = -1;
      states[i].nextstate = i;
    }
  }
}

//
//   Sprites
//

char** sprnames;
int num_sprites;
static char** deh_spritenames;
static int deh_spritenames_size;
static byte* sprnames_state;

static void InitSprites(void)
{
  int i;

  sprnames = original_sprnames;

  num_sprites = NUMSPRITES;

  deh_spritenames_size = num_sprites + 1;
  deh_spritenames = malloc(deh_spritenames_size * sizeof(*deh_spritenames));
  for (i = 0; i < num_sprites; i++)
    deh_spritenames[i] = strdup(sprnames[i]);
  deh_spritenames[num_sprites] = NULL;

  sprnames_state = calloc(num_sprites, sizeof(*sprnames_state));
}

static void EnsureSpritesCapacity(int limit)
{
  static boolean first_allocation = true;

  while (limit >= num_sprites)
  {
    int old_num_sprites = num_sprites;

    num_sprites *= 2;

    if (first_allocation)
    {
      first_allocation = false;
      sprnames = malloc(num_sprites * sizeof(*sprnames));
      memcpy(sprnames, original_sprnames, old_num_sprites * sizeof(*sprnames));
    }
    else
    {
      sprnames = I_Realloc(sprnames, num_sprites * sizeof(*sprnames));
    }
    memset(sprnames + old_num_sprites, 0, (num_sprites - old_num_sprites) * sizeof(*sprnames));

    sprnames_state = I_Realloc(sprnames_state, num_sprites * sizeof(*sprnames_state));
    memset(sprnames_state + old_num_sprites, 0,
      (num_sprites - old_num_sprites) * sizeof(*sprnames_state));
  }
}

static void FreeSprites(void)
{
  int i;

  for (i = 0; i < deh_spritenames_size; i++)
  {
    if (deh_spritenames[i])
      free(deh_spritenames[i]);
  }
  free(deh_spritenames);
  free(sprnames_state);
}

int dsdh_GetDehSpriteIndex(const char* key)
{
  int i;

  for (i = 0; i < num_sprites; ++i)
  {
    if (sprnames[i] && !strncasecmp(sprnames[i], key, 4) && !sprnames_state[i])
    {
      sprnames_state[i] = true; // sprite has been edited
      return i;
    }
  }

  return -1;
}

int dsdh_GetOriginalSpriteIndex(const char* key)
{
  int i;
  const char* c;

  for (i = 0; deh_spritenames[i]; ++i)
    if (!strncasecmp(deh_spritenames[i], key, 4))
      return i;

  // is it a number?
  for (c = key; *c; c++)
    if (!isdigit(*c))
      return -1;

  i = atoi(key);
  EnsureSpritesCapacity(i);

  return i;
}

//
//   SFX
//
#include "sounds.h"

sfxinfo_t* S_sfx;
int num_sfx;
static char** deh_soundnames;
static int deh_soundnames_size;
static byte* sfx_state;

static void InitSFX(void)
{
  int i;

  S_sfx = original_S_sfx;

  num_sfx = NUMSFX;

  deh_soundnames_size = num_sfx + 1;
  deh_soundnames = malloc(deh_soundnames_size * sizeof(*deh_soundnames));
  for (i = 1; i < num_sfx; i++)
    if (S_sfx[i].name != NULL)
      deh_soundnames[i] = strdup(S_sfx[i].name);
    else
      deh_soundnames[i] = NULL;
  deh_soundnames[0] = NULL;
  deh_soundnames[num_sfx] = NULL;

  sfx_state = calloc(num_sfx, sizeof(*sfx_state));
}

static void FreeSFX(void)
{
  int i;

  for (i = 1; i < deh_soundnames_size; i++)
  {
    if (deh_soundnames[i])
      free(deh_soundnames[i]);
  }
  free(deh_soundnames);
  free(sfx_state);
}

void dsdh_EnsureSFXCapacity(int limit)
{
  int i;
  static int first_allocation = true;

  while (limit >= num_sfx)
  {
    int old_num_sfx = num_sfx;

    num_sfx *= 2;

    if (first_allocation)
    {
      first_allocation = false;
      S_sfx = malloc(num_sfx * sizeof(*S_sfx));
      memcpy(S_sfx, original_S_sfx, old_num_sfx * sizeof(*S_sfx));
    }
    else
    {
      S_sfx = I_Realloc(S_sfx, num_sfx * sizeof(*S_sfx));
    }
    memset(S_sfx + old_num_sfx, 0, (num_sfx - old_num_sfx) * sizeof(*S_sfx));

    sfx_state = I_Realloc(sfx_state, num_sfx * sizeof(*sfx_state));
    memset(sfx_state + old_num_sfx, 0,
      (num_sfx - old_num_sfx) * sizeof(*sfx_state));

    for (i = old_num_sfx; i < num_sfx; ++i)
    {
      S_sfx[i].priority = 127;
      S_sfx[i].pitch = -1;
      S_sfx[i].volume = -1;
      S_sfx[i].lumpnum = -1;
    }
  }
}

int dsdh_GetDehSFXIndex(const char* key, size_t length)
{
  int i;

  for (i = 1; i < num_sfx; ++i)
  {
    if (S_sfx[i].name &&
        strlen(S_sfx[i].name) == length &&
        !strncasecmp(S_sfx[i].name, key, length) &&
        !sfx_state[i])
    {
      sfx_state[i] = true; // sfx has been edited
      return i;
    }
  }

  return -1;
}

int dsdh_GetOriginalSFXIndex(const char* key)
{
  int i;
  const char* c;

  for (i = 1; deh_soundnames[i]; ++i)
    if (!strncasecmp(deh_soundnames[i], key, 6))
      return i;

  // is it a number?
  for (c = key; *c; c++)
    if (!isdigit(*c))
      return -1;

  i = atoi(key);
  dsdh_EnsureSFXCapacity(i);

  return i;
}

//
//   Music
//

musicinfo_t *S_music;
int num_music;
static byte *music_state;

static void InitMusic(void)
{
  S_music = original_S_music;
  num_music = NUMMUSIC;

  music_state = calloc(num_music, sizeof(*music_state));
}

int dsdh_GetDehMusicIndex(const char* key, int length)
{
  int i;

  for (i = 1; i < num_music; ++i)
  {
    if (S_music[i].name &&
      strlen(S_music[i].name) == length &&
      !strncasecmp(S_music[i].name, key, length) &&
      !music_state[i])
    {
      music_state[i] = true; // music has been edited
      return i;
    }
  }

  return -1;
}

static void FreeMusic(void)
{
  free(music_state);
}

//
//  Things
//
#include "p_map.h" // MELEERANGE

mobjinfo_t* mobjinfo;
int num_mobj_types;

static void InitMobjInfo(void)
{
  num_mobj_types = NUMMOBJTYPES;

  mobjinfo = original_mobjinfo;
}

void dsdh_EnsureMobjInfoCapacity(int limit)
{
  int i;
  static boolean first_allocation = true;

  while (limit >= num_mobj_types)
  {
    int old_num_mobj_types = num_mobj_types;

    num_mobj_types *= 2;

    if (first_allocation)
    {
      first_allocation = false;
      mobjinfo = malloc(num_mobj_types * sizeof(*mobjinfo));
      memcpy(mobjinfo, original_mobjinfo, old_num_mobj_types * sizeof(*mobjinfo));
    }
    else
    {
      mobjinfo = I_Realloc(mobjinfo, num_mobj_types * sizeof(*mobjinfo));
    }
    memset(mobjinfo + old_num_mobj_types, 0,
      (num_mobj_types - old_num_mobj_types) * sizeof(*mobjinfo));

    for (i = old_num_mobj_types; i < num_mobj_types; ++i)
    {
      mobjinfo[i].droppeditem = MT_NULL;
      mobjinfo[i].infighting_group = IG_DEFAULT;
      mobjinfo[i].projectile_group = PG_DEFAULT;
      mobjinfo[i].splash_group = SG_DEFAULT;
      mobjinfo[i].altspeed = NO_ALTSPEED;
      mobjinfo[i].meleerange = MELEERANGE;
    }
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
