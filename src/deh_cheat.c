//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Parses "Cheat" sections in dehacked files
//

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "m_cheat.h"

typedef struct
{
    const char *name;
    struct cheat_s *seq;
} deh_cheat_t;

static deh_cheat_t allcheats[] = {
    {"Change music",     NULL},
    {"Chainsaw",         NULL},
    {"God mode",         NULL},
    {"Ammo & Keys",      NULL},
    {"Ammo",             NULL},
    {"No Clipping 1",    NULL},
    {"No Clipping 2",    NULL},
    {"Invincibility",    NULL},
    {"Berserk",          NULL},
    {"Invisibility",     NULL},
    {"Radiation Suit",   NULL},
    {"Auto-map",         NULL},
    {"Lite-Amp Goggles", NULL},
    {"BEHOLD menu",      NULL},
    {"Level Warp",       NULL},
    {"Player Position",  NULL},
    {"Map cheat",        NULL},
};

static deh_cheat_t *FindCheatByName(char *name)
{
    for (size_t i = 0; i < arrlen(allcheats); ++i)
    {
        if (!strcasecmp(allcheats[i].name, name))
        {
            return &allcheats[i];
        }
    }

    return NULL;
}

static void *DEH_CheatStart(deh_context_t *context, char *line)
{
    return NULL;
}

static void DEH_CheatParseLine(deh_context_t *context, char *line, void *tag)
{
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    deh_cheat_t *cheat = FindCheatByName(variable_name);
    if (cheat == NULL)
    {
        DEH_Warning(context, "Unknown cheat '%s'", variable_name);
        return;
    }

    // TODO: currently no-op, need to merge inmore changes
}

deh_section_t deh_section_cheat =
{
    "Cheat",
    NULL,
    DEH_CheatStart,
    DEH_CheatParseLine,
    NULL,
    NULL,
};
