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

#include "g_umapinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "mn_menu.h"
#include "w_wad.h"
#include "z_zone.h"

// allow no 0-tag specials here, unless a level exit.
#define UMAPINFO_BOSS_SPECIAL \
    (tag != 0 \
    || special == 11 || special == 51 || special == 52 || special == 124 \
    || special == 2069 || special == 2070 || special == 2071 \
    || special == 2072 || special == 2073 || special == 2074)

mapentry_t *umapinfo = NULL;

static level_t *secretlevels;

static void ReplaceString(char **to, const char *from)
{
    if (*to != NULL)
    {
        free(*to);
    }
    *to = M_StringDuplicate(from);
}

static void FreeMapEntry(mapentry_t *mape)
{
    if (mape->levelname)
    {
        free(mape->levelname);
    }
    if (mape->label)
    {
        free(mape->label);
    }
    if (mape->intertext)
    {
        free(mape->intertext);
    }
    if (mape->intertextsecret)
    {
        free(mape->intertextsecret);
    }
    if (mape->author)
    {
        free(mape->author);
    }
    array_free(mape->bossactions);
    memset(mape, 0, sizeof(*mape));
}

// Parses a set of string and concatenates them
// Returns a pointer to the string (must be freed)

static char *ParseMultiString(scanner_t *s)
{
    char *build = NULL;

    do
    {
        SC_MustGetToken(s, TK_StringConst);
        if (build == NULL)
        {
            build = M_StringDuplicate(SC_GetString(s));
        }
        else
        {
            char *tmp = build;
            build = M_StringJoin(tmp, "\n", SC_GetString(s));
            free(tmp);
        }
    } while (SC_CheckToken(s, ','));

    return build;
}

// Parses a lump name. The buffer must be at least 9 characters.

static void ParseLumpName(scanner_t *s, char *buffer)
{
    SC_MustGetToken(s, TK_StringConst);
    if (strlen(SC_GetString(s)) > 8)
    {
        SC_Error(s, "String too long. Maximum size is 8 characters.");
    }
    strncpy(buffer, SC_GetString(s), 8);
    buffer[8] = 0;
    M_StringToUpper(buffer);
}

// Parses a standard property that is already known
// These do not get stored in the property list
// but in dedicated struct member variables.

static void ParseStandardProperty(scanner_t *s, mapentry_t *mape)
{
    SC_MustGetToken(s, TK_Identifier);
    char *prop = M_StringDuplicate(SC_GetString(s));

    SC_MustGetToken(s, '=');
    if (!strcasecmp(prop, "levelname"))
    {
        SC_MustGetToken(s, TK_StringConst);
        ReplaceString(&mape->levelname, SC_GetString(s));
    }
    else if (!strcasecmp(prop, "label"))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(SC_GetString(s), "clear"))
            {
                mape->flags |= MapInfo_LabelClear;
            }
            else
            {
                SC_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else
        {
            mape->flags &= ~MapInfo_LabelClear;
            SC_MustGetToken(s, TK_StringConst);
            ReplaceString(&mape->label, SC_GetString(s));
        }
    }
    else if (!strcasecmp(prop, "author"))
    {
        SC_MustGetToken(s, TK_StringConst);
        ReplaceString(&mape->author, SC_GetString(s));
    }
    else if (!strcasecmp(prop, "episode"))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(SC_GetString(s), "clear"))
            {
                MN_ClearEpisodes();
            }
            else
            {
                SC_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else
        {
            char lumpname[9] = {0};
            char *alttext = NULL;
            char key = 0;

            ParseLumpName(s, lumpname);
            if (SC_CheckToken(s, ','))
            {
                SC_MustGetToken(s, TK_StringConst);
                alttext = M_StringDuplicate(SC_GetString(s));
                if (SC_CheckToken(s, ','))
                {
                    SC_MustGetToken(s, TK_StringConst);
                    const char *tmp = SC_GetString(s);
                    key = M_ToLower(tmp[0]);
                }
            }

            MN_AddEpisode(mape->mapname, lumpname, alttext, key);

            if (alttext)
            {
                free(alttext);
            }
        }
    }
    else if (!strcasecmp(prop, "next"))
    {
        ParseLumpName(s, mape->nextmap);
        if (!G_ValidateMapName(mape->nextmap, NULL, NULL))
        {
            SC_Error(s, "Invalid map name %s.", mape->nextmap);
        }
    }
    else if (!strcasecmp(prop, "nextsecret"))
    {
        ParseLumpName(s, mape->nextsecret);
        level_t level = {0};
        if (!G_ValidateMapName(mape->nextsecret, &level.episode, &level.map))
        {
            SC_Error(s, "Invalid map name %s", mape->nextsecret);
        }
        array_push(secretlevels, level);
    }
    else if (!strcasecmp(prop, "levelpic"))
    {
        ParseLumpName(s, mape->levelpic);
    }
    else if (!strcasecmp(prop, "skytexture"))
    {
        ParseLumpName(s, mape->skytexture);
    }
    else if (!strcasecmp(prop, "music"))
    {
        ParseLumpName(s, mape->music);
    }
    else if (!strcasecmp(prop, "endpic"))
    {
        mape->flags |= MapInfo_EndGameArt;
        ParseLumpName(s, mape->endpic);
    }
    else if (!strcasecmp(prop, "endcast"))
    {
        SC_MustGetToken(s, TK_BoolConst);
        if (SC_GetBoolean(s))
        {
            mape->flags |= MapInfo_EndGameCast;
        }
        else
        {
            mape->flags &= ~MapInfo_EndGameCast;
            mape->flags |= MapInfo_EndGameClear;
        }
    }
    else if (!strcasecmp(prop, "endbunny"))
    {
        SC_MustGetToken(s, TK_BoolConst);
        if (SC_GetBoolean(s))
        {
            mape->flags |= MapInfo_EndGameBunny;
        }
        else
        {
            mape->flags &= ~MapInfo_EndGameBunny;
            mape->flags |= MapInfo_EndGameClear;
        }
    }
    else if (!strcasecmp(prop, "endgame"))
    {
        SC_MustGetToken(s, TK_BoolConst);
        if (SC_GetBoolean(s))
        {
            mape->flags |= MapInfo_EndGameStandard;
        }
        else
        {
            mape->flags &= ~MapInfo_EndGameStandard;
            mape->flags |= MapInfo_EndGameClear;
        }
    }
    else if (!strcasecmp(prop, "exitpic"))
    {
        ParseLumpName(s, mape->exitpic);
    }
    else if (!strcasecmp(prop, "enterpic"))
    {
        ParseLumpName(s, mape->enterpic);
    }
    else if (!strcasecmp(prop, "exitanim"))
    {
        ParseLumpName(s, mape->exitanim);
    }
    else if (!strcasecmp(prop, "enteranim"))
    {
        ParseLumpName(s, mape->enteranim);
    }
    else if (!strcasecmp(prop, "nointermission"))
    {
        SC_MustGetToken(s, TK_BoolConst);
        if (SC_GetBoolean(s))
        {
            mape->flags |= MapInfo_NoIntermission;
        }
        else
        {
            mape->flags &= ~MapInfo_NoIntermission;
        }
    }
    else if (!strcasecmp(prop, "partime"))
    {
        SC_MustGetToken(s, TK_IntConst);
        mape->partime = SC_GetNumber(s);
    }
    else if (!strcasecmp(prop, "intertext"))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(SC_GetString(s), "clear"))
            {
                mape->flags |= MapInfo_InterTextClear;
            }
            else
            {
                SC_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else
        {
            mape->flags &= ~MapInfo_InterTextClear;
            if (mape->intertext)
            {
                free(mape->intertext);
            }
            mape->intertext = ParseMultiString(s);
        }
    }
    else if (!strcasecmp(prop, "intertextsecret"))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(SC_GetString(s), "clear"))
            {
                mape->flags |= MapInfo_InterTextSecretClear;
            }
            else
            {
                SC_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else
        {
            mape->flags &= ~MapInfo_InterTextSecretClear;
            if (mape->intertextsecret)
            {
                free(mape->intertextsecret);
            }
            mape->intertextsecret = ParseMultiString(s);
        }
    }
    else if (!strcasecmp(prop, "interbackdrop"))
    {
        ParseLumpName(s, mape->interbackdrop);
    }
    else if (!strcasecmp(prop, "intermusic"))
    {
        ParseLumpName(s, mape->intermusic);
    }
    else if (!strcasecmp(prop, "bossaction"))
    {
        if (SC_CheckToken(s, TK_Identifier) &&
            !strcasecmp(SC_GetString(s), "clear"))
        {
            mape->flags |= MapInfo_BossActionClear;
            array_free(mape->bossactions);
        }
        else
        {
            mape->flags &= ~MapInfo_BossActionClear;
            int  type = -1, special, tag;

            if (SC_CheckToken(s, TK_IntConst))
            {
                // DeHackEd Type vs mobjtype off by one
                type = SC_GetNumber(s) - 1;

                // ID24HACKED
                //   Invalid index 0xFFFFFFFF, Negative indices not supported
                if (type <= -1)
                {
                    SC_Error(s, "bossaction: invalid negative thing type");
                }
            }
            else
            {
                for (type = 0; type < num_mobj_types; ++type)
                {
                    if (mobjinfo[type].mnemonic != NULL
                        && !strcasecmp(SC_GetString(s), mobjinfo[type].mnemonic))
                    {
                        break;
                    }
                }

                int temp = -1;
                if (M_StringStartsWith(SC_GetString(s), "Deh_Actor_") &&
                    sscanf(SC_GetString(s), "Deh_Actor_%d", &temp) == 1)
                {
                    type = temp;
                }

                if (type >= num_mobj_types)
                {
                    SC_Error(s, "bossaction: unknown thing '%s'",
                            SC_GetString(s));
                }
            }

            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            special = SC_GetNumber(s);
            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            tag = SC_GetNumber(s);

            if (UMAPINFO_BOSS_SPECIAL)
            {
                bossaction_t bossaction = {type, special, tag};
                array_push(mape->bossactions, bossaction);
            }
        }
    }
    else if (!strcasecmp(prop, "bossactionednum"))
    {
        if (SC_CheckToken(s, TK_Identifier) &&
            !strcasecmp(SC_GetString(s), "clear"))
        {
            mape->flags |= MapInfo_BossActionClear;
            array_free(mape->bossactions);
        }
        else
        {
            mape->flags &= ~MapInfo_BossActionClear;
            int doomednum, type, special, tag;

            SC_MustGetToken(s, TK_IntConst);
            doomednum = SC_GetNumber(s);

            // ID24HACKED
            //   Invalid index 0xFFFF, negative indices not supported
            if (doomednum <= -1)
            {
                SC_Error(s, "bossaction: invalid negative doomednum");
            }

            for (type = 0; type < num_mobj_types; ++type)
            {
                if (mobjinfo[type].doomednum == doomednum)
                {
                    break;
                }
            }

            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            special = SC_GetNumber(s);
            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            tag = SC_GetNumber(s);

            if (UMAPINFO_BOSS_SPECIAL)
            {
                bossaction_t bossaction = {type, special, tag};
                array_push(mape->bossactions, bossaction);
            }
        }
    }
    // If no known property name was given, skip all comma-separated values
    // after the = sign
    else
    {
        do
        {
            SC_GetNextToken(s, true);
        } while (SC_CheckToken(s, ','));
    }

    free(prop);
}

static void ParseMapEntry(scanner_t *s, mapentry_t *entry)
{
    SC_MustGetToken(s, TK_Identifier);
    if (strcasecmp(SC_GetString(s), "map"))
    {
        SC_Error(s, "Expected 'map' but got '%s' instead", SC_GetString(s));
    }

    SC_MustGetToken(s, TK_Identifier);
    if (!G_ValidateMapName(SC_GetString(s), NULL, NULL))
    {
        SC_Error(s, "Invalid map name %s", SC_GetString(s));
    }
    ReplaceString(&entry->mapname, SC_GetString(s));

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        ParseStandardProperty(s, entry);
    }
}

void G_ParseMapInfo(int lumpnum)
{
    scanner_t *s = SC_Open("UMAPINFO", W_CacheLumpNum(lumpnum, PU_CACHE),
                           W_LumpLength(lumpnum));
    while (SC_TokensLeft(s))
    {
        mapentry_t parsed = {0};
        ParseMapEntry(s, &parsed);

        // Set default level progression here to simplify the checks elsewhere.
        // Doing this lets us skip all normal code for this if nothing has been
        // defined.
        if (parsed.flags & MapInfo_EndGame)
        {
            parsed.nextmap[0] = 0;
        }
        else if (!parsed.nextmap[0] && !(parsed.flags & MapInfo_EndGameClear))
        {
            if (!strcasecmp(parsed.mapname, "MAP30"))
            {
                parsed.flags |= MapInfo_EndGameCast;
            }
            else if (!strcasecmp(parsed.mapname, "E1M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, gamemode == retail && !pwad_help2 ? "CREDIT" : "HELP2");
            }
            else if (!strcasecmp(parsed.mapname, "E2M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, "VICTORY2");
            }
            else if (!strcasecmp(parsed.mapname, "E3M8"))
            {
                parsed.flags |= MapInfo_EndGameBunny;
            }
            else if (!strcasecmp(parsed.mapname, "E4M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, "ENDPIC");
            }
            else
            {
                int ep, map;
                if (G_ValidateMapName(parsed.mapname, &ep, &map))
                {
                    M_CopyLumpName(parsed.nextmap, MapName(ep, map + 1));
                }
            }
        }

        // Does this entry already exist? If yes, replace it.
        int i;
        for (i = 0; i < array_size(umapinfo); ++i)
        {
            if (!strcmp(parsed.mapname, umapinfo[i].mapname))
            {
                FreeMapEntry(&umapinfo[i]);
                umapinfo[i] = parsed;
                break;
            }
        }
        // Not found so create a new one.
        if (i == array_size(umapinfo))
        {
            array_push(umapinfo, parsed);
        }
    }

    SC_Close(s);
}

mapentry_t *G_LookupMapinfo(int episode, int map)
{
    char lumpname[9] = {0};
    M_StringCopy(lumpname, MapName(episode, map), sizeof(lumpname));

    mapentry_t *entry;
    array_foreach(entry, umapinfo)
    {
        if (!strcasecmp(lumpname, entry->mapname))
        {
            return entry;
        }
    }

    return NULL;
}

// Check if the given map name can be expressed as a gameepisode/gamemap pair
// and be reconstructed from it.

boolean G_ValidateMapName(const char *mapname, int *episode, int *map)
{
    if (strlen(mapname) > 8)
    {
        return false;
    }

    char lumpname[9], mapuname[9];
    int e = -1, m = -1;

    M_StringCopy(mapuname, mapname, 8);
    mapuname[8] = 0;
    M_StringToUpper(mapuname);

    if (gamemode != commercial)
    {
        if (sscanf(mapuname, "E%dM%d", &e, &m) != 2)
        {
            return false;
        }
        M_CopyLumpName(lumpname, MapName(e, m));
    }
    else
    {
        if (sscanf(mapuname, "MAP%d", &m) != 1)
        {
            return false;
        }
        M_CopyLumpName(lumpname, MapName(e = 1, m));
    }

    if (episode)
    {
        *episode = e;
    }
    if (map)
    {
        *map = m;
    }

    return strcmp(mapuname, lumpname) == 0;
}

boolean G_IsSecretMap(int episode, int map)
{
    level_t *level;
    array_foreach(level, secretlevels)
    {
        if (level->episode == episode && level->map == map)
        {
            return true;
        }
    }
    return false;
}
