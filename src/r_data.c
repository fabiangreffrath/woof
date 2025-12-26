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
//      Preparation of data for rendering,
//      generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include "r_data.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_think.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_swap.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "r_bmaps.h" // [crispy] R_BrightmapForTexName()
#include "r_defs.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_skydefs.h"
#include "r_state.h"
#include "r_tranmap.h"
#include "v_patch.h"
#include "v_video.h" // cr_dark, cr_shaded
#include "w_wad.h"
#include "z_zone.h"

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef PACKED_PREFIX struct
{
  short originx;
  short originy;
  short patch;
  short stepdir;         // unused in Doom but might be used in Phase 2 Boom
  short colormap;        // unused in Doom but might be used in Phase 2 Boom
} PACKED_SUFFIX mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef PACKED_PREFIX struct
{
  char       name[8];
  int        masked;
  short      width;
  short      height;
  char       pad[4];       // unused in Doom but might be used in Boom Phase 2
  short      patchcount;
  mappatch_t patches[1];
} PACKED_SUFFIX maptexture_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

// killough 4/17/98: make firstcolormaplump,lastcolormaplump external
int firstcolormaplump, lastcolormaplump;      // killough 4/17/98

int       firstflat, lastflat, numflats;
int       first_tx, last_tx, num_tx;
int       firstspritelump, lastspritelump, numspritelumps;
int       numtextures;
texture_t **textures;
int       *texturewidthmask;
int       *texturewidth;
fixed_t   *textureheight; //needed for texture pegging (and TFE fix - killough)
int       *texturecompositesize;
short     **texturecolumnlump;
unsigned  **texturecolumnofs;  // killough 4/9/98: make 32-bit
unsigned  **texturecolumnofs2;
byte      **texturecomposite;
byte      **texturecomposite2;
int       *flattranslation;             // for global animation
int       *flatterrain;
int       *texturetranslation;
const byte **texturebrightmap; // [crispy] brightmaps


// needed for pre-rendering
fixed_t   *spritewidth, *spriteoffset, *spritetopoffset;

//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//

//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

static void R_DrawColumnInCache(const column_t *patch, byte *cache,
				int originy, int cacheheight, byte *marks)
{
  int top = -1;
  while (patch->topdelta != 0xff)
    {
      int count = patch->length;
      int position;
      // [FG] support for tall patches in DeePsea format
      if (patch->topdelta <= top)
      {
            top += patch->topdelta;
      }
      else
      {
            top = patch->topdelta;
      }
      position = originy + top;

      if (position < 0)
        {
          count += position;
          position = 0;
        }

      if (position + count > cacheheight)
        count = cacheheight - position;

      if (count > 0)
        {
          memcpy (cache + position, (byte *)patch + 3, count);

          // killough 4/9/98: remember which cells in column have been drawn,
          // so that column can later be converted into a series of posts, to
          // fix the Medusa bug.

          memset (marks + position, 0xff, count);
        }

      patch = (column_t *)((byte *) patch + patch->length + 4);
    }
}

//
// R_GenerateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug

static void R_GenerateComposite(int texnum)
{
  byte *block = Z_Malloc(texturecompositesize[texnum], PU_STATIC,
                         (void **) &texturecomposite[texnum]);
  texture_t *texture = textures[texnum];
  // Composite the columns together.
  texpatch_t *patch = texture->patches;
  short *collump = texturecolumnlump[texnum];
  unsigned *colofs = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit
  unsigned *colofs2 = texturecolumnofs2[texnum];
  int i = texture->patchcount;
  // killough 4/9/98: marks to identify transparent regions in merged textures
  byte *marks = Z_Calloc(texture->width, texture->height, PU_STATIC, 0), *source;

  // [FG] memory block for opaque textures
  byte *block2 = Z_Malloc(texture->width * texture->height, PU_STATIC,
                          (void **) &texturecomposite2[texnum]);
  // [FG] initialize composite background to palette index 0 (usually black)
  memset(block, 0, texturecompositesize[texnum]);

  for (; --i >=0; patch++)
    {
      patch_t *realpatch = V_CachePatchNum(patch->patch, PU_CACHE);
      int x, x1 = patch->originx, x2 = x1 + SHORT(realpatch->width);
      const int *cofs = realpatch->columnofs - x1;

      if (x1 < 0)
        x1 = 0;
      if (x2 > texture->width)
        x2 = texture->width;
      for (x = x1; x < x2 ; x++)
        // [FG] generate composites for single-patched columns as well
//      if (collump[x] == -1)      // Column has multiple patches?
          // killough 1/25/98, 4/9/98: Fix medusa bug.
          R_DrawColumnInCache((column_t*)((byte*) realpatch + LONG(cofs[x])),
                              // [FG] single-patched columns are normally not composited
                              // but directly read from the patch lump ignoring their originy
                              block + colofs[x], collump[x] == -1 ? patch->originy : 0,
			      texture->height, marks + x*texture->height);
    }

  // killough 4/9/98: Next, convert multipatched columns into true columns,
  // to fix Medusa bug while still allowing for transparent regions.

  source = Z_Malloc(texture->height, PU_STATIC, 0);       // temporary column
  for (i=0; i < texture->width; i++)
    // [FG] generate composites for all columns
//  if (collump[i] == -1)                 // process only multipatched columns
      {
        column_t *col = (column_t *)(block + colofs[i] - 3);  // cached column
        const byte *mark = marks + i * texture->height;
        int j = 0;
        // [FG] absolut topdelta for first 254 pixels, then relative
        int abstop, reltop = 0;
        boolean relative = false;

        // save column in temporary so we can shuffle it around
        memcpy(source, (byte *) col + 3, texture->height);
        // [FG] copy composited columns to opaque texture
        memcpy(block2 + colofs2[i], source, texture->height);

        for (;;)  // reconstruct the column by scanning transparency marks
          {
	    unsigned len;        // killough 12/98

            while (j < texture->height && reltop < 254 && !mark[j]) // skip transparent cells
              j++, reltop++;

            if (j >= texture->height)           // if at end of column
              {
                col->topdelta = -1;             // end-of-column marker
                break;
              }

            // [FG] absolut topdelta for first 254 pixels, then relative
            col->topdelta = relative ? reltop : j; // starting offset of post

            // [FG] once we pass the 254 boundary, topdelta becomes relative
            if ((abstop = j) >= 254)
            {
              relative = true;
              reltop = 0;
            }

	    // killough 12/98:
	    // Use 32-bit len counter, to support tall 1s multipatched textures

	    for (len = 0; j < texture->height && reltop < 254 && mark[j]; j++, reltop++)
              len++;                    // count opaque cells

	    col->length = len; // killough 12/98: intentionally truncate length

            // copy opaque cells from the temporary back into the column
            memcpy((byte *) col + 3, source + abstop, len);
            col = (column_t *)((byte *) col + len + 4); // next post
          }
      }
  Z_Free(source);         // free temporary column
  Z_Free(marks);          // free transparency marks

  // Now that the texture has been built in column cache,
  // it is purgable from zone memory.

  Z_ChangeTag(block, PU_CACHE);
  Z_ChangeTag(block2, PU_CACHE);
}

//
// R_GenerateLookup
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

static void R_GenerateLookup(int texnum, int *const errors)
{
  const texture_t *texture = textures[texnum];

  // Composited texture not created yet.

  short *collump = texturecolumnlump[texnum];
  unsigned *colofs = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit
  unsigned *colofs2 = texturecolumnofs2[texnum];

  // killough 4/9/98: keep count of posts in addition to patches.
  // Part of fix for medusa bug for multipatched 2s normals.

  struct {
    unsigned patches, posts;
  } *count = Z_Calloc(sizeof *count, texture->width, PU_STATIC, 0);

  // killough 12/98: First count the number of patches per column.

  const texpatch_t *patch = texture->patches;
  int i = texture->patchcount;

  while (--i >= 0)
    {
      int pat = patch->patch;
      const patch_t *realpatch = V_CachePatchNum(pat, PU_CACHE);
      int x, x1 = patch++->originx, x2 = x1 + SHORT(realpatch->width);
      const int *cofs = realpatch->columnofs - x1;
      
      if (x2 > texture->width)
	x2 = texture->width;
      if (x1 < 0)
	x1 = 0;
      for (x = x1 ; x<x2 ; x++)
	{
	  count[x].patches++;
	  collump[x] = pat;
	  colofs[x] = LONG(cofs[x])+3;
	}
    }

  // killough 4/9/98: keep a count of the number of posts in column,
  // to fix Medusa bug while allowing for transparent multipatches.
  //
  // killough 12/98:
  // Post counts are only necessary if column is multipatched,
  // so skip counting posts if column comes from a single patch.
  // This allows arbitrarily tall textures for 1s walls.
  //
  // If texture is >= 256 tall, assume it's 1s, and hence it has
  // only one post per column. This avoids crashes while allowing
  // for arbitrarily tall multipatched 1s textures.

  // [FG] generate composites for all textures
//  if (texture->patchcount > 1 && texture->height < 256)
    {
      // killough 12/98: Warn about a common column construction bug
      unsigned limit = texture->height*3+3; // absolute column size limit
      int badcol = 1;

      for (i = texture->patchcount, patch = texture->patches; --i >= 0;)
	{
	  int pat = patch->patch;
	  const patch_t *realpatch = V_CachePatchNum(pat, PU_CACHE);
	  int x, x1 = patch++->originx, x2 = x1 + SHORT(realpatch->width);
	  const int *cofs = realpatch->columnofs - x1;
	  
	  if (x2 > texture->width)
	    x2 = texture->width;
	  if (x1 < 0)
	    x1 = 0;

	  for (x = x1 ; x<x2 ; x++)
	    // [FG] generate composites for all columns
//	    if (count[x].patches > 1)        // Only multipatched columns
	      {
		const column_t *col =
		  (column_t*)((byte*) realpatch+LONG(cofs[x]));
		const byte *base = (const byte *) col;

		// count posts
		for (;col->topdelta != 0xff; count[x].posts++)
		  if ((unsigned)((byte *) col - base) <= limit)
		    col = (column_t *)((byte *) col + col->length + 4);
		  else
		    { // killough 12/98: warn about column construction bug
		      if (badcol)
			{
			  badcol = 0;
			  I_Printf(VB_DEBUG, "Warning: Texture %8.8s "
				 "(height %d) has bad column(s)"
				 " starting at x = %d.",
				 texture->name, texture->height, x);
			}
		      break;
		    }
	      }
	}
    }

  // Now count the number of columns
  //  that are covered by more than one patch.
  // Fill in the lump / offset, so columns
  //  with only a single patch are all done.

  texturecomposite[texnum] = 0;
  texturecomposite2[texnum] = 0;

  {
    int x = texture->width;
    int height = texture->height;
    int csize = 0, err = 0;        // killough 10/98

    while (--x >= 0)
      {
	if (!count[x].patches)     // killough 4/9/98
	// [FG] treat missing patches as non-fatal
//	  if (devparm)
	    {
	      // killough 8/8/98
	      I_Printf(VB_DEBUG, "R_GenerateLookup:"
		     " Column %d is without a patch in texture %.8s",
		     x, texture->name);
//	      ++*errors;
	    }
//	  else
//	    err = 1;               // killough 10/98

        // [FG] treat patch-less columns the same as multi-patched
        if (count[x].patches > 1 || !count[x].patches)       // killough 4/9/98
          {
            // killough 1/25/98, 4/9/98:
            //
            // Fix Medusa bug, by adding room for column header
            // and trailer bytes for each post in merged column.
            // For now, just allocate conservatively 4 bytes
            // per post per patch per column, since we don't
            // yet know how many posts the merged column will
            // require, and it's bounded above by this limit.

            collump[x] = -1;              // mark lump as multipatched
          // [FG] moved up here, the rest in this loop
          // applies to single-patched textures as well
          }
            colofs[x] = csize + 3;        // three header bytes in a column
	    // killough 12/98: add room for one extra post
            csize += 4*count[x].posts+5;  // 1 stop byte plus 4 bytes per post
        csize += height;                  // height bytes of texture data
        // [FG] initialize opaque texture column offset
        colofs2[x] = x * height;
      }

    texturecompositesize[texnum] = csize;
    
    if (err)       // killough 10/98: non-verbose output
      {
	I_Printf(VB_WARNING, "R_GenerateLookup: Column without a patch in texture %.8s",
	       texture->name);
	++*errors;
      }
  }
  Z_Free(count);                    // killough 4/9/98
}

//
// R_GetColumn
// Updated to support Non-power-of-2 textures, everywhere
//

byte *R_GetColumn(int tex, int col)
{
  const int width = texturewidth[tex];
  const int mask = texturewidthmask[tex];
  int ofs;

  if (mask + 1 == width)
  {
    col &= mask;
  }
  else
  {
    while (col < 0)
    {
      col += width;
    }
    col %= width;
  }

  ofs  = texturecolumnofs2[tex][col];

  if (!texturecomposite2[tex])
    R_GenerateComposite(tex);

  return texturecomposite2[tex] + ofs;
}

// [FG] wrapping column getter function for composited translucent mid-textures on 2S walls
byte *R_GetColumnMasked(int tex, int col)
{
  int ofs;

  while (col < 0)
    col += texturewidth[tex];

  col %= texturewidth[tex];
  ofs  = texturecolumnofs[tex][col];

  if (!texturecomposite[tex])
    R_GenerateComposite(tex);

  return texturecomposite[tex] + ofs;
}

//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//

static inline void RegisterTexture(texture_t *texture, int i)
{
    // [crispy] initialize brightmaps
    texturebrightmap[i] = R_BrightmapForTexName(texture->name);

    // killough 4/9/98: make column offsets 32-bit;
    // clean up malloc-ing to use sizeof
    // killough 12/98: fix sizeofs
    texturecolumnlump[i] =
        Z_Malloc(texture->width * sizeof(**texturecolumnlump), PU_STATIC, 0);
    texturecolumnofs[i] =
        Z_Malloc(texture->width * sizeof(**texturecolumnofs), PU_STATIC, 0);
    texturecolumnofs2[i] =
        Z_Malloc(texture->width * sizeof(**texturecolumnofs2), PU_STATIC, 0);

    int j;
    for (j = 1; j * 2 <= texture->width; j <<= 1)
        ;
    texturewidthmask[i] = j - 1;
    textureheight[i] = texture->height << FRACBITS;
    texturewidth[i] = texture->width;
}

void R_InitTextures (void)
{
  maptexture_t *mtexture;
  texture_t    *texture;
  mappatch_t   *mpatch;
  texpatch_t   *patch;
  int  i, j, k;
  int  *maptex;
  int  *maptex1, *maptex2;
  char name[9];
  char *names;
  char *name_p;
  int  *patchlookup;
  int  numpatches;
  int  nummappatches;
  int  offset;
  int  maxoff, maxoff2;
  int  numtextures1, numtextures2, tx_numtextures;
  int  *directory;
  int  errors = 0;

  // Load the patch names from pnames.lmp.
  name[8] = 0;
  names = W_CacheLumpName("PNAMES", PU_STATIC);
  nummappatches = LONG(*((int *)names));
  name_p = names+4;
  numpatches = nummappatches;

  first_tx = W_CheckNumForName("TX_START") + 1;
  last_tx  = W_CheckNumForName("TX_END") - 1;
  tx_numtextures = last_tx - first_tx + 1;

  if (tx_numtextures > 0)
  {
    numpatches += tx_numtextures;
  }

  patchlookup = Z_Malloc(numpatches*sizeof(*patchlookup), PU_STATIC, 0);  // killough

  for (i=0 ; i<nummappatches ; i++)
    {
      strncpy (name,name_p+i*8, 8);
      patchlookup[i] = W_CheckNumForName(name);

      // [EB] some wads use the texture namespace but then still use those in pnames
      if (patchlookup[i] == -1)
        patchlookup[i] = (W_CheckNumForName)(name, ns_textures);

      if (patchlookup[i] == -1)
        {
          // killough 4/17/98:
          // Some wads use sprites as wall patches, so repeat check and
          // look for sprites this time, but only if there were no wall
          // patches found. This is the same as allowing for both, except
          // that wall patches always win over sprites, even when they
          // appear first in a wad. This is a kludgy solution to the wad
          // lump namespace problem.

          patchlookup[i] = (W_CheckNumForName)(name, ns_sprites);

          if (patchlookup[i] == -1)
            I_Printf(VB_DEBUG, "Warning: patch %.8s, index %d does not exist",name,i);
        }

      if (patchlookup[i] != -1 && !V_LumpIsPatch(patchlookup[i]))
        {
          I_Printf(VB_WARNING, "R_InitTextures: patch %.8s, index %d is invalid", name, i);
          patchlookup[i] = (W_CheckNumForName)("TNT1A0", ns_sprites);
        }

    }
  Z_Free(names);

  // Load the map texture definitions from textures.lmp.
  // The data is contained in one or two lumps,
  //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.

  maptex = maptex1 = W_CacheLumpName("TEXTURE1", PU_STATIC);
  numtextures1 = LONG(*maptex);
  maxoff = W_LumpLength(W_GetNumForName("TEXTURE1"));
  directory = maptex+1;

  if (W_CheckNumForName("TEXTURE2") != -1)
    {
      maptex2 = W_CacheLumpName("TEXTURE2", PU_STATIC);
      numtextures2 = LONG(*maptex2);
      maxoff2 = W_LumpLength(W_GetNumForName("TEXTURE2"));
    }
  else
    {
      maptex2 = NULL;
      numtextures2 = 0;
      maxoff2 = 0;
    }
  numtextures = numtextures1 + numtextures2;

  if (tx_numtextures > 0)
  {
    for (int p = 0; p < tx_numtextures ; p++)
    {
      patchlookup[nummappatches + p] = first_tx + p;
    }
    numtextures += tx_numtextures;
  }

  // killough 4/9/98: make column offsets 32-bit;
  // clean up malloc-ing to use sizeof

  textures = Z_Malloc(numtextures*sizeof*textures, PU_STATIC, 0);
  texturecolumnlump =
    Z_Malloc(numtextures*sizeof*texturecolumnlump, PU_STATIC, 0);
  texturecolumnofs =
    Z_Malloc(numtextures*sizeof*texturecolumnofs, PU_STATIC, 0);
  texturecolumnofs2 =
    Z_Malloc(numtextures*sizeof*texturecolumnofs2, PU_STATIC, 0);
  texturecomposite =
    Z_Malloc(numtextures*sizeof*texturecomposite, PU_STATIC, 0);
  texturecomposite2 =
    Z_Malloc(numtextures*sizeof*texturecomposite2, PU_STATIC, 0);
  texturecompositesize =
    Z_Malloc(numtextures*sizeof*texturecompositesize, PU_STATIC, 0);
  texturewidthmask =
    Z_Malloc(numtextures*sizeof*texturewidthmask, PU_STATIC, 0);
  texturewidth =
    Z_Malloc(numtextures*sizeof*texturewidth, PU_STATIC, 0);
  textureheight = Z_Malloc(numtextures*sizeof*textureheight, PU_STATIC, 0);
  texturebrightmap = Z_Malloc (numtextures * sizeof(*texturebrightmap), PU_STATIC, 0);

  // Complex printing shit factored out
  M_ProgressBarStart(numtextures, __func__);

  // TEXTURE1 & TEXTURE2 only. TX_ markers parsed below.
  for (i=0 ; i<numtextures1 + numtextures2 ; i++, directory++)
    {
      M_ProgressBarMove(i); // killough

      if (i == numtextures1)
        {
          // Start looking in second texture file.
          maptex = maptex2;
          maxoff = maxoff2;
          directory = maptex+1;
        }

      offset = LONG(*directory);

      if (offset > maxoff)
        I_Error("bad texture directory");

      mtexture = (maptexture_t *) ( (byte *)maptex + offset);

      texture = textures[i] =
        Z_Malloc(sizeof(texture_t) +
                 sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
                 PU_STATIC, 0);

      texture->width = SHORT(mtexture->width);
      texture->height = SHORT(mtexture->height);
      texture->patchcount = SHORT(mtexture->patchcount);

      M_CopyLumpName(texture->name, mtexture->name);
      mpatch = mtexture->patches;
      patch = texture->patches;

      for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
        {
          short p;
          patch->originx = SHORT(mpatch->originx);
          patch->originy = SHORT(mpatch->originy);
          p = SHORT(mpatch->patch);
          // [crispy] catch out-of-range patches
          if (p >= 0 && p < nummappatches)
          {
          patch->patch = patchlookup[p];
          }
          else
          {
            patch->patch = -1;
          }
          if (patch->patch == -1)
            {	      // killough 8/8/98
              I_Printf(VB_WARNING, "R_InitTextures: Missing patch %d in texture %.8s",
                     SHORT(mpatch->patch), texture->name); // killough 4/17/98
              // [FG] treat missing patches as non-fatal, substitute dummy patch
//            ++errors;
              patch->patch = (W_CheckNumForName)("TNT1A0", ns_sprites);
            }
        }

      RegisterTexture(texture, i);
    }
 
  // TX_ marker (texture namespace) parsed here
  if (tx_numtextures > 0)
  {
    for (i = (numtextures1 + numtextures2), k = 0; i < numtextures; i++, k++)
    {
      M_ProgressBarMove(i);

      int tx_lump = first_tx + k;
      texture = textures[i] = Z_Malloc(sizeof(texture_t), PU_STATIC, 0);
      M_CopyLumpName(texture->name, lumpinfo[tx_lump].name);

      if (!V_LumpIsPatch(tx_lump))
      {
        I_Printf(VB_WARNING, "R_InitTextures: Texture %.8s in wrong format",
                 texture->name);
        tx_lump = (W_CheckNumForName)("TNT1A0", ns_sprites);
      }

      patch_t* tx_patch = V_CachePatchNum(tx_lump, PU_CACHE);
      texture->width = tx_patch->width;
      texture->height = tx_patch->height;
      texture->patchcount = 1;

      texture->patches->patch = patchlookup[nummappatches + k];
      texture->patches->originx = 0;
      texture->patches->originy = 0;

      RegisterTexture(texture, i);
    }
  }

  M_ProgressBarEnd();

  Z_Free(patchlookup);         // killough

  Z_Free(maptex1);
  if (maptex2)
    Z_Free(maptex2);

  if (errors)
    I_Error("\n\n%d errors.", errors);
    
  // Precalculate whatever possible.
  for (i=0 ; i<numtextures ; i++)
    R_GenerateLookup(i, &errors);

  if (errors)
    I_Error("\n\n%d errors.", errors);

  // Create translation table for global animation.
  // killough 4/9/98: make column offsets 32-bit;
  // clean up malloc-ing to use sizeof

  texturetranslation =
    Z_Malloc((numtextures+1)*sizeof*texturetranslation, PU_STATIC, 0);

  for (i=0 ; i<numtextures ; i++)
    texturetranslation[i] = i;

  // killough 1/31/98: Initialize texture hash table
  for (i = 0; i<numtextures; i++)
    textures[i]->index = -1;
  while (--i >= 0)
    {
      int j = W_LumpNameHash(textures[i]->name) % (unsigned) numtextures;
      textures[i]->next = textures[j]->index;   // Prepend to chain
      textures[j]->index = i;
    }
}

//
// R_InitFlats
//
void R_InitFlats(void)
{
  int i;

  firstflat = W_GetNumForName("F_START") + 1;
  lastflat  = W_GetNumForName("F_END") - 1;
  numflats  = lastflat - firstflat + 1;

  // Create translation table for global animation.
  // killough 4/9/98: make column offsets 32-bit;
  // clean up malloc-ing to use sizeof

  flattranslation =
    Z_Malloc((numflats+1)*sizeof(*flattranslation), PU_STATIC, 0);

  flatterrain =
    Z_Malloc((numflats+1)*sizeof(*flatterrain), PU_STATIC, 0);

  for (i=0 ; i<numflats ; i++)
  {
    flattranslation[i] = i;
    flatterrain[i] = 0; // terrain_solid
  }
}

//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
// so the sprite does not need to be cached completely
// just for having the header info ready during rendering.
//
void R_InitSpriteLumps(void)
{
  int i;
  patch_t *patch;

  firstspritelump = W_GetNumForName("S_START") + 1;
  lastspritelump = W_GetNumForName("S_END") - 1;
  numspritelumps = lastspritelump - firstspritelump + 1;

  // killough 4/9/98: make columnd offsets 32-bit;
  // clean up malloc-ing to use sizeof

  spritewidth = Z_Malloc(numspritelumps*sizeof*spritewidth, PU_STATIC, 0);
  spriteoffset = Z_Malloc(numspritelumps*sizeof*spriteoffset, PU_STATIC, 0);
  spritetopoffset =
    Z_Malloc(numspritelumps*sizeof*spritetopoffset, PU_STATIC, 0);

  M_ProgressBarStart(numspritelumps, __func__);

  for (i=0 ; i< numspritelumps ; i++)
    {
      M_ProgressBarMove(i); // killough

      patch = V_CachePatchNum(firstspritelump+i, PU_CACHE);
      spritewidth[i] = SHORT(patch->width)<<FRACBITS;
      spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
      spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
    }

  M_ProgressBarEnd();
}

//
// R_InitColormaps
//
// killough 3/20/98: rewritten to allow dynamic colormaps
// and to remove unnecessary 256-byte alignment
//
// killough 4/4/98: Add support for C_START/C_END markers
//

invul_mode_t invul_mode;
static byte invul_orig[256];

void R_InvulMode(void)
{
  if (colormaps == NULL || beta_emulation)
    return;

  switch (STRICTMODE(invul_mode))
  {
    case INVUL_VANILLA:
      default_comp[comp_skymap] = 1;
      memcpy(&colormaps[0][256*32], invul_orig, 256);
      break;
    case INVUL_MBF:
      default_comp[comp_skymap] = 0;
      memcpy(&colormaps[0][256*32], invul_orig, 256);
      break;
    case INVUL_GRAY:
      default_comp[comp_skymap] = 0;
      memcpy(&colormaps[0][256*32], invul_gray, 256);
      break;
  }
}

void R_InitColormaps(void)
{
  int i;
  firstcolormaplump = W_GetNumForName("C_START");
  lastcolormaplump  = W_GetNumForName("C_END");
  numcolormaps = lastcolormaplump - firstcolormaplump;
  colormaps = Z_Malloc(sizeof(*colormaps) * numcolormaps, PU_STATIC, 0);

  colormaps[0] = W_CacheLumpNum(W_GetNumForName("COLORMAP"), PU_STATIC);

  for (i=1; i<numcolormaps; i++)
    colormaps[i] = W_CacheLumpNum(i+firstcolormaplump, PU_STATIC);

  // [FG] dark/shaded color translation table
  cr_dark = &colormaps[0][256*15];
  cr_shaded = &colormaps[0][256*6];

  memcpy(invul_orig, &colormaps[0][256*32], 256);
  R_InvulMode();
}

// killough 4/4/98: get colormap number from name
// killough 4/11/98: changed to return -1 for illegal names
// killough 4/17/98: changed to use ns_colormaps tag

int R_ColormapNumForName(const char *name)
{
  register int i = 0;
  if (strncasecmp(name,"COLORMAP",8))     // COLORMAP predefined to return 0
    if ((i = (W_CheckNumForName)(name, ns_colormaps)) != -1)
      i -= firstcolormaplump;
  return i;
}

//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//

void R_InitData(void)
{
  // [crispy] Moved R_InitFlats() to the top, because it sets firstflat/lastflat
  // which are required by R_InitTextures() to prevent flat lumps from being
  // mistaken as patches and by R_InitFlatBrightmaps() to set brightmaps for
  // flats.
  R_InitFlats();
  R_InitFlatBrightmaps();
  R_InitTextures();
  R_InitSpriteLumps();
  R_InitTranMap();                      // killough 2/21/98, 3/6/98
  R_InitColormaps();                    // killough 3/20/98
  R_InitSkyDefs();
}

//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
// killough 4/17/98: changed to use ns_flats namespace
//

int R_FlatNumForName(const char *name)    // killough -- const added
{
  int i = (W_CheckNumForName)(name, ns_flats);
  if (i == NO_TEXTURE)
  {
    I_Printf(VB_WARNING, "R_FlatNumForName: %.8s not found", name);
    return i;
  }
  return i - firstflat;
}

byte *R_MissingFlat(void)
{
    static byte *buffer = NULL;

    if (buffer == NULL)
    {
        const byte c1 = colrngs[CR_PURPLE][v_lightest_color];
        const byte c2 = v_darkest_color;

        buffer = Z_Malloc(FLATSIZE, PU_LEVEL, (void **)&buffer);

        for (int i = 0; i < FLATSIZE; i++)
        {
            buffer[i] = ((i & 16) == 16) != ((i & 1024) == 1024) ? c1 : c2;
        }
    }

    return buffer;
}

//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
// Rewritten by Lee Killough to use hash table for fast lookup. Considerably
// reduces the time needed to start new levels. See w_wad.c for comments on
// the hashing algorithm, which is also used for lump searches.
//
// killough 1/21/98, 1/31/98
//

int R_CheckTextureNumForName(const char *name)
{
  int i = 0;
  if (*name != '-')     // "NoTexture" marker.
    {
      i = textures[W_LumpNameHash(name) % (unsigned) numtextures]->index;
      while (i >= 0 && strncasecmp(textures[i]->name,name,8))
        i = textures[i]->next;
    }
  return i;
}

//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//

int R_TextureNumForName(const char *name)  // const added -- killough
{
  int i = R_CheckTextureNumForName(name);
  if (i == -1)
  {
    // [FG] treat missing textures as non-fatal
    I_Printf(VB_WARNING, "R_TextureNumForName: %.8s not found", name);
    return 0;
  }
  return i;
}

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// Totally rewritten by Lee Killough to use less memory,
// to avoid using alloca(), and to improve performance.

void R_PrecacheLevel(void)
{
  register int i;
  register byte *hitlist;

  if (demoplayback)
    return;

  {
    size_t size = numflats > num_sprites  ? numflats : num_sprites;
    hitlist = Z_Malloc(numtextures > size ? numtextures : size, PU_STATIC, 0);
  }

  // Precache flats.

  memset(hitlist, 0, numflats);

  for (i = numsectors; --i >= 0; )
  {
    if (sectors[i].floorpic > NO_TEXTURE)
        hitlist[sectors[i].floorpic] = 1;
    if (sectors[i].ceilingpic > NO_TEXTURE)
        hitlist[sectors[i].ceilingpic] = 1;
  }

  for (i = numflats; --i >= 0; )
    if (hitlist[i])
      V_CacheFlatNum(firstflat + i, PU_CACHE);

  // Precache textures.

  memset(hitlist, 0, numtextures);

  for (i = numsides; --i >= 0;)
    hitlist[sides[i].bottomtexture] =
      hitlist[sides[i].toptexture] =
      hitlist[sides[i].midtexture] = 1;

  // Sky texture is always present.
  // Note that F_SKY1 is the name used to
  //  indicate a sky floor/ceiling as a flat,
  //  while the sky texture is stored like
  //  a wall texture, with an episode dependend
  //  name.

  sky_t *sky;
  array_foreach(sky, levelskies)
  {
    hitlist[sky->background.texture] = 1;
  }

  for (i = numtextures; --i >= 0; )
    if (hitlist[i])
      {
        texture_t *texture = textures[i];
        int j = texture->patchcount;
        while (--j >= 0)
          V_CachePatchNum(texture->patches[j].patch, PU_CACHE);
      }

  // Precache sprites.
  memset(hitlist, 0, num_sprites);

  {
    thinker_t *th;
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
      if (th->function.p1 == P_MobjThinker)
        hitlist[((mobj_t *)th)->sprite] = 1;
  }

  for (i=num_sprites; --i >= 0;)
    if (hitlist[i])
      {
        int j = sprites[i].numframes;
        while (--j >= 0)
          {
            short *sflump = sprites[i].spriteframes[j].lump;
            int k = 7;
            do
              V_CachePatchNum(firstspritelump + sflump[k], PU_CACHE);
            while (--k >= 0);
          }
      }
  Z_Free(hitlist);
}

//-----------------------------------------------------------------------------
//
// $Log: r_data.c,v $
// Revision 1.23  1998/05/23  08:05:57  killough
// Reformatting
//
// Revision 1.21  1998/05/07  00:52:03  killough
// beautification
//
// Revision 1.20  1998/05/03  22:55:15  killough
// fix #includes at top
//
// Revision 1.19  1998/05/01  18:23:06  killough
// Make error messages look neater
//
// Revision 1.18  1998/04/28  22:56:07  killough
// Improve error handling of bad textures
//
// Revision 1.17  1998/04/27  01:58:08  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:38:58  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/16  10:47:40  killough
// Improve missing flats error message
//
// Revision 1.14  1998/04/14  08:12:31  killough
// Fix seg fault
//
// Revision 1.13  1998/04/12  09:52:51  killough
// Fix ?bad merge? causing seg fault
//
// Revision 1.12  1998/04/12  02:03:51  killough
// rename tranmap main_tranmap, better colormap support
//
// Revision 1.11  1998/04/09  13:19:35  killough
// Fix Medusa for transparent middles, and remove 64K composite texture size limit
//
// Revision 1.10  1998/04/06  04:39:58  killough
// Support multiple colormaps and C_START/C_END
//
// Revision 1.9  1998/03/23  03:33:29  killough
// Add support for an arbitrary number of colormaps, e.g. WATERMAP
//
// Revision 1.8  1998/03/09  07:26:03  killough
// Add translucency map caching
//
// Revision 1.7  1998/03/02  11:54:26  killough
// Don't initialize tranmap until needed
//
// Revision 1.6  1998/02/23  04:54:03  killough
// Add automatic translucency filter generator
//
// Revision 1.5  1998/02/02  13:35:36  killough
// Improve hashing algorithm
//
// Revision 1.4  1998/01/26  19:24:38  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  06:11:42  killough
// Fix Medusa bug, tune hash function
//
// Revision 1.2  1998/01/22  05:55:56  killough
// Improve hashing algorithm
//
// Revision 1.3  1997/01/29 20:10
// ???
//
//-----------------------------------------------------------------------------
