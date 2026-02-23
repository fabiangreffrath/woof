//
//  Copyright (c) 2015, Braden "Blzut3" Obrzut
//  Copyright (c) 2026, Roman Fomin
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the
//       distribution.
//     * The names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.


#include "decl_misc.h"
#include "doomtype.h"
#include "dsdh_main.h"
#include "i_system.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_scanner.h"
#include "sounds.h"

typedef enum
{
    TYPE_Boolean,
    TYPE_Int,
    TYPE_Float,
    TYPE_String,
    TYPE_ListOfStrings,
    TYPE_Sound
} valtype_t;

typedef enum
{
    prop_sound_lump,
    prop_sound_prefix,
    prop_sound_end
} sndproptype_t;

typedef struct
{
    const char *string;
    const char **list;
    int number;
    double decimal;
} sndpropvalue_t;

typedef struct
{
    sndproptype_t type;
    sndpropvalue_t value;
} sndproperty_t;

typedef struct
{
    const char *name;
    sndproperty_t *props;
    boolean prefix;
    int sfx_number;
} sound_t;

static hashmap_t *sounds;

static struct
{
    valtype_t valtype;
    const char *name;
} decl_sound_props[] = {
    [prop_sound_lump]   = {TYPE_ListOfStrings, "lump"},
    [prop_sound_prefix] = {TYPE_Boolean, "prefix"},
};

static sndproperty_t *GetProp(sound_t *sound, sndproptype_t type)
{
    sndproperty_t *prop;
    array_foreach(prop, sound->props)
    {
        if (prop->type == type)
        {
            return prop;
        }
    }
    return NULL;
}

void DECL_ParseSound(scanner_t *sc)
{
    sound_t sound = {0};

    SC_MustGetToken(sc, TK_Identifier);
    char *sound_name = M_StringDuplicate(SC_GetString(sc));
    M_StringToLower(sound_name);
    sound.name = sound_name;

    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *prop_name = SC_GetString(sc);
        sndproptype_t type;
        for (type = 0; type < prop_sound_end; ++type)
        {
            if (!strcasecmp(prop_name, decl_sound_props[type].name))
            {
                break;
            }
        }
        if (type == prop_sound_end)
        {
            SC_Error(sc, "Unknown property '%s'.", prop_name);
            return;
        }

        if (type == prop_sound_prefix)
        {
            SC_MustGetToken(sc, TK_BoolConst);
            sound.prefix = SC_GetBoolean(sc);
        }
        else
        {
            sndpropvalue_t value = {0};
            switch (decl_sound_props[type].valtype)
            {
                case TYPE_Int:
                    value.number = DECL_GetNegativeInteger(sc);
                    break;
                case TYPE_Float:
                    SC_MustGetToken(sc, TK_FloatConst);
                    value.decimal = SC_GetDecimal(sc);
                    break;
                case TYPE_String:
                    SC_MustGetToken(sc, TK_StringConst);
                    value.string = M_StringDuplicate(SC_GetString(sc));
                    break;
                case TYPE_ListOfStrings:
                    do
                    {
                        SC_MustGetToken(sc, TK_StringConst);
                        char *string = M_StringDuplicate(SC_GetString(sc));
                        M_StringToLower(string);
                        array_push(value.list, string);
                    } while (SC_CheckToken(sc, ','));
                    break;
                case TYPE_Sound:
                    break;
                default:
                    break;
            }

            sndproperty_t *old_prop = GetProp(&sound, type);
            if (old_prop)
            {
                old_prop->value = value;
            }
            else
            {
                sndproperty_t new_prop = {.type = type, .value = value};
                array_push(sound.props, new_prop);
            }
        }
    }

    if (!sounds)
    {
        sounds = hashmap_init_str(32, sizeof(sound_t));
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
    sound_t *sound;
    hashmap_foreach(sound, sounds)
    {
        sndproperty_t *prop = GetProp(sound, prop_sound_lump);

        boolean random = (array_size(prop->value.list) > 1);
        random_sound_t random_sound = {0};

        for (int i = 0; i < array_size(prop->value.list); ++i)
        {
            char *lumpname = M_StringDuplicate(prop->value.list[i]);
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
        sound_t *sound = hashmap_get_str(sounds, name);
        free(name);
        if (sound)
        {
            return sound->sfx_number;
        }
    }

    I_Error("Sound '%s' is not defined.", sound_name);
    return sfx_None;
}

int DECL_RandomSound(int sfx_number)
{
    if (random_sounds)
    {
        random_sound_t *sound = hashmap_get(random_sounds, sfx_number);
        if (sound)
        {
            int index = P_Random(pr_misc) % array_size(sound->sfx);
            return sound->sfx[index];
        }
    }

    I_Error("Random sound %d not found.", sfx_number);
    return sfx_None;
}
