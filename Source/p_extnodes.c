// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: r_defs.h,v 1.18 1998/04/27 02:12:59 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2015-2020 Fabian Greffrath
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
//      support maps with NODES in compressed or uncompressed ZDBSP format or DeePBSP format
//
//-----------------------------------------------------------------------------

#include "d_io.h"
#include "r_main.h"
#include "w_wad.h"

// [FG] support maps with NODES in compressed ZDBSP format
#include <zlib.h>

#include "p_extnodes.h"

// [FG] support maps with NODES in DeePBSP format

typedef PACKED_STRUCT (
{
    int v1;
    int v2;
    unsigned short angle;
    unsigned short linedef;
    short side;
    unsigned short offset;
}) mapseg_deepbsp_t;

typedef PACKED_STRUCT (
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
}) mapnode_deepbsp_t;

typedef PACKED_STRUCT (
{
    unsigned short numsegs;
    int firstseg;
}) mapsubsector_deepbsp_t;

// [FG] support maps with NODES in ZDBSP format

typedef PACKED_STRUCT (
{
    unsigned int v1, v2;
    unsigned short linedef;
    unsigned char side;
}) mapseg_zdbsp_t;

typedef PACKED_STRUCT (
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
}) mapnode_zdbsp_t;

typedef PACKED_STRUCT (
{
    unsigned int numsegs;
}) mapsubsector_zdbsp_t;

// [FG] support maps with NODES in compressed or uncompressed ZDBSP format or DeePBSP format

mapformat_t P_CheckMapFormat (int lumpnum)
{
    mapformat_t format = MFMT_DOOMBSP;
    byte *nodes = NULL;
    int b;

    if ((b = lumpnum+ML_BLOCKMAP+1) < numlumps &&
        !strcasecmp(lumpinfo[b].name, "BEHAVIOR"))
	I_Error("P_SetupLevel: Hexen map format not supported in %s.\n",
	        lumpinfo[lumpnum-ML_NODES].name);

    if ((b = lumpnum+ML_NODES) < numlumps &&
        (nodes = W_CacheLumpNum(b, PU_STATIC)) &&
        W_LumpLength(b) > 8)
    {
	if (!memcmp(nodes, "xNd4\0\0\0\0", 8))
	    format = MFMT_DEEPBSP;
	else if (!memcmp(nodes, "XNOD", 4))
	    format = MFMT_ZDBSPX;
	else if (!memcmp(nodes, "ZNOD", 4))
	    format = MFMT_ZDBSPZ;
    }

    if (nodes)
	Z_Free(nodes);

    return format;
}

// [FG] recalculate seg offsets

static fixed_t GetOffset(vertex_t *v1, vertex_t *v2)
{
    fixed_t dx, dy;
    fixed_t r;

    dx = (v1->x - v2->x)>>FRACBITS;
    dy = (v1->y - v2->y)>>FRACBITS;
    r = (fixed_t)(sqrt(dx*dx + dy*dy))<<FRACBITS;

    return r;
}

// [FG] support maps with DeePBSP nodes

void P_LoadSegs_DeePBSP (int lump)
{
  int  i;
  byte *data;

  numsegs = W_LumpLength(lump) / sizeof(mapseg_deepbsp_t);
  segs = Z_Malloc(numsegs*sizeof(seg_t),PU_LEVEL,0);
  memset(segs, 0, numsegs*sizeof(seg_t));
  data = W_CacheLumpNum(lump,PU_STATIC);

  for (i=0; i<numsegs; i++)
    {
      seg_t *li = segs+i;
      mapseg_deepbsp_t *ml = (mapseg_deepbsp_t *) data + i;

      int side, linedef;
      line_t *ldef;
      int vn1, vn2;

      // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
      vn1 = LONG(ml->v1);
      vn2 = LONG(ml->v2);

      // [FG] extended nodes
      li->v1 = &vertexes[vn1];
      li->v2 = &vertexes[vn2];

      li->angle = (SHORT(ml->angle))<<16;
      li->offset = (SHORT(ml->offset))<<16;
      linedef = (unsigned short)SHORT(ml->linedef); // [FG] extended nodes
      ldef = &lines[linedef];
      li->linedef = ldef;
      side = SHORT(ml->side);
      li->sidedef = &sides[ldef->sidenum[side]];
      li->frontsector = sides[ldef->sidenum[side]].sector;

      // killough 5/3/98: ignore 2s flag if second sidedef missing:
      if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=NO_INDEX)
        li->backsector = sides[ldef->sidenum[side^1]].sector;
      else
	li->backsector = 0;
    }

  Z_Free (data);
}

void P_LoadSubsectors_DeePBSP (int lump)
{
  mapsubsector_deepbsp_t *data;
  int  i;

  numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_deepbsp_t);
  subsectors = Z_Malloc(numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
  data = (mapsubsector_deepbsp_t *)W_CacheLumpNum(lump, PU_STATIC);

  memset(subsectors, 0, numsubsectors*sizeof(subsector_t));

  for (i=0; i<numsubsectors; i++)
    {
      // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
      subsectors[i].numlines = (unsigned short)SHORT(data[i].numsegs);
      subsectors[i].firstline = LONG(data[i].firstseg);
    }

  Z_Free (data);
}

void P_LoadNodes_DeePBSP (int lump)
{
  byte *data;
  int  i;

  numnodes = W_LumpLength (lump) / sizeof(mapnode_deepbsp_t);
  nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);
  data = W_CacheLumpNum (lump, PU_STATIC);

  // [FG] skip header
  data += 8;

  for (i=0; i<numnodes; i++)
    {
      node_t *no = nodes + i;
      mapnode_deepbsp_t *mn = (mapnode_deepbsp_t *) data + i;
      int j;

      no->x = SHORT(mn->x)<<FRACBITS;
      no->y = SHORT(mn->y)<<FRACBITS;
      no->dx = SHORT(mn->dx)<<FRACBITS;
      no->dy = SHORT(mn->dy)<<FRACBITS;

      for (j=0 ; j<2 ; j++)
        {
          int k;
          // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
          no->children[j] = LONG(mn->children[j]);

          for (k=0 ; k<4 ; k++)
            no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
        }
    }

  W_CacheLumpNum(lump, PU_CACHE);
}

// [FG] support maps with compressed or uncompressed ZDBSP nodes
// adapted from prboom-plus/src/p_setup.c:1040-1331
// heavily modified, condensed and simplyfied
// - removed most paranoid checks, brought in line with Vanilla P_LoadNodes()
// - removed const type punning pointers
// - inlined P_LoadZSegs()
// - added support for compressed ZDBSP nodes
// - added support for flipped levels

// [MB] 2020-04-22: Fix endianess for ZDoom extended nodes

void P_LoadNodes_ZDBSP (int lump, boolean compressed)
{
    byte *data;
    unsigned int i;
    byte *output;

    unsigned int orgVerts, newVerts;
    unsigned int numSubs, currSeg;
    unsigned int numSegs;
    unsigned int numNodes;
    vertex_t *newvertarray = NULL;

    data = W_CacheLumpNum(lump, PU_LEVEL);

    // 0. Uncompress nodes lump (or simply skip header)

    if (compressed)
    {
	const int len =  W_LumpLength(lump);
	int outlen, err;
	z_stream *zstream;

	// first estimate for compression rate:
	// output buffer size == 2.5 * input size
	outlen = 2.5 * len;
	output = Z_Malloc(outlen, PU_STATIC, 0);

	// initialize stream state for decompression
	zstream = malloc(sizeof(*zstream));
	memset(zstream, 0, sizeof(*zstream));
	zstream->next_in = data + 4;
	zstream->avail_in = len - 4;
	zstream->next_out = output;
	zstream->avail_out = outlen;

	if (inflateInit(zstream) != Z_OK)
	    I_Error("P_LoadNodes: Error during ZDBSP nodes decompression initialization!");

	// resize if output buffer runs full
	while ((err = inflate(zstream, Z_SYNC_FLUSH)) == Z_OK)
	{
	    int outlen_old = outlen;
	    outlen = 2 * outlen_old;
	    output = Z_Realloc(output, outlen, PU_STATIC, 0);
	    zstream->next_out = output + outlen_old;
	    zstream->avail_out = outlen - outlen_old;
	}

	if (err != Z_STREAM_END)
	    I_Error("P_LoadNodes: Error during ZDBSP nodes decompression!");

	fprintf(stderr, "P_LoadNodes: ZDBSP nodes compression ratio %.3f\n",
	        (float)zstream->total_out/zstream->total_in);

	data = output;

	if (inflateEnd(zstream) != Z_OK)
	    I_Error("P_LoadNodes: Error during ZDBSP nodes decompression shut-down!");

	// release the original data lump
	W_CacheLumpNum(lump, PU_CACHE);
	free(zstream);
    }
    else
    {
	// skip header
	data += 4;
    }

    // 1. Load new vertices added during node building

    orgVerts = LONG(*((unsigned int*)data));
    data += sizeof(orgVerts);

    newVerts = LONG(*((unsigned int*)data));
    data += sizeof(newVerts);

    if (orgVerts + newVerts == (unsigned int)numvertexes)
    {
	newvertarray = vertexes;
    }
    else
    {
	newvertarray = Z_Malloc((orgVerts + newVerts) * sizeof(vertex_t), PU_LEVEL, 0);
	memcpy(newvertarray, vertexes, orgVerts * sizeof(vertex_t));
	memset(newvertarray + orgVerts, 0, newVerts * sizeof(vertex_t));
    }

    for (i = 0; i < newVerts; i++)
    {
	newvertarray[i + orgVerts].r_x =
	newvertarray[i + orgVerts].x = LONG(*((unsigned int*)data));
	data += sizeof(newvertarray[0].x);

	newvertarray[i + orgVerts].r_y =
	newvertarray[i + orgVerts].y = LONG(*((unsigned int*)data));
	data += sizeof(newvertarray[0].y);
    }

    if (vertexes != newvertarray)
    {
	for (i = 0; i < (unsigned int)numlines; i++)
	{
	    lines[i].v1 = lines[i].v1 - vertexes + newvertarray;
	    lines[i].v2 = lines[i].v2 - vertexes + newvertarray;
	}

	Z_Free(vertexes);
	vertexes = newvertarray;
	numvertexes = orgVerts + newVerts;
    }

    // 2. Load subsectors

    numSubs = LONG(*((unsigned int*)data));
    data += sizeof(numSubs);

    if (numSubs < 1)
	I_Error("P_LoadNodes: No subsectors in map!");

    numsubsectors = numSubs;
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

    for (i = currSeg = 0; i < numsubsectors; i++)
    {
	mapsubsector_zdbsp_t *mseg = (mapsubsector_zdbsp_t*) data + i;

	subsectors[i].firstline = currSeg;
	subsectors[i].numlines = LONG(mseg->numsegs);
	currSeg += LONG(mseg->numsegs);
    }

    data += numsubsectors * sizeof(mapsubsector_zdbsp_t);

    // 3. Load segs

    numSegs = LONG(*((unsigned int*)data));
    data += sizeof(numSegs);

    // The number of stored segs should match the number of segs used by subsectors
    if (numSegs != currSeg)
    {
	I_Error("P_LoadNodes: Incorrect number of segs in ZDBSP nodes!");
    }

    numsegs = numSegs;
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);

    for (i = 0; i < numsegs; i++)
    {
	line_t *ldef;
	unsigned int linedef;
	unsigned char side;
	seg_t *li = segs + i;
	mapseg_zdbsp_t *ml = (mapseg_zdbsp_t *) data + i;
	unsigned int v1, v2;

	v1 = LONG(ml->v1);
	v2 = LONG(ml->v2);
	li->v1 = &vertexes[v1];
	li->v2 = &vertexes[v2];

	linedef = (unsigned short)SHORT(ml->linedef);
	ldef = &lines[linedef];
	li->linedef = ldef;
	side = ml->side;

        // e6y: check for wrong indexes
        if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
        {
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
                    linedef, i, (unsigned)ldef->sidenum[side]);
        }

	li->sidedef = &sides[ldef->sidenum[side]];
	li->frontsector = sides[ldef->sidenum[side]].sector;

	// seg angle and offset are not included
	li->angle = R_PointToAngle2(segs[i].v1->x, segs[i].v1->y, segs[i].v2->x, segs[i].v2->y);
	li->offset = GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

	// killough 5/3/98: ignore 2s flag if second sidedef missing:
	if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=NO_INDEX)
	  li->backsector = sides[ldef->sidenum[side^1]].sector;
	else
	  li->backsector = 0;
    }

    data += numsegs * sizeof(mapseg_zdbsp_t);

    // 4. Load nodes

    numNodes = LONG(*((unsigned int*)data));
    data += sizeof(numNodes);

    numnodes = numNodes;
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

    for (i = 0; i < numnodes; i++)
    {
	int j, k;
	node_t *no = nodes + i;
	mapnode_zdbsp_t *mn = (mapnode_zdbsp_t *) data + i;

	no->x = SHORT(mn->x)<<FRACBITS;
	no->y = SHORT(mn->y)<<FRACBITS;
	no->dx = SHORT(mn->dx)<<FRACBITS;
	no->dy = SHORT(mn->dy)<<FRACBITS;

	for (j = 0; j < 2; j++)
	{
	    no->children[j] = LONG(mn->children[j]);

	    for (k = 0; k < 4; k++)
		no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
	}
    }

    if (compressed)
	Z_Free(output);
    else
    W_CacheLumpNum(lump, PU_CACHE);
}
