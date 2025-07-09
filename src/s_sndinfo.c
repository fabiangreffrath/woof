//
// Copyright(C) 2025 ceski
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
//
// DESCRIPTION:
//      SNDINFO
//

#include <math.h>

#include "doomtype.h"
#include "doomdef.h"
#include "dsdhacked.h"
#include "i_printf.h"
#include "i_sound.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "s_sndinfo.h"
#include "sounds.h"
#include "w_wad.h"

// DoomEd numbers 14001 to 14064 are supported.
#define MAX_AMBIENT_DATA 64

typedef struct sound_def_s
{
    char *sound_name;
    char *lump_name;
    int lumpnum;
} sound_def_t;

static ambient_data_t *ambient_data;

const ambient_data_t *S_GetAmbientData(int index)
{
    if (!ambient_data || index < 1 || index > MAX_AMBIENT_DATA)
    {
        return NULL;
    }

    return &ambient_data[index - 1];
}

//
// ParseSoundDefinition
//
// Reference: https://zdoom.org/wiki/SNDINFO
//
// Old syntax: <sound_name> <lump_name>
// New syntax: <sound_name> = <lump_name>
//
static void ParseSoundDefinition(scanner_t *s, sound_def_t **sound_defs)
{
    sound_def_t def;

    def.sound_name = M_StringDuplicate(SC_GetString(s));

    // Skip equal sign if found.
    SC_CheckToken(s, '=');

    if (!SC_SameLine(s))
    {
        SC_Warning(s, "expected lump name");
        free(def.sound_name);
        return;
    }

    SC_MustGetToken(s, TK_RawString);
    def.lump_name = M_StringDuplicate(SC_GetString(s));
    def.lumpnum = W_CheckNumForName(def.lump_name);

    if (def.lumpnum < 0)
    {
        SC_Warning(s, "lump not found: %s", def.lump_name);
        free(def.sound_name);
        free(def.lump_name);
        if (SC_SameLine(s))
        {
            SC_GetNextLineToken(s);
        }
        return;
    }

    // If there are multiple sound definitions with the same sound name, only
    // the last sound definition will be used.
    for (int i = 0; i < array_size(*sound_defs); i++)
    {
        if (!strcasecmp(def.sound_name, (*sound_defs)[i].sound_name))
        {
            I_Printf(VB_WARNING, "SNDINFO: duplicate sound definition: %s",
                     def.sound_name);
            array_delete(*sound_defs, i);
            break;
        }
    }

    array_push(*sound_defs, def);
}

//
// ParseAmbientSoundCommand
//
// Reference: https://zdoom.org/wiki/SNDINFO
//
// $ambient <index> <sound_name> [type] <mode> <volume>
//
// [type]: point [attenuation]
//         surround
//         [world]
//
// <mode>: continuous
//         random <min_sec> <max_sec>
//         periodic <sec>
//
static void ParseAmbientSoundCommand(scanner_t *s, char ***sound_names,
                                     ambient_data_t **ambient_data)
{
    ambient_data_t amb = {0};

    // Index
    SC_MustGetToken(s, TK_IntConst);
    const int index = SC_GetNumber(s);
    if (index < 1 || index > MAX_AMBIENT_DATA)
    {
        SC_Error(s, "index not in range 1 to %d (found %d)", 
                 MAX_AMBIENT_DATA, index);
    }

    // Array index
    const int array_index = index - 1;

    // Sound name
    SC_MustGetToken(s, TK_RawString);
    char *sound_name = M_StringDuplicate(SC_GetString(s));

    // Type is optional, but mode is required.
    SC_MustGetStringOrIdent(s);

    amb.close_dist = S_CLOSE_DIST;
    amb.clipping_dist = S_CLIPPING_DIST;

    // Type (optional)
    if (!strcasecmp(SC_GetString(s), "point"))
    {
        amb.type = AMB_TYPE_POINT;

        if (SC_CheckToken(s, TK_FloatConst))
        {
            double attenuation = SC_GetDecimal(s);
            attenuation = MAX(0.0, attenuation);

            if (attenuation > 0.0)
            {
                // Limit the range based on volume calculations in
                // AdjustSoundParams.
                int min_dist = 0;
                int max_dist = INT_MAX / 127;

                double dist = amb.close_dist / attenuation;
                dist = CLAMP(dist, min_dist, max_dist);
                amb.close_dist = lround(dist);

                // Make sure clipping distance is always greater than close
                // distance.
                min_dist = amb.close_dist + 1;
                max_dist = INT_MAX / 127 + 1;

                dist = amb.clipping_dist / attenuation;
                dist = CLAMP(dist, min_dist, max_dist);
                amb.clipping_dist = lround(dist);
            }
        }

        // Keep looking for mode.
        SC_MustGetStringOrIdent(s);
    }
    else
    {
        amb.type = AMB_TYPE_WORLD;

        if (!strcasecmp(SC_GetString(s), "surround")
            || !strcasecmp(SC_GetString(s), "world"))
        {
            // Keep looking for mode.
            SC_MustGetStringOrIdent(s);
        }
    }

    // Mode
    if (!strcasecmp(SC_GetString(s), "continuous"))
    {
        amb.mode = AMB_MODE_CONTINUOUS;
        amb.min_tics = -1;
        amb.max_tics = -1;
    }
    else if (!strcasecmp(SC_GetString(s), "random"))
    {
        amb.mode = AMB_MODE_RANDOM;

        // Limit the range based on max value from M_Random().
        const int max_tics = INT_MAX / 255;

        SC_MustGetToken(s, TK_FloatConst);
        double tics = SC_GetDecimal(s) * TICRATE;
        tics = CLAMP(tics, 0.0, max_tics);
        amb.min_tics = lround(tics);

        SC_MustGetToken(s, TK_FloatConst);
        tics = SC_GetDecimal(s) * TICRATE;
        tics = CLAMP(tics, amb.min_tics, max_tics);
        amb.max_tics = lround(tics);
    }
    else if (!strcasecmp(SC_GetString(s), "periodic"))
    {
        amb.mode = AMB_MODE_PERIODIC;

        SC_MustGetToken(s, TK_FloatConst);
        double tics = SC_GetDecimal(s) * TICRATE;
        tics = CLAMP(tics, 0.0, INT_MAX);
        amb.min_tics = lround(tics);
        amb.max_tics = amb.min_tics;
    }
    else
    {
        SC_Error(s, "invalid mode");
    }

    if (!amb.min_tics && !amb.max_tics)
    {
        SC_Error(s, "invalid interval");
    }

    // Volume
    SC_MustGetToken(s, TK_FloatConst);
    double volume = SC_GetDecimal(s);
    volume = CLAMP(volume, 0.0, 1.0);
    amb.volume_scale = lround(volume * 127.0);

    // Ambient sounds are resolved later.
    amb.sfx_id = sfx_None;

    // If there are multiple $ambient commands with the same index, only the
    // last $ambient command will be used.
    if ((*sound_names)[array_index])
    {
        I_Printf(VB_WARNING, "SNDINFO: duplicate $ambient index: %d", index);
        free((*sound_names)[array_index]);
    }

    (*sound_names)[array_index] = sound_name;
    (*ambient_data)[array_index] = amb;
}

static boolean ResolveAmbientSounds(sound_def_t *sound_defs,
                                    char **sound_names,
                                    ambient_data_t *ambient_data)
{
    int num_resolved = 0;

    for (int i = 0; i < MAX_AMBIENT_DATA; i++)
    {
        const char *sound_name = sound_names[i];

        if (sound_name)
        {
            ambient_data_t *amb = &ambient_data[i];

            for (int j = 0; j < array_size(sound_defs); j++)
            {
                const sound_def_t *def = &sound_defs[j];

                if (!strcasecmp(sound_name, def->sound_name))
                {
                    amb->sfx_id = dsdh_GetNewSFXIndex();
                    sfxinfo_t *sfx = &S_sfx[amb->sfx_id];
                    sfx->name = M_StringDuplicate(def->lump_name);
                    sfx->lumpnum = def->lumpnum;
                    sfx->ambient = true;
                    sfx->looping = (amb->mode == AMB_MODE_CONTINUOUS);
                    num_resolved++;
                    //I_Printf(VB_DEBUG, "SNDINFO: %s", sound_name);
                    break;
                }
            }

            if (amb->sfx_id == sfx_None)
            {
                I_Printf(VB_WARNING, "SNDINFO: '%s' undefined", sound_name);
            }
        }
    }

    //I_Printf(VB_DEBUG, "SNDINFO: Resolved %d ambient sounds", num_resolved);
    return (num_resolved > 0);
}

static void FreeSoundDefinitions(sound_def_t **sound_defs)
{
    for (int i = 0; i < array_size(*sound_defs); i++)
    {
        sound_def_t *def = &(*sound_defs)[i];

        if (def->sound_name)
        {
            free(def->sound_name);
        }

        if (def->lump_name)
        {
            free(def->lump_name);
        }
    }

    array_free(*sound_defs);
}

static void FreeSoundNames(char ***sound_names)
{
    for (int i = 0; i < MAX_AMBIENT_DATA; i++)
    {
        if ((*sound_names)[i])
        {
            free((*sound_names)[i]);
        }
    }

    free(*sound_names);
}

void S_ParseSndInfo(int lumpnum)
{
    const char *lump_data = W_CacheLumpNum(lumpnum, PU_CACHE);
    const int lump_length = W_LumpLength(lumpnum);
    scanner_t *s = SC_Open("SNDINFO", lump_data, lump_length);
    sound_def_t *sound_defs = NULL;
    char **sound_names = calloc(MAX_AMBIENT_DATA, sizeof(*sound_names));
    ambient_data = calloc(MAX_AMBIENT_DATA, sizeof(*ambient_data));

    while (SC_TokensLeft(s))
    {
        SC_CheckToken(s, TK_RawString);
        const char *string = SC_GetString(s);
        if (string[0] != '$')
        {
            ParseSoundDefinition(s, &sound_defs);
        }
        else if (!strcasecmp(string, "$ambient"))
        {
            ParseAmbientSoundCommand(s, &sound_names, &ambient_data);
        }
        else
        {
            SC_GetNextLineToken(s);
        }
    }

    if (!ResolveAmbientSounds(sound_defs, sound_names, ambient_data))
    {
        free(ambient_data);
        ambient_data = NULL;
    }

    FreeSoundDefinitions(&sound_defs);
    FreeSoundNames(&sound_names);
    SC_Close(s);
}

void S_PostParseSndInfo(void)
{
    snd_ambient = (ambient_data && default_snd_ambient);
}
