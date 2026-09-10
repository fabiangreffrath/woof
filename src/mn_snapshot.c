//
//  Copyright (C) 2022-2026 Fabian Greffrath
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
// DESCRIPTION:
//      Savegame snapshots
//

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_io.h"
#include "r_main.h"
#include "v_video.h"

#include "base64/base64.h"

static const char snapshot_str[] = "WOOF_SNAPSHOT";
static const int snapshot_len = arrlen(snapshot_str);
static const int snapshot_size = (SCREENWIDTH * SCREENHEIGHT) * sizeof(pixel_t);

static pixel_t *snapshots[10];
static pixel_t *current_snapshot;
static char savegametimes[10][32];

void MN_ResetSnapshot(int i)
{
    if (snapshots[i])
    {
        free(snapshots[i]);
        snapshots[i] = NULL;
    }
}

// [FG] try to read snapshot data from the end of a savegame file

boolean MN_ReadSnapshot(int i, const byte *buf, int len)
{
    MN_ResetSnapshot(i);

    if (buf == NULL)
    {
        return false;
    }

    // Check if base64-encoded or legacy
    if (len == 0)
    {
        byte *str;
        len = strlen((char *)buf);

        if ((snapshots[i] = malloc(snapshot_size * sizeof(**snapshots))) == NULL)
        {
            return false;
        }

        size_t decoded_size;
        if ((str = base64_decode(buf, len, &decoded_size)) == NULL)
        {
            return false;
        }

        if (decoded_size != snapshot_size)
        {
            free(str);
            return false;
        }

        if (memcpy(snapshots[i], str, snapshot_size) == NULL)
        {
            free(str);
            return false;
        }

        free(str);
    }
    else
    {
        const byte *str;

        if ((str = buf + len - (snapshot_len + snapshot_size)) < buf)
        {
            return false;
        }

        if (strncasecmp((char *)str, snapshot_str, snapshot_len) != 0)
        {
            return false;
        }

        if ((snapshots[i] = malloc(snapshot_size * sizeof(**snapshots))) == NULL)
        {
            return false;
        }

        if (memcpy(snapshots[i], str + snapshot_len, snapshot_size) == NULL)
        {
            return false;
        }
    }

    return true;
}

void MN_ReadSavegameTime(int i, char *name)
{
    time_t mtime = (time_t)M_FileMTime(name);

    if (mtime == 0)
    {
        savegametimes[i][0] = '\0';
    }
    else
    {
// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
        strftime(savegametimes[i], sizeof(savegametimes[i]), "%x %X",
                 localtime(&mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
    }
}

char *MN_GetSavegameTime(int i)
{
    return savegametimes[i];
}

// [FG] take a snapshot in SCREENWIDTH*SCREENHEIGHT resolution, i.e.
//      in hires mode only only each second pixel in each second row is saved,
//      in widescreen mode only the non-widescreen part in the middle is saved

static void TakeSnapshot(void)
{
    int old_screenblocks = screenblocks;

    R_SetViewSize(11);
    R_ExecuteSetViewSize();
    R_RenderPlayerView(&players[displayplayer]);

    if (!current_snapshot)
    {
        current_snapshot = malloc(snapshot_size * sizeof(**snapshots));
    }

    pixel_t *p = current_snapshot;
    const pixel_t *s = I_VideoBuffer;

    for (int x = video.deltaw; x < NONWIDEWIDTH + video.deltaw; x++)
    {
        const int line = V_ScaleX(x) * video.height;
        pixel_t *p2 = p;

        for (int y = 0; y < SCREENHEIGHT; y++)
        {
            *p2 = s[line + V_ScaleY(y)];
            p2 += SCREENWIDTH;
        }

        p++;
    }

    R_SetViewSize(old_screenblocks);
}

char *MN_WriteSnapshot(void)
{
    TakeSnapshot();

    return (char*)base64_encode(current_snapshot, snapshot_size, NULL);
}

// [FG] draw snapshot for the n'th savegame, if no snapshot is found
//      fill the area with palette index 0 (i.e. mostly black)

boolean MN_DrawSnapshot(int n, int x, int y, int w, int h)
{
    if (!snapshots[n])
    {
        V_FillRect(x, y, w, h, v_darkest_color);
        return false;
    }

    vrect_t rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    V_ScaleRect(&rect);

    const fixed_t step_x = (SCREENWIDTH << FRACBITS) / rect.sw;
    const fixed_t step_y = (SCREENHEIGHT << FRACBITS) / rect.sh;

    pixel_t *dest = I_VideoBuffer + (rect.sx * video.height) + rect.sy;

    fixed_t srcx, srcy;
    int destx, desty;
    pixel_t *destcol, *srcline;

    for (destx = 0, srcx = 0; destx < rect.sw; destx++, srcx += step_x)
    {
        destcol = dest + (destx * video.height);
        srcline = snapshots[n] + (srcx >> FRACBITS);

        for (desty = 0, srcy = 0; desty < rect.sh; desty++, srcy += step_y)
        {
            *destcol++ = srcline[(srcy >> FRACBITS) * SCREENWIDTH];
        }
    }

    return true;
}
