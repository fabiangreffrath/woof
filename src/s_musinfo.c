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
#include "g_umapinfo.h"
#include "i_printf.h"
#include "p_mobj.h"
#include "s_sound.h"
#include "m_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

musinfo_t musinfo = {0};

//
// S_ParseMusInfo
// Parses MUSINFO lump.
//

void S_ParseMusInfo(const char *mapid)
{
    int lumpnum = W_CheckNumForName("MUSINFO");
    if (lumpnum < 0)
    {
        return;
    }

    scanner_t *s = SC_Open("MUSINFO", W_CacheLumpNum(lumpnum, PU_CACHE),
                           W_LumpLength(lumpnum));

    while (SC_TokensLeft(s))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (!strcasecmp(SC_GetString(s), mapid))
            {
                break;
            }
        }
        else
        {
            SC_GetNextLineToken(s);
        }
    }

    while (SC_TokensLeft(s))
    {
        if (SC_CheckToken(s, TK_Identifier))
        {
            if (G_ValidateMapName(SC_GetString(s), NULL, NULL))
            {
                break;
            }
        }
        else if (SC_CheckToken(s, TK_IntConst))
        {
            int num = SC_GetNumber(s);
            // Check number in range
            if (num > 0 && num < MAX_MUS_ENTRIES)
            {
                SC_MustGetToken(s, TK_RawString);
                lumpnum = W_CheckNumForName(SC_GetString(s));
                if (lumpnum > 0)
                {
                    musinfo.items[num] = lumpnum;
                }
                else
                {
                    I_Printf(VB_WARNING, "S_ParseMusInfo: Unknown MUS lump %s",
                             SC_GetString(s));
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
            SC_GetNextLineToken(s);
        }
    }

    SC_Close(s);
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
