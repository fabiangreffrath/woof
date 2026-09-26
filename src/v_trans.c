//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Paul Haeberli
// Copyright(C) 2014-2023 Fabian Greffrath
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
// Color translation tables
//

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "m_argv.h"
#include "w_wad.h"
#include "v_srgb.h"
#include "v_trans.h"

/*
Date: Sun, 26 Oct 2014 10:36:12 -0700
From: paul haeberli <paulhaeberli@yahoo.com>
Subject: Re: colors and color conversions
To: Fabian Greffrath <fabian@greffrath.com>

Yes, this seems exactly like the solution I was looking for. I just
couldn't find code to do the HSV->RGB conversion. Speaking of the code,
would you allow me to use this code in my software? The Doom source code
is licensed under the GNU GPL, so this code yould have to be under a
compatible license.

    Yes. I'm happy to contribute this code to your project.  GNU GPL or anything
    compatible sounds fine.

Regarding the conversions, the procedure you sent me will leave grays
(r=g=b) untouched, no matter what I set as HUE, right? Is it possible,
then, to also use this routine to convert colors *to* gray?

    You can convert any color to an equivalent grey by setting the saturation
    to 0.0


    - Paul Haeberli
*/

#define CTOLERANCE 0.0001

typedef struct vect
{
    double x;
    double y;
    double z;
} vect;

static void hsv_to_rgb(vect *hsv, vect *rgb)
{
    double h, s, v;

    h = hsv->x;
    s = hsv->y;
    v = hsv->z;
    h *= 360.0;
    if (s < CTOLERANCE)
    {
        rgb->x = v;
        rgb->y = v;
        rgb->z = v;
    }
    else
    {
        int i;
        double f, p, q, t;

        if (h >= 360.0)
        {
            h -= 360.0;
        }
        h /= 60.0;
        i = (int)floor(h);
        f = h - i;
        p = v * (1.0 - s);
        q = v * (1.0 - (s * f));
        t = v * (1.0 - (s * (1.0 - f)));
        switch (i)
        {
            case 0:
                rgb->x = v;
                rgb->y = t;
                rgb->z = p;
                break;
            case 1:
                rgb->x = q;
                rgb->y = v;
                rgb->z = p;
                break;
            case 2:
                rgb->x = p;
                rgb->y = v;
                rgb->z = t;
                break;
            case 3:
                rgb->x = p;
                rgb->y = q;
                rgb->z = v;
                break;
            case 4:
                rgb->x = t;
                rgb->y = p;
                rgb->z = v;
                break;
            case 5:
                rgb->x = v;
                rgb->y = p;
                rgb->z = q;
                break;
        }
    }
}

static void rgb_to_hsv(vect *rgb, vect *hsv)
{
    double h, s, v;
    double cmax, cmin;
    double r, g, b;

    r = rgb->x;
    g = rgb->y;
    b = rgb->z;
    /* find the cmax and cmin of r g b */
    cmax = r;
    cmin = r;
    cmax = (g > cmax ? g : cmax);
    cmin = (g < cmin ? g : cmin);
    cmax = (b > cmax ? b : cmax);
    cmin = (b < cmin ? b : cmin);
    v = cmax; /* value */
    if (cmax > CTOLERANCE)
    {
        s = (cmax - cmin) / cmax;
    }
    else
    {
        s = 0.0;
    }
    if (s < CTOLERANCE)
    {
        h = 0.0;
    }
    else
    {
        double cdelta;
        double rc, gc, bc;

        cdelta = cmax - cmin;
        rc = (cmax - r) / cdelta;
        gc = (cmax - g) / cdelta;
        bc = (cmax - b) / cdelta;
        if (r == cmax)
        {
            h = bc - gc;
        }
        else if (g == cmax)
        {
            h = 2.0 + rc - bc;
        }
        else
        {
            h = 4.0 + gc - rc;
        }
        h = h * 60.0;
        if (h < 0.0)
        {
            h += 360.0;
        }
    }
    hsv->x = h / 360.0;
    hsv->y = s;
    hsv->z = v;
}

static byte ColorizeBoomTranslation(palette_t pal, int cr, byte source)
{
    vect rgb, hsv;

    rgb.x = list_playpal[pal].base_linear[source].r;
    rgb.y = list_playpal[pal].base_linear[source].g;
    rgb.z = list_playpal[pal].base_linear[source].b;

    rgb_to_hsv(&rgb, &hsv);

    if (cr == CR_BRIGHT)
    {
        hsv.z *= 1.4;
    }
    else if (cr == CR_GRAY || cr == CR_BLACK || cr == CR_WHITE)
    {
        hsv.y = 0.0;

        if (cr == CR_BLACK)
        {
            hsv.z = 0.5 * hsv.z;
        }
        else if (cr == CR_WHITE)
        {
            hsv.z = 1.0 - 0.5 * hsv.z;
        }
    }
    else
    {
        // [crispy] hack colors to full saturation
        hsv.y = 1.0;

        if (cr == CR_GREEN)
        {
            // hsv.x = 135./360.;
            hsv.x = (144.0 * hsv.z + 120.0 * (1.0 - hsv.z)) / 360.0;
        }
        else if (cr == CR_GOLD)
        {
            // hsv.x = 45./360.;
            // hsv.x = (50. * hsv.z + 30. * (1. - hsv.z))/360.;
            hsv.x = (7.0 + 53.0 * hsv.z) / 360.0;
            hsv.y = 1.0 - 0.4 * hsv.z;
            hsv.z = MIN(hsv.z, 0.2) + 0.8 * hsv.z;
        }
        else if (cr == CR_RED || cr == CR_BRICK)
        {
            hsv.x = 0.0;

            if (cr == CR_BRICK)
            {
                hsv.y = 0.5 * hsv.y;
            }
        }
        else if (cr == CR_BLUE1 || cr == CR_BLUE2)
        {
            hsv.x = 240.0 / 360.0;

            if (cr == CR_BLUE1)
            {
                hsv.y = 1.0 - 0.5 * hsv.z;
                hsv.z = MIN(hsv.z, 0.5) + 0.5 * hsv.z;
            }
        }
        else if (cr == CR_ORANGE || cr == CR_TAN || cr == CR_BROWN)
        {
            hsv.x = 30.0 / 360.0;

            if (cr == CR_TAN)
            {
                hsv.y = 0.5 * hsv.y;
            }
            else if (cr == CR_BROWN)
            {
                hsv.z = 0.5 * hsv.z;
            }
            else
            {
                hsv.y = (hsv.z < 0.6) ? 1.0 : (2.2 - 2.0 * hsv.z);
                hsv.z = MIN(hsv.z, 0.5) + 0.5 * hsv.z;
            }
        }
        else if (cr == CR_YELLOW)
        {
            hsv.x = 60.0 / 360.0;
            hsv.y = 1.0 - 0.85 * hsv.z;
            hsv.z = 1.0;
        }
        else if (cr == CR_PURPLE)
        {
            hsv.x = 300.0 / 360.0;
        }
    }

    hsv_to_rgb(&hsv, &rgb);

    rgb.x = sRGB_LinearToByte(rgb.x);
    rgb.y = sRGB_LinearToByte(rgb.y);
    rgb.z = sRGB_LinearToByte(rgb.z);

    return V_GetNearestColor(PAL_GLOBAL, (byte)rgb.x, (byte)rgb.y, (byte)rgb.z);
}

// [FG] dark/shaded color translation table
byte *cr_dark;
byte *cr_shaded;

//
// V_InitColorTranslation
//
// Loads the color translation tables from predefined lumps at game start
// No return value
//
// Used for translating text colors from the red palette range
// to other colors. The first nine entries can be used to dynamically
// switch the output of text color thru the HUlib_drawText routine
// by embedding ESCn in the text to obtain color n. Symbols for n are
// provided in v_video.h.
//

xlat_t xlat[CR_LIMIT] =
{
    [CR_BRICK]  = { .name = "CRBRICK",  .str = "\x1b\x30" },
    [CR_TAN]    = { .name = "CRTAN",    .str = "\x1b\x31" },
    [CR_GRAY]   = { .name = "CRGRAY",   .str = "\x1b\x32" },
    [CR_GREEN]  = { .name = "CRGREEN",  .str = "\x1b\x33" },
    [CR_BROWN]  = { .name = "CRBROWN",  .str = "\x1b\x34" },
    [CR_GOLD]   = { .name = "CRGOLD",   .str = "\x1b\x35" },
    [CR_RED]    = { .name = "CRRED",    .str = "\x1b\x36" },
    [CR_BLUE1]  = { .name = "CRBLUE",   .str = "\x1b\x37" },
    [CR_ORANGE] = { .name = "CRORANGE", .str = "\x1b\x38" },
    [CR_YELLOW] = { .name = "CRYELLOW", .str = "\x1b\x39" },
    [CR_BLUE2]  = { .name = "CRBLUE2",  .str = "\x1b\x3a" },
    [CR_BLACK]  = { .name = "CRBLACK",  .str = "\x1b\x3b" },
    [CR_PURPLE] = { .name = "CRPURPLE", .str = "\x1b\x3c" },
    [CR_WHITE]  = { .name = "CRWHITE",  .str = "\x1b\x3d" },
    [CR_BRIGHT] = { .name = NULL,       .str = NULL },
    [CR_NONE]   = { .name = NULL,       .str = NULL },
};

// [FG] translate between blood color value as per EE spec
//      and actual color translation table index

static const xlat_index_t bloodcolor[] =
{
    CR_RED,     // 0 - Red (normal)
    CR_GRAY,    // 1 - Grey
    CR_GREEN,   // 2 - Green
    CR_BLUE2,   // 3 - Blue
    CR_YELLOW,  // 4 - Yellow
    CR_BLACK,   // 5 - Black
    CR_PURPLE,  // 6 - Purple
    CR_WHITE,   // 7 - White
    CR_ORANGE,  // 8 - Orange
};

xlat_index_t V_BloodColor(int blood)
{
    return bloodcolor[blood];
}

xlat_index_t V_CRByName(const char *name)
{
    for (const xlat_t *p = xlat; p->name; ++p)
    {
        if (!strcmp(p->name, name))
        {
            return p - xlat;
        }
    }
    return CR_NONE;
}

byte invul_gray[256];

// killough 5/2/98: tiny engine driven by table above
void V_InitColorTranslation(void)
{
    const boolean iwad_playpal = W_IsIWADLump(playpal_global->num);

    int force_rebuild = M_CheckParm("-tranmap");

    // [crispy] preserve gray drop shadow in IWAD status bar numbers
    const boolean keepgray = W_IsIWADLump(W_GetNumForName("sttnum0"));

    for (xlat_index_t cr = CR_BRICK; cr < CR_NONE; cr++)
    {
        xlat_t *cr_p = &xlat[cr];
        int lumpnum = (cr_p->name) ? W_CheckNumForName(cr_p->name) : -1;
        cr_p->lump = (lumpnum != -1) ? W_CacheLumpNum(lumpnum, PU_STATIC) : NULL;

        // [FG] allocate new color translation table
        cr_p->table = malloc(256);

        // keep original translation table entries if they apply
        // against the original palette or if they are from a PWAD
        const boolean keeporig =
            (iwad_playpal || W_IsWADLump(lumpnum)) && !force_rebuild;

        // [FG] translate to target color
        for (int i = 0; i < PLAYPAL_SIZE; i++)
        {
            // keep only entries that are not identity anyway
            if (keeporig && cr_p->lump
                && (cr_p->lump[i] != (byte)i || (keepgray && i == 109)))
            {
                cr_p->table[i] = cr_p->lump[i];
            }
            else
            {
                cr_p->table[i] = ColorizeBoomTranslation(PAL_GLOBAL, cr, (byte)i);
            }
        }
    }

    const rgb_t *pal_rover = playpal_global->base;
    for (int i = 0; i < PLAYPAL_SIZE; ++i)
    {
        // Use linear sRGB coefficients to get accurate grayscale.
        // * https://30fps.net/pages/better-srgb-to-greyscale/
        // * https://en.wikipedia.org/wiki/Rec._709#Luma_coefficients
        double red   = sRGB_ByteToLinear(pal_rover[i].r);
        double green = sRGB_ByteToLinear(pal_rover[i].g);
        double blue  = sRGB_ByteToLinear(pal_rover[i].b);
        const byte gray = sRGB_LinearToByte(red * 0.2126 + green * 0.7152 + blue * 0.0722);
        invul_gray[i] = V_GetNearestColor(PAL_GLOBAL, gray, gray, gray);
    }
}
