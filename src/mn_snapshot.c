//
//  Copyright (C) 2022 Fabian Greffrath
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

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include "i_printf.h"
#  include "m_misc.h"
#else
#  include <locale.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "m_io.h"
#include "r_main.h"
#include "v_video.h"

static const char snapshot_str[] = "WOOF_SNAPSHOT";
static const int snapshot_len = arrlen(snapshot_str);
static const int snapshot_size = SCREENWIDTH * SCREENHEIGHT;

static byte *snapshots[10];
static byte *current_snapshot;
static char savegametimes[10][32];

const int MN_SnapshotDataSize(void)
{
    return snapshot_len + snapshot_size;
}

void MN_ResetSnapshot(int i)
{
    if (snapshots[i])
    {
        free(snapshots[i]);
        snapshots[i] = NULL;
    }
}

// [FG] try to read snapshot data from the end of a savegame file

boolean MN_ReadSnapshot(int i, FILE *fp)
{
    char str[16] = {0};

    MN_ResetSnapshot(i);

    if (fseek(fp, -MN_SnapshotDataSize(), SEEK_END) != 0)
    {
        return false;
    }

    if (fread(str, 1, snapshot_len, fp) != snapshot_len)
    {
        return false;
    }

    if (strncasecmp(str, snapshot_str, snapshot_len) != 0)
    {
        return false;
    }

    if ((snapshots[i] = malloc(snapshot_size * sizeof(**snapshots))) == NULL)
    {
        return false;
    }

    if (fread(snapshots[i], 1, snapshot_size, fp) != snapshot_size)
    {
        return false;
    }

    return true;
}

void MN_ReadSavegameTime(int i, char *name)
{
#if defined(_WIN32)
    HANDLE hFile;
    FILETIME ftWrite;
    SYSTEMTIME stUTC, stLocal;
    wchar_t wdate[64];

    hFile = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        savegametimes[i][0] = '\0';
        return;
    }

    if (!GetFileTime(hFile, NULL, NULL, &ftWrite))
    {
        savegametimes[i][0] = '\0';
        I_Printf(VB_ERROR, "MN_ReadSavegameTime: GetFileTime failed");
        CloseHandle(hFile);
        return;
    }

    FileTimeToSystemTime(&ftWrite, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &stLocal, NULL,
                    wdate, arrlen(wdate), NULL);
    char *date = M_ConvertWideToUtf8(wdate);
    M_snprintf(savegametimes[i], sizeof(savegametimes[i]), "%s %.2d:%.2d:%.2d",
               date, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    free(date);
    CloseHandle(hFile);
#else
    struct stat st;

    if (M_stat(name, &st) == -1)
    {
        savegametimes[i][0] = '\0';
    }
    else
    {
        // Print date and time in the Load/Save Game menus in the current locale
        setlocale(LC_TIME, "");

// [FG] suppress the most useless compiler warning ever
#  if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wformat-y2k"
#  endif
        strftime(savegametimes[i], sizeof(savegametimes[i]), "%x %X",
                 localtime(&st.st_mtime));
#  if defined(__GNUC__)
#    pragma GCC diagnostic pop
#  endif
    }
#endif
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

    byte *p = current_snapshot;

    const byte *s = I_VideoBuffer;

    int x, y;
    for (y = 0; y < SCREENHEIGHT; y++)
    {
        for (x = video.deltaw; x < NONWIDEWIDTH + video.deltaw; x++)
        {
            *p++ = s[V_ScaleY(y) * video.pitch + V_ScaleX(x)];
        }
    }

    R_SetViewSize(old_screenblocks);
}

void MN_WriteSnapshot(byte *p)
{
    TakeSnapshot();

    memcpy(p, snapshot_str, snapshot_len);
    p += snapshot_len;

    memcpy(p, current_snapshot, snapshot_size);
    p += snapshot_size;
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

    byte *dest = I_VideoBuffer + rect.sy * video.pitch + rect.sx;

    fixed_t srcx, srcy;
    int destx, desty;
    byte *destline, *srcline;

    for (desty = 0, srcy = 0; desty < rect.sh; desty++, srcy += step_y)
    {
        destline = dest + desty * video.pitch;
        srcline = snapshots[n] + (srcy >> FRACBITS) * SCREENWIDTH;

        for (destx = 0, srcx = 0; destx < rect.sw; destx++, srcx += step_x)
        {
            *destline++ = srcline[srcx >> FRACBITS];
        }
    }

    return true;
}
