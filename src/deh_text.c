//
// Copyright(C) 2005-2014 Simon Howard
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
// Parses Text substitution sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_englsh.h"
#include "d_main.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "m_misc.h"

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static const char *const banners[] = {
    [banner_indetermined] = BANNER_INDETERMINED,
    [banner_shareware] = BANNER_SHAREWARE,
    [banner_1_666_system] = BANNER_1_666_SYSTEM,
    [banner_1_9_system] = BANNER_1_9_SYSTEM,
    [banner_1_9_registered] = BANNER_1_9_REGISTERED,
    [banner_1_9_special] = BANNER_1_9_SPECIAL,
    [banner_ultimate] = BANNER_ULTIMATE,
};

static const char *const cbanners[] = {
    [cbanner_1_666] = CBANNER_1_666,
    [cbanner_1_9] = CBANNER_1_9,
    [cbanner_tnt] = CBANNER_TNT,
    [cbanner_plutonia] = CBANNER_PLUTONIA,
};

const char *GetBanner(void)
{
    const char *banner = NULL;

    if (gamemode == commercial)
    {
        size_t i = (gamemission == pack_plut)  ? cbanner_plutonia
                   : (gamemission == pack_tnt) ? cbanner_tnt
                                               : cbanner_1_9;
        banner = cbanners[i];
        
    }
    else if (gamemode == retail)
    {
        banner = banners[banner_ultimate];
    }
    else if (gamemode == registered)
    {
        banner = banners[banner_1_9_registered];
    }
    else if (gamemode == shareware)
    {
        banner = banners[banner_shareware];
    }
    else
    {
        banner = banners[banner_indetermined];
    }

    return banner;
}

void DEH_SetBannerGameDescription(void)
{
    const char *banner = GetBanner();

    if (banner && DEH_HasStringReplacement(banner))
    {
        // We only support version 1.9 of Vanilla Doom
        const int version = DV_VANILLA;
        char *deh_gamename = M_StringDuplicate(DEH_String(banner));
        char *fp = deh_gamename;

        // Expand "%i" in deh_gamename to include the Doom version number.
        // We cannot use sprintf() here, because deh_gamename isn't a string
        // literal.
        fp = strstr(fp, "%i");
        if (fp)
        {
            *fp++ = '0' + version / 100;
            memmove(fp, fp + 1, strlen(fp));

            fp = strstr(fp, "%i");
            if (fp)
            {
                *fp++ = '0' + version % 100;
                memmove(fp, fp + 1, strlen(fp));
            }
        }
        // Cut off trailing and leading spaces to get the basic name
        gamedescription = CleanString(deh_gamename);
    }
}

// [crispy] support INCLUDE NOTEXT directive in BEX files
boolean bex_notext = false;

static void *DEH_TextStart(deh_context_t *context, char *line)
{
    int from_len, to_len;

    if (sscanf(line, "Text %i %i", &from_len, &to_len) != 2)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // Skip text section
    if (bex_notext)
    {
        for (int i = 0; i < from_len; i++)
        {
            DEH_GetChar(context);
        }
        for (int i = 0; i < to_len; i++)
        {
            DEH_GetChar(context);
        }
        return NULL;
    }

    char *from_text = malloc(from_len + 1);
    char *to_text = malloc(to_len + 1);

    // read in the "from" text
    for (int i = 0; i < from_len; ++i)
    {
        from_text[i] = DEH_GetChar(context);
    }
    from_text[from_len] = '\0';

    // read in the "to" text
    for (int i = 0; i < to_len; ++i)
    {
        to_text[i] = DEH_GetChar(context);
    }
    to_text[to_len] = '\0';

    DEH_AddStringReplacement(from_text, to_text);

    free(from_text);
    free(to_text);

    return NULL;
}

static void DEH_TextParseLine(deh_context_t *context, char *line, void *tag)
{
    // not used
}

deh_section_t deh_section_text =
{
    "Text",
    NULL,
    DEH_TextStart,
    DEH_TextParseLine,
    NULL,
    NULL,
};
