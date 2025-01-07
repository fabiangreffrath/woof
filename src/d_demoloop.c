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
//    ID24 DemoLoop

#include "doomdef.h"
#include "doomstat.h"

#include "i_printf.h"
#include "m_array.h"
#include "m_json.h"
#include "w_wad.h"

#include "d_demoloop.h"

// DEMO1 lump playback.
static const demoloop_entry_t dl_demo1 = {
    "DEMO1",
    "",
    0,
    TYPE_DEMO_LUMP,
    OUTRO_WIPE_SCREEM_MELT,
};

// DEMO2 lump playback.
static const demoloop_entry_t dl_demo2 = {
    "DEMO2",
    "",
    0,
    TYPE_DEMO_LUMP,
    OUTRO_WIPE_SCREEM_MELT,
};

// DEMO3 lump playback.
static const demoloop_entry_t dl_demo3 = {
    "DEMO3",
    "",
    0,
    TYPE_DEMO_LUMP,
    OUTRO_WIPE_SCREEM_MELT,
};

// DEMO4 lump, only present in Ultimate Doom.
// The DEMO4 entry crashed in the original Final Doom EXE's loop.
static const demoloop_entry_t dl_demo4 = {
    "DEMO4",
    "",
    0,
    TYPE_DEMO_LUMP,
    OUTRO_WIPE_SCREEM_MELT,
};

// Oddly, Doom's TITLEPIC lasts for a brifer period than Doom II.
// About ~4.857 seconds, as opposed to Doom II's exact 11 seconds.
static const demoloop_entry_t dl_doom1_titlepic = {
    "TITLEPIC",
    "D_INTRO",
    170,
    TYPE_ART_SCREEN,
    OUTRO_WIPE_SCREEM_MELT,
};

// HELP2 titlescreen lasts for about ~5.714 seconds.
// Used only in Registered mode.
static const demoloop_entry_t dl_help2 = {
    "HELP2",
    "",
    200,
    TYPE_ART_SCREEN,
    OUTRO_WIPE_SCREEM_MELT,
};

// Plain Doom II, Commercial mode, TITLEPIC lasts for exactly 11.0 seconds.
static const demoloop_entry_t dl_doom2_titlepic = {
    "TITLEPIC",
    "D_DM2TTL",
    385,
    TYPE_ART_SCREEN,
    OUTRO_WIPE_SCREEM_MELT,
};

// CREDIT titlescreen lasts for about ~5.714 seconds.
// Used in Retail and Commercial mode.
static const demoloop_entry_t dl_credit = {
    "CREDIT",
    "",
    200,
    TYPE_ART_SCREEN,
    OUTRO_WIPE_SCREEM_MELT,
};

// Used to check for fault tolerance
static const demoloop_entry_t dl_none = {
    "",
    "",
    0,
    TYPE_NONE,
    OUTRO_WIPE_NONE,
};

// Doom
demoloop_entry_t demoloop_registered[6] = {
    dl_doom1_titlepic,
    dl_demo1,
    dl_credit,
    dl_demo2,
    dl_help2,
    dl_demo3,
};

// Ultiamte Doom
demoloop_entry_t demoloop_retail[7] = {
    dl_doom1_titlepic,
    dl_demo1,
    dl_credit,
    dl_demo2,
    dl_credit,
    dl_demo3,
    dl_demo4,
};

// Doom II & fixed Final Doom
demoloop_entry_t demoloop_commercial[6] = {
    dl_doom2_titlepic,
    dl_demo1,
    dl_credit,
    dl_demo2,
    dl_doom2_titlepic,
    dl_demo3,
};

// TNT: Evilution & The Plutonia Experiement
demoloop_entry_t demoloop_final[7] = {
    dl_doom2_titlepic,
    dl_demo1,
    dl_credit,
    dl_demo2,
    dl_doom2_titlepic,
    dl_demo3,
    dl_demo4,
};

// For local parsing purposes only.
static demoloop_entry_t current_entry;

demoloop_t demoloop;
int        demoloop_count = 0;
int        demoloop_current = -1;


// TODO: actually finish this and make it work
demoloop_entry_t D_ParseDemoLoopEntry(json_t *entry)
{
    const char* primary_lump   = JS_GetStringValue(entry, "primarylump");
    const char* secondary_lump = JS_GetStringValue(entry, "secondarylump");
    double      seconds        = JS_GetNumberValue(entry, "duration");
    int         type           = JS_GetIntegerValue(entry, "type");
    int         outro_wipe     = JS_GetIntegerValue(entry, "outro_wipe");

    // We don't want a malformed entry to creep in, and break the titlescreen.
    // If one such entry does exist, skip it.
    // TODO: modify later to check locally for lump type.
    if (type <= TYPE_NONE || type > TYPE_DEMO_LUMP)
    {
        return dl_none;
    }

    // Similarly, but this time it isn't game-breaking.
    // Let it gracefully default to "closest vanilla behavior".
    if (outro_wipe <= OUTRO_WIPE_NONE || outro_wipe > OUTRO_WIPE_SCREEM_MELT)
    {
        outro_wipe = OUTRO_WIPE_SCREEM_MELT;
    }

    // Providing the time in seconds is much more intuitive for the end users.
    int duration = seconds * TICRATE;

    demoloop_entry_t demoloop_entry = {
        primary_lump,
        secondary_lump,
        duration,
        type,
        outro_wipe,
    };

    return demoloop_entry;
}

void D_ParseDemoLoop(void)
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
        current_entry = D_ParseDemoLoopEntry(entry);

        // Should there be a malformed entry, discard it.
        if (current_entry.type == TYPE_NONE)
        {
            continue;
        }

        array_push(demoloop, current_entry);
        demoloop_count++;
    }

    return;
}

void D_GetDefaultDemoLoop(GameMission_t mission, GameMode_t mode)
{
    switch(mission) {
        case doom:
            // The versions of Doom that have HELP2, instead.
            if (mode == registered || mode == shareware) {
                demoloop = demoloop_registered;
                demoloop_count = 6;
                break;
            }
        case pack_chex3v:
            // Check for chex3d2.wad, "Chex Quest 3: Modding Edition".
            if (mode == commercial) {
                demoloop = demoloop_commercial;
                demoloop_count = 6;
                break;
            }
        case pack_rekkr:
        case pack_chex:
            // Plain Ultimate Doom
            demoloop = demoloop_retail;
            demoloop_count = 7;
            break;

        case doom2:
        case pack_hacx:
            // Plain Doom II
            demoloop = demoloop_commercial;
            demoloop_count = 6;
            break;

        case pack_tnt:
        case pack_plut:
            // Plain Final Doom
            demoloop = demoloop_final;
            demoloop_count = 7;
            break;

        case none:
        default:
            // How did we get here?
            demoloop = NULL;
            demoloop_count = 0;
            break;
    }

    return;
}

void D_SetupDemoLoop(void) {
    if (W_CheckNumForName("DEMOLOOP") >= 0)
    {
        D_ParseDemoLoop();
    }

    if (demoloop == NULL)
    {
        D_GetDefaultDemoLoop(gamemission, gamemode);
    }
}
