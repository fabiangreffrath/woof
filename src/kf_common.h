//
// Copyright(C) 2025 Roman Fomin
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

#include "m_array.h"
#include "p_dirty.h"
#include "r_defs.h"
#include "r_state.h"

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        // killough 10/98: save full floor & ceiling heights, including fraction
        write32(sector->floorheight,
                sector->ceilingheight,
                sector->floor_xoffs,
                sector->floor_yoffs,
                sector->ceiling_xoffs,
                sector->ceiling_yoffs,
                sector->floor_rotation,
                sector->ceiling_rotation,
                sector->tint);

        write16(sector->floorpic,
                sector->ceilingpic,
                sector->lightlevel,
                sector->special, // needed?   yes -- transfer types
                sector->tag);    // needed?   need them -- killough 

        // Woof!
        writep_mobj(sector->soundtarget);
        writep_thinker(sector->floordata);
        writep_thinker(sector->ceilingdata);
        ArchiveThingList(sector);
        PrepareArchiveTouchingThingList(sector);
    }

    const line_t *line;

    int size = array_size(dirty_lines);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i];
        write16(line->special);
    }

    const side_t *side;

    size = array_size(dirty_sides);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i];

        write16(side->toptexture,
                side->bottomtexture,
                side->midtexture);

        write32(side->textureoffset,
                side->rowoffset);
    }
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        sector->floorheight = read32();
        sector->ceilingheight = read32();
        sector->floor_xoffs = read32();
        sector->floor_yoffs = read32();
        sector->ceiling_xoffs = read32();
        sector->ceiling_yoffs = read32();
        sector->floor_rotation = read32();
        sector->ceiling_rotation = read32();
        sector->tint = read32();

        sector->floorpic = read16();
        sector->ceilingpic = read16();
        sector->lightlevel = read16();
        sector->special = read16();
        sector->tag = read16();

        // Woof!
        sector->soundtarget = readp_mobj();
        sector->floordata = readp_thinker();
        sector->ceilingdata = readp_thinker();
        UnArchiveThingList(sector);
        PrepareUnArchiveTouchingThingList(sector);
    }

    line_t *line;
    const partial_line_t *clean_line;

    int oldsize = read32();
    int size = array_size(dirty_lines);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i];
        if (i < oldsize)
        {
            line->special = read16();
        }
        else
        {
            clean_line = &clean_lines[i];
            line->special = clean_line->special;
            line->dirty = false;
        }
    }

    side_t *side;
    const partial_side_t *clean_side;

    oldsize = read32();
    size = array_size(dirty_sides);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i];
        if (i < oldsize)
        {
            side->toptexture = read16();
            side->bottomtexture = read16();
            side->midtexture = read16();    
            side->textureoffset = read32();
            side->rowoffset = read32(); 
        }
        else
        {
            clean_side = &clean_sides[i];
            side->toptexture = clean_side->toptexture;
            side->bottomtexture = clean_side->bottomtexture;
            side->midtexture = clean_side->midtexture;
            side->textureoffset = clean_side->textureoffset;
            side->rowoffset = clean_side->rowoffset;
            side->dirty = false;
        }
    }
}
