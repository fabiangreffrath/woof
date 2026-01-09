//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
// Copyright(C) 2025 Guilherme Miranda
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
// Parses [PARS] sections in BEX files
//

#include <stdio.h>
#include <string.h>

#include "deh_bex_partimes.h"
#include "deh_io.h"

boolean bex_partimes = false;

int bex_pars[][9] = {
    {30,  75,  120, 90,  165, 180, 180, 30,  165},
    {90,  90,  90,  120, 90,  360, 240, 30,  170},
    {90,  45,  90,  150, 90,  90,  165, 30,  135},
    {165, 255, 135, 150, 180, 390, 135, 360, 180}, // E4:BFG
    {90,  150, 360, 420, 780, 420, 780, 300, 660}, // Sigil v1.21
    {480, 300, 240, 420, 510, 840, 960, 390, 450}, // Sigil II v1.0
};

int bex_cpars[] =
{
    30,  90,  120, 120, 90,  150, 120, 120, 270, 90, 210,
    150, 150, 150, 210, 150, 420, 150, 210, 150,
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180, 120, 30,
    30,  30, 30, // MAP 33, 34
};

static void *DEH_BEXPartimesStart(deh_context_t *context, char *line)
{
    char s[7];

    if (sscanf(line, "%6s", s) == 0 || strcmp("[PARS]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXPartimesParseLine(deh_context_t *context, char *line, void *tag)
{
    int episode, map, partime;

    if (sscanf(line, "par %32d %32d %32d", &episode, &map, &partime) == 3)
    {
        // E4:BFG, Sigil, Sigil II
        if (episode >= 1 && episode <= 6 && map >= 1 && map <= 9)
        {
            bex_pars[episode - 1][map - 1] = partime;
            bex_partimes |= true;
        }
        else
        {
            DEH_Warning(context, "Invalid episode or map: E%dM%d", episode, map);
            return;
        }
    }
    else if (sscanf(line, "par %32d %32d", &map, &partime) == 2)
    {
        // Added maps 33 & 34
        if (map >= 1 && map <= 34)
        {
            bex_cpars[map - 1] = partime;
            bex_partimes |= true;
        }
        else
        {
            DEH_Warning(context, "Invalid map: MAP%02d", map);
            return;
        }
    }
    else
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }
}

deh_section_t deh_section_bex_partimes =
{
    "[PARS]",
    NULL,
    DEH_BEXPartimesStart,
    DEH_BEXPartimesParseLine,
    NULL,
    NULL,
};
