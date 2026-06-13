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

#include "d_player.h"
#include "deh_bex_partimes.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dsdh_main.h"
#include "g_game.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "m_swap.h"
#include "mn_menu.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

mapentry_t *umapinfo = NULL;

static level_t *secretlevels;

static const char *const actor_names[] =
{
    "DoomPlayer", "ZombieMan", "ShotgunGuy", "Archvile", "ArchvileFire",
    "Revenant", "RevenantTracer", "RevenantTracerSmoke", "Fatso", "FatShot",
    "ChaingunGuy", "DoomImp", "Demon", "Spectre", "Cacodemon", "BaronOfHell",
    "BaronBall", "HellKnight", "LostSoul", "SpiderMastermind", "Arachnotron",
    "Cyberdemon", "PainElemental", "WolfensteinSS", "CommanderKeen",
    "BossBrain", "BossEye", "BossTarget", "SpawnShot", "SpawnFire",
    "ExplosiveBarrel", "DoomImpBall", "CacodemonBall", "Rocket", "PlasmaBall",
    "BFGBall", "ArachnotronPlasma", "BulletPuff", "Blood", "TeleportFog",
    "ItemFog", "TeleportDest", "BFGExtra", "GreenArmor", "BlueArmor",
    "HealthBonus", "ArmorBonus", "BlueCard", "RedCard", "YellowCard",
    "YellowSkull", "RedSkull", "BlueSkull", "Stimpack", "Medikit", "Soulsphere",
    "InvulnerabilitySphere", "Berserk", "BlurSphere", "RadSuit", "Allmap",
    "Infrared", "Megasphere", "Clip", "ClipBox", "RocketAmmo", "RocketBox",
    "Cell", "CellPack", "Shell", "ShellBox", "Backpack", "BFG9000", "Chaingun",
    "Chainsaw", "RocketLauncher", "PlasmaRifle", "Shotgun", "SuperShotgun",
    "TechLamp", "TechLamp2", "Column", "TallGreenColumn", "ShortGreenColumn",
    "TallRedColumn", "ShortRedColumn", "SkullColumn", "HeartColumn", "EvilEye",
    "FloatingSkull", "TorchTree", "BlueTorch", "GreenTorch", "RedTorch",
    "ShortBlueTorch", "ShortGreenTorch", "ShortRedTorch", "Stalagtite",
    "TechPillar", "CandleStick", "Candelabra", "BloodyTwitch", "Meat2", "Meat3",
    "Meat4", "Meat5", "NonsolidMeat2", "NonsolidMeat4", "NonsolidMeat3",
    "NonsolidMeat5", "NonsolidTwitch", "DeadCacodemon", "DeadMarine",
    "DeadZombieMan", "DeadDemon", "DeadLostSoul", "DeadDoomImp",
    "DeadShotgunGuy", "GibbedMarine", "GibbedMarineExtra", "HeadsOnAStick",
    "Gibs", "HeadOnAStick", "HeadCandles", "DeadStick", "LiveStick", "BigTree",
    "BurningBarrel", "HangNoGuts", "HangBNoBrain", "HangTLookingDown",
    "HangTSkull", "HangTLookingUp", "HangTNoBrain", "ColonGibs",
    "SmallBloodPool", "BrainStem",
    // Boom/MBF additions
    "PointPusher", "PointPuller", "MBFHelperDog", "PlasmaBall1", "PlasmaBall2",
    "EvilSceptre", "UnholyBible", "MusicChanger", "Deh_Actor_145",
    "Deh_Actor_146", "Deh_Actor_147", "Deh_Actor_148", "Deh_Actor_149",
    // DEHEXTRA Actors start here
    "Deh_Actor_150", // Extra thing 0
    "Deh_Actor_151", // Extra thing 1
    "Deh_Actor_152", // Extra thing 2
    "Deh_Actor_153", // Extra thing 3
    "Deh_Actor_154", // Extra thing 4
    "Deh_Actor_155", // Extra thing 5
    "Deh_Actor_156", // Extra thing 6
    "Deh_Actor_157", // Extra thing 7
    "Deh_Actor_158", // Extra thing 8
    "Deh_Actor_159", // Extra thing 9
    "Deh_Actor_160", // Extra thing 10
    "Deh_Actor_161", // Extra thing 11
    "Deh_Actor_162", // Extra thing 12
    "Deh_Actor_163", // Extra thing 13
    "Deh_Actor_164", // Extra thing 14
    "Deh_Actor_165", // Extra thing 15
    "Deh_Actor_166", // Extra thing 16
    "Deh_Actor_167", // Extra thing 17
    "Deh_Actor_168", // Extra thing 18
    "Deh_Actor_169", // Extra thing 19
    "Deh_Actor_170", // Extra thing 20
    "Deh_Actor_171", // Extra thing 21
    "Deh_Actor_172", // Extra thing 22
    "Deh_Actor_173", // Extra thing 23
    "Deh_Actor_174", // Extra thing 24
    "Deh_Actor_175", // Extra thing 25
    "Deh_Actor_176", // Extra thing 26
    "Deh_Actor_177", // Extra thing 27
    "Deh_Actor_178", // Extra thing 28
    "Deh_Actor_179", // Extra thing 29
    "Deh_Actor_180", // Extra thing 30
    "Deh_Actor_181", // Extra thing 31
    "Deh_Actor_182", // Extra thing 32
    "Deh_Actor_183", // Extra thing 33
    "Deh_Actor_184", // Extra thing 34
    "Deh_Actor_185", // Extra thing 35
    "Deh_Actor_186", // Extra thing 36
    "Deh_Actor_187", // Extra thing 37
    "Deh_Actor_188", // Extra thing 38
    "Deh_Actor_189", // Extra thing 39
    "Deh_Actor_190", // Extra thing 40
    "Deh_Actor_191", // Extra thing 41
    "Deh_Actor_192", // Extra thing 42
    "Deh_Actor_193", // Extra thing 43
    "Deh_Actor_194", // Extra thing 44
    "Deh_Actor_195", // Extra thing 45
    "Deh_Actor_196", // Extra thing 46
    "Deh_Actor_197", // Extra thing 47
    "Deh_Actor_198", // Extra thing 48
    "Deh_Actor_199", // Extra thing 49
    "Deh_Actor_200", // Extra thing 50
    "Deh_Actor_201", // Extra thing 51
    "Deh_Actor_202", // Extra thing 52
    "Deh_Actor_203", // Extra thing 53
    "Deh_Actor_204", // Extra thing 54
    "Deh_Actor_205", // Extra thing 55
    "Deh_Actor_206", // Extra thing 56
    "Deh_Actor_207", // Extra thing 57
    "Deh_Actor_208", // Extra thing 58
    "Deh_Actor_209", // Extra thing 59
    "Deh_Actor_210", // Extra thing 60
    "Deh_Actor_211", // Extra thing 61
    "Deh_Actor_212", // Extra thing 62
    "Deh_Actor_213", // Extra thing 63
    "Deh_Actor_214", // Extra thing 64
    "Deh_Actor_215", // Extra thing 65
    "Deh_Actor_216", // Extra thing 66
    "Deh_Actor_217", // Extra thing 67
    "Deh_Actor_218", // Extra thing 68
    "Deh_Actor_219", // Extra thing 69
    "Deh_Actor_220", // Extra thing 70
    "Deh_Actor_221", // Extra thing 71
    "Deh_Actor_222", // Extra thing 72
    "Deh_Actor_223", // Extra thing 73
    "Deh_Actor_224", // Extra thing 74
    "Deh_Actor_225", // Extra thing 75
    "Deh_Actor_226", // Extra thing 76
    "Deh_Actor_227", // Extra thing 77
    "Deh_Actor_228", // Extra thing 78
    "Deh_Actor_229", // Extra thing 79
    "Deh_Actor_230", // Extra thing 80
    "Deh_Actor_231", // Extra thing 81
    "Deh_Actor_232", // Extra thing 82
    "Deh_Actor_233", // Extra thing 83
    "Deh_Actor_234", // Extra thing 84
    "Deh_Actor_235", // Extra thing 85
    "Deh_Actor_236", // Extra thing 86
    "Deh_Actor_237", // Extra thing 87
    "Deh_Actor_238", // Extra thing 88
    "Deh_Actor_239", // Extra thing 89
    "Deh_Actor_240", // Extra thing 90
    "Deh_Actor_241", // Extra thing 91
    "Deh_Actor_242", // Extra thing 92
    "Deh_Actor_243", // Extra thing 93
    "Deh_Actor_244", // Extra thing 94
    "Deh_Actor_245", // Extra thing 95
    "Deh_Actor_246", // Extra thing 96
    "Deh_Actor_247", // Extra thing 97
    "Deh_Actor_248", // Extra thing 98
    "Deh_Actor_249", // Extra thing 99
};

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

            MN_AddEpisode(mape->lumpname, lumpname, alttext, key);

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
    else if (!strcasecmp(prop, "endfinale"))
    {
        mape->flags |= MapInfo_EndGameCustomFinale;
        ParseLumpName(s, mape->endfinale);
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
        mape->partime = SC_GetNumber(s) * TICRATE;
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
        SC_MustGetToken(s, TK_Identifier);
        if (!strcasecmp(SC_GetString(s), "clear"))
        {
            mape->flags |= MapInfo_BossActionClear;
            array_free(mape->bossactions);
        }
        else
        {
            mape->flags &= ~MapInfo_BossActionClear;
            int type, special, tag;
            for (type = 0; type < arrlen(actor_names); ++type)
            {
                if (!strcasecmp(SC_GetString(s), actor_names[type]))
                {
                    break;
                }
            }
            if (type == arrlen(actor_names))
            {
                SC_Error(s, "bossaction: unknown thing type '%s'",
                         SC_GetString(s));
            }
            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            special = SC_GetNumber(s);
            SC_MustGetToken(s, ',');
            SC_MustGetToken(s, TK_IntConst);
            tag = SC_GetNumber(s);
            // allow no 0-tag specials here, unless a level exit.
            if (tag != 0 || special == 11 || special == 51 || special == 52
                || special == 124 || special == 2069 || special == 2070
                || special == 2071 || special == 2072 || special == 2073
                || special == 2074)
            {
                type = DSDH_ThingTranslate(type);
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
    ReplaceString(&entry->lumpname, SC_GetString(s));

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
            if (!strcasecmp(parsed.lumpname, "MAP30"))
            {
                parsed.flags |= MapInfo_EndGameCast;
            }
            else if (!strcasecmp(parsed.lumpname, "E1M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, gamemode == retail && !pwad_help2 ? "CREDIT" : "HELP2");
            }
            else if (!strcasecmp(parsed.lumpname, "E2M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, "VICTORY2");
            }
            else if (!strcasecmp(parsed.lumpname, "E3M8"))
            {
                parsed.flags |= MapInfo_EndGameBunny;
            }
            else if (!strcasecmp(parsed.lumpname, "E4M8"))
            {
                parsed.flags |= MapInfo_EndGameArt;
                M_CopyLumpName(parsed.endpic, "ENDPIC");
            }
            else
            {
                int ep, map;
                if (G_ValidateMapName(parsed.lumpname, &ep, &map))
                {
                    M_CopyLumpName(parsed.nextmap, MapName(ep, map + 1));
                }
            }
        }

        // Does this entry already exist? If yes, replace it.
        int i;
        for (i = 0; i < array_size(umapinfo); ++i)
        {
            if (!strcmp(parsed.lumpname, umapinfo[i].lumpname))
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

//
// Abstract away map information calls
//

mapentry_t *MI_MapEntry(int episode, int map)
{
    char lumpname[9] = {0};
    M_StringCopy(lumpname, MapName(episode, map), sizeof(lumpname));

    mapentry_t *entry;
    array_foreach(entry, umapinfo)
    {
        if (!strcasecmp(lumpname, entry->lumpname))
        {
            return entry;
        }
    }

    return NULL;
}

void MI_UpdateGameMap(int epi, int map)
{
  gameepisode = epi;
  gamemap = map;
  gamemapinfo = MI_MapEntry(gameepisode, gamemap);
}

void MI_UpdateLastMapInfo(wbstartstruct_t *wminfo)
{
    wminfo->lastmapinfo = gamemapinfo;
    wminfo->nextmapinfo = NULL;
}

void MI_UpdateNextMapInfo(wbstartstruct_t *wminfo)
{
    wminfo->nextmapinfo = MI_MapEntry(wminfo->nextep + 1, wminfo->next + 1);
}

void MI_NextMap(int *episode, int *map)
{
    // UMAPINFO
    if (gamemapinfo)
    {
        const char *next = NULL;

        if (gamemapinfo->nextsecret[0])
        {
            next = gamemapinfo->nextsecret;
        }
        else if (gamemapinfo->nextmap[0])
        {
            next = gamemapinfo->nextmap;
        }

        if (next)
        {
            G_ValidateMapName(next, episode, map);
        }
        return;
    }

    // Legacy
    byte doom_next[4][9] = {
        {12, 13, 19, 15, 16, 17, 18, 21, 14},
        {22, 23, 24, 25, 29, 27, 28, 31, 26},
        {32, 33, 34, 35, 36, 39, 38, 41, 37},
        {42, 49, 44, 45, 46, 47, 48, -1, 43}
    };

    byte doom2_next[32] = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                           13, 14, 15, 31, 17, 18, 19, 20, 21, 22, 23,
                           24, 25, 26, 27, 28, 29, 30, -1, 32, 16};

    // secret level
    doom2_next[14] = (haswolflevels ? 31 : 16);

    // shareware doom has only episode 1
    doom_next[0][7] = (gamemode == shareware ? -1 : 21);

    doom_next[2][7] = (gamemode == registered ? -1 : 41);

    // doom2_next and doom_next are 0 based, unlike gameepisode and gamemap
    *episode = gameepisode - 1;
    *map = gamemap - 1;

    if (gamemode == commercial)
    {
        *episode = 1;
        if (*map >= 0 && *map <= 31)
        {
            *map = doom2_next[*map];
        }
        else
        {
            *map = gamemap + 1;
        }
    }
    else
    {
        if (*episode >= 0 && *episode <= 3 && *map >= 0 && *map <= 8)
        {
            int next = doom_next[*episode][*map];
            *episode = next / 10;
            *map = next % 10;
        }
        else
        {
            *episode = gameepisode;
            *map = gamemap + 1;
        }
    }
}

MI_ShowNext_t MI_ShowNextLoc(void)
{
    // UMAPINFO
    if (gamemapinfo)
    {
        if (gamemapinfo->endpic[0])
        {
            return WI_ShowNextDone;
        }
        else
        {
            return WI_ShowNextLoc | WI_ShowNextEpisodal;
        }
    }

    // Legacy
    if (gamemode != commercial
        && (gamemap == 8 || (gamemission == pack_chex && gamemap == 5)))
    {
        return WI_ShowNextDone;
    }
    else
    {
        return WI_ShowNextLoc;
    }
}

boolean MI_SkipShowNextLoc(void)
{
    // UMAPINFO
    if (gamemapinfo)
    {
        return (gamemapinfo->flags & MapInfo_EndGame) != 0;
    }

    // Legacy
    return false;
}

boolean MI_BossAction(mobj_t *mo, line_t *junk, thinker_t **th)
{
    // UMAPINFO
    if (gamemapinfo && gamemapinfo->flags & MapInfo_BossActionClear)
    {
        return true;
    }

    if (gamemapinfo && array_size(gamemapinfo->bossactions))
    {
        // make sure there is a player alive for victory
        int i;
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i] && players[i].health > 0)
            {
                break;
            }
        }

        if (i == MAXPLAYERS)
        {
            return true; // no one left alive, so do not end game
        }

        bossaction_t *bossaction;
        array_foreach(bossaction, gamemapinfo->bossactions)
        {
            if (bossaction->type == mo->type)
            {
                break;
            }
        }

        if (bossaction == array_end(gamemapinfo->bossactions))
        {
            return true; // no matches found
        }

        if (!P_CheckBossDeath(mo))
        {
            return true; // other boss not dead
        }

        array_foreach(bossaction, gamemapinfo->bossactions)
        {
            if (bossaction->type == mo->type)
            {
                *junk = *lines;
                junk->special = (short)bossaction->special;
                junk->args[0] = (short)bossaction->tag;
                // use special semantics for line activation to block problem
                // types.
                if (!P_UseSpecialLine(mo, junk, 0, true))
                {
                    P_CrossSpecialLine(junk, 0, mo, true);
                }
            }
        }
    }

    // No legacy
    return true;
}

static boolean IsVanillaMap(int e, int m)
{
    if (gamemode == commercial)
    {
        return (e == 1 && m > 0 && m <= 32);
    }
    else
    {
        return (e > 0 && e <= 4 && m > 0 && m <= 9);
    }
}

static inline const char *GetVanillaMapname()
{
    return (gamemode != commercial)
               ? mapnames[(gameepisode - 1) * 9 + gamemap - 1]
           : (gamemission == pack_tnt)  ? mapnamest[gamemap - 1]
           : (gamemission == pack_plut) ? mapnamesp[gamemap - 1]
                                        : mapnames2[gamemap - 1];
}

static inline const char *GetVanillaMapnameOverflow()
{
    return (gamemission == doom2)       ? mapnamesp[gamemap - 33]
           : (gamemission == pack_plut) ? mapnamest[gamemap - 33]
                                        : "";
}

const char *MI_GetLevelTitle(void)
{
    const char *result = "";

    if (gamemapinfo && gamemapinfo->levelname)
    {
        if (!(gamemapinfo->flags & MapInfo_LabelClear))
        {
            static char *string;
            if (string)
            {
                free(string);
            }
            string = M_StringJoin(gamemapinfo->label ? gamemapinfo->label
                                                     : gamemapinfo->lumpname,
                                  ": ", gamemapinfo->levelname);
            result = string;
        }
        else
        {
            result = gamemapinfo->levelname;
        }
    }
    else if (gamestate == GS_LEVEL)
    {
        if (IsVanillaMap(gameepisode, gamemap))
        {
            result = DEH_String(GetVanillaMapname());
        }
        // WADs like pl2.wad have a MAP33, and rely on the layout in the
        // Vanilla executable, where it is possible to overflow the end of one
        // array into the next.
        else if (gamemode == commercial && gamemap >= 33 && gamemap <= 35)
        {
            result = DEH_String(GetVanillaMapnameOverflow());
        }
        else
        {
            // initialize the map title widget with the generic map lump name
            result = MapName(gameepisode, gamemap);
        }
    }

    return result;
}

int MI_SkyTexture(void)
{
    // UMAPINFO
    if (gamemapinfo && gamemapinfo->skytexture[0])
    {
        return R_TextureNumForName(gamemapinfo->skytexture);
    }

    // Legacy

    // DOOM determines the sky texture to be used
    // depending on the current episode, and the game version.
    int skytexture = NO_INDEX;
    if (gamemode == commercial)
    // || gamemode == pack_tnt   // jff 3/27/98 sorry guys pack_tnt,pack_plut
    // || gamemode == pack_plut) // aren't gamemodes, this was matching retail
    {
        skytexture = R_TextureNumForName("SKY3");
        if (gamemap < 12)
        {
            skytexture = R_TextureNumForName("SKY1");
        }
        else if (gamemap < 21)
        {
            skytexture = R_TextureNumForName("SKY2");
        }
    }
    else // jff 3/27/98 and lets not forget about DOOM and Ultimate DOOM huh?
    {
        switch (gameepisode)
        {
            default:
            case 1:
                skytexture = R_TextureNumForName("SKY1");
                break;
            case 2:
                // killough 10/98: beta version had different sky orderings
                skytexture =
                    R_TextureNumForName(beta_emulation ? "SKY1" : "SKY2");
                break;
            case 3:
                skytexture = R_TextureNumForName("SKY3");
                break;
            case 4: // Special Edition sky
                skytexture = R_TextureNumForName("SKY4");
                break;
        } // jff 3/27/98 end sky setting fix
    }

    return skytexture;
}

static int LegacyParTimes(void)
{
    int partime = 0;
    if (gamemode == commercial)
    {
        // MAP33 reads its par time from beyond the cpars[] array.
        if (demo_compatibility && gamemap == 33)
        {
            int cpars32;
            memcpy(&cpars32, DEH_String(GAMMALVL0), sizeof(int));
            partime = TICRATE * LONG(cpars32);
        }
        else if (gamemap >= 1 && gamemap <= 34)
        {
            partime = TICRATE * bex_cpars[gamemap - 1];
        }
    }
    else
    {
        // Doom Episode 4 doesn't have a par time, so this overflows into the
        // cpars[] array.
        if (demo_compatibility && gameepisode == 4 && gamemap >= 1
            && gamemap <= 9)
        {
            partime = TICRATE * bex_cpars[gamemap - 1];
        }
        else if (gameepisode >= 1 && gameepisode <= 6 && gamemap >= 1
                 && gamemap <= 9)
        {
            partime = TICRATE * bex_pars[gameepisode - 1][gamemap - 1];
        }
    }
    return partime;
}

int MI_PrepareIntermission(wbstartstruct_t *wminfo)
{
    // UMAPINFO
    if (gamemapinfo)
    {
        const char *next = "";

        if (gamemapinfo->endpic[0] && strcmp(gamemapinfo->endpic, "-") != 0
            && gamemapinfo->flags & MapInfo_NoIntermission)
        {
            return DC_Victory;
        }

        wminfo->partime = gamemapinfo->partime;
        umapinfo_partimes = true;

        if (!wminfo->partime)
        {
            wminfo->partime = LegacyParTimes();
            umapinfo_partimes = false;
        }

        if (secretexit)
        {
            next = gamemapinfo->nextsecret;
        }

        if (next[0] == 0)
        {
            next = gamemapinfo->nextmap;
        }

        if (next[0])
        {
            G_ValidateMapName(next, &wminfo->nextep, &wminfo->next);

            wminfo->nextep--;
            wminfo->next--;

            if (wminfo->nextep != wminfo->epsd)
            {
                for (int i = 0; i < MAXPLAYERS; i++)
                {
                    players[i].didsecret = false;
                }
            }

            wminfo->didsecret = players[consoleplayer].didsecret;
        }
        return 0;
    }

    // Legacy

    if (gamemode != commercial) // kilough 2/7/98
    {
        if (gamemap == 9)
        {
            for (int i = 0; i < MAXPLAYERS; i++)
            {
                players[i].didsecret = true;
            }
        }
    }

    wminfo->didsecret = players[consoleplayer].didsecret;

    // wminfo.next is 0 biased, unlike gamemap
    if (gamemode == commercial)
    {
        if (secretexit)
        {
            switch (gamemap)
            {
                case 15:
                    wminfo->next = 30;
                    break;
                case 31:
                    wminfo->next = 31;
                    break;
            }
        }
        else
        {
            switch (gamemap)
            {
                case 31:
                case 32:
                    wminfo->next = 15;
                    break;
                default:
                    wminfo->next = gamemap;
            }
        }
    }
    else
    {
        if (secretexit)
        {
            wminfo->next = 8; // go to secret level
        }
        else if (gamemap == 9)
        {
            // returning from secret level
            switch (gameepisode)
            {
                case 1:
                    wminfo->next = 3;
                    break;
                case 2:
                    wminfo->next = 5;
                    break;
                case 3:
                    wminfo->next = 6;
                    break;
                case 4:
                    wminfo->next = 2;
                    break;
            }
        }
        else
        {
            wminfo->next = gamemap; // go to next level
        }
    }

    return 0;
}

int MI_PrepareFinale(void)
{
    // UMAPINFO
    if (gamemapinfo)
    {
        MI_WinDisplay_t res = 0;
        if (gamemapinfo->intertextsecret && secretexit)
        {
            // '-' means that any default intermission was cleared.
            if (gamemapinfo->intertextsecret[0] != '-')
            {
                res = WD_StartFinale;
            }
            else
            {
                res = 0;
            }
        }
        else if (gamemapinfo->intertext && !secretexit)
        {
            // '-' means that any default intermission was cleared.
            if (gamemapinfo->intertext[0] != '-')
            {
                res = WD_StartFinale;
            }
            else
            {
                res = 0;
            }
        }
        else if (gamemapinfo->endpic[0] && gamemapinfo->endpic[0] != '-'
                 && !secretexit)
        {
            res = WD_Victory;
        }
        return res;
    }

    // Legacy
    MI_WinDisplay_t res = 0;
    if (gamemode == commercial)
    {
        switch (gamemap)
        {
            case 15:
            case 31:
                if (!secretexit)
                {
                    break;
                }
                // fallthrough
            case 6:
            case 11:
            case 20:
            case 30:
                res = WD_StartFinale;
                break;
        }
    }
    else if (gamemission == pack_chex && gamemap == 5)
    {
        res = WD_Victory;
    }
    else if (gamemap == 8)
    {
        res = WD_Victory;
    }

    return res;
}

void MI_WI_Start(wbstartstruct_t *wbs, const char **exitpic,
                 const char **enterpic, wi_animation_t **animation)
{
    // UMAPINFO
    if (wbs->lastmapinfo)
    {
        if (wbs->lastmapinfo->exitpic[0])
        {
            *exitpic = wbs->lastmapinfo->exitpic;
        }
        if (wbs->lastmapinfo->exitanim[0])
        {
            if (!*animation)
            {
                *animation = Z_Calloc(1, sizeof(**animation), PU_LEVEL, NULL);
            }
            (*animation)->interlevel_exiting =
                WI_ParseInterlevel(wbs->lastmapinfo->exitanim);
        }
    }

    if (wbs->nextmapinfo)
    {
        if (wbs->nextmapinfo->enterpic[0])
        {
            *enterpic = wbs->nextmapinfo->enterpic;
        }
        if (wbs->nextmapinfo->enteranim[0])
        {
            if (!*animation)
            {
                *animation = Z_Calloc(1, sizeof(**animation), PU_LEVEL, NULL);
            }
            (*animation)->interlevel_entering =
                WI_ParseInterlevel(wbs->nextmapinfo->enteranim);
        }
    }

    // Legacy
}

void MI_MapAnnouncement(char announce_string[120], char author_string[120],
                        const char string[120], size_t str_size)
{
    // UMAPINFO
    if (gamemapinfo && gamemapinfo->author)
    {
        M_snprintf(announce_string, str_size, "%s by %s", string,
                   gamemapinfo->author);
        if (MN_StringWidth(announce_string) > SCREENWIDTH)
        {
            M_StringCopy(announce_string, string, str_size);
            M_snprintf(author_string, str_size, "by %s", gamemapinfo->author);
        }
        return;
    }

    // Legacy
    M_StringCopy(announce_string, string, str_size);
}

void MI_Spechits(line_t *dummy, int *speciallines, boolean *trigger_keen)
{
    // UMAPINFO
    if (gamemapinfo && array_size(gamemapinfo->bossactions))
    {
        for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
        {
            if (th->function.p1 == P_MobjThinker)
            {
                mobj_t *mo = (mobj_t *)th;

                bossaction_t *bossaction;
                array_foreach(bossaction, gamemapinfo->bossactions)
                {
                    if (bossaction->type == mo->type)
                    {
                        dummy = lines;
                        dummy->special = (short)bossaction->special;
                        dummy->args[0] = (short)bossaction->tag;
                        // use special semantics for line activation to block
                        // problem types.
                        if (!P_UseSpecialLine(mo, dummy, 0, true))
                        {
                            P_CrossSpecialLine(dummy, 0, mo, true);
                        }

                        (*speciallines)++;

                        if (dummy->args[0] == 666)
                        {
                            *trigger_keen = false;
                        }
                    }
                }
            }
        }
        return;
    }

    // Legacy

    // [crispy] trigger tag 666/667 events
    if (gamemode == commercial)
    {
        if (gamemap == 7)
        {
            // Mancubi
            dummy->args[0] = 666;
            (*speciallines) += EV_DoFloor(dummy, lowerFloorToLowest);
            *trigger_keen = false;

            // Arachnotrons
            dummy->args[0] = 667;
            (*speciallines) += EV_DoFloor(dummy, raiseToTexture);
        }
    }
    else
    {
        if (gameepisode == 1)
        {
            // Barons of Hell
            dummy->args[0] = 666;
            (*speciallines) += EV_DoFloor(dummy, lowerFloorToLowest);
            *trigger_keen = false;
        }
        else if (gameepisode == 4)
        {
            if (gamemap == 6)
            {
                // Cyberdemons
                dummy->args[0] = 666;
                (*speciallines) += EV_DoDoor(dummy, blazeOpen);
                *trigger_keen = false;
            }
            else if (gamemap == 8)
            {
                // Spider Masterminds
                dummy->args[0] = 666;
                (*speciallines) += EV_DoFloor(dummy, lowerFloorToLowest);
                *trigger_keen = false;
            }
        }
    }
}

static inline int WRAP(int i, int w)
{
    while (i < 0)
    {
        i += w;
    }

    return i % w;
}

void MI_ChangeMusic(void)
{
    // UMAPINFO
    if (gamemapinfo && gamemapinfo->music[0])
    {
        int muslump = W_CheckNumForName(gamemapinfo->music);
        if (muslump >= 0)
        {
            S_ChangeMusInfoMusic(muslump, true);
            return;
        }
    }

    // Legacy
    int mnum;
    if (idmusnum != -1)
    {
        mnum = idmusnum; // jff 3/17/98 reload IDMUS music if not -1
    }
    else if (gamemode == commercial)
    {
        mnum = mus_runnin + WRAP(gamemap - 1, NUMMUSIC - mus_runnin);
    }
    else
    {
        mnum =
            mus_e1m1
            + WRAP((gameepisode - 1) * 9 + gamemap - 1, mus_runnin - mus_e1m1);
    }

    S_ChangeMusic(mnum, true);
}
