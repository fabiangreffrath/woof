//
//  Copyright(C) 2017 Christoph Oelckers
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

#ifndef G_UMAPINFO_H
#define G_UMAPINFO_H

#include "doomtype.h"
#include "d_player.h"
#include "doomtype.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "wi_stuff.h"

typedef enum
{
    MapInfo_LabelClear = (1u << 0),

    MapInfo_EndGameArt = (1u << 2),
    MapInfo_EndGameStandard = (1u << 3),
    MapInfo_EndGameCast = (1u << 4),
    MapInfo_EndGameBunny = (1u << 5),
    MapInfo_EndGameCustomFinale = (1u << 6),
    MapInfo_EndGame = (MapInfo_EndGameArt | MapInfo_EndGameStandard
                       | MapInfo_EndGameCast | MapInfo_EndGameBunny),
    MapInfo_EndGameClear = (1u << 7),

    MapInfo_NoIntermission = (1u << 8),
    MapInfo_InterTextClear = (1u << 9),
    MapInfo_InterTextSecretClear = (1u << 10),

    MapInfo_BossActionClear = (1u << 11)
} mapinfo_flags_t;

typedef struct
{
    int type;
    int special;
    int tag;
} bossaction_t;

typedef struct mapentry_s
{
    char *lumpname;
    char *levelname;
    char *label;
    char *intertext;
    char *intertextsecret;
    char *author;
    char levelpic[9];
    char nextmap[9];
    char nextsecret[9];
    char music[9];
    char skytexture[9];
    char endpic[9];
    char endfinale[9];
    char exitpic[9];
    char enterpic[9];
    char exitanim[9];
    char enteranim[9];
    char interbackdrop[9];
    char intermusic[9];
    int partime;
    bossaction_t *bossactions;
    mapinfo_flags_t flags;
} mapentry_t;

extern mapentry_t *umapinfo;

extern boolean EpiCustom;

boolean G_ValidateMapName(const char *mapname, int *episode, int *map);

void G_ParseMapInfo(int lumpnum);

boolean G_IsSecretMap(int episode, int map);

//
// Abstract away map information calls
//

typedef enum MI_ShowNext_e
{
    WI_ShowNextLoc = (1u << 0),
    WI_ShowNextDone = (1u << 1),
    WI_ShowNextEpisodal = (1u << 2),
} MI_ShowNext_t;

typedef enum MI_Completion_e
{
    DC_Victory = (1u << 0),
} MI_Completion_t;

typedef enum MI_WinDisplay_e
{
    WD_Victory = (1u << 0),
    WD_StartFinale = (1u << 1),
} MI_WinDisplay_t;

mapentry_t *MI_MapEntry(int episode, int map);
void MI_UpdateGameMap(int epi, int map);
void MI_UpdateLastMapInfo(wbstartstruct_t *wminfo);
void MI_UpdateNextMapInfo(wbstartstruct_t *wminfo);
void MI_NextMap(int *episode, int *map);
MI_ShowNext_t MI_ShowNextLoc(void);
boolean MI_SkipShowNextLoc(void);
boolean MI_BossAction(mobj_t *mo, line_t *junk, thinker_t **th);
const char *MI_GetLevelTitle(void);
int MI_SkyTexture(void);
int MI_PrepareIntermission(wbstartstruct_t *wminfo);
int MI_PrepareFinale(void);
void MI_WI_Start(wbstartstruct_t *wbs, const char **exitpic,
                 const char **enterpic, wi_animation_t **animation);
void MI_MapAnnouncement(char announce_string[120], char author_string[120],
                        const char string[120], size_t str_size);
void MI_Spechits(line_t *dummy, int *speciallines, boolean *trigger_keen);
void MI_ChangeMusic(void);

#endif
