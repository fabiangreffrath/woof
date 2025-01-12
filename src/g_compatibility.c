//
// Copyright(C) 2021 by Ryan Krafnick
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
//  DSDA Compatibility
//

#include <stdio.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"

#include "m_json.h"
#include "md5.h"

static const char *comp_names[] =
{
    [comp_telefrag] = "comp_telefrag",
    [comp_dropoff] = "comp_dropoff",
    [comp_vile] = "comp_vile",
    [comp_pain] = "comp_pain",
    [comp_skull] = "comp_skull",
    [comp_blazing] = "comp_blazing",
    [comp_doorlight] = "comp_doorlight",
    [comp_model] = "comp_model",
    [comp_god] = "comp_god",
    [comp_falloff] = "comp_falloff",
    [comp_floors] = "comp_floors",
    [comp_skymap] = "comp_skymap",
    [comp_pursuit] = "comp_pursuit",
    [comp_doorstuck] = "comp_doorstuck",
    [comp_staylift] = "comp_staylift",
    [comp_zombie] = "comp_zombie",
    [comp_stairs] = "comp_stairs",
    [comp_infcheat] = "comp_infcheat",
    [comp_zerotags] = "comp_zerotags",
    // from PrBoom+/Eternity Engine (part of mbf21 spec)
    [comp_respawn] = "comp_respawn",
    [comp_soul] = "comp_soul",
    // mbf21
    [comp_ledgeblock] = "comp_ledgeblock",
    [comp_friendlyspawn] = "comp_friendlyspawn",
    [comp_voodooscroller] = "comp_voodooscroller",
    [comp_reservedlineflag] = "comp_reservedlineflag"
};

typedef byte md5_digest_t[16];

typedef struct
{
    md5_digest_t digest;
    char string[33];
} md5_checksum_t;

typedef struct
{
    int comp;
    int value;
} option_t;

typedef struct
{
    md5_digest_t checksum;
    option_t *options;
    char *complevel;
} comp_record_t;

static comp_record_t *comp_database;

static int GetComp(const char *name)
{
    for (int i = 0; i < arrlen(comp_names); ++i)
    {
        if (!strcmp(comp_names[i], name))
        {
            return i;
        }
    }
    return -1;
}

void G_ParseCompDatabase(void)
{
    json_t *json = JS_Open("COMPDB", "compatibility", (version_t){1, 0, 0});
    if (json == NULL)
    {
        return;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_ERROR, "COMPDB: no data");
        JS_Close("COMPDB");
        return;
    }

    json_t *levels = JS_GetObject(data, "levels");
    json_t *level = NULL;
    JS_ArrayForEach(level, levels)
    {
        comp_record_t record = {0};

        const char *md5 = JS_GetStringValue(level, "md5");
        if (!md5)
        {
            continue;
        }
        if (!M_StringToDigest(md5, record.checksum, sizeof(md5_digest_t)))
        {
            I_Printf(VB_ERROR, "COMPDB: wrong key %s", md5);
            continue;
        }

        record.complevel = NULL;
        const char *complevel = JS_GetStringValue(level, "complevel");
        if (complevel)
        {
            record.complevel = M_StringDuplicate(complevel);
        }

        json_t *js_options = JS_GetObject(level, "options");
        json_t *js_option = NULL;
        JS_ArrayForEach(js_option, js_options)
        {
            option_t option = {0};
            const char *name = JS_GetStringValue(js_option, "name");
            if (!name)
            {
                continue;
            }
            int comp = GetComp(name);
            if (comp < 0)
            {
                continue;
            }
            option.comp = comp;
            option.value = JS_GetIntegerValue(js_option, "value");
            array_push(record.options, option);
        }
        array_push(comp_database, record);
    }

    JS_Close("COMPDB");
}

static void MD5UpdateLump(int lump, struct MD5Context *md5)
{
    MD5Update(md5, W_CacheLumpNum(lump, PU_CACHE), W_LumpLength(lump));
}

static void GetLevelCheckSum(int lump, md5_checksum_t* cksum)
{
    struct MD5Context md5;

    MD5Init(&md5);

    MD5UpdateLump(lump + ML_LABEL, &md5);
    MD5UpdateLump(lump + ML_THINGS, &md5);
    MD5UpdateLump(lump + ML_LINEDEFS, &md5);
    MD5UpdateLump(lump + ML_SIDEDEFS, &md5);
    MD5UpdateLump(lump + ML_SECTORS, &md5);

    // ML_BEHAVIOR when it becomes applicable to comp options

    MD5Final(cksum->digest, &md5);

    for (int i = 0; i < sizeof(cksum->digest); ++i)
    {
        sprintf(&cksum->string[i * 2], "%02x", cksum->digest[i]);
    }
    cksum->string[32] = '\0';
}

// For casual players that aren't careful about setting complevels, this
// function will apply comp options to automatically fix some issues that
// appear when playing wads in mbf21 (since this is the default).

void G_ApplyLevelCompatibility(int lump)
{
    static boolean restore_comp;
    static int old_comp[COMP_TOTAL];

    if (restore_comp)
    {
        if (demo_version != DV_MBF21)
        {
            demo_version = DV_MBF21;
            G_ReloadDefaults(true);
        }
        memcpy(comp, old_comp, sizeof(*comp));
        restore_comp = false;
    }
    else if (demorecording || demoplayback || netgame || !mbf21)
    {
        return;
    }

    md5_checksum_t cksum;

    GetLevelCheckSum(lump, &cksum);

    I_Printf(VB_DEBUG, "Level checksum: %s", cksum.string);

    comp_record_t *record;
    array_foreach(record, comp_database)
    {
        if (!memcmp(record->checksum, cksum.digest, sizeof(md5_digest_t)))
        {
            memcpy(old_comp, comp, sizeof(*comp));
            restore_comp = true;

            char *new_demover = record->complevel;
            if (new_demover)
            {
                demo_version = G_GetNamedComplevel(new_demover);
                G_ReloadDefaults(true);
                I_Printf(VB_INFO, "Automatically setting compatibility level \"%s\"",
                         G_GetCurrentComplevelName());
            }

            if (!mbf21)
            {
                return;
            }

            option_t *option;
            array_foreach(option, record->options)
            {
                comp[option->comp] = option->value;

                I_Printf(VB_INFO, "Automatically setting comp option \"%s = %d\"",
                         comp_names[option->comp], option->value);
            }

            return;
        }
    }
}
