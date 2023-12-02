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
//
// DESCRIPTION:
//  Gamma correction LUT stuff.
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "doomtype.h"
#include "r_draw.h"
#include "r_main.h"
#include "m_bbox.h"
#include "w_wad.h"   /* needed for color translation lump lookup */
#include "v_trans.h"
#include "v_video.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_misc2.h"

pixel_t *I_VideoBuffer;

// The screen buffer that the v_video.c code draws to.

static pixel_t *dest_screen = NULL;

//jff 2/18/98 palette color ranges for translation
//jff 4/24/98 now pointers set to predefined lumps to allow overloading

char *cr_brick;
char *cr_tan;
char *cr_gray;
char *cr_green;
char *cr_brown;
char *cr_gold;
char *cr_red;
char *cr_blue;
char *cr_blue2;
char *cr_orange;
char *cr_yellow;
char *cr_black;
char *cr_purple;
char *cr_white;
// [FG] dark/shaded color translation table
char *cr_dark;

//jff 4/24/98 initialize this at runtime
char *colrngs[CR_LIMIT] = {0};
char *red2col[CR_LIMIT] = {0};

// Now where did these came from?
byte gammatable[5][256] =
{
  {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
   17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
   33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
   49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
   65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
   81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
   97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
   113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
   128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
   144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
   160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
   176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
   208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
   224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
   240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

  {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
   32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
   56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
   78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
   99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
   115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
   130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
   146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
   161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
   175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
   190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
   205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
   219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
   233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
   247,248,249,250,251,252,252,253,254,255},

  {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
   43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
   70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
   94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
   113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
   129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
   144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
   160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
   174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
   188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
   202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
   216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
   229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
   242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
   255},

  {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
   57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
   86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
   108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
   125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
   141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
   155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
   169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
   183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
   195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
   207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
   219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
   231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
   242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
   253,253,254,254,255},

  {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
   78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
   107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
   125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
   142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
   156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
   169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
   182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
   193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
   204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
   214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
   224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
   234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
   243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
   251,252,252,253,254,254,255,255}
};

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

typedef struct {
  const char *name;
  char **map1, **map2, **map_orig;
} crdef_t;

// killough 5/2/98: table-driven approach
static const crdef_t crdefs[] = {
  {"CRBRICK",  &cr_brick,   &colrngs[CR_BRICK ], &red2col[CR_BRICK ]},
  {"CRTAN",    &cr_tan,     &colrngs[CR_TAN   ], &red2col[CR_TAN   ]},
  {"CRGRAY",   &cr_gray,    &colrngs[CR_GRAY  ], &red2col[CR_GRAY  ]},
  {"CRGREEN",  &cr_green,   &colrngs[CR_GREEN ], &red2col[CR_GREEN ]},
  {"CRBROWN",  &cr_brown,   &colrngs[CR_BROWN ], &red2col[CR_BROWN ]},
  {"CRGOLD",   &cr_gold,    &colrngs[CR_GOLD  ], &red2col[CR_GOLD  ]},
  {"CRRED",    &cr_red,     &colrngs[CR_RED   ], &red2col[CR_RED   ]},
  {"CRBLUE",   &cr_blue,    &colrngs[CR_BLUE1 ], &red2col[CR_BLUE1 ]},
  {"CRORANGE", &cr_orange,  &colrngs[CR_ORANGE], &red2col[CR_ORANGE]},
  {"CRYELLOW", &cr_yellow,  &colrngs[CR_YELLOW], &red2col[CR_YELLOW]},
  {"CRBLUE2",  &cr_blue2,   &colrngs[CR_BLUE2 ], &red2col[CR_BLUE2 ]},
  {"CRBLACK",  &cr_black,   &colrngs[CR_BLACK ], &red2col[CR_BLACK ]},
  {"CRPURPLE", &cr_purple,  &colrngs[CR_PURPLE], &red2col[CR_PURPLE]},
  {"CRWHITE",  &cr_white,   &colrngs[CR_WHITE ], &red2col[CR_WHITE ]},
  {NULL}
};

// [FG] translate between blood color value as per EE spec
//      and actual color translation table index

static const int bloodcolor[] = {
  CR_RED,    // 0 - Red (normal)
  CR_GRAY,   // 1 - Grey
  CR_GREEN,  // 2 - Green
  CR_BLUE2,  // 3 - Blue
  CR_YELLOW, // 4 - Yellow
  CR_BLACK,  // 5 - Black
  CR_PURPLE, // 6 - Purple
  CR_WHITE,  // 7 - White
  CR_ORANGE, // 8 - Orange
};

int V_BloodColor(int blood)
{
  return bloodcolor[blood];
}

int v_lightest_color, v_darkest_color;

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
      (*p->map2)[i] = V_Colorize(playpal, p - crdefs, (byte) i);
    }

    // [FG] override with original color translations
    if (iwad_playpal && !force_rebuild)
    {
      for (i = 0; i < 256; i++)
      {
        if (((*p->map_orig)[i] != (char) i) || (keepgray && i == 109))
        {
          (*p->map2)[i] = (*p->map_orig)[i];
        }
      }
    }

    *p->map1 = *p->map2;
  }

  v_lightest_color = I_GetPaletteIndex(playpal, 0xFF, 0xFF, 0xFF);
  v_darkest_color  = I_GetPaletteIndex(playpal, 0x00, 0x00, 0x00);
}

void WriteGeneratedLumpWad(const char *filename)
{
    int i;
    const size_t num_lumps = arrlen(crdefs);
    lumpinfo_t *lumps = calloc(num_lumps, sizeof(*lumps));

    for (i = 0; i < num_lumps - 1; i++) // last entry is dummy
    {
        M_CopyLumpName(lumps[i].name, crdefs[i].name);
        lumps[i].data = *crdefs[i].map2;
        lumps[i].size = 256;
    }

    M_CopyLumpName(lumps[i].name, "TRANMAP");
    lumps[i].data = main_tranmap;
    lumps[i].size = 256 * 256;

    WriteLumpWad(filename, lumps, num_lumps);

    free(lumps);
}

#define WIDE_SCREENWIDTH 576 // corresponds to 2.4:1 in original resolution

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
    byte *translation;
} patch_column_t;

static void (*drawcolfunc)(const patch_column_t *patchcol);

static void V_DrawPatchColumn(const patch_column_t *patchcol)
{
    int      count;
    byte     *dest;    // killough
    fixed_t  frac;     // killough
    fixed_t  fracstep;

    count = patchcol->y2 - patchcol->y1 + 1;

    if (count <= 0) // Zero length, column does not exceed a pixel.
    {
        return;
    }

#ifdef RANGECHECK
    if ((unsigned int)patchcol->x  >= (unsigned int)video.width ||
        (unsigned int)patchcol->y1 >= (unsigned int)video.height)
    {
        I_Error("V_DrawPatchColumn: %i to %i at %i", patchcol->y1, patchcol->y2,
                patchcol->x);
    }
#endif

    dest = V_ADDRESS(dest_screen, patchcol->x, patchcol->y1);

    // Determine scaling, which is the only mapping to be done.
    fracstep = patchcol->step;
    frac = patchcol->frac + ((patchcol->y1 * fracstep) & 0xFFFF);

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.       (Yeah, right!!! -- killough)
    //
    // killough 2/1/98: more performance tuning
    // haleyjd 06/21/06: rewrote and specialized for screen patches
    {
        const byte *source = patchcol->source;

        while ((count -= 2) >= 0)
        {
            *dest = source[frac >> FRACBITS];
            dest += linesize;
            frac += fracstep;
            *dest = source[frac >> FRACBITS];
            dest += linesize;
            frac += fracstep;
        }
        if (count & 1)
            *dest = source[frac >> FRACBITS];
    }
}

static void V_DrawPatchColumnTL(const patch_column_t *patchcol)
{
    int      count;
    byte     *dest;    // killough
    fixed_t  frac;     // killough
    fixed_t  fracstep;

    count = patchcol->y2 - patchcol->y1 + 1;

    if (count <= 0) // Zero length, column does not exceed a pixel.
    {
        return;
    }

#ifdef RANGECHECK
    if ((unsigned int)patchcol->x  >= (unsigned int)video.width ||
        (unsigned int)patchcol->y1 >= (unsigned int)video.height)
    {
        I_Error("V_DrawPatchColumn: %i to %i at %i", patchcol->y1, patchcol->y2,
                patchcol->x);
    }
#endif

    dest = V_ADDRESS(dest_screen, patchcol->x, patchcol->y1);

    // Determine scaling, which is the only mapping to be done.
    fracstep = patchcol->step;
    frac = patchcol->frac + ((patchcol->y1 * fracstep) & 0xFFFF);

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.       (Yeah, right!!! -- killough)
    //
    // killough 2/1/98: more performance tuning
    // haleyjd 06/21/06: rewrote and specialized for screen patches
    {
        const byte *source = patchcol->source;
        const byte *translation = patchcol->translation;

        while ((count -= 2) >= 0)
        {
            *dest = translation[source[frac >> FRACBITS]];
            dest += linesize;
            frac += fracstep;
            *dest = translation[source[frac >> FRACBITS]];
            dest += linesize;
            frac += fracstep;
        }
        if (count & 1)
        {
            *dest = translation[source[frac >> FRACBITS]];
        }
    }
}

static void V_DrawMaskedColumn(patch_column_t *patchcol, const int ytop,
                               column_t *column)
{
  for( ; column->topdelta != 0xff; column = (column_t *)((byte *)column + column->length + 4))
  {
      // calculate unclipped screen coordinates for post
      int columntop = ytop + column->topdelta;

      if (columntop >= 0)
      {
          // SoM: Make sure the lut is never referenced out of range
          if (columntop >= video.unscaledh)
              return;

          patchcol->y1 = y1lookup[columntop];
          patchcol->frac = 0;
      }
      else
      {
          patchcol->frac = (-columntop) << FRACBITS;
          patchcol->y1 = 0;
      }

      if (columntop + column->length - 1 < 0)
          continue;
      if (columntop + column->length - 1 < video.unscaledh)
          patchcol->y2 = y2lookup[columntop + column->length - 1];
      else
          patchcol->y2 = y2lookup[video.unscaledh - 1];

      // SoM: The failsafes should be completely redundant now...
      // haleyjd 05/13/08: fix clipping; y2lookup not clamped properly
      if ((column->length > 0 && patchcol->y2 < patchcol->y1) ||
          patchcol->y2 >= video.height)
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

void V_DrawPatchInt(int x, int y, patch_t *patch, boolean flipped,
                    byte *translation)
{
    int        x1, x2, w;
    fixed_t    iscale, xiscale, startfrac = 0;
    int        maxw;
    patch_column_t patchcol = {0};

    w = patch->width;

    // calculate edges of the shape
    if (flipped)
    {
        // If flipped, then offsets are flipped as well which means they
        // technically offset from the right side of the patch (x2)
        x2 = x + patch->leftoffset;
        x1 = x2 - (w - 1);
    }
    else
    {
        x1 = x - patch->leftoffset;
        x2 = x1 + w - 1;
    }

    iscale        = video.xstep;
    patchcol.step = video.ystep;
    maxw          = video.unscaledw;

    // off the left or right side?
    if (x2 < 0 || x1 >= maxw)
        return;

    if (translation)
    {
        patchcol.translation = translation;
        drawcolfunc = V_DrawPatchColumnTL;
    }
    else
    {
        drawcolfunc = V_DrawPatchColumn;
    }

    xiscale = flipped ? -iscale : iscale;

    // haleyjd 10/10/08: must handle coordinates outside the screen buffer
    // very carefully here.
    if (x1 >= 0)
        x1 = x1lookup[x1];
    else
        x1 = -x2lookup[-x1 - 1];

    if (x2 < video.unscaledw)
        x2 = x2lookup[x2];
    else
        x2 = x2lookup[video.unscaledw - 1];

    patchcol.x = (x1 < 0) ? 0 : x1;

    // SoM: Any time clipping occurs on screen coords, the resulting clipped
    // coords should be checked to make sure we are still on screen.
    if (x2 < x1)
        return;

    // SoM: Ok, so the startfrac should ALWAYS be the last post of the patch
    // when the patch is flipped minus the fractional "bump" from the screen
    // scaling, then the patchcol.x to x1 clipping will place the frac in the
    // correct column no matter what. This also ensures that scaling will be
    // uniform. If the resolution is 320x2X0 the iscale will be 65537 which
    // will create some fractional bump down, so it is safe to assume this puts
    // us just below patch->width << 16
    if (flipped)
        startfrac = (w << FRACBITS) - ((x1 * iscale) & 0xffff) - 1;
    else
        startfrac = (x1 * iscale) & 0xffff;

    if (patchcol.x > x1)
        startfrac += xiscale * (patchcol.x - x1);

    {
        column_t *column;
        int      texturecolumn;

        const int ytop = y - patch->topoffset;
        for( ; patchcol.x <= x2; patchcol.x++, startfrac += xiscale)
        {
            texturecolumn = startfrac >> FRACBITS;

    #ifdef RANGECHECK
            if(texturecolumn < 0 || texturecolumn >= w)
            {
                I_Error("V_DrawPatchInt: bad texturecolumn %d\n", texturecolumn);
            }
    #endif

            column = (column_t *)((byte *)patch + patch->columnofs[texturecolumn]);
            V_DrawMaskedColumn(&patchcol, ytop, column);
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
    V_DrawPatchInt(x, y, patch, flipped, NULL);
}

void V_DrawPatchTranslated(int x, int y, patch_t *patch, char *outr)
{
    x += video.deltaw;
    V_DrawPatchInt(x, y, patch, false, (byte *)outr);
}

void V_DrawPatchFullScreen(patch_t *patch)
{
    const int x = (video.unscaledw - SHORT(patch->width)) / 2;

    patch->leftoffset = 0;
    patch->topoffset = 0;

    // [crispy] fill pillarboxes in widescreen mode
    if (video.unscaledw != NONWIDEWIDTH)
    {
        memset(dest_screen, v_darkest_color, video.width * video.height);
    }

    V_DrawPatchInt(x, 0, patch, false, NULL);
}

void V_ScaleRect(vrect_t *rect)
{
    rect->sx = x1lookup[rect->cx1];
    rect->sy = y1lookup[rect->cy1];
    rect->sw = x2lookup[rect->cx2] - rect->sx + 1;
    rect->sh = y2lookup[rect->cy2] - rect->sy + 1;

#ifdef RANGECHECK
    // sanity check - out-of-bounds values should not come out of the scaling
    // arrays so long as they are accessed within bounds.
    if(rect->sx < 0 || rect->sx + rect->sw > video.width ||
       rect->sy < 0 || rect->sy + rect->sh > video.height)
    {
        I_Error("V_ScaleRect: internal error - invalid scaling lookups");
    }
#endif
}

void V_ClipRect(vrect_t *rect)
{
    // clip to left and top edges
    rect->cx1 = rect->x >= 0 ? rect->x : 0;
    rect->cy1 = rect->y >= 0 ? rect->y : 0;

    // determine right and bottom edges
    rect->cx2 = rect->x + rect->w - 1;
    rect->cy2 = rect->y + rect->h - 1;

    // clip right and bottom edges
    if (rect->cx2 >= video.unscaledw)
        rect->cx2 =  video.unscaledw - 1;
    if (rect->cy2 >= video.unscaledh)
        rect->cy2 =  video.unscaledh - 1;

    // determine clipped width and height
    rect->cw = rect->cx2 - rect->cx1 + 1;
    rect->ch = rect->cy2 - rect->cy1 + 1;
}

//
// V_CopyRect
//
// Copies a source rectangle in a screen buffer to a destination
// rectangle in another screen buffer. Source origin in srcx,srcy,
// destination origin in destx,desty, common size in width and height.
//

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty)
{
    vrect_t srcrect, dstrect;
    byte *src, *dest;
    int usew, useh;

#ifdef RANGECHECK
    // rejection if source rect is off-screen
    if (srcx + width < 0 || srcy + height < 0 ||
        srcx >= video.unscaledw || srcy >= video.unscaledh ||
        destx + width < 0 || desty + height < 0 ||
        destx >= video.unscaledw || desty >= video.unscaledh)
    {
        I_Error("Bad V_CopyRect");
    }
#endif

    // populate source rect
    srcrect.x = srcx;
    srcrect.y = srcy;
    srcrect.w = width;
    srcrect.h = height;

    V_ClipRect(&srcrect);

    // clipped away completely?
    if (srcrect.cw <= 0 || srcrect.ch <= 0)
        return;

    // populate dest rect
    dstrect.x = destx;
    dstrect.y = desty;
    dstrect.w = width;
    dstrect.h = height;

    V_ClipRect(&dstrect);

    // clipped away completely?
    if (dstrect.cw <= 0 || dstrect.ch <= 0)
        return;

    V_ScaleRect(&srcrect);
    V_ScaleRect(&dstrect);

    // use the smaller of the two scaled rect widths / heights
    usew = (srcrect.sw < dstrect.sw ? srcrect.sw : dstrect.sw);
    useh = (srcrect.sh < dstrect.sh ? srcrect.sh : dstrect.sh);

    src  = V_ADDRESS(source, srcrect.sx, srcrect.sy);
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
    const byte *source;
    byte *dest;
    int dx, dy;
    vrect_t dstrect;

    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = width;
    dstrect.h = height;

    V_ClipRect(&dstrect);

    // clipped away completely?
    if (dstrect.cw <= 0 || dstrect.ch <= 0)
        return;

    // change in origin due to clipping
    dx = dstrect.cx1 - x;
    dy = dstrect.cy1 - y;

    V_ScaleRect(&dstrect);

    source = src + dy * width + dx;
    dest = V_ADDRESS(dest_screen, dstrect.sx, dstrect.sy);

    {
        int     w;
        fixed_t xfrac, yfrac;
        int     xtex, ytex;
        byte    *row;

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
    byte *dest, *row;
    fixed_t xfrac, yfrac;
    int xtex, ytex, h;
    vrect_t dstrect;

    dstrect.x = 0;
    dstrect.y = line;
    dstrect.w = width;
    dstrect.h = height;

    V_ClipRect(&dstrect);
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

void V_GetBlock(int x, int y, int width, int height, byte *dest)
{
  byte *src;

#ifdef RANGECHECK
  if (x<0
      ||x+width >SCREENWIDTH
      || y<0
      || y+height>SCREENHEIGHT )
    I_Error ("Bad V_GetBlock");
#endif

  if (hires)   // killough 11/98: hires support
    y<<=2, x<<=1, width<<=1, height<<=1;

  src = dest_screen + y*SCREENWIDTH+x;
  while (height--)
    {
      memcpy (dest, src, width);
      src += SCREENWIDTH << hires;
      dest += width;
    }
}

// [FG] non hires-scaling variant of V_DrawBlock, used in disk icon drawing

void V_PutBlock(int x, int y, int width, int height, byte *src)
{
  byte *dest;

#ifdef RANGECHECK
  if (x<0
      ||x+width >SCREENWIDTH
      || y<0
      || y+height>SCREENHEIGHT )
    I_Error ("Bad V_PutBlock");
#endif

  if (hires)
    y<<=2, x<<=1, width<<=1, height<<=1;

  dest = dest_screen + y*SCREENWIDTH+x;

  while (height--)
    {
      memcpy (dest, src, width);
      dest += SCREENWIDTH << hires;
      src += width;
    }
}

void V_DrawHorizLine(int x, int y, int width, byte color)
{
    byte line[WIDE_SCREENWIDTH];
    memset(line, color, width);
    V_DrawBlock(x, y, width, 1, line);
}

void V_ShadeScreen(void)
{
  int y;
  byte *dest = dest_screen;
  const int targshade = 20, step = 2;
  static int oldtic = -1;
  static int screenshade;

  // [FG] start a new sequence
  if (gametic - oldtic > targshade / step)
  {
    screenshade = 0;
  }

  for (y = 0; y < video.width * video.height; y++)
  {
    dest[y] = colormaps[0][screenshade * 256 + dest[y]];
  }

  if (screenshade < targshade && gametic != oldtic)
  {
    screenshade += step;

    if (screenshade > targshade)
      screenshade = targshade;
  }

  oldtic = gametic;
}

//
// V_DrawBackground
// Fills the back screen with a pattern
//  for variable screen sizes
//

void V_DrawBackground(const char *patchname)
{
    const byte *src = W_CacheLumpNum(firstflat + R_FlatNumForName(patchname), PU_CACHE);

    V_TileBlock64(0, video.unscaledw, video.unscaledh, src);
}

//
// V_Init
//

void V_Init(void)
{
    int i;
    fixed_t frac, lastfrac;

    linesize = video.width;

    x1lookup[0] = 0;
    lastfrac = frac = 0;
    for(i = 0; i < video.width; i++)
    {
        if(frac >> FRACBITS > lastfrac >> FRACBITS)
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
    for(i = 0; i < video.height; i++)
    {
        if(frac >> FRACBITS > lastfrac >> FRACBITS)
        {
            y1lookup[frac >> FRACBITS] = i;
            y2lookup[lastfrac >> FRACBITS] = i - 1;
            lastfrac = frac;
        }

        frac += video.ystep;
    }
    y2lookup[video.unscaledh - 1] = video.height - 1;
    y1lookup[video.unscaledh] = y2lookup[video.unscaledh] = video.height;
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

