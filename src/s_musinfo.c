//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2005-2006 Florian Schulze, Colin Phipps, Neil Stevens, Andrey
// Budko
// Copyright(C) 2017 Fabian Greffrath
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
//     support MUSINFO lump (dynamic music changing)
//

#include "s_musinfo.h"

#include "doomtype.h"
#include "g_game.h"
#include "i_printf.h"
#include "p_mobj.h"
#include "s_sound.h"
#include "u_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

musinfo_t musinfo = {0};

//
// S_ParseMusInfo
// Parses MUSINFO lump.
//

void S_ParseMusInfo(const char *mapid)
{
    u_scanner_t *s;
    int num, lumpnum;

    lumpnum = W_CheckNumForName("MUSINFO");

    if (lumpnum < 0)
    {
        return;
    }

    s = U_ScanOpen(W_CacheLumpNum(lumpnum, PU_CACHE), W_LumpLength(lumpnum),
                   "MUSINFO");

    while (U_HasTokensLeft(s))
    {
        if (U_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(s->string, mapid))
            {
                break;
            }
        }
        else
        {
            U_GetNextLineToken(s);
        }
    }

    while (U_HasTokensLeft(s))
    {
        if (U_CheckToken(s, TK_Identifier))
        {
            if (G_ValidateMapName(s->string, NULL, NULL))
            {
                break;
            }
        }
        else if (U_CheckInteger(s))
        {
            num = s->number;
            // Check number in range
            if (num > 0 && num < MAX_MUS_ENTRIES)
            {
                U_GetString(s);
                lumpnum = W_CheckNumForName(s->string);
                if (lumpnum > 0)
                {
                    musinfo.items[num] = lumpnum;
                }
                else
                {
                    I_Printf(VB_WARNING, "S_ParseMusInfo: Unknown MUS lump %s",
                             s->string);
                }
            }
            else
            {
                I_Printf(VB_WARNING,
                         "S_ParseMusInfo: Number not in range 1 to %d",
                         MAX_MUS_ENTRIES - 1);
            }
        }
        else
        {
            U_GetNextLineToken(s);
        }
    }

    U_ScanClose(s);
}

void T_MusInfo(void)
{
    if (musinfo.tics < 0 || !musinfo.mapthing)
    {
        return;
    }

    if (musinfo.tics > 0)
    {
        musinfo.tics--;
    }
    else
    {
        if (!musinfo.tics && musinfo.lastmapthing != musinfo.mapthing)
        {
            // [crispy] encode music lump number in mapthing health
            int arraypt = musinfo.mapthing->health - 1000;

            if (arraypt >= 0 && arraypt < MAX_MUS_ENTRIES)
            {
                int lumpnum = musinfo.items[arraypt];

                if (lumpnum > 0 && lumpnum < numlumps)
                {
                    S_ChangeMusInfoMusic(lumpnum, true);
                }
            }

            musinfo.tics = -1;
        }
    }
}
