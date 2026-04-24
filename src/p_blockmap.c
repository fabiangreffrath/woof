//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2026 by
//  Guilherme Miranda
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

#include "doomdata.h"
#include "doomtype.h"
#include "m_arena.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_swap.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_zone.h"
#include <limits.h>

const char *bmap_format_names[] = {
    "",
    "+XBM1",
    "+BoomBlockmap",
};

#ifndef MBF_STRICT

// jff 10/6/98
// New code added to speed up calculation of internal blockmap
// Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
//

/* places to shift rel position for cell num */
#define blkshift 7
/* mask for rel position within cell */
#define blkmask  ((1 << blkshift) - 1)
/* size guardband around map used */
#define blkmargin 0

// jff 10/8/98 use guardband>0
// jff 10/12/98 0 ok with + 1 in rows,cols
typedef struct linelist_t // type used to list lines in each block
{
    int num;
    struct linelist_t *next;
} linelist_t;

//
// Subroutine to add a line number to a block list
// It simply returns if the line is already in the block
//
static void AddBlockLine(linelist_t **lists, int *count, int *done, int blockno,
                         int lineno)
{
    linelist_t *l;

    if (done[blockno])
    {
        return;
    }

    l = Z_Malloc(sizeof(linelist_t), PU_STATIC, 0);
    l->num = lineno;
    l->next = lists[blockno];
    lists[blockno] = l;
    count[blockno]++;
    done[blockno] = 1;
}

static void P_CreateBlockMap(void)
{
    int xorg, yorg;                 // blockmap origin (lower left)
    int nrows, ncols;               // blockmap dimensions
    linelist_t **blocklists = NULL; // array of pointers to lists of lines
    int *blockcount = NULL;         // array of counters of line lists
    int *blockdone = NULL;          // array keeping track of blocks/line
    int NBlocks;                    // number of cells = nrows*ncols
    int linetotal = 0;              // total length of all blocklists
    int map_minx = INT_MAX;         // init for map limits search
    int map_miny = INT_MAX;
    int map_maxx = INT_MIN;
    int map_maxy = INT_MIN;
    int i, j;

    // scan for map limits, which the blockmap must enclose

    // This fixes MBF's code, which has a bug where maxx/maxy
    // are wrong if the 0th node has the largest x or y
    if (numvertexes)
    {
        map_minx = map_maxx = vertexes[0].x;
        map_miny = map_maxy = vertexes[0].y;
    }

    for (i = 0; i < numvertexes; i++)
    {
        fixed_t t;

        if ((t = vertexes[i].x) < map_minx)
        {
            map_minx = t;
        }
        else if (t > map_maxx)
        {
            map_maxx = t;
        }
        if ((t = vertexes[i].y) < map_miny)
        {
            map_miny = t;
        }
        else if (t > map_maxy)
        {
            map_maxy = t;
        }
    }
    map_minx >>= FRACBITS; // work in map coords, not fixed_t
    map_maxx >>= FRACBITS;
    map_miny >>= FRACBITS;
    map_maxy >>= FRACBITS;

    // set up blockmap area to enclose level plus margin

    xorg = map_minx - blkmargin;
    yorg = map_miny - blkmargin;
    // jff 10/12/98
    ncols = (map_maxx + blkmargin - xorg + 1 + blkmask) >> blkshift;
    //+1 needed for
    nrows = (map_maxy + blkmargin - yorg + 1 + blkmask) >> blkshift;
    // map exactly 1 cell
    NBlocks = ncols * nrows;

    // create the array of pointers on NBlocks to blocklists
    // also create an array of linelist counts on NBlocks
    // finally make an array in which we can mark blocks done per line

    // CPhipps - calloc's
    blocklists = Z_Calloc(NBlocks, sizeof(linelist_t *), PU_STATIC, 0);
    blockcount = Z_Calloc(NBlocks, sizeof(int), PU_STATIC, 0);
    blockdone = Z_Malloc(NBlocks * sizeof(int), PU_STATIC, 0);

    // initialize each blocklist, and enter the trailing -1 in all blocklists
    // note the linked list of lines grows backwards

    for (i = 0; i < NBlocks; i++)
    {
        blocklists[i] = Z_Malloc(sizeof(linelist_t), PU_STATIC, 0);
        blocklists[i]->num = -1;
        blocklists[i]->next = NULL;
        blockcount[i]++;
    }

    // For each linedef in the wad, determine all blockmap blocks it touches,
    // and add the linedef number to the blocklists for those blocks

    for (i = 0; i < numlines; i++)
    {
        int x1 = lines[i].v1->x >> FRACBITS; // lines[i] map coords
        int y1 = lines[i].v1->y >> FRACBITS;
        int x2 = lines[i].v2->x >> FRACBITS;
        int y2 = lines[i].v2->y >> FRACBITS;
        int dx = x2 - x1;
        int dy = y2 - y1;
        int vert = !dx; // lines[i] slopetype
        int horiz = !dy;
        int spos = (dx ^ dy) > 0;
        int sneg = (dx ^ dy) < 0;
        int bx, by;                   // block cell coords
        int minx = x1 > x2 ? x2 : x1; // extremal lines[i] coords
        int maxx = x1 > x2 ? x1 : x2;
        int miny = y1 > y2 ? y2 : y1;
        int maxy = y1 > y2 ? y1 : y2;

        // no blocks done for this linedef yet

        memset(blockdone, 0, NBlocks * sizeof(int));

        // The line always belongs to the blocks containing its endpoints

        bx = (x1 - xorg) >> blkshift;
        by = (y1 - yorg) >> blkshift;
        AddBlockLine(blocklists, blockcount, blockdone, by * ncols + bx, i);
        bx = (x2 - xorg) >> blkshift;
        by = (y2 - yorg) >> blkshift;
        AddBlockLine(blocklists, blockcount, blockdone, by * ncols + bx, i);

        // For each column, see where the line aint its left edge, which
        // it contains, intersects the Linedef i. Add i to each corresponding
        // blocklist.

        if (!vert) // don't interesect vertical lines with columns
        {
            for (j = 0; j < ncols; j++)
            {
                // intersection of Linedef with x=xorg+(j<<blkshift)
                // (y-y1)*dx = dy*(x-x1)
                // y = dy*(x-x1)+y1*dx;

                int x = xorg + (j << blkshift); // (x,y) is intersection
                int y = (dy * (x - x1)) / dx + y1;
                int yb = (y - yorg) >> blkshift; // block row number
                int yp = (y - yorg) & blkmask;   // y position within block

                if (yb < 0 || yb > nrows - 1) // outside blockmap, continue
                {
                    continue;
                }

                if (x < minx || x > maxx) // line doesn't touch column
                {
                    continue;
                }

                // The cell that contains the intersection point is always added

                AddBlockLine(blocklists, blockcount, blockdone, ncols * yb + j,
                             i);

                // if the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit

                if (yp == 0) // intersection at a corner
                {
                    if (sneg) //   \ - blocks x,y-, x-,y
                    {
                        if (yb > 0 && miny < y)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * (yb - 1) + j, i);
                        }
                        if (j > 0 && minx < x)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * yb + j - 1, i);
                        }
                    }
                    else if (spos) //   / - block x-,y-
                    {
                        if (yb > 0 && j > 0 && minx < x)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * (yb - 1) + j - 1, i);
                        }
                    }
                    else if (horiz) //   - - block x-,y
                    {
                        if (j > 0 && minx < x)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * yb + j - 1, i);
                        }
                    }
                }
                else if (j > 0 && minx < x) // else not at corner: x-,y
                {
                    AddBlockLine(blocklists, blockcount, blockdone,
                                 ncols * yb + j - 1, i);
                }
            }
        }

        // For each row, see where the line aint its bottom edge, which
        // it contains, intersects the Linedef i. Add i to all the corresponding
        // blocklists.

        if (!horiz)
        {
            for (j = 0; j < nrows; j++)
            {
                // intersection of Linedef with y=yorg+(j<<blkshift)
                // (x,y) on Linedef i satisfies: (y-y1)*dx = dy*(x-x1)
                // x = dx*(y-y1)/dy+x1;

                int y = yorg + (j << blkshift); // (x,y) is intersection
                int x = (dx * (y - y1)) / dy + x1;
                int xb = (x - xorg) >> blkshift; // block column number
                int xp = (x - xorg) & blkmask;   // x position within block

                if (xb < 0 || xb > ncols - 1) // outside blockmap, continue
                {
                    continue;
                }

                if (y < miny || y > maxy) // line doesn't touch row
                {
                    continue;
                }

                // The cell that contains the intersection point is always added

                AddBlockLine(blocklists, blockcount, blockdone, ncols * j + xb,
                             i);

                // if the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit

                if (xp == 0) // intersection at a corner
                {
                    if (sneg) //   \ - blocks x,y-, x-,y
                    {
                        if (j > 0 && miny < y)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * (j - 1) + xb, i);
                        }
                        if (xb > 0 && minx < x)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * j + xb - 1, i);
                        }
                    }
                    else if (vert) //   | - block x,y-
                    {
                        if (j > 0 && miny < y)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * (j - 1) + xb, i);
                        }
                    }
                    else if (spos) //   / - block x-,y-
                    {
                        if (xb > 0 && j > 0 && miny < y)
                        {
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         ncols * (j - 1) + xb - 1, i);
                        }
                    }
                }
                else if (j > 0 && miny < y) // else not on a corner: x,y-
                {
                    AddBlockLine(blocklists, blockcount, blockdone,
                                 ncols * (j - 1) + xb, i);
                }
            }
        }
    }

    // Add initial 0 to all blocklists
    // count the total number of lines (and 0's and -1's)

    memset(blockdone, 0, NBlocks * sizeof(int));
    for (i = 0, linetotal = 0; i < NBlocks; i++)
    {
        AddBlockLine(blocklists, blockcount, blockdone, i, 0);
        linetotal += blockcount[i];
    }

    // Create the blockmap lump

    // blockmaplump = malloc_IfSameLevel(blockmaplump, sizeof(*blockmaplump) *
    // (4 + NBlocks + linetotal));
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * (4 + NBlocks + linetotal),
                            PU_LEVEL, 0);

    // blockmap header

    blockmaplump[0] = bmaporgx = xorg << FRACBITS;
    blockmaplump[1] = bmaporgy = yorg << FRACBITS;
    blockmaplump[2] = bmapwidth = ncols;
    blockmaplump[3] = bmapheight = nrows;

    // offsets to lists and block lists

    for (i = 0; i < NBlocks; i++)
    {
        linelist_t *bl = blocklists[i];
        int offs = blockmaplump[4 + i] = // set offset to block's list
            (i ? blockmaplump[4 + i - 1] : 4 + NBlocks)
            + (i ? blockcount[i - 1] : 0);

        // add the lines in each block's list to the blockmaplump
        // delete each list node as we go

        while (bl)
        {
            linelist_t *tmp = bl->next;
            blockmaplump[offs++] = bl->num;
            Z_Free(bl);
            bl = tmp;
        }
    }

    // free all temporary storage

    Z_Free(blocklists);
    Z_Free(blockcount);
    Z_Free(blockdone);
}

#else // MBF_STRICT

//
// killough 10/98:
//
// Rewritten to use faster algorithm.
//
// New procedure uses Bresenham-like algorithm on the linedefs, adding the
// linedef to each block visited from the beginning to the end of the linedef.
//
// The algorithm's complexity is on the order of nlines*total_linedef_length.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.

static void P_CreateBlockMap(void)
{
    register int i;
    fixed_t minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

    // First find limits of map

    for (i = 0; i < numvertexes; i++)
    {
        if (vertexes[i].x >> FRACBITS < minx)
        {
            minx = vertexes[i].x >> FRACBITS;
        }
        else if (vertexes[i].x >> FRACBITS > maxx)
        {
            maxx = vertexes[i].x >> FRACBITS;
        }
        if (vertexes[i].y >> FRACBITS < miny)
        {
            miny = vertexes[i].y >> FRACBITS;
        }
        else if (vertexes[i].y >> FRACBITS > maxy)
        {
            maxy = vertexes[i].y >> FRACBITS;
        }
    }

    // Save blockmap parameters

    bmaporgx = minx << FRACBITS;
    bmaporgy = miny << FRACBITS;
    bmapwidth = ((maxx - minx) >> MAPBTOFRAC) + 1;
    bmapheight = ((maxy - miny) >> MAPBTOFRAC) + 1;

    // Compute blockmap, which is stored as a 2d array of variable-sized lists.
    //
    // Pseudocode:
    //
    // For each linedef:
    //
    //   Map the starting and ending vertices to blocks.
    //
    //   Starting in the starting vertex's block, do:
    //
    //     Add linedef to current block's list, dynamically resizing it.
    //
    //     If current block is the same as the ending vertex's block, exit loop.
    //
    //     Move to an adjacent block by moving towards the ending block in
    //     either the x or y direction, to the block which contains the linedef.
    {
        // blocklist structure
        typedef struct
        {
            int n, nalloc, *list;
        } bmap_t;

        // size of blockmap
        unsigned tot = bmapwidth * bmapheight;
        // array of blocklists
        bmap_t *bmap = Z_Calloc(sizeof *bmap, tot, PU_STATIC, 0);

        for (i = 0; i < numlines; i++)
        {
            // starting coordinates
            int x = (lines[i].v1->x >> FRACBITS) - minx;
            int y = (lines[i].v1->y >> FRACBITS) - miny;

            // x-y deltas
            int adx = lines[i].dx >> FRACBITS;
            int ady = lines[i].dy >> FRACBITS;
            int dx = adx < 0 ? -1 : 1;
            int dy = ady < 0 ? -1 : 1;

            // difference in preferring to move across y (>0) instead of x (<0)
            int diff = 0;
            if (!adx)
            {
                diff = 1;
            }
            else if (!ady)
            {
                diff = -1;
            }
            else
            {
                diff = (((x >> MAPBTOFRAC) << MAPBTOFRAC)
                        + (dx > 0 ? MAPBLOCKUNITS - 1 : 0) - x)
                           * (ady = abs(ady)) * dx
                       - (((y >> MAPBTOFRAC) << MAPBTOFRAC)
                          + (dy > 0 ? MAPBLOCKUNITS - 1 : 0) - y)
                             * (adx = abs(adx)) * dy;
            }

            // starting block, and pointer to its blocklist structure
            int b = (y >> MAPBTOFRAC) * bmapwidth + (x >> MAPBTOFRAC);

            // ending block
            int bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC)
                           * bmapwidth
                       + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

            // delta for pointer when moving across y
            dy *= bmapwidth;

            // deltas for diff inside the loop
            adx <<= MAPBTOFRAC;
            ady <<= MAPBTOFRAC;

            // Now we simply iterate block-by-block until we reach the end
            // block.
            while ((unsigned)b < tot) // failsafe -- should ALWAYS be true
            {
                // Increase size of allocated list if necessary
                if (bmap[b].n >= bmap[b].nalloc)
                {
                    bmap[b].nalloc = bmap[b].nalloc ? bmap[b].nalloc * 2 : 8;
                    bmap[b].list = realloc(
                        bmap[b].list, bmap[b].nalloc * sizeof(*bmap->list));
                }

                // Add linedef to end of list
                bmap[b].list[bmap[b].n++] = i;

                // If we have reached the last block, exit
                if (b == bend)
                {
                    break;
                }

                // Move in either the x or y direction to the next block
                if (diff < 0)
                {
                    diff += ady, b += dx;
                }
                else
                {
                    diff -= adx, b += dy;
                }
            }
        }

        // Compute the total size of the blockmap.
        //
        // Compression of empty blocks is performed by reserving two offset
        // words at tot and tot+1.
        //
        // 4 words, unused if this routine is called, are reserved at the start.
        {
            // we need at least 1 word per block, plus reserved's
            int count = tot + 6;

            for (i = 0; i < tot; i++)
            {
                if (bmap[i].n)
                {
                    // 1 header word + 1 trailer word + blocklist
                    count += bmap[i].n + 2;
                }
            }

            // Allocate blockmap lump with computed count
            blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);
        }

        // Now compress the blockmap.
        {
            int ndx = tot += 4; // Advance index to start of linedef lists
            bmap_t *bp = bmap;  // Start of uncompressed blockmap

            blockmaplump[ndx++] = 0;  // Store an empty blockmap list at start
            blockmaplump[ndx++] = -1; // (Used for compression)

            for (i = 4; i < tot; i++, bp++)
            {
                // Non-empty blocklist
                if (bp->n)
                {
                    // Store index & header
                    blockmaplump[blockmaplump[i] = ndx++] = 0;
                    do
                    {
                        // Copy linedef list
                        blockmaplump[ndx++] = bp->list[--bp->n];
                    } while (bp->n);
                    // Store trailer
                    blockmaplump[ndx++] = -1;
                    // Free linedef list
                    Z_Free(bp->list);
                }
                else // Empty blocklist: point to reserved empty blocklist
                {
                    blockmaplump[i] = tot;
                }
            }

            Z_Free(bmap); // Free uncompressed blockmap
        }
    }
}

#endif // MBF_STRICT

// Check if there is at least one block in BLOCKMAP
// which does not have 0 as the first item in the list

static void P_SetSkipBlockStart(void)
{
    int x, y;

    skipblstart = true;

    for (y = 0; y < bmapheight; y++)
    {
        for (x = 0; x < bmapwidth; x++)
        {
            int32_t *list;
            int32_t *blockoffset;

            blockoffset = blockmaplump + y * bmapwidth + x + 4;

            list = blockmaplump + *blockoffset;

            if (*list != 0)
            {
                skipblstart = false;
                return;
            }
        }
    }
}

static bmap_format_t CheckBlockmapFormat(int lump, int lump_size)
{
    bmap_format_t format = BMAP_DoomBlockmap;
    boolean force_rebuild = false;
    void *data = W_CacheLumpNum(lump, PU_CACHE);
    int vanilla_size = lump_size / sizeof(uint16_t);

    // Try extended formats
    if (lump_size >= 8 && memcmp(data, "XBM1\0\0\0\0", 8) == 0)
    {
        format = BMAP_XBM1;
    }

    //!
    // @category mod
    //
    // Forces a (re-)building of the BLOCKMAP lumps for loaded maps.
    //
    force_rebuild |= M_ParmExists("-blockmap");

    // [FG] always rebuild too short blockmaps
    force_rebuild |= (format == BMAP_DoomBlockmap && vanilla_size < 4);

    // Vanilla only supports 16bit values for offsets.
    force_rebuild |= (format == BMAP_DoomBlockmap && vanilla_size >= 65536);

    if (force_rebuild)
    {
        format = BMAP_BoomBuilder;
    }

    Z_Free(data);
    return format;
}

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//

static void LoadBlockmap_DoomBlockmap(int lump, int bmap_size)
{
    short *wadblockmaplump = W_CacheLumpNum(lump, PU_STATIC);
    int count = bmap_size / sizeof(uint16_t);
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

    // killough 3/1/98: Expand wad blockmap into larger internal one,
    // by treating all offsets except -1 as unsigned and zero-extending
    // them. This potentially doubles the size of blockmaps allowed,
    // because Doom originally considered the offsets as always signed.

    blockmaplump[0] = SHORT(wadblockmaplump[0]);
    blockmaplump[1] = SHORT(wadblockmaplump[1]);
    blockmaplump[2] = (int32_t)(SHORT(wadblockmaplump[2])) & FRACMASK;
    blockmaplump[3] = (int32_t)(SHORT(wadblockmaplump[3])) & FRACMASK;

    for (int i = 4; i < count; i++)
    {
        short t = SHORT(wadblockmaplump[i]); // killough 3/1/98
        blockmaplump[i] = t == -1 ? -1l : (int32_t)t & FRACMASK;
    }

    Z_Free(wadblockmaplump);

    bmaporgx = IntToFixed(blockmaplump[0]);
    bmaporgy = IntToFixed(blockmaplump[1]);
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];

    P_SetSkipBlockStart();
}

static void LoadBlockmap_XBM1(int lump, int bmap_size)
{
    int32_t *data = W_CacheLumpNum(lump, PU_STATIC);
    int count = bmap_size / sizeof(uint32_t);
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

    // skip prefix header
    // data[0] = "XBM1"
    // data[1] = "\0\0\0\0"
    bmaporgx = blockmaplump[0] = LONG(data[2]);
    bmaporgy = blockmaplump[1] = LONG(data[3]);
    bmapwidth = blockmaplump[2] = LONG(data[4]);
    bmapheight = blockmaplump[3] = LONG(data[5]);

    for (int i = 4, k = 6; i < count; i++, k++)
    {
        blockmaplump[i] = LONG(data[k]);
    }

    Z_Free(data);

    P_SetSkipBlockStart();
}

bmap_format_t P_LoadBlockMap(int lump)
{
    int bmap_size = W_LumpLengthWithName(lump, "BLOCKMAP");
    bmap_format_t format = CheckBlockmapFormat(lump, bmap_size);

    switch (format)
    {
        case BMAP_DoomBlockmap:
            LoadBlockmap_DoomBlockmap(lump, bmap_size);
            break;
        case BMAP_XBM1:
            LoadBlockmap_XBM1(lump, bmap_size);
            break;
        case BMAP_BoomBuilder:
            P_CreateBlockMap();
            break;
    }

    // clear out mobj chains
    blocklinks_size = sizeof(*blocklinks) * bmapwidth * bmapheight;
    blocklinks = M_ArenaAlloc(world_arena, blocklinks_size, alignof(mobj_t *));
    memset(blocklinks, 0, blocklinks_size);
    blockmap = blockmaplump + 4;

    return format;
}
