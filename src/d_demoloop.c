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

#include "doomdef.h"
#include "doomstat.h"

#include "i_printf.h"
#include "m_array.h"
#include "m_json.h"
#include "m_misc.h"

#include "d_demoloop.h"

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

// For local parsing purposes only.
static demoloop_entry_t current_entry;

demoloop_t demoloop;
int        demoloop_count = 0;

void D_ParseDemoLoopEntry(json_t *entry)
{
    const char* primary_buffer   = JS_GetStringValue(entry, "primarylump");
    const char* secondary_buffer = JS_GetStringValue(entry, "secondarylump");
    double      seconds          = JS_GetNumberValue(entry, "duration");
    int         type             = JS_GetIntegerValue(entry, "type");
    int         outro_wipe       = JS_GetIntegerValue(entry, "outro_wipe");

    // We don't want a malformed entry to creep in, and break the titlescreen.
    // If one such entry does exist, skip it.
    // TODO: modify later to check locally for lump type.
    if (primary_buffer == NULL || type <= TYPE_NONE || type > TYPE_DEMO)
    {
        primary_buffer = "";
        type = TYPE_NONE;
    }

    // Similarly, but this time it isn't game-breaking.
    // Let it gracefully default to "closest vanilla behavior".
    if (outro_wipe <= WIPE_NONE || outro_wipe > WIPE_MELT)
    {
        outro_wipe = WIPE_MELT;
    }

    // Remove pointer reference to in-memory JSON data.
    M_CopyLumpName(current_entry.primary_lump, primary_buffer);
    M_CopyLumpName(current_entry.secondary_lump, secondary_buffer);
    // Providing the time in seconds is much more intuitive for the end users.
    current_entry.duration   = seconds * TICRATE;
    current_entry.type       = type;
    current_entry.outro_wipe = outro_wipe;
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
        D_ParseDemoLoopEntry(entry);

        // Should there be a malformed entry, discard it.
        if (current_entry.type == TYPE_NONE)
        {
            continue;
        }

        array_push(demoloop, current_entry);
        demoloop_count++;
    }

    // No need to keep in memory
    JS_Close("DEMOLOOP");
    return;
}

void D_GetDefaultDemoLoop(GameMission_t mission, GameMode_t mode)
{
    switch(mode)
    {
        case shareware:
        case registered:
            demoloop = demoloop_registered;
            demoloop_count = arrlen(demoloop_registered);
            break;

        case retail:
            demoloop = demoloop_retail;
            demoloop_count = arrlen(demoloop_retail);
            break;

        case commercial:
            demoloop = demoloop_commercial;
            demoloop_count = arrlen(demoloop_commercial);
            break;

        case indetermined:
        default:
            // How did we get here?
            demoloop = NULL;
            break;
    }

    return;
}

void D_SetupDemoLoop(void) {
    D_ParseDemoLoop();

    if (demoloop == NULL)
    {
        D_GetDefaultDemoLoop(gamemission, gamemode);
    }
}
