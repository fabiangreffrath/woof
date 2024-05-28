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
//      The actual span/column drawing functions.
//      Here find the main potential for optimization,
//       e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "r_bsp.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "v_fmt.h"
#include "v_video.h"
#include "z_zone.h"

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//

int  scaledviewwidth;
int  scaledviewheight;        // killough 11/98
int  scaledviewx;
int  scaledviewy;
int  viewwidth;
int  viewheight;
int  viewwindowx;
int  viewwindowy; 
static byte **ylookup = NULL;
static int  *columnofs = NULL;
static int  linesize;  // killough 11/98

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//

byte translations[3][256];
 
byte *tranmap;          // translucency filter maps 256x256   // phares 
byte *main_tranmap;     // killough 4/11/98

// Backing buffer containing the bezel drawn around the screen and surrounding
// background.

static pixel_t *background_buffer = NULL;

//
// R_DrawColumn
// Source is the top of the column to scale.
//

lighttable_t *dc_colormap[2]; // [crispy] brightmaps
int     dc_x; 
int     dc_yl; 
int     dc_yh; 
fixed_t dc_iscale; 
fixed_t dc_texturemid;
int     dc_texheight;    // killough
byte    *dc_source;      // first pixel in a column (possibly virtual) 
byte    dc_skycolor;

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 

void R_DrawColumn (void) 
{ 
  int              count; 
  register byte    *dest;            // killough
  register fixed_t frac;            // killough
  fixed_t          fracstep;     

  count = dc_yh - dc_yl + 1; 

  if (count <= 0)    // Zero length, column does not exceed a pixel.
    return; 
                                 
#ifdef RANGECHECK 
  if ((unsigned)dc_x >= video.width
      || dc_yl < 0
      || dc_yh >= video.height) 
    I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

  // Framebuffer destination address.
  // Use ylookup LUT to avoid multiply with ScreenWidth.
  // Use columnofs LUT for subwindows? 

  dest = ylookup[dc_yl] + columnofs[dc_x];  

  // Determine scaling, which is the only mapping to be done.

  fracstep = dc_iscale; 
  frac = dc_texturemid + (dc_yl-centery)*fracstep; 

  // Inner loop that does the actual texture mapping,
  //  e.g. a DDA-lile scaling.
  // This is as fast as it gets.       (Yeah, right!!! -- killough)
  //
  // killough 2/1/98: more performance tuning

  {
    register const byte *source = dc_source;            
    register lighttable_t *const *colormap = dc_colormap;
    register int heightmask = dc_texheight-1;
    register const byte *brightmap = dc_brightmap;
    if (dc_texheight & heightmask)   // not a power of 2 -- killough
      {
        heightmask++;
        heightmask <<= FRACBITS;
          
        if (frac < 0)
          while ((frac += heightmask) <  0);
        else
          while (frac >= heightmask)
            frac -= heightmask;
          
        do
          {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            
            // [crispy] brightmaps
            byte src = source[frac>>FRACBITS];
            *dest = colormap[brightmap[src]][src];
            dest += linesize;                     // killough 11/98
            if ((frac += fracstep) >= heightmask)
              frac -= heightmask;
          } 
        while (--count);
      }
    else
      {
        while ((count-=2)>=0)   // texture height is a power of 2 -- killough
          {
            byte src = source[(frac>>FRACBITS) & heightmask];
            *dest = colormap[brightmap[src]][src];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            src = source[(frac>>FRACBITS) & heightmask];
            *dest = colormap[brightmap[src]][src];
            dest += linesize;   // killough 11/98
            frac += fracstep;
          }
        if (count & 1)
        {
          byte src = source[(frac>>FRACBITS) & heightmask];
          *dest = colormap[brightmap[src]][src];
        }
      }
  }
} 

// Here is the version of R_DrawColumn that deals with translucent  // phares
// textures and sprites. It's identical to R_DrawColumn except      //    |
// for the spot where the color index is stuffed into *dest. At     //    V
// that point, the existing color index and the new color index
// are mapped through the TRANMAP lump filters to get a new color
// index whose RGB values are the average of the existing and new
// colors.
//
// Since we're concerned about performance, the 'translucent or
// opaque' decision is made outside this routine, not down where the
// actual code differences are.

void R_DrawTLColumn (void)                                           
{ 
  int              count; 
  register byte    *dest;           // killough
  register fixed_t frac;            // killough
  fixed_t          fracstep;

  count = dc_yh - dc_yl + 1; 

  // Zero length, column does not exceed a pixel.
  if (count <= 0)
    return; 
                                 
#ifdef RANGECHECK 
  if ((unsigned)dc_x >= video.width
      || dc_yl < 0
      || dc_yh >= video.height) 
    I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

  // Framebuffer destination address.
  // Use ylookup LUT to avoid multiply with ScreenWidth.
  // Use columnofs LUT for subwindows? 

  dest = ylookup[dc_yl] + columnofs[dc_x];  
  
  // Determine scaling,
  //  which is the only mapping to be done.

  fracstep = dc_iscale; 
  frac = dc_texturemid + (dc_yl-centery)*fracstep; 

  // Inner loop that does the actual texture mapping,
  //  e.g. a DDA-lile scaling.
  // This is as fast as it gets.       (Yeah, right!!! -- killough)
  //
  // killough 2/1/98, 2/21/98: more performance tuning
  
  {
    register const byte *source = dc_source;            
    register lighttable_t *const *colormap = dc_colormap;
    register int heightmask = dc_texheight-1;
    register const byte *brightmap = dc_brightmap;
    if (dc_texheight & heightmask)   // not a power of 2 -- killough
      {
        heightmask++;
        heightmask <<= FRACBITS;
          
        if (frac < 0)
          while ((frac += heightmask) <  0);
        else
          while (frac >= heightmask)
            frac -= heightmask;
        
        do
          {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
              
            // [crispy] brightmaps
            byte src = source[frac>>FRACBITS];
            *dest = tranmap[(*dest<<8)+colormap[brightmap[src]][src]]; // phares
            dest += linesize;          // killough 11/98
            if ((frac += fracstep) >= heightmask)
              frac -= heightmask;
          } 
        while (--count);
      }
    else
      {
        while ((count-=2)>=0)   // texture height is a power of 2 -- killough
          {
            byte src = source[(frac>>FRACBITS) & heightmask];
            *dest = tranmap[(*dest<<8)+colormap[brightmap[src]][src]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            src = source[(frac>>FRACBITS) & heightmask];
            *dest = tranmap[(*dest<<8)+colormap[brightmap[src]][src]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
          }
        if (count & 1)
        {
          byte src = source[(frac>>FRACBITS) & heightmask];
          *dest = tranmap[(*dest<<8)+colormap[brightmap[src]][src]]; // phares
        }
      }
  }
} 

//
// Sky drawing: for showing just a color above the texture
// Taken from Eternity Engine eternity-engine/source/r_draw.c:L170-234
//
void R_DrawSkyColumn(void)
{
  int count;
  byte *dest;
  fixed_t frac;
  fixed_t fracstep;

  count = dc_yh - dc_yl + 1;

  if (count <= 0)
    return;

#ifdef RANGECHECK
  if ((unsigned)dc_x >= video.width
    || dc_yl < 0
    || dc_yh >= video.height)
    I_Error ("R_DrawSkyColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

  dest = ylookup[dc_yl] + columnofs[dc_x];

  fracstep = dc_iscale; 
  frac = dc_texturemid + (dc_yl - centery) * fracstep;

  {
    const byte *source = dc_source;
    const lighttable_t *colormap = dc_colormap[0];
    const byte skycolor = dc_skycolor;
    int heightmask = dc_texheight - 1;

    // Fill in the median color here
    // Have two intermediary fade lines, using the main_tranmap structure
    int i, n;

    if (frac < -2 * FRACUNIT)
      {
        n = (-frac - 2 * FRACUNIT + fracstep - 1) / fracstep;
        if (n > count)
          n = count;

        for (i = 0; i < n; ++i)
          {
            *dest = colormap[skycolor];
            dest += linesize;
            frac += fracstep;
          }

        if (!(count -= n))
          return;
      }

    if (frac < -FRACUNIT)
      {
        n = (-frac - FRACUNIT + fracstep - 1) / fracstep;
        if (n > count)
          n = count;

        for (i = 0; i < n; ++i)
          {
            *dest = main_tranmap[(main_tranmap[(colormap[source[0]] << 8) +
                                                colormap[skycolor]
                                              ] << 8
                                  ) + colormap[skycolor]
                                ];
            dest += linesize;
            frac += fracstep;
          }

        if (!(count -= n))
          return;
      }

    // Now it's on the edge
    if (frac < 0)
      {
        n = (-frac + fracstep - 1) / fracstep;
        if (n > count)
          n = count;

        for (i = 0; i < n; ++i)
          {
            *dest = main_tranmap[(colormap[source[0]] << 8) + colormap[skycolor]];
            dest += linesize;
            frac += fracstep;
          }

        if (!(count -= n))
          return;
      }

    if (dc_texheight & heightmask)   // not a power of 2 -- killough
      {
        heightmask++;
        heightmask <<= FRACBITS;

        while (frac >= heightmask)
          frac -= heightmask;

        do
          {
            *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            if ((frac += fracstep) >= heightmask)
              frac -= heightmask;
          } 
        while (--count);
      }
    else
      {
        while ((count-=2)>=0)   // texture height is a power of 2 -- killough
          {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
          }
        if (count & 1)
          *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
  }
}

//
// Spectre/Invisibility.
//

#define FUZZTABLE 50 

// killough 11/98: convert fuzzoffset to be screenwidth-independent
static const int fuzzoffset[FUZZTABLE] = {
  0,-1,0,-1,0,0,-1,
  0,0,-1,0,0,0,-1,
  0,0,0,-1,-1,-1,-1,
  0,-1,-1,0,0,0,0,-1,
  0,-1,0,0,-1,-1,0,
  0,-1,-1,-1,-1,0,0,
  0,0,-1,0,0,-1,0 
}; 

static int fuzzpos = 0; 

// [crispy] draw fuzz effect independent of rendering frame rate
static int fuzzpos_tic;

void R_SetFuzzPosTic(void)
{
  // [crispy] prevent the animation from remaining static
  if (fuzzpos == fuzzpos_tic)
  {
    fuzzpos = (fuzzpos + 1) % FUZZTABLE;
  }
  fuzzpos_tic = fuzzpos;
}

void R_SetFuzzPosDraw(void)
{
  fuzzpos = fuzzpos_tic;
}

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//

static void R_DrawFuzzColumn_orig(void)
{ 
  int      count; 
  byte     *dest; 
  boolean  cutoff = false;

  // Adjust borders. Low... 
  if (!dc_yl) 
    dc_yl = 1;

  // .. and high.
  if (dc_yh == viewheight-1) 
  {
    dc_yh = viewheight - 2; 
    cutoff = true;
  }
                 
  count = dc_yh - dc_yl; 

  // Zero length.
  if (count < 0) 
    return; 
    
#ifdef RANGECHECK 
  if ((unsigned) dc_x >= video.width
      || dc_yl < 0 
      || dc_yh >= video.height)
    I_Error ("R_DrawFuzzColumn: %i to %i at %i",
             dc_yl, dc_yh, dc_x);
#endif

  // Keep till detailshift bug in blocky mode fixed,
  //  or blocky mode removed.

  // Does not work with blocky mode.
  dest = ylookup[dc_yl] + columnofs[dc_x];
  
  // Looks like an attempt at dithering,
  // using the colormap #6 (of 0-31, a bit brighter than average).

  count++;        // killough 1/99: minor tuning

  do 
    {
      // Lookup framebuffer, and retrieve
      // a pixel that is either one row
      // above or below the current one.
      // Add index from colormap to index.
      // killough 3/20/98: use fullcolormap instead of colormaps
      // killough 11/98: use linesize

      // fraggle 1/8/2000: fix with the bugfix from lees
      // why_i_left_doom.html

      *dest = fullcolormap[6*256+dest[fuzzoffset[fuzzpos++] ? -linesize : linesize]];
      dest += linesize;             // killough 11/98

      // Clamp table lookup index.
      fuzzpos &= (fuzzpos - FUZZTABLE) >> (8*sizeof fuzzpos-1); //killough 1/99
    } 
  while (--count);

  // [crispy] if the line at the bottom had to be cut off,
  // draw one extra line using only pixels of that line and the one above
  if (cutoff)
  {
    *dest = fullcolormap[6*256+dest[linesize*fuzzoffset[fuzzpos]]];
  }
}

// [FG] "blocky" spectre drawing for hires mode

static void R_DrawFuzzColumn_block(void)
{
  int count;
  byte *dest;
  boolean cutoff = false;
  const int nx = video.xscale >> FRACBITS;
  const int ny = video.yscale >> FRACBITS;

  if (dc_x % nx)
    return;

  dc_yl += ny;
  dc_yl -= dc_yl % ny;
  dc_yh -= dc_yh % ny;

  if (!dc_yl)
    dc_yl = ny;

  if (dc_yh >= viewheight - ny)
  {
    dc_yh = viewheight - 2 * ny;
    cutoff = true;
  }

  count = dc_yh - dc_yl;

  if (count < 0)
    return;

#ifdef RANGECHECK
  if ((unsigned) dc_x >= video.width
      || dc_yl < 0
      || dc_yh >= video.height)
    I_Error ("R_DrawFuzzColumn: %i to %i at %i",
             dc_yl, dc_yh, dc_x);
#endif

  dest = ylookup[dc_yl] + columnofs[dc_x];

  count += ny;

  do
    {
      // [FG] draw only even pixels as (nx * ny) squares
      //      using the same fuzzoffset value
      const int offset = fuzzoffset[fuzzpos] ? -ny * linesize : ny * linesize;
      const byte fuzz = fullcolormap[6 * 256 + dest[offset]];
      int i;

      for (i = 0; i < ny && count - i > 0; i++)
      {
        memset(dest, fuzz, nx);
        dest += linesize;
      }

      fuzzpos++;
      fuzzpos &= (fuzzpos - FUZZTABLE) >> (8 * sizeof(fuzzpos) - 1);
    }
  while ((count -= ny) > 0);

  if (cutoff)
    {
      const int offset = ny * linesize * fuzzoffset[fuzzpos];
      const byte fuzz = fullcolormap[6 * 256 + dest[offset]];
      int i;

      for (i = 0; i < ny; i++)
      {
        memset(dest, fuzz, nx);
        dest += linesize;
      }
    }
}

// [FG] spectre drawing mode: 0 original, 1 blocky (hires)

boolean fuzzcolumn_mode;
void (*R_DrawFuzzColumn) (void) = R_DrawFuzzColumn_orig;
void R_SetFuzzColumnMode (void)
{
  if (fuzzcolumn_mode && current_video_height > SCREENHEIGHT)
    R_DrawFuzzColumn = R_DrawFuzzColumn_block;
  else
    R_DrawFuzzColumn = R_DrawFuzzColumn_orig;
}

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//

byte *dc_translation, *translationtables;

void R_DrawTranslatedColumn (void) 
{ 
  int      count; 
  byte     *dest; 
  fixed_t  frac;
  fixed_t  fracstep;     
 
  count = dc_yh - dc_yl; 
  if (count < 0) 
    return; 
                                 
#ifdef RANGECHECK 
  if ((unsigned)dc_x >= video.width
      || dc_yl < 0
      || dc_yh >= video.height)
    I_Error ( "R_DrawColumn: %i to %i at %i",
              dc_yl, dc_yh, dc_x);
#endif 

  // FIXME. As above.
  dest = ylookup[dc_yl] + columnofs[dc_x]; 

  // Looks familiar.
  fracstep = dc_iscale; 
  frac = dc_texturemid + (dc_yl-centery)*fracstep; 

  count++;        // killough 1/99: minor tuning

  // Here we do an additional index re-mapping.
  do 
    {
      // Translation tables are used
      //  to map certain colorramps to other ones,
      //  used with PLAY sprites.
      // Thus the "green" ramp of the player 0 sprite
      //  is mapped to gray, red, black/indigo. 
      
      // [crispy] brightmaps
      byte src = dc_source[frac>>FRACBITS];
      *dest = dc_colormap[dc_brightmap[src]][dc_translation[src]];
      dest += linesize;      // killough 11/98
        
      frac += fracstep; 
    }
  while (--count); 
} 

//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//

void R_InitTranslationTables (void)
{
  int i;
        
  // killough 5/2/98:
  // Remove dependency of colormaps aligned on 256-byte boundary

  translationtables = Z_Malloc(256*3, PU_STATIC, 0);
    
  // translate just the 16 green colors
  for (i=0; i<256; i++)
    if (i >= 0x70 && i<= 0x7f)
      {   // map green ramp to gray, brown, red
        translationtables[i] = 0x60 + (i&0xf);
        translationtables [i+256] = 0x40 + (i&0xf);
        translationtables [i+512] = 0x20 + (i&0xf);
      }
    else  // Keep all other colors as is.
      translationtables[i]=translationtables[i+256]=translationtables[i+512]=i;
}

//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//

int  ds_y; 
int  ds_x1; 
int  ds_x2;

lighttable_t *ds_colormap[2];
const byte *ds_brightmap;

fixed_t ds_xfrac; 
fixed_t ds_yfrac; 
fixed_t ds_xstep; 
fixed_t ds_ystep;

// start of a 64*64 tile image 
byte *ds_source;        

void R_DrawSpan (void) 
{ 
  byte *source;
  byte **colormap;
  byte *dest;
  const byte *brightmap;
    
  unsigned count;
  unsigned spot; 
  unsigned xtemp;
  unsigned ytemp;
                
  source = ds_source;
  colormap = ds_colormap;
  dest = ylookup[ds_y] + columnofs[ds_x1];       
  brightmap = ds_brightmap;
  count = ds_x2 - ds_x1 + 1; 
        
  // [FG] fix flat distortion towards the right of the screen
  while (count >= 4)
    { 
      byte src;
      ytemp = (ds_yfrac >> 10) & 0x0FC0;
      xtemp = (ds_xfrac >> 16) & 0x003F;
      spot = xtemp | ytemp;
      ds_xfrac += ds_xstep;
      ds_yfrac += ds_ystep;
      src = source[spot];
      dest[0] = colormap[brightmap[src]][src];

      ytemp = (ds_yfrac >> 10) & 0x0FC0;
      xtemp = (ds_xfrac >> 16) & 0x003F;
      spot = xtemp | ytemp;
      ds_xfrac += ds_xstep;
      ds_yfrac += ds_ystep;
      src = source[spot];
      dest[1] = colormap[brightmap[src]][src];
        
      ytemp = (ds_yfrac >> 10) & 0x0FC0;
      xtemp = (ds_xfrac >> 16) & 0x003F;
      spot = xtemp | ytemp;
      ds_xfrac += ds_xstep;
      ds_yfrac += ds_ystep;
      src = source[spot];
      dest[2] = colormap[brightmap[src]][src];
        
      ytemp = (ds_yfrac >> 10) & 0x0FC0;
      xtemp = (ds_xfrac >> 16) & 0x003F;
      spot = xtemp | ytemp;
      ds_xfrac += ds_xstep;
      ds_yfrac += ds_ystep;
      src = source[spot];
      dest[3] = colormap[brightmap[src]][src];
                
      dest += 4;
      count -= 4;
    } 

  while (count)
    { 
      byte src;
      ytemp = (ds_yfrac >> 10) & 0x0FC0;
      xtemp = (ds_xfrac >> 16) & 0x003F;
      spot = xtemp | ytemp;
      ds_xfrac += ds_xstep;
      ds_yfrac += ds_ystep;
      src = source[spot];
      *dest++ = colormap[brightmap[src]][src];
      count--;
    } 
} 

void R_InitBufferRes(void)
{
  if (solidcol) Z_Free(solidcol);
  if (columnofs) Z_Free(columnofs);
  if (ylookup) Z_Free(ylookup);

  columnofs = Z_Malloc(video.width * sizeof(*columnofs), PU_STATIC, NULL);
  ylookup = Z_Malloc(video.height * sizeof(*ylookup), PU_STATIC, NULL);

  solidcol = Z_Calloc(1, video.width * sizeof(*solidcol), PU_STATIC, NULL);
}

//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//

void R_InitBuffer(void)
{
  int i;

  linesize = video.pitch;    // killough 11/98

  // Handle resize,
  //  e.g. smaller view windows
  //  with border and/or status bar.

  // Column offset. For windows.

  for (i = viewwidth; i--; )   // killough 11/98
    columnofs[i] = viewwindowx + i;

  // Same with base row offset.

  // Preclaculate all row offsets.

  for (i = viewheight; i--; )
    ylookup[i] = I_VideoBuffer + (i + viewwindowy) * linesize; // killough 11/98

  if (background_buffer != NULL)
  {
    Z_Free(background_buffer);
    background_buffer = NULL;
  }
}

void R_DrawBorder (int x, int y, int w, int h)
{
  int i, j;
  patch_t *patch;

  patch = V_CachePatchName("brdr_t", PU_CACHE);
  for (i = 0; i < w; i += 8)
    V_DrawPatch(x + i - video.deltaw, y - 8, patch);

  patch = V_CachePatchName("brdr_b", PU_CACHE);
  for (i = 0; i < w; i += 8)
    V_DrawPatch(x + i - video.deltaw, y + h, patch);

  patch = V_CachePatchName("brdr_l", PU_CACHE);
  for (j = 0; j < h; j += 8)
    V_DrawPatch(x - 8 - video.deltaw, y + j, patch);

  patch = V_CachePatchName("brdr_r", PU_CACHE);
  for (j = 0; j < h; j += 8)
    V_DrawPatch(x + w - video.deltaw, y + j, patch);

  // Draw beveled edge. 
  V_DrawPatch(x - 8 - video.deltaw,
              y - 8,
              V_CachePatchName("brdr_tl", PU_CACHE));
    
  V_DrawPatch(x + w - video.deltaw,
              y - 8,
              V_CachePatchName("brdr_tr", PU_CACHE));
    
  V_DrawPatch(x - 8 - video.deltaw,
              y + h,
              V_CachePatchName("brdr_bl", PU_CACHE));
    
  V_DrawPatch(x + w - video.deltaw,
              y + h,
              V_CachePatchName("brdr_br", PU_CACHE));
}

void R_FillBackScreen (void)
{
  if (scaledviewwidth == video.unscaledw)
    return;

  // Allocate the background buffer if necessary
  if (background_buffer == NULL)
  {
    int size = video.pitch * video.height;
    background_buffer = Z_Malloc(size * sizeof(*background_buffer), PU_STATIC, NULL);
  }

  V_UseBuffer(background_buffer);

  V_DrawBackground(gamemode == commercial ? "GRNROCK" : "FLOOR7_2");

  R_DrawBorder(scaledviewx, scaledviewy, scaledviewwidth, scaledviewheight);

  V_RestoreBuffer();
}

//
// Copy a screen buffer.
//

void R_VideoErase(int x, int y, int w, int h)
{
  if (background_buffer == NULL)
    return;

  V_CopyRect(x, y, background_buffer, w, h, x, y);
}

//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
// killough 11/98: 
// Rewritten to avoid relying on screen wraparound, so that it
// can scale to hires automatically in R_VideoErase().
//

void R_DrawViewBorder(void)
{
  if (scaledviewwidth == video.unscaledw || background_buffer == NULL)
    return;

  // copy top
  R_VideoErase(0, 0, video.unscaledw, scaledviewy);

  // copy sides
  R_VideoErase(0, scaledviewy, scaledviewx, scaledviewheight);
  R_VideoErase(scaledviewx + scaledviewwidth, scaledviewy, scaledviewx,
               scaledviewheight);

  // copy bottom
  R_VideoErase(0, scaledviewy + scaledviewheight, video.unscaledw, scaledviewy);
}

//----------------------------------------------------------------------------
//
// $Log: r_draw.c,v $
// Revision 1.16  1998/05/03  22:41:46  killough
// beautification
//
// Revision 1.15  1998/04/19  01:16:48  killough
// Tidy up last fix's code
//
// Revision 1.14  1998/04/17  15:26:55  killough
// fix showstopper
//
// Revision 1.13  1998/04/12  01:57:51  killough
// Add main_tranmap
//
// Revision 1.12  1998/03/23  03:36:28  killough
// Use new 'fullcolormap' for fuzzy columns
//
// Revision 1.11  1998/02/23  04:54:59  killough
// #ifdef out translucency code since its in asm
//
// Revision 1.10  1998/02/20  21:57:04  phares
// Preliminarey sprite translucency
//
// Revision 1.9  1998/02/17  06:23:40  killough
// #ifdef out code duplicated in asm for djgpp targets
//
// Revision 1.8  1998/02/09  03:18:02  killough
// Change MAXWIDTH, MAXHEIGHT defintions
//
// Revision 1.7  1998/02/02  13:17:55  killough
// performance tuning
//
// Revision 1.6  1998/01/27  16:33:59  phares
// more testing
//
// Revision 1.5  1998/01/27  16:32:24  phares
// testing
//
// Revision 1.4  1998/01/27  15:56:58  phares
// Comment about invisibility
//
// Revision 1.3  1998/01/26  19:24:40  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:05:55  killough
// Use unrolled version of R_DrawSpan
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
