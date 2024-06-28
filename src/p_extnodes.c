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
// DESCRIPTION:
//      support maps with NODES in uncompressed XNOD/XGLN or compressed
//      ZNOD/ZGLN formats, or DeePBSP format
//
//-----------------------------------------------------------------------------

#include <math.h>
#include <string.h>

#include "doomdata.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_swap.h"
#include "p_extnodes.h"
#include "p_setup.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_zone.h"

// [FG] support maps with NODES in compressed ZNOD/ZGLN formats
#include "miniz.h"

// [FG] support maps with NODES in DeePBSP format

#if defined(_MSC_VER)
#  pragma pack(push, 1)
#endif

typedef PACKED_PREFIX struct
{
    int v1;
    int v2;
    unsigned short angle;
    unsigned short linedef;
    short side;
    unsigned short offset;
} PACKED_SUFFIX mapseg_deep_t;

typedef PACKED_PREFIX struct
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
} PACKED_SUFFIX mapnode_deep_t;

typedef PACKED_PREFIX struct
{
    unsigned short numsegs;
    int firstseg;
} PACKED_SUFFIX mapsubsector_deep_t;

// [FG] support maps with NODES in XNOD/ZNOD format

typedef PACKED_PREFIX struct
{
    unsigned int v1, v2;
    unsigned short linedef;
    unsigned char side;
} PACKED_SUFFIX mapseg_xnod_t;

typedef PACKED_PREFIX struct
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
} PACKED_SUFFIX mapnode_xnod_t;

typedef PACKED_PREFIX struct
{
    unsigned int numsegs;
} PACKED_SUFFIX mapsubsector_xnod_t;

#if defined(_MSC_VER)
#  pragma pack(pop)
#endif

// [FG] support maps with NODES in uncompressed XNOD/XGLN or compressed
// ZNOD/ZGLN formats, or DeePBSP format

mapformat_t P_CheckMapFormat(int lumpnum)
{
    mapformat_t format = MFMT_DOOM;
    byte *lump_data = NULL;
    int size_subs = 0, size_nodes = 0;

    if (W_LumpExistsWithName(lumpnum + ML_BLOCKMAP + 1, "BEHAVIOR"))
    {
        I_Error("P_SetupLevel: Hexen map format not supported in %s.\n",
                lumpinfo[lumpnum].name);
    }

    //!
    // @category mod
    //
    // Forces rebuilding of nodes.
    //

    if (M_CheckParm("-bsp"))
    {
        return MFMT_UNSUPPORTED;
    }

    //!
    // @category mod
    //
    // Forces extended (non-GL) ZDoom nodes.
    //

    if (!M_CheckParm("-force_old_zdoom_nodes"))
    {
        size_subs = W_LumpLengthWithName(lumpnum + ML_SSECTORS, "SSECTORS");

        if (size_subs >= sizeof(mapsubsector_t))
        {
            lump_data = W_CacheLumpNum(lumpnum + ML_SSECTORS, PU_STATIC);

            if (!memcmp(lump_data, "XGLN", 4))
            {
                format = MFMT_XGLN;
            }
            else if (!memcmp(lump_data, "ZGLN", 4))
            {
                format = MFMT_ZGLN;
            }
            else if (!memcmp(lump_data, "XGL2", 4))
            {
                format = MFMT_XGL2;
            }
            else if (!memcmp(lump_data, "ZGL2", 4))
            {
                format = MFMT_ZGL2;
            }
            else if (!memcmp(lump_data, "XGL3", 4))
            {
                format = MFMT_XGL3;
            }
            else if (!memcmp(lump_data, "ZGL3", 4))
            {
                format = MFMT_ZGL3;
            }
        }
        else
        {
            format = MFMT_UNSUPPORTED;
        }
    }

    if (lump_data)
    {
        Z_Free(lump_data);
        lump_data = NULL;
    }

    if (format == MFMT_DOOM || format >= MFMT_UNSUPPORTED)
    {
        size_nodes = W_LumpLengthWithName(lumpnum + ML_NODES, "NODES");

        if (size_nodes >= sizeof(mapnode_t))
        {
            lump_data = W_CacheLumpNum(lumpnum + ML_NODES, PU_STATIC);

            if (!memcmp(lump_data, "xNd4\0\0\0\0", 8))
            {
                format = MFMT_DEEP;
            }
            else if (!memcmp(lump_data, "XNOD", 4))
            {
                format = MFMT_XNOD;
            }
            else if (!memcmp(lump_data, "ZNOD", 4))
            {
                format = MFMT_ZNOD;
            }
        }
        else
        {
            format = MFMT_UNSUPPORTED;
        }
    }

    // [FG] no nodes for exactly one subsector
    if (size_subs == sizeof(mapsubsector_t) && size_nodes == 0)
    {
        format = MFMT_DOOM;
    }

    if (lump_data)
    {
        Z_Free(lump_data);
        lump_data = NULL;
    }

    return format;
}

// [FG] recalculate seg offsets

int P_GetOffset(vertex_t *v1, vertex_t *v2)
{
    float a, b;
    int r;

    a = (float)(v1->x - v2->x) / (float)FRACUNIT;
    b = (float)(v1->y - v2->y) / (float)FRACUNIT;
    r = (int)(sqrt(a * a + b * b) * (float)FRACUNIT);

    return r;
}

// [FG] support maps with DeePBSP nodes

void P_LoadSegs_DEEP(int lump)
{
    int i;
    byte *data;

    numsegs = W_LumpLength(lump) / sizeof(mapseg_deep_t);
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
    memset(segs, 0, numsegs * sizeof(seg_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    for (i = 0; i < numsegs; i++)
    {
        seg_t *li = segs + i;
        mapseg_deep_t *ml = (mapseg_deep_t *)data + i;
        int side, linedef;
        line_t *ldef;
        int vn1, vn2;

        // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
        vn1 = LONG(ml->v1);
        vn2 = LONG(ml->v2);

        // [FG] extended nodes
        li->v1 = &vertexes[vn1];
        li->v2 = &vertexes[vn2];

        li->angle = (SHORT(ml->angle)) << 16;
        li->offset = (SHORT(ml->offset)) << 16;

        linedef = (unsigned short)SHORT(ml->linedef); // [FG] extended nodes
        ldef = &lines[linedef];
        li->linedef = ldef;

        side = SHORT(ml->side);
        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;

        // [FG] recalculate
        li->offset = P_GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

        if (ldef->flags & ML_TWOSIDED)
        {
            int sidenum = ldef->sidenum[side ^ 1];

            if (sidenum == NO_INDEX)
            {
                // this is wrong
                li->backsector = GetSectorAtNullAddress();
            }
            else
            {
                li->backsector = sides[sidenum].sector;
            }
        }
        else
        {
            li->backsector = 0;
        }
    }

    Z_Free(data);
}

void P_LoadSubsectors_DEEP(int lump)
{
    mapsubsector_deep_t *data;
    int i;

    numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_deep_t);
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);
    data = (mapsubsector_deep_t *)W_CacheLumpNum(lump, PU_STATIC);

    memset(subsectors, 0, numsubsectors * sizeof(subsector_t));

    for (i = 0; i < numsubsectors; i++)
    {
        // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
        subsectors[i].numlines = (unsigned short)SHORT(data[i].numsegs);
        subsectors[i].firstline = LONG(data[i].firstseg);
    }

    Z_Free(data);
}

void P_LoadNodes_DEEP(int lump)
{
    byte *data;
    int i;

    numnodes = W_LumpLength(lump) / sizeof(mapnode_deep_t);
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);
    data = W_CacheLumpNum(lump, PU_STATIC);

    // [FG] skip header
    data += 8;

    for (i = 0; i < numnodes; i++)
    {
        node_t *no = nodes + i;
        mapnode_deep_t *mn = (mapnode_deep_t *)data + i;
        int j;

        no->x = SHORT(mn->x) << FRACBITS;
        no->y = SHORT(mn->y) << FRACBITS;
        no->dx = SHORT(mn->dx) << FRACBITS;
        no->dy = SHORT(mn->dy) << FRACBITS;

        for (j = 0; j < 2; j++)
        {
            int k;

            // [MB] 2020-04-22: Fix endianess for DeePBSDP V4 nodes
            no->children[j] = LONG(mn->children[j]);

            for (k = 0; k < 4; k++)
            {
                no->bbox[j][k] = SHORT(mn->bbox[j][k]) << FRACBITS;
            }
        }
    }

    W_CacheLumpNum(lump, PU_CACHE);
}

// [FG] support maps with NODES in uncompressed XNOD/XGLN or compressed
// ZNOD/ZGLN formats adapted from prboom-plus/src/p_setup.c:1040-1331 heavily
// modified, condensed and simplyfied
// - removed most paranoid checks, brought more in line with Vanilla
// P_LoadNodes()
// - removed const type punning pointers
// - added support for compressed ZNOD/ZGLN and uncompressed XGLN formats

// [MB] 2020-04-22: Fix endianess for ZDoom extended nodes

static void P_LoadSegs_XNOD(byte *data)
{
    int i;

    for (i = 0; i < numsegs; i++)
    {
        line_t *ldef;
        unsigned int linedef;
        unsigned char side;
        seg_t *li = segs + i;
        mapseg_xnod_t *ml = (mapseg_xnod_t *)data + i;
        unsigned int v1, v2;

        v1 = LONG(ml->v1);
        v2 = LONG(ml->v2);
        li->v1 = &vertexes[v1];
        li->v2 = &vertexes[v2];

        linedef = (unsigned short)SHORT(ml->linedef);
        ldef = &lines[linedef];
        li->linedef = ldef;
        side = ml->side;

        // Andrey Budko: check for wrong indexes
        if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
        {
            I_Error("P_LoadSegs_XNOD: linedef %d for seg %d references a "
                    "non-existent sidedef %d",
                    linedef, i, (unsigned)ldef->sidenum[side]);
        }

        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;

        // seg angle and offset are not included
        li->angle = R_PointToAngle2(segs[i].v1->x, segs[i].v1->y, segs[i].v2->x,
                                    segs[i].v2->y);
        li->offset = P_GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

        if (ldef->flags & ML_TWOSIDED)
        {
            int sidenum = ldef->sidenum[side ^ 1];

            if (sidenum == NO_INDEX)
            {
                // this is wrong
                li->backsector = GetSectorAtNullAddress();
            }
            else
            {
                li->backsector = sides[sidenum].sector;
            }
        }
        else
        {
            li->backsector = 0;
        }
    }
}

// adapted from dsda-doom/prboom2/src/p_setup.c:P_LoadGLZSegs()

static void P_LoadSegs_XGLN(byte *data)
{
    int i, j;
    const mapseg_xnod_t *ml = (const mapseg_xnod_t *)data;

    for (i = 0; i < numsubsectors; ++i)
    {
        for (j = 0; j < subsectors[i].numlines; ++j)
        {
            unsigned int v1;
            // unsigned int partner;
            unsigned int line;
            unsigned char side;
            seg_t *seg;

            v1 = LONG(ml->v1);
            // partner = LONG(ml->v2);
            line = (unsigned short)SHORT(ml->linedef);
            side = ml->side;

            if (line == 0xffff)
            {
                line = 0xffffffff;
            }

            ml++;

            seg = &segs[subsectors[i].firstline + j];

            seg->v1 = &vertexes[v1];
            if (j == 0)
            {
                seg[subsectors[i].numlines - 1].v2 = seg->v1;
            }
            else
            {
                seg[-1].v2 = seg->v1;
            }

            if (line != 0xffffffff)
            {
                line_t *ldef;

                if ((unsigned int)line >= (unsigned int)numlines)
                {
                    I_Error("P_LoadSegs_XGLN: seg %d, %d references a "
                            "non-existent linedef %d",
                            i, j, (unsigned int)line);
                }

                ldef = &lines[line];
                seg->linedef = ldef;

                if (side != 0 && side != 1)
                {
                    I_Error("P_LoadSegs_XGLN: seg %d, %d references a "
                            "non-existent side %d",
                            i, j, (unsigned int)side);
                }

                if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
                {
                    I_Error("P_LoadSegs_XGLN: linedef %d for seg %d, %d "
                            "references a non-existent sidedef %d",
                            line, i, j, (unsigned)ldef->sidenum[side]);
                }

                seg->sidedef = &sides[ldef->sidenum[side]];

                /* cph 2006/09/30 - our frontsector can be the second side of
                 * the linedef, so must check for NO_INDEX in case we are
                 * incorrectly referencing the back of a 1S line */
                if (ldef->sidenum[side] != NO_INDEX)
                {
                    seg->frontsector = sides[ldef->sidenum[side]].sector;
                }
                else
                {
                    seg->frontsector = 0;
                    I_Printf(
                        VB_WARNING,
                        "P_LoadSegs_XGLN: front of seg %d, %d has no sidedef",
                        i, j);
                }

                if ((ldef->flags & ML_TWOSIDED)
                    && (ldef->sidenum[side ^ 1] != NO_INDEX))
                {
                    seg->backsector = sides[ldef->sidenum[side ^ 1]].sector;
                }
                else
                {
                    seg->backsector = 0;
                }

                seg->offset =
                    P_GetOffset(seg->v1, (side ? ldef->v2 : ldef->v1));
            }
            else
            {
                seg->angle = 0;
                seg->offset = 0;
                seg->linedef = NULL;
                seg->sidedef = NULL;
                seg->frontsector = segs[subsectors[i].firstline].frontsector;
                seg->backsector = seg->frontsector;
            }
        }

        // Need all vertices to be defined before setting angles
        for (j = 0; j < subsectors[i].numlines; ++j)
        {
            seg_t *seg;

            seg = &segs[subsectors[i].firstline + j];

            if (seg->linedef)
            {
                seg->angle = R_PointToAngle2(seg->v1->x, seg->v1->y, seg->v2->x,
                                             seg->v2->y);
            }
        }
    }
}

void P_LoadNodes_XNOD(int lump, boolean compressed, boolean glnodes)
{
    byte *data;
    unsigned int i;
    byte *output = NULL;

    unsigned int orgVerts, newVerts;
    unsigned int numSubs, currSeg;
    unsigned int numSegs;
    unsigned int numNodes;
    vertex_t *newvertarray = NULL;

    data = W_CacheLumpNum(lump, PU_LEVEL);

    // 0. Uncompress nodes lump (or simply skip header)

    if (compressed)
    {
        const int len = W_LumpLength(lump);
        int outlen, err;
        z_stream *zstream;

        // first estimate for compression rate:
        // output buffer size == 2.5 * input size
        outlen = 2.5 * len;
        output = Z_Malloc(outlen, PU_STATIC, 0);

        // initialize stream state for decompression
        zstream = Z_Malloc(sizeof(*zstream), PU_STATIC, 0);
        memset(zstream, 0, sizeof(*zstream));
        zstream->next_in = data + 4;
        zstream->avail_in = len - 4;
        zstream->next_out = output;
        zstream->avail_out = outlen;

        if (inflateInit(zstream) != Z_OK)
        {
            I_Error("P_LoadNodes_XNOD: Error during ZNOD nodes decompression "
                    "initialization!");
        }

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
        {
            I_Error("P_LoadNodes_XNOD: Error during ZNOD nodes decompression!");
        }

        I_Printf(VB_DEBUG,
                 "P_LoadNodes_XNOD: ZNOD nodes compression ratio %.3f",
                 (float)zstream->total_out / zstream->total_in);

        data = output;

        if (inflateEnd(zstream) != Z_OK)
        {
            I_Error("P_LoadNodes_XNOD: Error during ZNOD nodes decompression "
                    "shut-down!");
        }

        // release the original data lump
        W_CacheLumpNum(lump, PU_CACHE);
        Z_Free(zstream);
    }
    else
    {
        // skip header
        data += 4;
    }

    // 1. Load new vertices added during node building

    orgVerts = LONG(*((unsigned int *)data));
    data += sizeof(orgVerts);

    newVerts = LONG(*((unsigned int *)data));
    data += sizeof(newVerts);

    if (orgVerts + newVerts == (unsigned int)numvertexes)
    {
        newvertarray = vertexes;
    }
    else
    {
        newvertarray =
            Z_Malloc((orgVerts + newVerts) * sizeof(vertex_t), PU_LEVEL, 0);
        memcpy(newvertarray, vertexes, orgVerts * sizeof(vertex_t));
        memset(newvertarray + orgVerts, 0, newVerts * sizeof(vertex_t));
    }

    for (i = 0; i < newVerts; i++)
    {
        newvertarray[i + orgVerts].r_x = newvertarray[i + orgVerts].x =
            LONG(*((unsigned int *)data));
        data += sizeof(newvertarray[0].x);

        newvertarray[i + orgVerts].r_y = newvertarray[i + orgVerts].y =
            LONG(*((unsigned int *)data));
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

    numSubs = LONG(*((unsigned int *)data));
    data += sizeof(numSubs);

    if (numSubs < 1)
    {
        I_Error("P_LoadNodes_XNOD: No subsectors in map!");
    }

    numsubsectors = numSubs;
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

    for (i = currSeg = 0; i < numsubsectors; i++)
    {
        mapsubsector_xnod_t *mseg = (mapsubsector_xnod_t *)data + i;

        subsectors[i].firstline = currSeg;
        subsectors[i].numlines = LONG(mseg->numsegs);
        currSeg += LONG(mseg->numsegs);
    }

    data += numsubsectors * sizeof(mapsubsector_xnod_t);

    // 3. Load segs

    numSegs = LONG(*((unsigned int *)data));
    data += sizeof(numSegs);

    // The number of stored segs should match the number of segs used by
    // subsectors
    if (numSegs != currSeg)
    {
        I_Error("P_LoadNodes_XNOD: Incorrect number of segs in XNOD nodes!");
    }

    numsegs = numSegs;
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);

    if (glnodes)
    {
        P_LoadSegs_XGLN(data);
    }
    else
    {
        P_LoadSegs_XNOD(data);
    }

    data += numsegs * sizeof(mapseg_xnod_t);

    // 4. Load nodes

    numNodes = LONG(*((unsigned int *)data));
    data += sizeof(numNodes);

    numnodes = numNodes;
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

    for (i = 0; i < numnodes; i++)
    {
        int j, k;
        node_t *no = nodes + i;
        mapnode_xnod_t *mn = (mapnode_xnod_t *)data + i;

        no->x = SHORT(mn->x) << FRACBITS;
        no->y = SHORT(mn->y) << FRACBITS;
        no->dx = SHORT(mn->dx) << FRACBITS;
        no->dy = SHORT(mn->dy) << FRACBITS;

        for (j = 0; j < 2; j++)
        {
            no->children[j] = LONG(mn->children[j]);

            for (k = 0; k < 4; k++)
            {
                no->bbox[j][k] = SHORT(mn->bbox[j][k]) << FRACBITS;
            }
        }
    }

    if (compressed && output)
    {
        Z_Free(output);
    }
    else
    {
        W_CacheLumpNum(lump, PU_CACHE);
    }
}
