// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
//  Copyright (C) 2009 by James Haley, Simon Howard
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Stretch blitting for screen scaling and aspect ratio correction.
//      Inspired by Chocolate Doom.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "z_zone.h"
#include "doomtype.h"

#if defined(_MSC_VER)
#define d_inline _inline
#elif defined(__GNUC__)
#define d_inline __inline__
#else
#define d_inline
#endif

//
// i_scale_t structure
//
// Holds information needed to select a scaling function.
//

typedef struct i_scale_s
{
   int normalWidth;     // unscaled video screen width
   int normalHeight;    // unscaled video screen height
   int scaleWidth;      // resulting width
   int scaleHeight;     // resulting height
   int scaleFactor;     // scale factor
   int isAspectCorrect; // if non-zero, is aspect-ratio-corrected
   
   void (*func)(Uint8 *, SDL_Surface *); // routine
} i_scale_t;

//=============================================================================
// 
// 320x200 Stretches
//

//
// source - 320x200
// dest   - 640x400
//
static void I_Blit2x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *src, *dest, *row;
   int y = 200;

   src  = screen;
   dest = (Uint8 *)surface->pixels;

   while(y--)
   {
      int x = 320;

      row = dest;

      while(x--)
      {
         *row = *(row+1) =
            *(row+surface->pitch) = *(row+surface->pitch+1) = *src++;
         row += 2;
      }

      dest += 2 * surface->pitch;
   }
}

static i_scale_t iscale320x200_2x =
{
   320, 200,
   640, 400,
   2,
   0,
   I_Blit2x_320x200,
};

//
// source - 320x200
// dest   - 960x600
//
static void I_Blit3x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *src, *dest, *row;
   int y = 200;

   src  = screen;
   dest = (Uint8 *)surface->pixels;

   while(y--)
   {
      int x = 320;
      Uint8 *d;

      row = dest;

      while(x--)
      {
         d = row;

         *d = *(d+1) = *(d+2) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *src++;

         row += 3;
      }

      dest += 3 * surface->pitch;
   }
}

static i_scale_t iscale320x200_3x =
{
   320, 200,
   960, 600,
   3,
   0,
   I_Blit3x_320x200,
};

// 
// source - 320x200
// dest   - 1280x800
//
static void I_Blit4x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *src, *dest, *row;
   int y = 200;

   src  = screen;
   dest = (Uint8 *)surface->pixels;

   while(y--)
   {
      int x = 320;
      Uint8 *d;

      row = dest;

      while(x--)
      {
         d = row;

         *d = *(d+1) = *(d+2) = *(d+3) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *src++;

         row += 4;
      }

      dest += 4 * surface->pitch;
   }
}

static i_scale_t iscale320x200_4x =
{
   320,  200,
   1280, 800,
   4,
   0,
   I_Blit4x_320x200,
};

// 
// source - 320x200
// dest   - 1600x1000
//
static void I_Blit5x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *src, *dest, *row;
   int y = 200;

   src  = screen;
   dest = (Uint8 *)surface->pixels;

   while(y--)
   {
      int x = 320;
      Uint8 *d;

      row = dest;

      while(x--)
      {
         d = row;

         *d = *(d+1) = *(d+2) = *(d+3) = *(d+4) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *(d+4) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *(d+4) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *(d+4) = *src;
         d += surface->pitch;
         *d = *(d+1) = *(d+2) = *(d+3) = *(d+4) = *src++;

         row += 5;

      }

      dest += 5 * surface->pitch;
   }
}

static i_scale_t iscale320x200_5x =
{
   320,  200,
   1600, 1000,
   5,
   0,
   I_Blit5x_320x200,
};

//=============================================================================
// 
// 640x400 Stretches
//

//
// source - 640x400
// dest   - 1280x800
//
static void I_Blit2x_640x400(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *src, *dest, *row;
   int y = 400;

   src  = screen;
   dest = (Uint8 *)surface->pixels;

   while(y--)
   {
      int x = 640;

      row = dest;

      while(x--)
      {
         *row = 
            *(row+1) =
            *(row+surface->pitch) =
            *(row+surface->pitch+1) = *src++;

         row += 2;
      }

      dest += 2 * surface->pitch;
   }
}

static i_scale_t iscale640x400_2x =
{
   640,  400,
   1280, 800,
   2,
   0,
   I_Blit2x_640x400,
};

//=============================================================================
//
// Aspect Ratio Stretch Tables
//

//
// Search through the given palette, finding the nearest color that matches
// the given color.
//
// From Choco Doom
//
static int FindNearestColor(Uint8 *palette, int r, int g, int b)
{
   Uint8 *col;
   int best;
   int best_diff;
   int diff;
   int i;
   
   best = 0;
   best_diff = INT_MAX;
   
   for(i = 0; i < 256; ++i)
   {
      col = palette + i * 3;
      diff = (r - col[0]) * (r - col[0])
           + (g - col[1]) * (g - col[1])
           + (b - col[2]) * (b - col[2]);
      
      if (diff == 0)
      {
         return i;
      }
      else if (diff < best_diff)
      {
         best = i;
         best_diff = diff;
      }
   }
   
   return best;
}

//
// Create a stretch table.  This is a lookup table for blending colors.
// pct specifies the bias between the two colors: 0 = all y, 100 = all x.
// NB: This is identical to the lookup tables used in other ports for
// translucency.
//
// From Choco Doom
//
static Uint8 *GenerateStretchTable(byte *palette, int pct)
{
   Uint8 *result;
   int x, y;
   int r, g, b;
   Uint8 *col1;
   Uint8 *col2;
   
   result = Z_Malloc(256 * 256, PU_STATIC, NULL);
   
   for(x = 0; x < 256; ++x)
   {
      for(y = 0; y < 256; ++y)
      {
         col1 = palette + x * 3;
         col2 = palette + y * 3;
         r = (((int) col1[0]) * pct + ((int) col2[0]) * (100 - pct)) / 100;
         g = (((int) col1[1]) * pct + ((int) col2[1]) * (100 - pct)) / 100;
         b = (((int) col1[2]) * pct + ((int) col2[2]) * (100 - pct)) / 100;
         result[x * 256 + y] = FindNearestColor(palette, r, g, b);
      }
   }
   
   return result;
}

static Uint8 *stretch_tables[2];

void I_InitStretchTables(byte *palette)
{
   stretch_tables[0] = GenerateStretchTable(palette, 20);
   stretch_tables[1] = GenerateStretchTable(palette, 40);
}

//=============================================================================
//
// 320x200 Aspect Ratio Correction
//

static d_inline void WriteBlendedLine1x(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                        Uint8 *stretch_table)
{
   int x;
   
   for(x = 0; x < 320; ++x)
   {
      *dest = stretch_table[*src1 * 256 + *src2];
      ++dest;
      ++src1;
      ++src2;
   }
}

//
// source - 320x200
// dest   - 320x240
//
static void I_Stretch1x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 6 lines are written to dest_buffer
   // (200 -> 240)
   
   for(y = 0; y < 200; y += 5)
   {
      // 100% line 0
      memcpy(screenp, bufp, 320);
      screenp += surface->pitch;
      
      // 20% line 0, 80% line 1
      WriteBlendedLine1x(screenp, bufp, bufp + 320, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 40% line 1, 60% line 2
      WriteBlendedLine1x(screenp, bufp, bufp + 320, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 60% line 2, 40% line 3
      WriteBlendedLine1x(screenp, bufp + 320, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 80% line 3, 20% line 4
      WriteBlendedLine1x(screenp, bufp + 320, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 4
      memcpy(screenp, bufp, 320);
      screenp += surface->pitch; bufp += 320;
   }
}

static i_scale_t iscale320x240_1x =
{
   320, 200,
   320, 240,
   1,
   1,
   I_Stretch1x_320x200,
};

static d_inline void WriteLine2x(Uint8 *dest, Uint8 *src)
{
   int x;
   
   for(x = 0; x < 320; ++x)
   {
      dest[0] = *src;
      dest[1] = *src;
      dest += 2;
      ++src;
   }
}

static d_inline void WriteBlendedLine2x(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                        Uint8 *stretch_table)
{
   int x;
   int val;
   
   for(x = 0; x < 320; ++x)
   {
      val = stretch_table[*src1 * 256 + *src2];
      dest[0] = val;
      dest[1] = val;
      dest += 2;
      ++src1;
      ++src2;
   }
} 

//
// source - 320x200
// dest   - 640x480
//
static void I_Stretch2x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 12 lines are written to dest_buffer.
   // (200 -> 480)
   
   for(y = 0; y < 200; y += 5)
   {
      // 100% line 0
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 40% line 0, 60% line 1
      WriteBlendedLine2x(screenp, bufp, bufp + 320, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 1
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 80% line 1, 20% line 2
      WriteBlendedLine2x(screenp, bufp + 320, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 2
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 20% line 2, 80% line 3
      WriteBlendedLine2x(screenp, bufp, bufp + 320, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 3
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 60% line 3, 40% line 4
      WriteBlendedLine2x(screenp, bufp + 320, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 4
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine2x(screenp, bufp);
      screenp += surface->pitch; bufp += 320;
   }
}

static i_scale_t iscale320x240_2x =
{
   320, 200,
   640, 480,
   2,
   1,
   I_Stretch2x_320x200,
};

static d_inline void WriteLine3x(Uint8 *dest, Uint8 *src)
{
   int x;
   
   for(x = 0; x < 320; ++x)
   {
      dest[0] = *src;
      dest[1] = *src;
      dest[2] = *src;
      dest += 3;
      ++src;
   }
}

static d_inline void WriteBlendedLine3x(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                        Uint8 *stretch_table)
{
   int x;
   int val;
   
   for(x = 0; x < 320; ++x)
   {
      val = stretch_table[*src1 * 256 + *src2];
      dest[0] = val;
      dest[1] = val;
      dest[2] = val;
      dest += 3;
      ++src1;
      ++src2;
   }
} 

//
// source - 320x200
// dest   - 960x720
//
static void I_Stretch3x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   byte *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 18 lines are written to dest_buffer.
   // (200 -> 720)
   
   for(y = 0; y < 200; y += 5)
   {
      // 100% line 0
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 60% line 0, 40% line 1
      WriteBlendedLine3x(screenp, bufp + 320, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 1
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 1
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 1
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 20% line 1, 80% line 2
      WriteBlendedLine3x(screenp, bufp, bufp + 320, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 2
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 80% line 2, 20% line 3
      WriteBlendedLine3x(screenp, bufp + 320, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 3
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 3
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 3
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 40% line 3, 60% line 4
      WriteBlendedLine3x(screenp, bufp, bufp + 320, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 4
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine3x(screenp, bufp);
      screenp += surface->pitch; bufp += 320;
   }
}

static i_scale_t iscale320x240_3x =
{
   320, 200,
   960, 720,
   3,
   1,
   I_Stretch3x_320x200,
};

static d_inline void WriteLine4x(Uint8 *dest, Uint8 *src)
{
   int x;
   
   for(x = 0; x < 320; ++x)
   {
      dest[0] = *src;
      dest[1] = *src;
      dest[2] = *src;
      dest[3] = *src;
      dest += 4;
      ++src;
   }
}

static d_inline void WriteBlendedLine4x(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                        Uint8 *stretch_table)
{
   int x;
   int val;
   
   for(x = 0; x < 320; ++x)
   {
      val = stretch_table[*src1 * 256 + *src2];
      dest[0] = val;
      dest[1] = val;
      dest[2] = val;
      dest[3] = val;
      dest += 4;
      ++src1;
      ++src2;
   }
} 

//
// source - 320x200
// dest   - 1280x960
//
static void I_Stretch4x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   byte *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 24 lines are written to dest_buffer.
   // (200 -> 960)
   
   for(y = 0; y < 200; y += 5)
   {
      // 100% line 0
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 90% line 0, 20% line 1
      WriteBlendedLine4x(screenp, bufp + 320, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 1
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 1
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 1
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 1
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 60% line 1, 40% line 2
      WriteBlendedLine4x(screenp, bufp + 320, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 2
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 40% line 2, 60% line 3
      WriteBlendedLine4x(screenp, bufp, bufp + 320, stretch_tables[1]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 3
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 3
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 3
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 3
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 20% line 3, 80% line 4
      WriteBlendedLine4x(screenp, bufp, bufp + 320, stretch_tables[0]);
      screenp += surface->pitch; bufp += 320;
      
      // 100% line 4
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine4x(screenp, bufp);
      screenp += surface->pitch; bufp += 320;
   }
}

static i_scale_t iscale320x240_4x =
{
   320,  200,
   1280, 960,
   4,
   1,
   I_Stretch4x_320x200,
};

static d_inline void WriteLine5x(Uint8 *dest, Uint8 *src)
{
    int x;

    for(x = 0; x < 320; ++x)
    {
        dest[0] = *src;
        dest[1] = *src;
        dest[2] = *src;
        dest[3] = *src;
        dest[4] = *src;
        dest += 5;
        ++src;
    }
}

//
// source - 320x200
// dest   - 1600x1200
//
static void I_Stretch5x_320x200(Uint8 *screen, SDL_Surface *surface)
{
   byte *bufp, *screenp;
   int y;
      
   // Need to byte-copy from buffer into the screen buffer
   
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 1 line of src_buffer, 6 lines are written to dest_buffer.
   // (200 -> 1200)
   
   for(y = 0; y < 200; y += 1)
   {
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine5x(screenp, bufp);
      screenp += surface->pitch; bufp += 320;
   }
}

static i_scale_t iscale320x240_5x =
{
   320,  200,
   1600, 1200,
   5,
   1,
   I_Stretch5x_320x200,
};

//=============================================================================
//
// 640x400 Aspect Ratio Correction
//

static d_inline void WriteBlendedLine1x2(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                         Uint8 *stretch_table)
{
   int x;
   
   for(x = 0; x < 640; ++x)
   {
      *dest = stretch_table[*src1 * 256 + *src2];
      ++dest;
      ++src1;
      ++src2;
   }
}

//
// source - 640x400
// dest   - 640x480
//
static void I_Stretch1x_640x400(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 6 lines are written to dest_buffer
   // (400 -> 480)
   
   for(y = 0; y < 400; y += 5)
   {
      // 100% line 0
      memcpy(screenp, bufp, 640);
      screenp += surface->pitch;
      
      // 20% line 0, 80% line 1
      WriteBlendedLine1x2(screenp, bufp, bufp + 640, stretch_tables[0]);
      screenp += surface->pitch; bufp += 640;
      
      // 40% line 1, 60% line 2
      WriteBlendedLine1x2(screenp, bufp, bufp + 640, stretch_tables[1]);
      screenp += surface->pitch; bufp += 640;
      
      // 60% line 2, 40% line 3
      WriteBlendedLine1x2(screenp, bufp + 640, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 640;
      
      // 80% line 3, 20% line 4
      WriteBlendedLine1x2(screenp, bufp + 640, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 640;
      
      // 100% line 4
      memcpy(screenp, bufp, 640);
      screenp += surface->pitch; bufp += 640;
   }
}

static i_scale_t iscale640x480_1x =
{
   640, 400,
   640, 480,
   1,
   1,
   I_Stretch1x_640x400,
};

static d_inline void WriteLine2x2(Uint8 *dest, Uint8 *src)
{
   int x;
   
   for(x = 0; x < 640; ++x)
   {
      dest[0] = *src;
      dest[1] = *src;
      dest += 2;
      ++src;
   }
}

static d_inline void WriteBlendedLine2x2(Uint8 *dest, Uint8 *src1, Uint8 *src2, 
                                        Uint8 *stretch_table)
{
   int x;
   int val;
   
   for(x = 0; x < 640; ++x)
   {
      val = stretch_table[*src1 * 256 + *src2];
      dest[0] = val;
      dest[1] = val;
      dest += 2;
      ++src1;
      ++src2;
   }
} 

//
// source - 640x400
// dest   - 1280x960
//
static void I_Stretch2x_640x400(Uint8 *screen, SDL_Surface *surface)
{
   Uint8 *bufp, *screenp;
   int y;
   
   // Need to byte-copy from buffer into the screen buffer
   
   bufp    = screen;
   screenp = (Uint8 *)surface->pixels;
   
   // For every 5 lines of src_buffer, 12 lines are written to dest_buffer.
   // (400 -> 960)
   
   for(y = 0; y < 400; y += 5)
   {
      // 100% line 0
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 0
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 40% line 0, 60% line 1
      WriteBlendedLine2x2(screenp, bufp, bufp + 640, stretch_tables[1]);
      screenp += surface->pitch; bufp += 640;
      
      // 100% line 1
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 80% line 1, 20% line 2
      WriteBlendedLine2x2(screenp, bufp + 640, bufp, stretch_tables[0]);
      screenp += surface->pitch; bufp += 640;
      
      // 100% line 2
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 2
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 20% line 2, 80% line 3
      WriteBlendedLine2x2(screenp, bufp, bufp + 640, stretch_tables[0]);
      screenp += surface->pitch; bufp += 640;
      
      // 100% line 3
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 60% line 3, 40% line 4
      WriteBlendedLine2x2(screenp, bufp + 640, bufp, stretch_tables[1]);
      screenp += surface->pitch; bufp += 640;
      
      // 100% line 4
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch;
      
      // 100% line 4
      WriteLine2x2(screenp, bufp);
      screenp += surface->pitch; bufp += 640;
   }
}

static i_scale_t iscale640x480_2x =
{
   640,  400,
   1280, 960,
   2,
   1,
   I_Stretch2x_640x400,
};

static i_scale_t *scalers[] =
{
   &iscale320x200_2x, // 320x200 -> 640x400
   &iscale320x200_3x, // 320x200 -> 960x600
   &iscale320x200_4x, // 320x200 -> 1280x800
   &iscale320x200_5x, // 320x200 -> 1600x1000
   &iscale640x400_2x, // 640x400 -> 1280x800
   &iscale320x240_1x, // 320x200 -> 320x240
   &iscale320x240_2x, // 320x200 -> 640x480
   &iscale320x240_3x, // 320x200 -> 960x720
   &iscale320x240_4x, // 320x200 -> 1280x960
   &iscale320x240_5x, // 320x200 -> 1600x1200
   &iscale640x480_1x, // 640x400 -> 640x480
   &iscale640x480_2x, // 640x400 -> 1280x960
};

#define NUMSCALERS (sizeof(scalers) / sizeof(i_scale_t *))

void I_GetScaler(int sw, int sh, int ar, int scale, int *nw, int *nh, 
                 void (**f)(Uint8 *, SDL_Surface *))
{
   int i;

   for(i = 0; i < NUMSCALERS; ++i)
   {
      if(sw == scalers[i]->normalWidth    && // match current video width
         sh == scalers[i]->normalHeight   && // match current video height
         scalers[i]->scaleFactor == scale && // match requested scale factor
         scalers[i]->isAspectCorrect == ar)  // match requested a.r. correction
      {
         *nw = scalers[i]->scaleWidth;  // return new width in nw
         *nh = scalers[i]->scaleHeight; // return new height in nh
         *f  = scalers[i]->func;        // return scaler routine
         return;
      }
   }

   // if no scaler was found, no scaling :(
}

// EOF

