//
// Copyright(C) 2025 ceski
// Copyright(C) 2026, Roman Fomin
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

#include <math.h>

#include "decl_sounds.h"
#include "decl_defs.h"
#include "doomdef.h"
#include "doomtype.h"
#include "dsdh_main.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_scanner.h"
#include "sounds.h"

typedef enum
{
    TYPE_None,
    TYPE_Int,
    TYPE_Float,
    TYPE_String,
    TYPE_Sound
} valtype_t;

typedef enum
{
    prop_amb_index,
    prop_amb_sound,
    prop_amb_attenuation,
    prop_amb_type,
    prop_amb_period,
    prop_amb_minperiod,
    prop_amb_maxperiod,
    prop_amb_volume
} ambproptype_t;

typedef struct
{
    ambproptype_t type;
    propvalue_t value;
} ambproperty_t;

typedef struct
{
    const char *name;
    boolean prefix;
    const char **lumps;
    int sfx_number;
} decl_sound_t;

static hashmap_t *sounds;

typedef struct
{
    ambproperty_t *props;
    int index;
    ambient_mode_t mode;
} decl_ambient_t;

typedef struct
{
    valtype_t valtype;
    const char *name;
} decl_prop_t;

static decl_prop_t decl_ambient_props[] = {
    [prop_amb_index] =       {TYPE_None, "index"},
    [prop_amb_sound] =       {TYPE_Sound, "sound"},
    [prop_amb_attenuation] = {TYPE_Float, "attenuation"},
    [prop_amb_type] =        {TYPE_None, "type"},
    [prop_amb_period] =      {TYPE_Float, "period"},
    [prop_amb_minperiod] =   {TYPE_Float, "minperiod"},
    [prop_amb_maxperiod] =   {TYPE_Float, "maxperiod"},
    [prop_amb_volume] =      {TYPE_Float, "volume"},
};

void DECL_ParseSound(scanner_t *sc)
{
    decl_sound_t sound = {0};

    SC_MustGetToken(sc, TK_Identifier);
    char *sound_name = M_StringDuplicate(SC_GetString(sc));
    M_StringToLower(sound_name);
    sound.name = sound_name;

    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        switch (SC_CheckKeyword(sc, "prefix", "lump"))
        {
            case 0:
                sound.prefix = true;
                break;
            case 1:
                array_clear(sound.lumps);
                do
                {
                    SC_MustGetToken(sc, TK_StringConst);
                    char *string = M_StringDuplicate(SC_GetString(sc));
                    M_StringToLower(string);
                    array_push(sound.lumps, string);
                } while (SC_CheckToken(sc, ','));
                break;
            default:
                SC_Error(sc, "Unknown property '%s'.", SC_GetString(sc));
                break;
        }
    }

    if (!sounds)
    {
        sounds = hashmap_init_str(32, sizeof(decl_sound_t));
    }
    hashmap_put_str(sounds, sound.name, &sound);
}

typedef struct
{
    int *sfx;
} random_sound_t;

static hashmap_t *random_sounds;

static int CheckSound(const char *name)
{
    for (int i = 0; i < num_sfx; ++i)
    {
        if (S_sfx[i].name && strcasecmp(name, S_sfx[i].name))
        {
            return i;
        }
    }
    return sfx_None;
}

void DECL_InstallSounds(void)
{
    decl_sound_t *sound;
    hashmap_foreach(sound, sounds)
    {
        boolean random = (array_size(sound->lumps) > 1);
        random_sound_t random_sound = {0};

        for (int i = 0; i < array_size(sound->lumps); ++i)
        {
            char *lumpname = M_StringDuplicate(sound->lumps[i]);
            M_StringToLower(lumpname);

            int sfx_number = sfx_None;
            if (!sound->prefix && M_StringStartsWith(lumpname, "ds"))
            {
                const char *shortname = lumpname + 2;
                sfx_number = CheckSound(shortname);
            }
            else
            {
                sfx_number = CheckSound(lumpname);
            }

            if (sfx_number == sfx_None)
            {
                sfx_number = DSDH_SoundsGetNewIndex();
                S_sfx[sfx_number].name = lumpname;
            }

            if (!sound->prefix)
            {
                S_sfx[sfx_number].flags |= SFX_NoPrefix;
            }

            if (random)
            {
                array_push(random_sound.sfx, sfx_number);
            }
            else
            {
                sound->sfx_number = sfx_number;
            }
        }

        if (random)
        {
            int sfx_number = DSDH_SoundsGetNewIndex();
            S_sfx[sfx_number].flags |= SFX_Random;

            sound->sfx_number = sfx_number;

            if (!random_sounds)
            {
                random_sounds = hashmap_init(32, sizeof(random_sound_t));
            }
            hashmap_put(random_sounds, sfx_number, &random_sound);
        }
    }
}

int DECL_SoundMapping(const char *sound_name)
{
    if (sounds)
    {
        char *name = M_StringDuplicate(sound_name);
        M_StringToLower(name);
        decl_sound_t *sound = hashmap_get_str(sounds, name);
        free(name);
        if (sound)
        {
            return sound->sfx_number;
        }
    }

    I_Error("Sound '%s' is not defined.", sound_name);
    return sfx_None;
}

int S_RandomSound(int sfx_number)
{
    if (random_sounds)
    {
        random_sound_t *sound = hashmap_get(random_sounds, sfx_number);
        if (sound && array_size(sound->sfx))
        {
            int index = P_Random(pr_misc) % array_size(sound->sfx);
            return sound->sfx[index];
        }
    }

    I_Error("Random sound %d not found.", sfx_number);
    return sfx_None;
}

// DoomEd numbers 14001 to 14064 are supported.
#define MAX_AMBIENT_DATA 64

static hashmap_t *ambient_sounds;
static hashmap_t *ambient_data;

void DECL_ParseAmbient(scanner_t *sc)
{
    decl_ambient_t ambient = {0};

    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *prop_name = SC_GetString(sc);
        ambproptype_t type;
        for (type = 0; type < arrlen(decl_ambient_props); ++type)
        {
            if (!strcasecmp(prop_name, decl_ambient_props[type].name))
            {
                break;
            }
        }
        if (type == arrlen(decl_ambient_props))
        {
            SC_Error(sc, "Unknown property '%s'.", prop_name);
            return;
        }

        if (type == prop_amb_index)
        {
            SC_MustGetToken(sc, TK_IntConst);
            int index = SC_GetNumber(sc);
            if (index < 1 || index > MAX_AMBIENT_DATA)
            {
                SC_Error(sc, "Index not in range 1 to %d (found %d).", 
                         MAX_AMBIENT_DATA, index);
            }
            ambient.index = index;
        }
        else if (type == prop_amb_type)
        {
            SC_MustGetToken(sc, TK_Identifier);
            static const char *keywords[] =
            {
                [AMB_MODE_CONTINUOUS] = "continuous",
                [AMB_MODE_RANDOM] = "random",
                [AMB_MODE_PERIODIC] = "periodic"
            };
            ambient.mode = SC_RequireKeywordInternal(sc, keywords, arrlen(keywords));
        }
        else
        {
            propvalue_t value = {0};
            switch (decl_ambient_props[type].valtype)
            {
                case TYPE_Float:
                    SC_MustGetToken(sc, TK_FloatConst);
                    value.decimal = SC_GetDecimal(sc);
                    break;
                case TYPE_Sound:
                    SC_MustGetToken(sc, TK_Identifier);
                    value.string = M_StringDuplicate(SC_GetString(sc));
                    break;
                default:
                    break;
            }

            ambproperty_t *prop;
            array_foreach(prop, ambient.props)
            {
                if (prop->type == type)
                {
                    prop->value = value;
                    break;
                }
            }
            if (prop == array_end(ambient.props))
            {
                ambproperty_t new_prop = {.type = type, .value = value};
                array_push(ambient.props, new_prop);
            }
        }
    }

    if (!ambient_sounds)
    {
        ambient_sounds = hashmap_init(16, sizeof(decl_ambient_t));
    }
    hashmap_put(ambient_sounds, ambient.index, &ambient);
}

// Limit the range based on max value from M_Random().
#define MAX_TICS (INT_MAX / 255)

void DECL_InstallAmbient(void)
{
    if (!ambient_sounds)
    {
        return;
    }

    snd_ambient = true;

    decl_ambient_t *ambient;
    hashmap_foreach(ambient, ambient_sounds)
    {
        ambient_data_t data = {
            .close_dist = S_CLOSE_DIST,
            .clipping_dist = S_CLIPPING_DIST,
            .type = AMB_TYPE_POINT
        };

        ambproperty_t *prop;
        array_foreach(prop, ambient->props)
        {
            switch (prop->type)
            {
                case prop_amb_sound:
                    data.sfx_id = DECL_SoundMapping(prop->value.string);
                    break;
                case prop_amb_attenuation:
                    {
                        double attenuation = prop->value.decimal;
                        attenuation = MAX(0.0, attenuation);

                        if (attenuation > 0.0)
                        {
                            // Limit the range based on volume calculations in
                            // AdjustSoundParams.
                            int min_dist = 0;
                            int max_dist = INT_MAX / 127;

                            double dist = data.close_dist / attenuation;
                            dist = clamp(dist, min_dist, max_dist);
                            data.close_dist = lround(dist);

                            // Make sure clipping distance is always greater
                            // than close distance.
                            min_dist = data.close_dist + 1;
                            max_dist = INT_MAX / 127 + 1;

                            dist = data.clipping_dist / attenuation;
                            dist = clamp(dist, min_dist, max_dist);
                            data.clipping_dist = lround(dist);
                        }
                    }
                    break;
                case prop_amb_period:
                    if (ambient->mode == AMB_MODE_PERIODIC)
                    {
                        double tics = prop->value.decimal * TICRATE;
                        tics = clamp(tics, 0.0, MAX_TICS);
                        data.min_tics = lround(tics);
                        data.max_tics = data.min_tics;
                    }
                    break;
                case prop_amb_minperiod:
                    if (ambient->mode == AMB_MODE_RANDOM)
                    {
                        double tics = prop->value.decimal * TICRATE;
                        data.min_tics = lround(tics);
                    }
                    break;
                case prop_amb_maxperiod:
                    if (ambient->mode == AMB_MODE_RANDOM)
                    {
                        double tics = prop->value.decimal * TICRATE;
                        data.max_tics = lround(tics);
                    }
                    break;
                case prop_amb_volume:
                    {
                        double volume = clamp(prop->value.decimal, 0.0, 1.0);
                        data.volume_scale = lround(volume * 127.0);
                    }
                    break;
                default:
                    break;
            }

            if (ambient->mode == AMB_MODE_CONTINUOUS)
            {
                data.min_tics = -1;
                data.max_tics = -1;
            }
            else if (ambient->mode == AMB_MODE_RANDOM)
            {
                data.min_tics = clampi(data.min_tics, 0, MAX_TICS);
                data.max_tics = clampi(data.max_tics, data.min_tics, MAX_TICS);
            }

            sfxinfo_t *sfx = &S_sfx[data.sfx_id];
            sfx->flags |= SFX_Ambient;
            sfx->looping = (ambient->mode == AMB_MODE_CONTINUOUS);

            if (!ambient_data)
            {
                ambient_data = hashmap_init(16, sizeof(ambient_data_t));
            }
            hashmap_put(ambient_data, ambient->index, &data);
        }
    }
}

const ambient_data_t *S_GetAmbientData(int index)
{
    if (!ambient_data || index < 1 || index > MAX_AMBIENT_DATA)
    {
        return NULL;
    }

    return hashmap_get(ambient_data, index);
}
