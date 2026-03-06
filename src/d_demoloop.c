//
//  Copyright (C) 2025 Guilherme Miranda
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
//  DESCRIPTION:
//    ID24 DemoLoop Specification - User customizable title screen sequence.
//    Though originally hardcoded on the original Doom engine this allows
//    for Doom modders to define custom sequences, using any provided custom
//    graphic-and-music, or DEMO lump.
//

#include <math.h>

#include "doomdef.h"
#include "doomstat.h"

#include "i_printf.h"
#include "m_array.h"
#include "m_json.h"
#include "m_misc.h"
#include "sounds.h"
#include "w_wad.h"

#include "d_demoloop.h"

// Support for DeHackEd Text substitution, fixes PL2.WAD
#define DEH_MUSIC_LUMP(buffer, mus_id) \
            M_snprintf(buffer, sizeof buffer, "d_%s", S_music[mus_id].name);

// Doom
static demoloop_entry_t demoloop_registered[] = {
    { "TITLEPIC", "D_INTRO",  170, TYPE_ART,  WIPE_MELT },
    { "DEMO1",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "CREDIT",   "",         200, TYPE_ART,  WIPE_MELT },
    { "DEMO2",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "HELP2",    "",         200, TYPE_ART,  WIPE_MELT },
    { "DEMO3",    "",         0,   TYPE_DEMO, WIPE_MELT },
};

// Ultimate Doom
static demoloop_entry_t demoloop_retail[] = {
    { "TITLEPIC", "D_INTRO",  170, TYPE_ART,  WIPE_MELT },
    { "DEMO1",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "CREDIT",   "",         200, TYPE_ART,  WIPE_MELT },
    { "DEMO2",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "CREDIT",   "",         200, TYPE_ART,  WIPE_MELT },
    { "DEMO3",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "DEMO4",    "",         0,   TYPE_DEMO, WIPE_MELT },
};

// Doom II & Final Doom
static demoloop_entry_t demoloop_commercial[] = {
    { "TITLEPIC", "D_DM2TTL", 385, TYPE_ART,  WIPE_MELT },
    { "DEMO1",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "CREDIT",   "",         200, TYPE_ART,  WIPE_MELT },
    { "DEMO2",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "TITLEPIC", "D_DM2TTL", 385, TYPE_ART,  WIPE_MELT },
    { "DEMO3",    "",         0,   TYPE_DEMO, WIPE_MELT },
    { "DEMO4",    "",         0,   TYPE_DEMO, WIPE_MELT },
};

demoloop_t demoloop = NULL;
int        demoloop_count = 0;

static void D_ParseOutroWipe(json_t *json, demoloop_entry_t *entry)
{
    entry->outro_wipe = JS_GetIntegerValue(json, "outrowipe");

    if (entry->outro_wipe != WIPE_IMMEDIATE && entry->outro_wipe != WIPE_MELT)
    {
        I_Printf(VB_WARNING, "DEMOLOOP: invalid outrowipe, using screen melt");
        entry->outro_wipe = WIPE_MELT;
    }
}

static void D_ParseDuration(json_t *json, demoloop_entry_t *entry)
{
    const double duration_seconds = JS_GetNumberValue(json, "duration");
    double duration_tics = duration_seconds * TICRATE;
    duration_tics = CLAMP(duration_tics, 0, INT_MAX);
    entry->duration = lround(duration_tics);
}

static boolean D_ParseSecondaryLump(json_t *json, demoloop_entry_t *entry)
{
    const char *secondary_lump = JS_GetStringValue(json, "secondarylump");

    if (secondary_lump == NULL || secondary_lump[0] == '\0')
    {
        // Secondary lump is optional.
        entry->secondary_lump[0] = '\0';
    }
    else
    {
        M_CopyLumpName(entry->secondary_lump, secondary_lump);

        if (W_CheckNumForName(entry->secondary_lump) < 0)
        {
            I_Printf(VB_WARNING, "DEMOLOOP: invalid secondarylump");
            return false;
        }
    }

    return true;
}

static boolean D_ParsePrimaryLump(json_t *json, demoloop_entry_t *entry)
{
    const char *primary_lump = JS_GetStringValue(json, "primarylump");

    if (primary_lump == NULL || primary_lump[0] == '\0')
    {
        I_Printf(VB_WARNING, "DEMOLOOP: undefined primarylump");
        return false;
    }

    M_CopyLumpName(entry->primary_lump, primary_lump);
    return true;
}

static boolean D_ParseDemoLoopEntry(json_t *json)
{
    demoloop_entry_t entry = {0};

    entry.type = JS_GetIntegerValue(json, "type");

    switch (entry.type)
    {
        case TYPE_ART:
            if (!D_ParsePrimaryLump(json, &entry)
                || !D_ParseSecondaryLump(json, &entry))
            {
                return false;
            }
            D_ParseDuration(json, &entry);
            D_ParseOutroWipe(json, &entry);
            break;

        case TYPE_DEMO:
            if (!D_ParsePrimaryLump(json, &entry))
            {
                return false;
            }
            entry.secondary_lump[0] = '\0';
            entry.duration = 0;
            D_ParseOutroWipe(json, &entry);
            break;

        default:
            I_Printf(VB_WARNING, "DEMOLOOP: invalid type");
            return false;
    }

    array_push(demoloop, entry);
    return true;
}

static void D_ParseDemoLoop(void)
{
    // Does the JSON lump even exist?
    json_t *json = JS_Open("DEMOLOOP", "demoloop", (version_t){1, 0, 0});
    if (json == NULL)
    {
        return;
    }

    // Does lump actually have any data?
    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_WARNING, "DEMOLOOP: data object not defined");
        JS_Close("DEMOLOOP");
        return;
    }

    // Does is it even have the definitions we are looking for?
    json_t *entry_list = JS_GetObject(data, "entries");
    if (JS_IsNull(entry_list) || !JS_IsArray(entry_list))
    {
        I_Printf(VB_WARNING, "DEMOLOOP: no entries defined");
        JS_Close("DEMOLOOP");
        return;
    }

    // If so, now parse them.
    json_t *entry;
    JS_ArrayForEach(entry, entry_list)
    {
        if (!D_ParseDemoLoopEntry(entry))
        {
            array_free(demoloop);
            JS_Close("DEMOLOOP");
            return;
        }
    }
    demoloop_count = array_size(demoloop);

    // No need to keep in memory
    JS_Close("DEMOLOOP");
}

static void D_GetDefaultDemoLoop(GameMode_t mode)
{
    switch(mode)
    {
        case shareware:
        case registered:
            DEH_MUSIC_LUMP(demoloop_registered[0].secondary_lump, mus_intro)

            demoloop = demoloop_registered;
            demoloop_count = arrlen(demoloop_registered);
            break;

        case retail:
            DEH_MUSIC_LUMP(demoloop_retail[0].secondary_lump, mus_intro)

            if (pwad_help2)
            {
                M_CopyLumpName(demoloop_retail[4].primary_lump, "HELP2");
            }

            demoloop = demoloop_retail;
            demoloop_count = arrlen(demoloop_retail);
            break;

        case commercial:
            DEH_MUSIC_LUMP(demoloop_commercial[0].secondary_lump, mus_dm2ttl)
            DEH_MUSIC_LUMP(demoloop_commercial[4].secondary_lump, mus_dm2ttl)

            demoloop = demoloop_commercial;
            demoloop_count = arrlen(demoloop_commercial);

            if (W_CheckNumForName(demoloop_commercial[6].primary_lump) < 0)
            {
                // Ignore missing DEMO4.
                demoloop_count--;
            }
            break;

        default:
            I_Error("Invalid gamemode");
            break;
    }
}

static void D_TitlePicFix(void)
{
    if (W_CheckNumForName("TITLEPIC") < 0 && W_CheckNumForName("DMENUPIC") >= 0)
    {
        for (int i = 0; i < demoloop_count; i++)
        {
            demoloop_entry_t *entry = &demoloop[i];

            if (!strcasecmp(entry->primary_lump, "TITLEPIC"))
            {
                // Workaround for Doom 3: BFG Edition.
                M_CopyLumpName(entry->primary_lump, "DMENUPIC");
            }
        }
    }
}

static void D_CheckPrimaryLumps(void)
{
    for (int i = 0; i < demoloop_count; i++)
    {
        demoloop_entry_t *entry = &demoloop[i];

        if (W_CheckNumForName(entry->primary_lump) < 0)
        {
            I_Printf(VB_WARNING, "DEMOLOOP: invalid primarylump");
            array_free(demoloop);
            break;
        }
    }
}

void D_SetupDemoLoop(void)
{
    D_ParseDemoLoop();

    if (demoloop)
    {
        D_TitlePicFix();
        D_CheckPrimaryLumps();
    }

    if (demoloop == NULL)
    {
        D_GetDefaultDemoLoop(gamemode);
        D_TitlePicFix();
    }
}
