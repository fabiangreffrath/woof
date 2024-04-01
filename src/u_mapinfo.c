//-----------------------------------------------------------------------------
//
// Copyright 2017 Christoph Oelckers
// Copyright 2019 Fernando Carmona Varo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_misc.h"
#include "u_mapinfo.h"
#include "u_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

void M_AddEpisode(const char *map, const char *gfx, const char *txt,
                  const char *alpha);
void M_ClearEpisodes(void);

int G_ValidateMapName(const char *mapname, int *pEpi, int *pMap);

umapinfo_t U_mapinfo;

umapinfo_t default_mapinfo;

static const char *const ActorNames[] =
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
    NULL};

static void FreeMap(mapentry_t *mape)
{
    if (mape->mapname)
    {
        free(mape->mapname);
    }
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
    mape->mapname = NULL;
}

static void ReplaceString(char **pptr, const char *newstring)
{
    if (*pptr != NULL)
    {
        free(*pptr);
    }
    *pptr = strdup(newstring);
}

static void UpdateMapEntry(mapentry_t *mape, mapentry_t *newe)
{
    if (newe->mapname)
    {
        ReplaceString(&mape->mapname, newe->mapname);
    }
    if (newe->levelname)
    {
        ReplaceString(&mape->levelname, newe->levelname);
    }
    if (newe->label)
    {
        ReplaceString(&mape->label, newe->label);
    }
    if (newe->author)
    {
        strcpy(mape->author, newe->author);
    }
    if (newe->intertext)
    {
        ReplaceString(&mape->intertext, newe->intertext);
    }
    if (newe->intertextsecret)
    {
        ReplaceString(&mape->intertextsecret, newe->intertextsecret);
    }
    if (newe->levelpic[0])
    {
        strcpy(mape->levelpic, newe->levelpic);
    }
    if (newe->nextmap[0])
    {
        strcpy(mape->nextmap, newe->nextmap);
    }
    if (newe->nextsecret[0])
    {
        strcpy(mape->nextsecret, newe->nextsecret);
    }
    if (newe->music[0])
    {
        strcpy(mape->music, newe->music);
    }
    if (newe->skytexture[0])
    {
        strcpy(mape->skytexture, newe->skytexture);
    }
    if (newe->endpic[0])
    {
        strcpy(mape->endpic, newe->endpic);
    }
    if (newe->exitpic[0])
    {
        strcpy(mape->exitpic, newe->exitpic);
    }
    if (newe->enterpic[0])
    {
        strcpy(mape->enterpic, newe->enterpic);
    }
    if (newe->interbackdrop[0])
    {
        strcpy(mape->interbackdrop, newe->interbackdrop);
    }
    if (newe->intermusic[0])
    {
        strcpy(mape->intermusic, newe->intermusic);
    }
    if (newe->partime)
    {
        mape->partime = newe->partime;
    }
    if (newe->nointermission)
    {
        mape->nointermission = newe->nointermission;
    }
    if (newe->numbossactions)
    {
        mape->numbossactions = newe->numbossactions;
        if (mape->numbossactions == -1)
        {
            if (mape->bossactions)
            {
                free(mape->bossactions);
            }
            mape->bossactions = NULL;
        }
        else
        {
            mape->bossactions = realloc(mape->bossactions,
                                        sizeof(bossaction_t) * mape->numbossactions);
            memcpy(mape->bossactions, newe->bossactions,
                   sizeof(bossaction_t) * mape->numbossactions);
        }
    }
}

// -----------------------------------------------
// Parses a set of string and concatenates them
// Returns a pointer to the string (must be freed)
// -----------------------------------------------
static char *ParseMultiString(u_scanner_t *s, int error)
{
    char *build = NULL;

    if (U_CheckToken(s, TK_Identifier))
    {
        if (!strcasecmp(s->string, "clear"))
        {
            // this was explicitly deleted to override the default.
            return strdup("-");
        }
        else
        {
            U_Error(s, "Either 'clear' or string constant expected");
        }
    }

    do
    {
        U_MustGetToken(s, TK_StringConst);
        if (build == NULL)
        {
            build = strdup(s->string);
        }
        else
        {
            // plus room for one \n and one \0
            size_t newlen = strlen(build) + strlen(s->string) + 2;
            build = I_Realloc(build, newlen);
            // Replace the existing text's \0 terminator with a \n
            strcat(build, "\n");
            // Concatenate the new line onto the existing text
            strcat(build, s->string);
        }
    } while (U_CheckToken(s, ','));
    return build;
}

// -----------------------------------------------
// Parses a lump name. The buffer must be at least 9 characters.
// If parsed name is longer than 8 chars, sets NULL pointer.
//
// returns 1 on successfully parsing an element
//         0 on parse error in last read token
// -----------------------------------------------
static int ParseLumpName(u_scanner_t *s, char *buffer)
{
    if (!U_MustGetToken(s, TK_StringConst))
    {
        return 0;
    }

    if (strlen(s->string) > 8)
    {
        U_Error(s, "String too long. Maximum size is 8 characters.");
        return 1; // not a parse error
    }
    strncpy(buffer, s->string, 8);
    buffer[8] = 0;
    M_StringToUpper(buffer);
    return 1;
}

// -----------------------------------------------
// Parses a standard property that is already known
// These do not get stored in the property list
// but in dedicated struct member variables.
//
// returns 1 on successfully parsing an element
//         0 on parse error in last read token
// -----------------------------------------------
static int ParseStandardProperty(u_scanner_t *s, mapentry_t *mape)
{
    char *pname;
    int status = 1; // 1 for success, 0 for parse error
    if (!U_MustGetToken(s, TK_Identifier))
    {
        return 0;
    }

    pname = strdup(s->string);
    U_MustGetToken(s, '=');

    if (!strcasecmp(pname, "levelname"))
    {
        if (U_MustGetToken(s, TK_StringConst))
        {
            ReplaceString(&mape->levelname, s->string);
        }
    }
    else if (!strcasecmp(pname, "label"))
    {
        if (U_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(s->string, "clear"))
            {
                ReplaceString(&mape->label, "-");
            }
            else
            {
                U_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else if (U_MustGetToken(s, TK_StringConst))
        {
            ReplaceString(&mape->label, s->string);
        }
    }
    else if (!strcasecmp(pname, "author"))
    {
        if (U_MustGetToken(s, TK_StringConst))
        {
            ReplaceString(&mape->author, s->string);
        }
    }
    else if (!strcasecmp(pname, "episode"))
    {
        if (U_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(s->string, "clear"))
            {
                M_ClearEpisodes();
            }
            else
            {
                U_Error(s, "Either 'clear' or string constant expected");
            }
        }
        else
        {
            char lumpname[9] = {0};
            char *alttext = NULL;
            char *key = NULL;

            ParseLumpName(s, lumpname);
            if (U_CheckToken(s, ','))
            {
                if (U_MustGetToken(s, TK_StringConst))
                {
                    alttext = strdup(s->string);
                }
                if (U_CheckToken(s, ','))
                {
                    if (U_MustGetToken(s, TK_StringConst))
                    {
                        key = strdup(s->string);
                        key[0] = M_ToLower(key[0]);
                    }
                }
            }

            M_AddEpisode(mape->mapname, lumpname, alttext, key);

            if (alttext)
            {
                free(alttext);
            }
            if (key)
            {
                free(key);
            }
        }
    }
    else if (!strcasecmp(pname, "next"))
    {
        status = ParseLumpName(s, mape->nextmap);
        if (!G_ValidateMapName(mape->nextmap, NULL, NULL))
        {
            U_Error(s, "Invalid map name %s.", mape->nextmap);
            status = 0;
        }
    }
    else if (!strcasecmp(pname, "nextsecret"))
    {
        status = ParseLumpName(s, mape->nextsecret);
        if (!G_ValidateMapName(mape->nextsecret, NULL, NULL))
        {
            U_Error(s, "Invalid map name %s", mape->nextsecret);
            status = 0;
        }
    }
    else if (!strcasecmp(pname, "levelpic"))
    {
        status = ParseLumpName(s, mape->levelpic);
    }
    else if (!strcasecmp(pname, "skytexture"))
    {
        status = ParseLumpName(s, mape->skytexture);
    }
    else if (!strcasecmp(pname, "music"))
    {
        status = ParseLumpName(s, mape->music);
    }
    else if (!strcasecmp(pname, "endpic"))
    {
        status = ParseLumpName(s, mape->endpic);
    }
    else if (!strcasecmp(pname, "endcast"))
    {
        U_MustGetToken(s, TK_BoolConst);
        if (s->sc_boolean)
        {
            strcpy(mape->endpic, "$CAST");
        }
        else
        {
            strcpy(mape->endpic, "-");
        }
    }
    else if (!strcasecmp(pname, "endbunny"))
    {
        U_MustGetToken(s, TK_BoolConst);
        if (s->sc_boolean)
        {
            strcpy(mape->endpic, "$BUNNY");
        }
        else
        {
            strcpy(mape->endpic, "-");
        }
    }
    else if (!strcasecmp(pname, "endgame"))
    {
        U_MustGetToken(s, TK_BoolConst);
        if (s->sc_boolean)
        {
            strcpy(mape->endpic, "!");
        }
        else
        {
            strcpy(mape->endpic, "-");
        }
    }
    else if (!strcasecmp(pname, "exitpic"))
    {
        status = ParseLumpName(s, mape->exitpic);
    }
    else if (!strcasecmp(pname, "enterpic"))
    {
        status = ParseLumpName(s, mape->enterpic);
    }
    else if (!strcasecmp(pname, "nointermission"))
    {
        if (U_MustGetToken(s, TK_BoolConst))
        {
            mape->nointermission = s->sc_boolean;
        }
    }
    else if (!strcasecmp(pname, "partime"))
    {
        if (U_MustGetInteger(s))
        {
            mape->partime = s->number;
        }
    }
    else if (!strcasecmp(pname, "intertext"))
    {
        char *lname = ParseMultiString(s, 1);
        if (!lname)
        {
            return 0;
        }
        if (mape->intertext != NULL)
        {
            free(mape->intertext);
        }
        mape->intertext = lname;
    }
    else if (!strcasecmp(pname, "intertextsecret"))
    {
        char *lname = ParseMultiString(s, 1);
        if (!lname)
        {
            return 0;
        }
        if (mape->intertextsecret != NULL)
        {
            free(mape->intertextsecret);
        }
        mape->intertextsecret = lname;
    }
    else if (!strcasecmp(pname, "interbackdrop"))
    {
        status = ParseLumpName(s, mape->interbackdrop);
    }
    else if (!strcasecmp(pname, "intermusic"))
    {
        status = ParseLumpName(s, mape->intermusic);
    }
    else if (!strcasecmp(pname, "bossaction"))
    {
        if (U_MustGetToken(s, TK_Identifier))
        {
            if (!strcasecmp(s->string, "clear"))
            {
                // mark level free of boss actions
                if (mape->bossactions)
                {
                    free(mape->bossactions);
                }
                mape->bossactions = NULL;
                mape->numbossactions = -1;
            }
            else
            {
                int i, special, tag;
                for (i = 0; ActorNames[i]; i++)
                {
                    if (!strcasecmp(s->string, ActorNames[i]))
                    {
                        break;
                    }
                }
                if (ActorNames[i] == NULL)
                {
                    U_Error(s, "bossaction: unknown thing type '%s'",
                            s->string);
                    return 0;
                }
                U_MustGetToken(s, ',');
                U_MustGetInteger(s);
                special = s->number;
                U_MustGetToken(s, ',');
                U_MustGetInteger(s);
                tag = s->number;
                // allow no 0-tag specials here, unless a level exit.
                if (tag != 0 || special == 11 || special == 51 || special == 52
                    || special == 124)
                {
                    if (mape->numbossactions == -1)
                    {
                        mape->numbossactions = 1;
                    }
                    else
                    {
                        mape->numbossactions++;
                    }
                    mape->bossactions = realloc(mape->bossactions,
                                                sizeof(bossaction_t) * mape->numbossactions);
                    bossaction_t *bossaction = &mape->bossactions[mape->numbossactions - 1];
                    bossaction->type = i;
                    bossaction->special = special;
                    bossaction->tag = tag;
                }
            }
        }
    }
    // If no known property name was given, skip all comma-separated values
    // after the = sign
    else if (s->token == '=')
    {
        do
        {
            U_GetNextToken(s, true);
        } while (U_CheckToken(s, ','));
    }

    free(pname);
    return status;
}

// -----------------------------------------------
//
// Parses a complete map entry
//
// -----------------------------------------------

static int ParseMapEntry(u_scanner_t *s, mapentry_t *val)
{
    val->mapname = NULL;

    if (!U_MustGetIdentifier(s, "map"))
    {
        return 0;
    }

    U_MustGetToken(s, TK_Identifier);
    if (!G_ValidateMapName(s->string, NULL, NULL))
    {
        U_Error(s, "Invalid map name %s", s->string);
        return 0;
    }

    ReplaceString(&val->mapname, s->string);
    U_MustGetToken(s, '{');
    while (!U_CheckToken(s, '}'))
    {
        if (!ParseStandardProperty(s, val))
        {
            // If there was an error parsing, skip to next token
            U_GetNextToken(s, true);
        }
    }
    return 1;
}

// -----------------------------------------------
//
// Parses a complete UMAPINFO lump
//
// -----------------------------------------------

static boolean UpdateDefaultMapEntry(mapentry_t *val, int num)
{
    int i;

    for (i = 0; i < default_mapinfo.mapcount; ++i)
    {
        if (!strcmp(val->mapname, default_mapinfo.maps[i].mapname))
        {
            memset(&U_mapinfo.maps[num], 0, sizeof(mapentry_t));
            UpdateMapEntry(&U_mapinfo.maps[num], &default_mapinfo.maps[i]);
            UpdateMapEntry(&U_mapinfo.maps[num], val);
            FreeMap(val);
            return true;
        }
    }

    return false;
}

void U_ParseMapDefInfo(int lumpnum)
{
    const char *buffer = W_CacheLumpNum(lumpnum, PU_CACHE);
    size_t length = W_LumpLength(lumpnum);
    u_scanner_t *s = U_ScanOpen(buffer, length, "UMAPDEF");

    while (U_HasTokensLeft(s))
    {
        mapentry_t parsed = {0};
        if (!ParseMapEntry(s, &parsed))
        {
            U_Error(s, "Skipping entry: %s", s->string);
            continue;
        }

        default_mapinfo.mapcount++;
        default_mapinfo.maps = realloc(default_mapinfo.maps,
                                       sizeof(mapentry_t) * default_mapinfo.mapcount);
        default_mapinfo.maps[default_mapinfo.mapcount - 1] = parsed;
    }
    U_ScanClose(s);
}

void U_ParseMapInfo(int lumpnum)
{
    unsigned int i;
    const char *buffer = W_CacheLumpNum(lumpnum, PU_CACHE);
    size_t length = W_LumpLength(lumpnum);
    u_scanner_t *s = U_ScanOpen(buffer, length, "UMAPINFO");

    while (U_HasTokensLeft(s))
    {
        mapentry_t parsed = {0};
        if (!ParseMapEntry(s, &parsed))
        {
            U_Error(s, "Skipping entry: %s", s->string);
            continue;
        }

        // Set default level progression here to simplify the checks elsewhere.
        // Doing this lets us skip all normal code for this if nothing has been
        // defined.
        if (parsed.endpic[0] && (strcmp(parsed.endpic, "-") != 0))
        {
            parsed.nextmap[0] = 0;
        }
        else if (!parsed.nextmap[0] && !parsed.endpic[0])
        {
            if (!strcasecmp(parsed.mapname, "MAP30"))
            {
                strcpy(parsed.endpic, "$CAST");
            }
            else if (!strcasecmp(parsed.mapname, "E1M8"))
            {
                strcpy(parsed.endpic, gamemode == retail ? "CREDIT" : "HELP2");
            }
            else if (!strcasecmp(parsed.mapname, "E2M8"))
            {
                strcpy(parsed.endpic, "VICTORY2");
            }
            else if (!strcasecmp(parsed.mapname, "E3M8"))
            {
                strcpy(parsed.endpic, "$BUNNY");
            }
            else if (!strcasecmp(parsed.mapname, "E4M8"))
            {
                strcpy(parsed.endpic, "ENDPIC");
            }
            else
            {
                int ep, map;

                G_ValidateMapName(parsed.mapname, &ep, &map);

                strcpy(parsed.nextmap, MAPNAME(ep, map + 1));
            }
        }

        // Does this property already exist? If yes, replace it.
        for (i = 0; i < U_mapinfo.mapcount; i++)
        {
            if (!strcmp(parsed.mapname, U_mapinfo.maps[i].mapname))
            {
                FreeMap(&U_mapinfo.maps[i]);

                if (!UpdateDefaultMapEntry(&parsed, i))
                {
                    U_mapinfo.maps[i] = parsed;
                }
                break;
            }
        }
        // Not found so create a new one.
        if (i == U_mapinfo.mapcount)
        {
            U_mapinfo.mapcount++;
            U_mapinfo.maps = realloc(U_mapinfo.maps,
                                     sizeof(mapentry_t) * U_mapinfo.mapcount);

            if (!UpdateDefaultMapEntry(&parsed, i))
            {
                U_mapinfo.maps[U_mapinfo.mapcount - 1] = parsed;
            }
        }
    }
    U_ScanClose(s);
}

boolean U_CheckField(char *str)
{
    return str && str[0] && strcmp(str, "-");
}
