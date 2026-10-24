//
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2019 Simon Judd
// Copyright(C) 2024 Roman Fomin
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

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomtype.h"
#include "m_misc.h"
#include "m_swap.h"
#include "r_defs.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "v_palette.h"
#include "v_patch.h"
#include "v_trans.h"
#include "v_video.h"
#include "z_zone.h"

typedef struct
{
    uint16_t width;
    patch_t *patch;
} fon2_char_t;

typedef struct
{
    byte magic[4];
    uint16_t charheight;
    byte firstc;
    byte lastc;
    byte constantw;
    byte shading;
    byte palsize;
    byte kerning; // flag field, but with only one flag
} fon2_header_t;

static fon2_char_t *chars = NULL;
static int numchars;
static int height;
static int firstc;
static boolean upper;
static int kerning;

#define FON2_SPACE 12

boolean MN_LoadFon2(const byte *gfx_data, int size)
{
    if (size < (int)sizeof(fon2_header_t))
    {
        return false;
    }

    const fon2_header_t *header = (fon2_header_t *)gfx_data;
    if (memcmp(header->magic, "FON2", 4))
    {
        return false;
    }

    height = SHORT(header->charheight);
    if (height == 0)
    {
        return false;
    }

    const byte *p = gfx_data + sizeof(fon2_header_t);

    if (header->kerning)
    {
        kerning = SHORT(*(int16_t *)p);
        p += 2;
    }

    firstc = header->firstc;
    if (header->lastc < 'z')
    {
        upper = true;
    }
    numchars = header->lastc - header->firstc + 1;
    chars = malloc(numchars * sizeof(*chars));

    for (int i = 0; i < numchars; ++i)
    {
        chars[i].width = SHORT(*(uint16_t *)p);
        // The width information is enumerated for each character only if they
        // are not constant width. Regardless, move the read pointer away after
        // the last.
        if (!(header->constantw) || (i == numchars - 1))
        {
            p += 2;
        }
    }

    // Build translation table for palette.
    byte *translate = malloc(header->palsize + 1);
    for (int i = 0; i < header->palsize + 1; ++i)
    {
        int r = *p++;
        int g = *p++;
        int b = *p++;
        translate[i] = V_GetNearestColor(PAL_GLOBAL, r, g, b);
    }

    // 0 is transparent, last is border color
    byte color_key = translate[0];

    // The picture data follows, using the same RLE as FON1 and IMGZ.
    for (int i = 0; i < numchars; ++i)
    {
        // A big font is not necessarily continuous; several characters
        // may be skipped; they are given a width of 0.
        if (!chars[i].width)
        {
            continue;
        }

        int numpixels = chars[i].width * height;
        byte *data = malloc(numpixels);
        byte *d = data;
        byte code = 0;
        int length = 0;

        while (numpixels)
        {
            code = *p++;
            if (code < 0x80)
            {
                length = code + 1;
                for (int k = 0; k < length; ++k)
                {
                    d[k] = translate[p[k]];
                }
                d += length;
                p += length;
                numpixels -= length;
            }
            else if (code > 0x80)
            {
                length = 0x101 - code;
                code = *p++;
                memset(d, translate[code], length);
                d += length;
                numpixels -= length;
            }
        }

        chars[i].patch = V_LinearToTransPatch(data, chars[i].width, height, NULL,
                                              color_key, PU_STATIC, NULL);
        free(data);
    }

    free(translate);
    return true;
}

boolean MN_DrawFon2String(int x, int y, byte *cr, const char *str)
{
    if (!numchars)
    {
        return false;
    }

    int c, cx;

    cx = x;

    while (*str)
    {
        c = *str++;

        c = (upper ? M_ToUpper(c) : c) - firstc;
        if (c < 0 || c >= numchars)
        {
            cx += FON2_SPACE;
            continue;
        }

        if (chars[c].width)
        {
            V_DrawPatchTranslated(cx, y, chars[c].patch, cr);
            cx += chars[c].width + kerning;
        }
        else
        {
            cx += FON2_SPACE;
        }
    }

    return true;
}

int MN_GetFon2PixelWidth(const char *str)
{
    if (!numchars)
    {
        return 0;
    }

    int len = 0;
    int c;

    while (*str)
    {
        c = *str++;

        c = (upper ? M_ToUpper(c) : c) - firstc;
        if (c < 0 || c > numchars)
        {
            len += FON2_SPACE; // space
            continue;
        }

        if (chars[c].width)
        {
            len += chars[c].width + kerning;
        }
        else
        {
            len += FON2_SPACE;
        }
    }
    len -= kerning;
    return len;
}

static int kerning = 0;

void MN_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch)
{
    int w;
    int c;

    byte *cr = cr1;

    while (*ch)
    {
        c = *ch++; // get next char

        if (c == '\x1b' && *ch)
        {
            c = *ch++;
            if (c >= '0' && c <= '0' + CR_NONE)
            {
                cr = xlat[c - '0'].table;
            }
            else if (c == '0' + CR_ORIG)
            {
                cr = cr1;
            }
            continue;
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            cx += SPACEWIDTH; // space
            continue;
        }

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH + video.deltaw)
        {
            break;
        }

        patch_t *patch = hu_font[c];

        // V_DrawpatchTranslated() will draw the string in the
        // desired color, colrngs[color]
        if (cr && cr2)
        {
            V_DrawPatchTranslatedTwice(cx, cy, patch, cr, cr2);
        }
        else
        {
            V_DrawPatchTranslated(cx, cy, patch, cr);
        }

        // The screen is cramped, so trim one unit from each
        // character so they butt up against each other.
        cx += w + kerning;
    }
}

void MN_DrawString(int cx, int cy, int color, const char *ch)
{
    MN_DrawStringCR(cx, cy, xlat[color].table, NULL, ch);
}

int MN_GetPixelWidth(const char *ch)
{
    int len = 0;
    int c;

    while (*ch)
    {
        c = *ch++; // pick up next char

        if (c == '\x1b') // skip color
        {
            if (*ch)
            {
                ch++;
            }
            continue;
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            len += SPACEWIDTH; // space
            continue;
        }

        len += SHORT(hu_font[c]->width);
        len += kerning; // adjust so everything fits
    }
    len -= kerning; // replace what you took away on the last char only
    return len;
}

int MN_StringHeight(const char *ch)
{
    int c, h = SHORT(hu_font[0]->height);
    while (*ch)
    {
        c = *ch++;
        if (c == '\n')
        {
            h += SHORT(hu_font[0]->height);
        }
    }
    return h;
}

int MN_StringWidth(const char *ch)
{
    int c, w = 0;

    while (*ch)
    {
        c = *ch++;
        if (c == '\x1b') // skip code for color change
        {
            if (*ch)
            {
                ch++;
            }
            continue;
        }
        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            w += SPACEWIDTH;
            continue;
        }
        w += SHORT(hu_font[c]->width);
    }

    return w;
}

void MN_SetHUFontKerning(void)
{
    if (MN_StringWidth("abcdefghijklmnopqrstuvwxyz01234") > 230)
    {
        kerning = -1;
    }
}
