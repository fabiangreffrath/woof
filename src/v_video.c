//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2013 James Haley et al.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
// DESCRIPTION:
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"
#include "m_swap.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_fmt.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h" // needed for color translation lump lookup
#include "z_zone.h"

pixel_t *I_VideoBuffer;

// The screen buffer that the v_video.c code draws to.

static pixel_t *dest_screen = NULL;

// jff 2/18/98 palette color ranges for translation
// jff 4/24/98 now pointers set to predefined lumps to allow overloading

byte *cr_brick;
byte *cr_tan;
byte *cr_gray;
byte *cr_green;
byte *cr_brown;
byte *cr_gold;
byte *cr_red;
byte *cr_blue;
byte *cr_blue2;
byte *cr_orange;
byte *cr_yellow;
byte *cr_black;
byte *cr_purple;
byte *cr_white;
// [FG] dark/shaded color translation table
byte *cr_dark;
byte *cr_shaded;
byte *cr_bright;

// jff 4/24/98 initialize this at runtime
byte *colrngs[CR_LIMIT] = {0};
byte *red2col[CR_LIMIT] = {0};

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

typedef struct
{
    const char *name;
    byte **map1, **map2, **map_orig;
} crdef_t;

// killough 5/2/98: table-driven approach
static const crdef_t crdefs[] =
{
    {"CRBRICK",  &cr_brick,  &colrngs[CR_BRICK],  &red2col[CR_BRICK]},
    {"CRTAN",    &cr_tan,    &colrngs[CR_TAN],    &red2col[CR_TAN]},
    {"CRGRAY",   &cr_gray,   &colrngs[CR_GRAY],   &red2col[CR_GRAY]},
    {"CRGREEN",  &cr_green,  &colrngs[CR_GREEN],  &red2col[CR_GREEN]},
    {"CRBROWN",  &cr_brown,  &colrngs[CR_BROWN],  &red2col[CR_BROWN]},
    {"CRGOLD",   &cr_gold,   &colrngs[CR_GOLD],   &red2col[CR_GOLD]},
    {"CRRED",    &cr_red,    &colrngs[CR_RED],    &red2col[CR_RED]},
    {"CRBLUE",   &cr_blue,   &colrngs[CR_BLUE1],  &red2col[CR_BLUE1]},
    {"CRORANGE", &cr_orange, &colrngs[CR_ORANGE], &red2col[CR_ORANGE]},
    {"CRYELLOW", &cr_yellow, &colrngs[CR_YELLOW], &red2col[CR_YELLOW]},
    {"CRBLUE2",  &cr_blue2,  &colrngs[CR_BLUE2],  &red2col[CR_BLUE2]},
    {"CRBLACK",  &cr_black,  &colrngs[CR_BLACK],  &red2col[CR_BLACK]},
    {"CRPURPLE", &cr_purple, &colrngs[CR_PURPLE], &red2col[CR_PURPLE]},
    {"CRWHITE",  &cr_white,  &colrngs[CR_WHITE],  &red2col[CR_WHITE]},
    {NULL}
};

// [FG] translate between blood color value as per EE spec
//      and actual color translation table index

static const int bloodcolor[] =
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

int V_BloodColor(int blood)
{
    return bloodcolor[blood];
}

crange_idx_e V_CRByName(const char *name)
{
    for (const crdef_t *p = crdefs; p->name; ++p)
    {
        if (!strcmp(p->name, name))
        {
            return p - crdefs;
        }
    }
    return CR_NONE;
}

int v_lightest_color, v_darkest_color;

byte invul_gray[256];

// killough 5/2/98: tiny engine driven by table above
void V_InitColorTranslation(void)
{
    register const crdef_t *p;

    int playpal_lump = W_GetNumForName("PLAYPAL");
    byte *playpal = W_CacheLumpNum(playpal_lump, PU_STATIC);
    boolean iwad_playpal = W_IsIWADLump(playpal_lump);

    int force_rebuild = M_CheckParm("-tranmap");

    // [crispy] preserve gray drop shadow in IWAD status bar numbers
    boolean keepgray = W_IsIWADLump(W_GetNumForName("sttnum0"));

    for (p = crdefs; p->name; p++)
    {
        int i, lumpnum = W_GetNumForName(p->name);

        *p->map_orig = W_CacheLumpNum(lumpnum, PU_STATIC);

        // [FG] color translation table provided by PWAD
        if (W_IsWADLump(lumpnum) && !force_rebuild)
        {
            *p->map1 = *p->map2 = *p->map_orig;
            continue;
        }

        // [FG] allocate new color translation table
        *p->map2 = malloc(256);

        // [FG] translate all colors to target color
        for (i = 0; i < 256; i++)
        {
            (*p->map2)[i] = V_Colorize(playpal, p - crdefs, (byte)i);
        }

        // [FG] override with original color translations
        if (iwad_playpal && !force_rebuild)
        {
            for (i = 0; i < 256; i++)
            {
                if (((*p->map_orig)[i] != (byte)i) || (keepgray && i == 109))
                {
                    (*p->map2)[i] = (*p->map_orig)[i];
                }
            }
        }

        *p->map1 = *p->map2;
    }

    cr_bright = malloc(256);
    for (int i = 0; i < 256; ++i)
    {
        cr_bright[i] = V_Colorize(playpal, CR_BRIGHT, (byte)i);
    }

    v_lightest_color = I_GetNearestColor(playpal, 0xFF, 0xFF, 0xFF);
    v_darkest_color  = I_GetNearestColor(playpal, 0x00, 0x00, 0x00);

    byte *palsrc = playpal;
    for (int i = 0; i < 256; ++i)
    {
        double red   = *palsrc++ / 256.0;
        double green = *palsrc++ / 256.0;
        double blue  = *palsrc++ / 256.0;

        // formula is taken from dcolors.c preseving "Carmack's typo"
        // https://doomwiki.org/wiki/Carmack%27s_typo
        int gray = (red * 0.299 + green * 0.587 + blue * 0.144) * 255;
        invul_gray[i] = I_GetNearestColor(playpal, gray, gray, gray);
    }
}

video_t video;

#define WIDE_SCREENWIDTH 864 // Up to 32:9 aspect ratio (3.6).

static int x1lookup[WIDE_SCREENWIDTH + 1];
static int y1lookup[SCREENHEIGHT + 1];
static int x2lookup[WIDE_SCREENWIDTH + 1];
static int y2lookup[SCREENHEIGHT + 1];
static int linesize;

#define V_ADDRESS(buffer, x, y) ((buffer) + (y) * linesize + (x))

typedef struct
{
    int x;
    int y1, y2;

    fixed_t frac;
    fixed_t step;

    byte *source;
} patch_column_t;

static byte *translation;

static byte *translation1, *translation2;

static void (*drawcolfunc)(const patch_column_t *patchcol);

#define DRAW_COLUMN(NAME, SRCPIXEL)                                           \
    static void DrawPatchColumn##NAME(const patch_column_t *patchcol)         \
    {                                                                         \
        int count = patchcol->y2 - patchcol->y1 + 1;                          \
                                                                              \
        if (count <= 0)                                                       \
            return;                                                           \
                                                                              \
        if ((unsigned int)patchcol->x >= (unsigned int)video.width            \
            || (unsigned int)patchcol->y1 >= (unsigned int)video.height)      \
        {                                                                     \
            I_Error("%i to %i at %i", patchcol->y1, patchcol->y2,             \
                    patchcol->x);                                             \
        }                                                                     \
                                                                              \
        pixel_t *dest = V_ADDRESS(dest_screen, patchcol->x, patchcol->y1);    \
                                                                              \
        const fixed_t fracstep = patchcol->step;                              \
        fixed_t frac =                                                        \
            patchcol->frac + ((patchcol->y1 * fracstep) & FRACMASK);          \
                                                                              \
        const byte *source = patchcol->source;                                \
                                                                              \
        while ((count -= 2) >= 0)                                             \
        {                                                                     \
            *dest = SRCPIXEL;                                                 \
            dest += linesize;                                                 \
            frac += fracstep;                                                 \
            *dest = SRCPIXEL;                                                 \
            dest += linesize;                                                 \
            frac += fracstep;                                                 \
        }                                                                     \
        if (count & 1)                                                        \
        {                                                                     \
            *dest = SRCPIXEL;                                                 \
        }                                                                     \
    }

DRAW_COLUMN(, source[frac >> FRACBITS])
DRAW_COLUMN(TR, translation[source[frac >> FRACBITS]])
DRAW_COLUMN(TRTR, translation2[translation1[source[frac >> FRACBITS]]])
DRAW_COLUMN(TL, tranmap[(*dest << 8) + source[frac >> FRACBITS]])
DRAW_COLUMN(TRTL, tranmap[(*dest << 8) + translation[source[frac >> FRACBITS]]])

static void DrawMaskedColumn(patch_column_t *patchcol, const int ytop,
                             column_t *column)
{
    for (; column->topdelta != 0xff;
         column = (column_t *)((byte *)column + column->length + 4))
    {
        // calculate unclipped screen coordinates for post
        int columntop = ytop + column->topdelta;

        if (columntop >= 0)
        {
            // SoM: Make sure the lut is never referenced out of range
            if (columntop >= SCREENHEIGHT)
            {
                return;
            }

            patchcol->y1 = y1lookup[columntop];
            patchcol->frac = 0;
        }
        else
        {
            patchcol->frac = (-columntop) << FRACBITS;
            patchcol->y1 = 0;
        }

        if (columntop + column->length - 1 < 0)
        {
            continue;
        }
        if (columntop + column->length - 1 < SCREENHEIGHT)
        {
            patchcol->y2 = y2lookup[columntop + column->length - 1];
        }
        else
        {
            patchcol->y2 = y2lookup[SCREENHEIGHT - 1];
        }

        // SoM: The failsafes should be completely redundant now...
        // haleyjd 05/13/08: fix clipping; y2lookup not clamped properly
        if ((column->length > 0 && patchcol->y2 < patchcol->y1)
            || patchcol->y2 >= video.height)
        {
            patchcol->y2 = video.height - 1;
        }

        // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
        if (patchcol->y1 <= patchcol->y2 && patchcol->y2 < video.height)
        {
            patchcol->source = (byte *)column + 3;
            drawcolfunc(patchcol);
        }
    }
}

static void DrawPatchInternal(int x, int y, patch_t *patch, boolean flipped)
{
    int x1, x2, w;
    fixed_t iscale, xiscale, startfrac = 0;
    patch_column_t patchcol = {0};

    w = SHORT(patch->width);

    // calculate edges of the shape
    if (flipped)
    {
        // If flipped, then offsets are flipped as well which means they
        // technically offset from the right side of the patch (x2)
        x2 = x + SHORT(patch->leftoffset);
        x1 = x2 - (w - 1);
    }
    else
    {
        x1 = x - SHORT(patch->leftoffset);
        x2 = x1 + w - 1;
    }

    iscale = video.xstep;
    patchcol.step = video.ystep;

    // off the left or right side?
    if (x2 < 0 || x1 >= video.unscaledw)
    {
        return;
    }

    xiscale = flipped ? -iscale : iscale;

    // haleyjd 10/10/08: must handle coordinates outside the screen buffer
    // very carefully here.
    if (x1 >= 0)
    {
        x1 = x1lookup[x1];
    }
    else if (-x1 - 1 < video.unscaledw)
    {
        x1 = -x2lookup[-x1 - 1];
    }
    else // too far off-screen
    {
        x1 = -(DIV_ROUND_FLOOR(video.width * (-x1 - 1), video.unscaledw));
    }

    if (x2 < video.unscaledw)
    {
        x2 = x2lookup[x2];
    }
    else
    {
        x2 = x2lookup[video.unscaledw - 1];
    }

    patchcol.x = (x1 < 0) ? 0 : x1;

    // SoM: Any time clipping occurs on screen coords, the resulting clipped
    // coords should be checked to make sure we are still on screen.
    if (x2 < x1)
    {
        return;
    }

    // SoM: Ok, so the startfrac should ALWAYS be the last post of the patch
    // when the patch is flipped minus the fractional "bump" from the screen
    // scaling, then the patchcol.x to x1 clipping will place the frac in the
    // correct column no matter what. This also ensures that scaling will be
    // uniform. If the resolution is 320x2X0 the iscale will be 65537 which
    // will create some fractional bump down, so it is safe to assume this puts
    // us just below patch->width << 16
    if (flipped)
    {
        startfrac = (w << 16) - ((x1 * iscale) & FRACMASK) - 1;
    }
    else
    {
        startfrac = (x1 * iscale) & FRACMASK;
    }

    if (patchcol.x > x1)
    {
        startfrac += xiscale * (patchcol.x - x1);
    }

    {
        column_t *column;
        int texturecolumn;

        const int ytop = y - SHORT(patch->topoffset);
        for (; patchcol.x <= x2; patchcol.x++, startfrac += xiscale)
        {
            texturecolumn = startfrac >> FRACBITS;

            if (texturecolumn < 0)
            {
                continue;
            }
            else if (texturecolumn >= w)
            {
                break;
            }

            column = (column_t *)((byte *)patch
                                  + LONG(patch->columnofs[texturecolumn]));
            DrawMaskedColumn(&patchcol, ytop, column);
        }
    }
}

//
// V_DrawPatch
//
// Masks a column based masked pic to the screen.
//
// The patch is drawn at x,y in the buffer selected by scrn
// No return value
//
// V_DrawPatchFlipped
//
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
// Patch is drawn at x,y in screenbuffer scrn.
// No return value
//
// killough 11/98: Consolidated V_DrawPatch and V_DrawPatchFlipped into one
//

void V_DrawPatchGeneral(int x, int y, patch_t *patch, boolean flipped)
{
    x += video.deltaw;

    drawcolfunc = DrawPatchColumn;

    DrawPatchInternal(x, y, patch, flipped);
}

void V_DrawPatchTranslated(int x, int y, patch_t *patch, byte *outr)
{
    x += video.deltaw;

    if (outr)
    {
        translation = outr;
        drawcolfunc = DrawPatchColumnTR;
    }
    else
    {
        drawcolfunc = DrawPatchColumn;
    }

    DrawPatchInternal(x, y, patch, false);
}

void V_DrawPatchTL(int x, int y, struct patch_s *patch, byte *tl)
{
    x += video.deltaw;

    tranmap = tl;
    drawcolfunc = DrawPatchColumnTL;

    DrawPatchInternal(x, y, patch, false);
}

void V_DrawPatchTRTL(int x, int y, struct patch_s *patch, byte *outr, byte *tl)
{
    x += video.deltaw;

    translation = outr;
    tranmap = tl;
    drawcolfunc = DrawPatchColumnTRTL;

    DrawPatchInternal(x, y, patch, false);
}

void V_DrawPatchTRTR(int x, int y, patch_t *patch, byte *outr1, byte *outr2)
{
    x += video.deltaw;

    translation1 = outr1;
    translation2 = outr2;
    drawcolfunc = DrawPatchColumnTRTR;

    DrawPatchInternal(x, y, patch, false);
}

void V_DrawPatchFullScreen(patch_t *patch)
{
    const int x = DIV_ROUND_CLOSEST(video.unscaledw - SHORT(patch->width), 2);

    patch->leftoffset = 0;
    patch->topoffset = 0;

    // [crispy] fill pillarboxes in widescreen mode
    if (video.unscaledw != NONWIDEWIDTH)
    {
        V_FillRect(0, 0, video.unscaledw, SCREENHEIGHT, v_darkest_color);
    }

    drawcolfunc = DrawPatchColumn;

    DrawPatchInternal(x, 0, patch, false);
}

void V_ShadeScreen(void)
{
    const byte *darkcolormap = &colormaps[0][20 * 256];

    pixel_t *row = dest_screen;
    int height = video.height;

    while (height--)
    {
        int width = video.width;
        pixel_t *col = row;

        while (width--)
        {
            *col = darkcolormap[*col];
            ++col;
        }

        row += linesize;
    }
}

void V_ScaleRect(vrect_t *rect)
{
    rect->sx = x1lookup[rect->x];
    rect->sy = y1lookup[rect->y];
    rect->sw = x2lookup[rect->x + rect->w - 1] - rect->sx + 1;
    rect->sh = y2lookup[rect->y + rect->h - 1] - rect->sy + 1;
}

int V_ScaleX(int x)
{
    return x1lookup[x];
}

int V_ScaleY(int y)
{
    return y1lookup[y];
}

static void ClipRect(vrect_t *rect)
{
    // clip to left and top edges
    rect->cx1 = rect->x >= 0 ? rect->x : 0;
    rect->cy1 = rect->y >= 0 ? rect->y : 0;

    // determine right and bottom edges
    rect->cx2 = rect->x + rect->w - 1;
    rect->cy2 = rect->y + rect->h - 1;

    // clip right and bottom edges
    if (rect->cx2 >= video.unscaledw)
    {
        rect->cx2 = video.unscaledw - 1;
    }
    if (rect->cy2 >= SCREENHEIGHT)
    {
        rect->cy2 = SCREENHEIGHT - 1;
    }

    // determine clipped width and height
    rect->cw = rect->cx2 - rect->cx1 + 1;
    rect->ch = rect->cy2 - rect->cy1 + 1;
}

static void ScaleClippedRect(vrect_t *rect)
{
    rect->sx = x1lookup[rect->cx1];
    rect->sy = y1lookup[rect->cy1];
    rect->sw = x2lookup[rect->cx2] - rect->sx + 1;
    rect->sh = y2lookup[rect->cy2] - rect->sy + 1;
}

void V_FillRect(int x, int y, int width, int height, byte color)
{
    vrect_t dstrect;

    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = width;
    dstrect.h = height;

    ClipRect(&dstrect);

    // clipped away completely?
    if (dstrect.cw <= 0 || dstrect.ch <= 0)
    {
        return;
    }

    ScaleClippedRect(&dstrect);

    pixel_t *dest = V_ADDRESS(dest_screen, dstrect.sx, dstrect.sy);

    while (dstrect.sh--)
    {
        memset(dest, color, dstrect.sw);
        dest += linesize;
    }
}

//
// V_CopyRect
//
// Copies a source rectangle in a screen buffer to a destination
// rectangle in another screen buffer. Source origin in srcx,srcy,
// destination origin in destx,desty, common size in width and height.
//

void V_CopyRect(int srcx, int srcy, pixel_t *source, int width, int height,
                int destx, int desty)
{
    vrect_t srcrect, dstrect;
    pixel_t *src, *dest;
    int usew, useh;

#ifdef RANGECHECK
    if (srcx + width < 0 || srcy + height < 0 || srcx >= video.unscaledw
        || srcy >= SCREENHEIGHT || destx + width < 0 || desty + height < 0
        || destx >= video.unscaledw || desty >= SCREENHEIGHT)
    {
        I_Error("Bad coordinates");
    }
#endif

    srcrect.x = srcx;
    srcrect.y = srcy;
    srcrect.w = width;
    srcrect.h = height;

    ClipRect(&srcrect);

    // clipped away completely?
    if (srcrect.cw <= 0 || srcrect.ch <= 0)
    {
        return;
    }

    ScaleClippedRect(&srcrect);

    dstrect.x = destx;
    dstrect.y = desty;
    dstrect.w = width;
    dstrect.h = height;

    ClipRect(&dstrect);

    // clipped away completely?
    if (dstrect.cw <= 0 || dstrect.ch <= 0)
    {
        return;
    }

    ScaleClippedRect(&dstrect);

    // use the smaller of the two scaled rect widths / heights
    usew = (srcrect.sw < dstrect.sw ? srcrect.sw : dstrect.sw);
    useh = (srcrect.sh < dstrect.sh ? srcrect.sh : dstrect.sh);

    src = V_ADDRESS(source, srcrect.sx, srcrect.sy);
    dest = V_ADDRESS(dest_screen, dstrect.sx, dstrect.sy);

    while (useh--)
    {
        memcpy(dest, src, usew);
        src += linesize;
        dest += linesize;
    }
}

//
// V_DrawBlock
//
// Draw a linear block of pixels into the view buffer.
//
// The bytes at src are copied in linear order to the screen rectangle
// at x,y in screenbuffer scrn, with size width by height.
//

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src)
{
    const pixel_t *source;
    pixel_t *dest;
    vrect_t dstrect;

    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = width;
    dstrect.h = height;

    ClipRect(&dstrect);

    // clipped away completely?
    if (dstrect.cw <= 0 || dstrect.ch <= 0)
    {
        return;
    }

    // change in origin due to clipping
    int dx = dstrect.cx1 - x;
    int dy = dstrect.cy1 - y;

    ScaleClippedRect(&dstrect);

    source = src + dy * width + dx;
    dest = V_ADDRESS(dest_screen, dstrect.sx, dstrect.sy);

    {
        int w;
        fixed_t xfrac, yfrac;
        int xtex, ytex;
        pixel_t *row;

        yfrac = 0;

        while (dstrect.sh--)
        {
            row = dest;
            w = dstrect.sw;
            xfrac = 0;
            ytex = (yfrac >> FRACBITS) * width;

            while (w--)
            {
                xtex = (xfrac >> FRACBITS);
                *row++ = source[ytex + xtex];
                xfrac += video.xstep;
            }

            dest += linesize;
            yfrac += video.ystep;
        }
    }
}

void V_TileBlock64(int line, int width, int height, const byte *src)
{
    pixel_t *dest, *row;
    fixed_t xfrac, yfrac;
    int xtex, ytex, h;
    vrect_t dstrect;

    dstrect.x = 0;
    dstrect.y = line;
    dstrect.w = width;
    dstrect.h = height;

    V_ScaleRect(&dstrect);

    h = dstrect.sh;
    yfrac = dstrect.sy * video.ystep;

    dest = dest_screen;

    while (h--)
    {
        int w = dstrect.sw;
        row = dest;
        xfrac = 0;
        ytex = ((yfrac >> FRACBITS) & 63) << 6;

        while (w--)
        {
            xtex = (xfrac >> FRACBITS) & 63;
            *row++ = src[ytex + xtex];
            xfrac += video.xstep;
        }

        dest += linesize;
        yfrac += video.ystep;
    }
}

//
// V_GetBlock
//
// Gets a linear block of pixels from the view buffer.
//
// The pixels in the rectangle at x,y in screenbuffer scrn with size
// width by height are linearly packed into the buffer dest.
// No return value
//

void V_GetBlock(int x, int y, int width, int height, pixel_t *dest)
{
    pixel_t *src;

#ifdef RANGECHECK
    if (x < 0 || x + width > video.width || y < 0 || y + height > video.height)
    {
        I_Error("Bad coordinates");
    }
#endif

    src = V_ADDRESS(dest_screen, x, y);

    while (height--)
    {
        memcpy(dest, src, width);
        src += linesize;
        dest += width;
    }
}

// [FG] non hires-scaling variant of V_DrawBlock, used in disk icon drawing

void V_PutBlock(int x, int y, int width, int height, pixel_t *src)
{
    pixel_t *dest;

#ifdef RANGECHECK
    if (x < 0 || x + width > video.width || y < 0 || y + height > video.height)
    {
        I_Error("Bad coordinates");
    }
#endif

    dest = V_ADDRESS(dest_screen, x, y);

    while (height--)
    {
        memcpy(dest, src, width);
        dest += linesize;
        src += width;
    }
}

//
// V_DrawBackground
// Fills the back screen with a pattern
//  for variable screen sizes
//

void V_DrawBackground(const char *patchname)
{
    const byte *src =
        V_CacheFlatNum(firstflat + R_FlatNumForName(patchname), PU_CACHE);

    V_TileBlock64(0, video.unscaledw, SCREENHEIGHT, src);
}

//
// V_Init
//

void V_Init(void)
{
    fixed_t frac, lastfrac;

    linesize = video.pitch;

    video.xscale = (video.width << FRACBITS) / video.unscaledw;
    video.yscale = (video.height << FRACBITS) / SCREENHEIGHT;
    video.xstep = ((video.unscaledw << FRACBITS) / video.width) + 1;
    video.ystep = ((SCREENHEIGHT << FRACBITS) / video.height) + 1;

    x1lookup[0] = 0;
    lastfrac = frac = 0;
    for (int i = 0; i < video.width; i++)
    {
        if (frac >> FRACBITS > lastfrac >> FRACBITS)
        {
            x1lookup[frac >> FRACBITS] = i;
            x2lookup[lastfrac >> FRACBITS] = i - 1;
            lastfrac = frac;
        }

        frac += video.xstep;
    }
    x2lookup[video.unscaledw - 1] = video.width - 1;
    x1lookup[video.unscaledw] = x2lookup[video.unscaledw] = video.width;

    y1lookup[0] = 0;
    lastfrac = frac = 0;
    for (int i = 0; i < video.height; i++)
    {
        if (frac >> FRACBITS > lastfrac >> FRACBITS)
        {
            y1lookup[frac >> FRACBITS] = i;
            y2lookup[lastfrac >> FRACBITS] = i - 1;
            lastfrac = frac;
        }

        frac += video.ystep;
    }
    y2lookup[SCREENHEIGHT - 1] = video.height - 1;
    y1lookup[SCREENHEIGHT] = y2lookup[SCREENHEIGHT] = video.height;
}

// Set the buffer that the code draws to.

void V_UseBuffer(pixel_t *buffer)
{
    dest_screen = buffer;
}

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void)
{
    dest_screen = I_VideoBuffer;
}

//
// V_ScreenShot
//
// Modified by Lee Killough so that any number of shots can be taken,
// the code is faster, and no annoying "screenshot" message appears.
//
// killough 10/98: improved error-handling

void V_ScreenShot(void)
{
    boolean success = false;

    errno = 0;

    if (!M_access(screenshotdir,2))
    {
        static int shot;
        char lbmname[16] = {0};
        int tries = 10000;
        char *screenshotname = NULL;

        do
        {
            M_snprintf(lbmname, sizeof(lbmname), "%.4s%04d.png",
                       D_DoomExeName(), shot++); // [FG] PNG
            if (screenshotname)
              free(screenshotname);
            screenshotname = M_StringJoin(screenshotdir, DIR_SEPARATOR_S,
                                          lbmname);
        }
        while (!M_access(screenshotname,0) && --tries);

        if (tries)
        {
            // killough 10/98: detect failure and remove file if error
            // killough 11/98: add hires support
            if (!(success = I_WritePNGfile(screenshotname))) // [FG] PNG
            {
                int t = errno;
                M_remove(screenshotname);
                errno = t;
            }
        }
        if (screenshotname)
        {
            free(screenshotname);
        }
    }

    // 1/18/98 killough: replace "SCREEN SHOT" acknowledgement with sfx
    // players[consoleplayer].message = "screen shot"

    // killough 10/98: print error message and change sound effect if error
    S_StartSoundPitch(NULL,
                 !success
                 ? displaymsg("%s", errno ? strerror(errno)
                                          : "Could not take screenshot"),
                 sfx_oof
                 : gamemode == commercial ? sfx_radio
                                          : sfx_tink, PITCH_NONE);
}

//----------------------------------------------------------------------------
//
// $Log: v_video.c,v $
// Revision 1.10  1998/05/06  11:12:48  jim
// Formattted v_video.*
//
// Revision 1.9  1998/05/03  22:53:16  killough
// beautification, simplify translation lookup
//
// Revision 1.8  1998/04/24  08:09:39  jim
// Make text translate tables lumps
//
// Revision 1.7  1998/03/02  11:41:58  killough
// Add cr_blue_status for blue statusbar numbers
//
// Revision 1.6  1998/02/24  01:40:12  jim
// Tuned HUD font
//
// Revision 1.5  1998/02/23  04:58:17  killough
// Fix performance problems
//
// Revision 1.4  1998/02/19  16:55:00  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/17  23:00:36  jim
// Added color translation machinery and data
//
// Revision 1.2  1998/01/26  19:25:08  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
