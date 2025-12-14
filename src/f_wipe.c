//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//      Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomtype.h"
#include "f_wipe.h"
#include "i_video.h"
#include "m_random.h"
#include "r_main.h"
#include "v_flextran.h"
#include "v_video.h"
#include "z_zone.h"

// Even in hires mode, we simulate what happens on a 320x200 screen; ie.
// the screen is vertically sliced into 160 columns that fall to the bottom
// of the screen. Other hires implementations of the melt effect look weird
// when they try to scale up the original logic to higher resolutions.

// Number of "falling columns" on the screen.
static int wipe_columns;

// Distance a column falls before it reaches the bottom of the screen.
#define WIPE_ROWS 200

//
// SCREEN WIPE PACKAGE
//

static pixel_t *wipe_scr_start;
static pixel_t *wipe_scr_end;
static pixel_t *wipe_scr;

// [FG] cross-fading screen wipe implementation

static int fade_tick;

static int wipe_initColorXForm(int width, int height, int ticks)
{
    V_PutBlock(0, 0, width, height, wipe_scr_start);
    fade_tick = 0;
    return 0;
}

static int wipe_doColorXForm(int width, int height, int ticks)
{
    if (ticks <= 0)
    {
        return 0;
    }

    for (int y = 0; y < height; y++)
    {
        pixel_t *sta = wipe_scr_start + y * width;
        pixel_t *end = wipe_scr_end + y * width;
        pixel_t *dst = wipe_scr + y * video.width;

        for (int x = 0; x < width; x++)
        {
            unsigned int *fg2rgb = Col2RGB8[fade_tick];
            unsigned int *bg2rgb = Col2RGB8[64 - fade_tick];
            unsigned int fg, bg;

            fg = fg2rgb[end[x]];
            bg = bg2rgb[sta[x]];
            fg = (fg + bg) | 0x1f07c1f;
            dst[x] = RGB32k[0][0][fg & (fg >> 15)];
        }
    }

    fade_tick += 2 * ticks;

    return (fade_tick > 64);
}

static int wipe_exit(int width, int height, int ticks)
{
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
    return 0;
}

static int *ybuff1, *ybuff2;
static int *curry, *prevy;

static int wipe_initMelt(int width, int height, int ticks)
{
    wipe_columns = video.unscaledw / 2;

    ybuff1 = Z_Malloc(wipe_columns * sizeof(*ybuff1), PU_STATIC, NULL);
    ybuff2 = Z_Malloc(wipe_columns * sizeof(*ybuff2), PU_STATIC, NULL);

    curry = ybuff1;
    prevy = ybuff2;

    // copy start screen to main screen
    V_PutBlock(0, 0, width, height, wipe_scr_start);

    // setup initial column positions (y<0 => not ready to scroll yet)
    curry[0] = -(M_Random() % 16);
    for (int i = 1; i < wipe_columns; i++)
    {
        int r = (M_Random() % 3) - 1;
        curry[i] = curry[i - 1] + r;
        if (curry[i] > 0)
        {
            curry[i] = 0;
        }
        else if (curry[i] == -16)
        {
            curry[i] = -15;
        }
    }

    memcpy(prevy, curry, sizeof(int) * wipe_columns);

    return 0;
}

static int wipe_doMelt(int width, int height, int ticks)
{
    boolean done = true;

    if (ticks > 0)
    {
        while (ticks--)
        {
            int *temp = prevy;
            prevy = curry;
            curry = temp;

            for (int col = 0; col < wipe_columns; ++col)
            {
                if (prevy[col] < 0)
                {
                    curry[col] = prevy[col] + 1;
                    done = false;
                }
                else if (prevy[col] < WIPE_ROWS)
                {
                    int dy = (prevy[col] < 16) ? prevy[col] + 1 : 8;
                    curry[col] = MIN(prevy[col] + dy, WIPE_ROWS);

                    done = false;
                }
                else
                {
                    curry[col] = WIPE_ROWS;
                }
            }
        }
    }
    else
    {
        for (int col = 0; col < wipe_columns; ++col)
        {
            done &= curry[col] >= WIPE_ROWS;
        }
    }

    return done;
}

int wipe_renderMelt(int width, int height, int ticks)
{
    boolean done = true;

    // Scale up and then down to handle arbitrary dimensions with integer math
    int vertblocksize = height * 100 / WIPE_ROWS;
    int horizblocksize = width * 100 / wipe_columns;
    int currcol;
    int currcolend;
    int currrow;

    V_UseBuffer(wipe_scr);
    V_PutBlock(0, 0, width, height, wipe_scr_end);
    V_RestoreBuffer();

    for (int col = 0; col < wipe_columns; ++col)
    {
        int current;
        if (uncapped)
        {
            current = FixedToInt(
                LerpFixed(IntToFixed(prevy[col]), IntToFixed(curry[col])));
        }
        else
        {
            current = curry[col];
        }

        if (current < 0)
        {
            currcol = col * horizblocksize / 100;
            currcolend = (col + 1) * horizblocksize / 100;
            for (; currcol < currcolend; ++currcol)
            {
                pixel_t *source = wipe_scr_start + currcol;
                pixel_t *dest = wipe_scr + currcol;

                for (int i = 0; i < height; ++i)
                {
                    *dest = *source;
                    dest += width;
                    source += width;
                }
            }
        }
        else if (current < WIPE_ROWS)
        {
            currcol = col * horizblocksize / 100;
            currcolend = (col + 1) * horizblocksize / 100;

            currrow = current * vertblocksize / 100;

            for (; currcol < currcolend; ++currcol)
            {
                pixel_t *source = wipe_scr_start + currcol;
                pixel_t *dest = wipe_scr + currcol + (currrow * video.width);

                for (int i = 0; i < height - currrow; ++i)
                {
                    *dest = *source;
                    dest += width;
                    source += width;
                }
            }

            done = false;
        }
    }

    for (currcol = wipe_columns * horizblocksize / 100; currcol < width; ++currcol)
    {
        pixel_t *dest = wipe_scr + currcol;

        for (int i = 0; i < height; ++i)
        {
            *dest = v_darkest_color;
            dest += width;
        }
    }

    return done;
}

static int wipe_exitMelt(int width, int height, int ticks)
{
    Z_Free(ybuff1);
    Z_Free(ybuff2);
    wipe_exit(width, height, ticks);
    return 0;
}

int wipe_StartScreen(int x, int y, int width, int height)
{
    int size = width * height;
    wipe_scr_start = Z_Malloc(size * sizeof(*wipe_scr_start), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_start);
    return 0;
}

int wipe_EndScreen(int x, int y, int width, int height)
{
    int size = width * height;
    wipe_scr_end = Z_Malloc(size * sizeof(*wipe_scr_end), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(x, y, width, height, wipe_scr_start); // restore start scr.
    return 0;
}

static int wipe_NOP(int width, int height, int tics)
{
    return 0;
}

/*
// Copyright(C) 1992 Id Software, Inc.
// Copyright(C) 2007-2011 Moritz "Ripper" Kroll

===================
=
= FizzleFade
=
= It uses maximum-length Linear Feedback Shift Registers (LFSR) counters.
= You can find a list of them with lengths from 3 to 168 at:
= http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
= Many thanks to Xilinx for this list!!!
=
===================
*/

// XOR masks for the pseudo-random number sequence starting with n=17 bits
static const uint32_t rndmasks[] = {
                // n    XNOR from (starting at 1, not 0 as usual)
    0x00012000, // 17   17,14
    0x00020400, // 18   18,11
    0x00040023, // 19   19,6,2,1
    0x00090000, // 20   20,17
    0x00140000, // 21   21,19
    0x00300000, // 22   22,21
    0x00420000, // 23   23,18
    0x00e10000, // 24   24,23,22,17
    0x01200000, // 25   25,22      (this is enough for 8191x4095)
};

// Returns the number of bits needed to represent the given value
static int log2_ceil(uint32_t x)
{
    int n = 0;
    uint32_t v = 1;

    while (v < x)
    {
        n++;
        v <<= 1;
    }

    return n;
}

static unsigned int rndbits_y;
static unsigned int rndmask;
static unsigned int lastrndval;

static int wipe_initFizzle(int width, int height, int ticks)
{
    int rndbits_x = log2_ceil(video.unscaledw);
    rndbits_y = log2_ceil(WIPE_ROWS);

    int rndbits = rndbits_x + rndbits_y;
    if (rndbits < 17)
    {
        rndbits = 17; // no problem, just a bit slower
    }
    else if (rndbits > 25)
    {
        rndbits = 25; // fizzle fade will not fill whole screen
    }

    rndmask = rndmasks[rndbits - 17];

    V_PutBlock(0, 0, width, height, wipe_scr_start);

    lastrndval = 0;

    return 0;
}

static int wipe_doFizzle(int width, int height, int ticks)
{
    if (ticks <= 0)
    {
        return false;
    }

    const int pixperframe = (video.unscaledw * WIPE_ROWS) >> 5;
    unsigned int rndval = lastrndval;

    for (unsigned p = 0; p < pixperframe; p++)
    {
        // seperate random value into x/y pair

        unsigned int x = rndval >> rndbits_y;
        unsigned int y = rndval & ((1 << rndbits_y) - 1);

        // advance to next random element

        rndval = (rndval >> 1) ^ (rndval & 1 ? 0 : rndmask);

        if (x >= video.unscaledw || y >= WIPE_ROWS)
        {
            if (rndval == 0) // entire sequence has been completed
            {
                return true;
            }

            p--;
            continue;
        }

        // copy one pixel
        vrect_t rect = {x, y, 1, 1};
        V_ScaleRect(&rect);

        pixel_t *src = wipe_scr_end + rect.sy * width + rect.sx;
        pixel_t *dest = wipe_scr + rect.sy * width + rect.sx;

        while (rect.sh--)
        {
            memcpy(dest, src, rect.sw);
            src += width;
            dest += width;
        }

        if (rndval == 0) // entire sequence has been completed
        {
            return true;
        }
    }

    lastrndval = rndval;

    return false;
}

typedef int (*wipefunc_t)(int, int, int);

typedef struct
{
    wipefunc_t init;
    wipefunc_t update;
    wipefunc_t render;
    wipefunc_t exit;
} wipe_t;

static wipe_t wipes[] = {
    {wipe_NOP,            wipe_NOP,          wipe_NOP,        wipe_exit    },
    {wipe_initMelt,       wipe_doMelt,       wipe_renderMelt, wipe_exitMelt},
    {wipe_initColorXForm, wipe_doColorXForm, wipe_NOP,        wipe_exit    },
    {wipe_initFizzle,     wipe_doFizzle,     wipe_NOP,        wipe_exit    },
};

// killough 3/5/98: reformatted and cleaned up
int wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks)
{
    static boolean go; // when zero, stop the wipe

    if (!go) // initial stuff
    {
        go = 1;
        wipe_scr = I_VideoBuffer;
        wipes[wipeno].init(width, height, ticks);
    }

    int rc = wipes[wipeno].update(width, height, ticks);
    wipes[wipeno].render(width, height, ticks);

    if (rc) // final stuff
    {
        wipes[wipeno].exit(width, height, ticks);
        go = 0;
    }
    return !go;
}

//----------------------------------------------------------------------------
//
// $Log: f_wipe.c,v $
// Revision 1.3  1998/05/03  22:11:24  killough
// beautification
//
// Revision 1.2  1998/01/26  19:23:16  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
