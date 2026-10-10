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

#include "p_level.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_arena.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_bbox.h"
#include "m_fixed.h"
#include "m_swap.h"
#include "nano_bsp.h"
#include "p_bsp.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_udmf.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "r_tranmap.h"
#include "w_wad.h"
#include <math.h>

// A single Vertex.
typedef struct PACKED_PREFIX
{
    int16_t x, y;
} PACKED_SUFFIX mapvertex_t;

// A LineDef, as used for editing, and as input to the BSP builder.
typedef struct PACKED_PREFIX
{
    // [FG] extended nodes
    uint16_t v1;
    uint16_t v2;
    uint16_t flags;
    int16_t special;
    int16_t tag;
    uint16_t sidenum[2]; // sidenum[1] will be -1 (NO_INDEX) if one sided
} PACKED_SUFFIX maplinedef_t;

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
typedef struct PACKED_PREFIX
{
    int16_t textureoffset;
    int16_t rowoffset;
    char toptexture[8];
    char bottomtexture[8];
    char midtexture[8];
    int16_t sector; // Front sector, towards viewer.
} PACKED_SUFFIX mapsidedef_t;

// Sector definition, from editing.
typedef struct PACKED_PREFIX
{
    int16_t floorheight;
    int16_t ceilingheight;
    char floorpic[8];
    char ceilingpic[8];
    int16_t lightlevel;
    int16_t special;
    int16_t tag;
} PACKED_SUFFIX mapsector_t;

//-----------------------------------------------------------------------------

void P_DegenMobjThinker(mobj_t *const mobj)
{
    (void)mobj;
    I_Error("This function should never get called.");
}

static void AddLineToSector(sector_t *const s, line_t *const l)
{
    M_AddToBox(s->blockbox, l->v1->x, l->v1->y);
    M_AddToBox(s->blockbox, l->v2->x, l->v2->y);
    *s->lines++ = l;
}

static size_t GroupLines(map_t *const map)
{
    size_t total = 0;
    line_t **linebuffer;

    // look up sector number for each subsector
    for (size_t i = 0; i < numsubsectors; i++)
    {
        // figgi
        seg_t *seg = &segs[subsectors[i].firstline];
        subsectors[i].sector = NULL;
        for (size_t j = 0; j < subsectors[i].numlines; j++)
        {
            if (seg->sidedef)
            {
                subsectors[i].sector = seg->sidedef->sector;
                break;
            }
            seg++;
        }
        if (subsectors[i].sector == NULL)
        {
            I_Error("P_GroupLines: Subsector a part of no sector!");
        }
    }

    // count number of lines in each sector
    for (size_t i = 0; i < numlines; i++)
    {
        line_t *l = &lines[i];

        l->frontsector->linecount++;
        if (l->backsector && l->backsector != l->frontsector)
        {
            l->backsector->linecount++;
        }
    }

    // compute total number of lines and clear bounding boxes
    for (size_t i = 0; i < numsectors; i++)
    {
        total += sectors[i].linecount;
        M_ClearBox(sectors[i].blockbox);
    }

    // build line tables for each sector
    linebuffer = arena_alloc_num(world_arena, line_t *, total);

    for (size_t i = 0; i < numsectors; i++)
    {
        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;
    }

    for (size_t i = 0; i < numlines; i++)
    {
        line_t *line = &lines[i];
        AddLineToSector(line->frontsector, line);
        if (line->backsector && line->backsector != line->frontsector)
        {
            AddLineToSector(line->backsector, line);
        }
    }

    for (size_t i = 0; i < numsectors; i++)
    {
        sector_t *s = &sectors[i];
        fixed_t *box = s->blockbox;
        degenmobj_t *so = &s->soundorg;
        int block;

        // adjust pointers to point back to the beginning of each list
        s->lines -= s->linecount;

        // set the degenmobj_t to the middle of the bounding box
        so->x = box[BOXRIGHT] / 2 + box[BOXLEFT] / 2;
        so->y = box[BOXTOP] / 2 + box[BOXBOTTOM] / 2;

        so->thinker.function.p1 = P_DegenMobjThinker;

        // adjust bounding box to map blocks
        block = (box[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        box[BOXTOP] = block;

        block = (box[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        box[BOXBOTTOM] = block;

        block = (box[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        box[BOXRIGHT] = block;

        block = (box[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        box[BOXLEFT] = block;
    }
    return total;
}

static void SectorInitProperties(sector_t *const s)
{
    s->nextsec = -1; // jff 2/26/98 add fields to support locking out
    s->prevsec = -1; // stair retriggering until build completes

    s->heightsec = -1;       // sector used to get floor and ceiling height
    s->floorlightsec = -1;   // sector used to get floor lighting
    s->ceilinglightsec = -1; // sector used to get ceiling lighting:

    s->tint = s->tintfloor = s->tintceiling = -1;

    // killough 8/28/98: initialize all sectors to normal friction first
    s->friction = ORIG_FRICTION;
    s->movefactor = ORIG_FRICTION_FACTOR;
}

static void SectorInitInterpolation(sector_t *const s)
{
    // killough 3/7/98:
    // floor and ceiling flats offsets
    s->old_floor_xoffs = s->interp_floor_xoffs = s->floor_xoffs;
    s->old_floor_yoffs = s->interp_floor_yoffs = s->floor_yoffs;
    s->old_ceiling_xoffs = s->interp_ceiling_xoffs = s->ceiling_xoffs;
    s->old_ceiling_yoffs = s->interp_ceiling_yoffs = s->ceiling_yoffs;

    // [AM] Sector interpolation.  Even if we're
    //      not running uncapped, the renderer still
    //      uses this data.
    s->oldfloorheight = s->interpfloorheight = s->floorheight;
    s->oldceilingheight = s->interpceilingheight = s->ceilingheight;

    // [FG] inhibit sector interpolation during the 0th gametic
    s->oldceilgametic = s->oldfloorgametic = -1;
    s->old_ceil_offs_gametic = s->old_floor_offs_gametic = -1;
}

static void LinedefCalculateProperties(line_t *const l)
{
    const vertex_t v1 = *l->v1;
    const vertex_t v2 = *l->v2;
    const fixed_t dx = l->dx = v2.x - v1.x;
    const fixed_t dy = l->dy = v2.y - v1.y;

    l->angle = R_PointToAngle2(v1.x, v1.y, v2.x, v2.y);

    l->slopetype = !dx                    ? ST_VERTICAL
                   : !dy                  ? ST_HORIZONTAL
                   : FixedDiv(dy, dx) > 0 ? ST_POSITIVE
                                          : ST_NEGATIVE;

    if (v1.x < v2.x)
    {
        l->bbox[BOXLEFT] = v1.x;
        l->bbox[BOXRIGHT] = v2.x;
    }
    else
    {
        l->bbox[BOXLEFT] = v2.x;
        l->bbox[BOXRIGHT] = v1.x;
    }

    if (v1.y < v2.y)
    {
        l->bbox[BOXBOTTOM] = v1.y;
        l->bbox[BOXTOP] = v2.y;
    }
    else
    {
        l->bbox[BOXBOTTOM] = v2.y;
        l->bbox[BOXTOP] = v1.y;
    }

    /* calculate sound origin of line to be its midpoint */
    // Andrey Budko: fix sound origin for large levels
    l->soundorg.x = l->bbox[BOXLEFT] / 2 + l->bbox[BOXRIGHT] / 2;
    l->soundorg.y = l->bbox[BOXTOP] / 2 + l->bbox[BOXBOTTOM] / 2;
    l->soundorg.thinker.function.p1 = P_DegenMobjThinker;

    /* cph 2006/09/30 - fix sidedef errors right away.
     * cph 2002/07/20 - these errors are fatal if not fixed, so apply them
     * in compatibility mode - a desync is better than a crash! */
    for (int j = 0; j < 2; j++)
    {
        if (l->sidenum[j] != NO_INDEX && l->sidenum[j] >= numsides)
        {
            l->sidenum[j] = NO_INDEX;
            I_Printf(VB_DEBUG,
                     "%s: linedef %td has out-of-range sidedef number.",
                     __func__, l - lines);
        }
    }

    // killough 4/4/98: support special sidedef interpretation below
    if (l->sidenum[0] != NO_INDEX && l->special)
    {
        sides[l->sidenum[0]].special = l->special;
    }

    // killough 11/98: fix common wad errors (missing sidedefs):
    // Substitute dummy sidedef for missing right side
    if (l->sidenum[0] == NO_INDEX)
    {
        l->sidenum[0] = 0;
    }

    if ((l->sidenum[1] == NO_INDEX) && (l->flags & ML_TWOSIDED))
    {
        // Andrey Budko
        // ML_TWOSIDED flag shouldn't be cleared for compatibility purposes
        // see CLNJ-506.LMP at https://dsdarchive.com/wads/challenj
        if (!demo_compatibility || !overflow[emu_missedbackside].enabled)
        {
            // Clear 2s flag for missing left side
            l->flags &= ~ML_TWOSIDED;
        }

        // cph - print a warning about the bug
        I_Printf(
            VB_DEBUG,
            "%s: linedef %td has two-sided flag set, but no second sidedef.",
            __func__, l - lines);
    }

    // haleyjd 05/02/06: Reserved line flag. If set, we must clear all
    // BOOM or later extended line flags. This is necessitated by E2M7.
    if (l->flags & ML_RESERVED && comp[comp_reservedlineflag])
    {
        l->flags &= ML_VANILLA;
    }
}

static void SidedefInitProperties(side_t *const s)
{
    s->tint = -1;
}

static void SidedefInitInterpolation(side_t *const s)
{
    // [crispy] smooth texture scrolling
    s->oldtextureoffset = s->interptextureoffset = s->textureoffset;
    s->oldrowoffset = s->interprowoffset = s->rowoffset;
    s->oldgametic = -1;
}

static void ProcessLinedefSpecial(line_t *const l)
{
    // killough 4/11/98: handle special types
    // killough 4/11/98: translucent 2s textures
    if (l->special == 260)
    {
        // translucency from sidedef
        int32_t lump = sides[l->sidenum[0]].midindex;
        const byte *tranmap =
            !lump ? main_tranmap : W_CacheLumpNum(lump - 1, PU_STATIC);
        if (!l->args[0])
        {
            // if tag==0, affect this linedef only
            l->tranmap = tranmap;
        }
        else
        {
            for (size_t j = 0; j < numlines; j++)
            {
                if (lines[j].id == l->args[0])
                {
                    // if tag!=0, affect all matching linedefs
                    lines[j].tranmap = tranmap;
                }
            }
        }
    }
}

static void PostProcessLineDefs(map_t *map)
{
    for (size_t i = 0; i < numlines; i++)
    {
        line_t *l = &lines[i];
        // Andrey Budko: Can't be NO_INDEX here
        l->frontsector = sides[l->sidenum[0]].sector;
        l->backsector =
            (l->sidenum[1] != NO_INDEX) ? sides[l->sidenum[1]].sector : NULL;
        ProcessLinedefSpecial(l);
    }
}

static int32_t GetColormapOrTexture(int32_t *out, const char *texture_name)
{
    int32_t texture_index = R_TextureNumForName(texture_name);
    int32_t colormap_index = R_ColormapNumForName(texture_name);

    if (colormap_index < 0)
    {
        colormap_index = 0;
    }
    else
    {
        texture_index = 0;
    }

    *out = colormap_index;
    return texture_index;
}

static int32_t GetMusicOrTexture(int32_t *out, const char *texture_name)
{
    int32_t texture_index = R_TextureNumForName(texture_name);
    int32_t music_index = W_CheckNumForName(texture_name);

    if (music_index < 0)
    {
        music_index = 0;
    }
    else
    {
        texture_index = 0;
    }

    *out = music_index;
    return texture_index;
}

static int32_t GetTranmapOrTexture(int32_t *out, const char *texture_name)
{
    int32_t tranmap_index = 0;
    int32_t texture_index = 0;

    if (strncasecmp("TRANMAP", texture_name, 8) != 0)
    {
        tranmap_index = W_CheckNumForName(texture_name);

        if (tranmap_index >= 0 && W_LumpLength(tranmap_index) == 65536)
        {
            tranmap_index++;
            texture_index = 0;
        }
        else
        {
            tranmap_index = 0;
            texture_index = R_TextureNumForName(texture_name);
        }
    }

    *out = tranmap_index;
    return texture_index;
}

// killough 4/4/98: allow sidedef texture names to be overloaded
// killough 4/11/98: refined to allow colormaps to work as wall
// textures if invalid as colormaps but valid as textures.
static void ProcessSideDefs(side_t *s, char *bottom, char *mid, char *top)
{
    // clang-format off
    sector_t *sec = s->sector;
    switch (s->special)
    {
        case 2057: case 2058: case 2059: case 2060: case 2061: case 2062:
        case 2063: case 2064: case 2065: case 2066: case 2067: case 2068:
        case 2087: case 2088: case 2089: case 2090: case 2091: case 2092:
        case 2093: case 2094: case 2095: case 2096: case 2097: case 2098:
            s->toptexture = GetMusicOrTexture(&s->topindex, top);
            s->midtexture = R_TextureNumForName(mid);
            s->bottomtexture = GetMusicOrTexture(&s->bottomindex, bottom);
            break;

        case 2076: case 2077: case 2078: case 2079: case 2080: case 2081:
            s->toptexture = GetColormapOrTexture(&s->topindex, top);
            s->midtexture = R_TextureNumForName(mid);
            s->bottomtexture = GetColormapOrTexture(&s->bottomindex, bottom);
            break;

        case 2075:
            s->toptexture = GetColormapOrTexture(&s->topindex, top);
            s->midtexture = R_TextureNumForName(mid);
            s->bottomtexture = R_TextureNumForName(bottom);
            break;

        // variable colormap via 242 linedef
        case 242:
            s->toptexture = GetColormapOrTexture(&sec->topmap, top);
            s->midtexture = GetColormapOrTexture(&sec->midmap, mid);
            s->bottomtexture = GetColormapOrTexture(&sec->bottommap, bottom);
            break;

        // killough 4/11/98: apply translucency to 2s normal texture
        case 260:
            s->toptexture = R_TextureNumForName(top);
            s->midtexture = GetTranmapOrTexture(&s->midindex, mid);
            s->bottomtexture = R_TextureNumForName(bottom);
            break;

        // normal cases
        default:
            s->toptexture = R_TextureNumForName(top);
            s->midtexture = R_TextureNumForName(mid);
            s->bottomtexture = R_TextureNumForName(bottom);
            break;
    }
    // clang-format on
}

static void ProcessMapThing(mapthing_t *mt)
{
    // Do not spawn cool, new monsters if !commercial
    if (gamemode != commercial)
    {
        switch (mt->type)
        {
            case 68: // Arachnotron
            case 64: // Archvile
            case 88: // Boss Brain
            case 89: // Boss Shooter
            case 69: // Hell Knight
            case 67: // Mancubus
            case 71: // Pain Elemental
            case 65: // Former Human Commando
            case 66: // Revenant
            case 84: // Wolf SS
                return;
        }
    }

    P_SpawnMapThing(mt);
}

//-----------------------------------------------------------------------------

static void LoadVertexes_Doom(map_t *map)
{
    numvertexes = W_LumpLength(map->vertexes) / sizeof(mapvertex_t);
    vertexes = arena_alloc_num(world_arena, vertex_t, numvertexes);
    mapvertex_t *data = W_CacheLumpNum(map->vertexes, PU_STATIC);

    for (size_t i = 0; i < numvertexes; i++)
    {
        vertex_t *vv = &vertexes[i];
        mapvertex_t *mv = &data[i];

        int16_t x = SHORT(mv->x);
        int16_t y = SHORT(mv->y);

        vv->r_x = vv->x = IntToFixed(x);
        vv->r_y = vv->y = IntToFixed(y);
    }

    Z_Free(data);
}

static void LoadSectors_Doom(map_t *map)
{
    numsectors = W_LumpLength(map->sectors) / sizeof(mapsector_t);
    sectors = arena_alloc_num(world_arena, sector_t, numsectors);
    mapsector_t *data = W_CacheLumpNum(map->sectors, PU_STATIC);

    for (size_t i = 0; i < numsectors; i++)
    {
        sector_t *s = &sectors[i];
        mapsector_t *ms = &data[i];

        SectorInitProperties(s);

        s->floorheight = IntToFixed(SHORT(ms->floorheight));
        s->ceilingheight = IntToFixed(SHORT(ms->ceilingheight));
        s->floorpic = R_FlatNumForName(ms->floorpic);
        s->ceilingpic = R_FlatNumForName(ms->ceilingpic);
        s->lightlevel = SHORT(ms->lightlevel);
        s->special = SHORT(ms->special);
        s->oldspecial = SHORT(ms->special);
        s->tag = SHORT(ms->tag);

        SectorInitInterpolation(s);
    }

    Z_Free(data);
}

static void AllocateSideDefs_Doom(map_t *map)
{
    numsides = W_LumpLength(map->sidedefs) / sizeof(mapsidedef_t);
    sides = arena_alloc_num(world_arena, side_t, numsides);
}

static void LoadLineDefs_Doom(map_t *map)
{
    numlines = W_LumpLength(map->linedefs) / sizeof(maplinedef_t);
    lines = arena_alloc_num(world_arena, line_t, numlines);
    maplinedef_t *data = W_CacheLumpNum(map->linedefs, PU_STATIC);

    for (size_t i = 0; i < numlines; i++)
    {
        line_t *l = &lines[i];
        maplinedef_t *mld = &data[i];

        // [FG] extended nodes
        l->flags = USHORT(mld->flags);
        l->special = SHORT(mld->special);
        l->id = SHORT(mld->tag);
        l->args[0] = l->id; // UDMF spec
        l->v1 = &vertexes[USHORT(mld->v1)];
        l->v2 = &vertexes[USHORT(mld->v2)];

        l->sidenum[0] = USHORT(mld->sidenum[0]);
        l->sidenum[1] = USHORT(mld->sidenum[1]);

        FIX_NO_INDEX(l->sidenum[0]);
        FIX_NO_INDEX(l->sidenum[1]);

        LinedefCalculateProperties(l);
    }
    Z_Free(data);
}

static void LoadSideDefs_Doom(map_t *map)
{
    mapsidedef_t *data = W_CacheLumpNum(map->sidedefs, PU_STATIC);

    for (size_t i = 0; i < numsides; i++)
    {
        side_t *s = &sides[i];
        mapsidedef_t *ms = &data[i];

        SidedefInitProperties(s);
        s->textureoffset = IntToFixed(SHORT(ms->textureoffset));
        s->rowoffset = IntToFixed(SHORT(ms->rowoffset));
        s->sector = &sectors[SHORT(ms->sector)];
        SidedefInitInterpolation(s);

        /* cph 2006/09/30 - catch out-of-range sector numbers; use sector 0
         * instead */
        uint16_t sec = SHORT(ms->sector);
        if (sec >= numsectors)
        {
            I_Printf(VB_DEBUG, "%s: sidedef %zu has out-of-range sector num %u",
                     __func__, i, sec);
            sec = 0;
        }
        s->sector = &sectors[sec];

        ProcessSideDefs(s, ms->bottomtexture, ms->midtexture, ms->toptexture);
    }
    Z_Free(data);
}

void P_LoadThings_Doom(map_t *map)
{
    size_t numthings = W_LumpLength(map->things) / sizeof(mapthing_doom_t);
    mapthing_doom_t *data = W_CacheLumpNum(map->things, PU_STATIC);

    for (size_t i = 0; i < numthings; i++)
    {
        mapthing_t mt = {0};
        mapthing_doom_t *mtd = &data[i];

        // Do spawn all other stuff.
        mt.x = IntToFixed(SHORT(mtd->x));
        mt.y = IntToFixed(SHORT(mtd->y));
        mt.angle = SHORT(mtd->angle);
        mt.type = SHORT(mtd->type);
        mt.options = SHORT(mtd->options);

        mt.health = FRACUNIT;
        mt.tint = NO_INDEX;

        if (mt.options & MTF_EASY)
        {
            mt.options |= MTF_SKILL1 | MTF_SKILL2;
        }

        if (mt.options & MTF_NORMAL)
        {
            mt.options |= MTF_SKILL3;
        }

        if (mt.options & MTF_HARD)
        {
            mt.options |= MTF_SKILL4 | MTF_SKILL5;
        }

        ProcessMapThing(&mt);
    }

    Z_Free(data);
}

//-----------------------------------------------------------------------------

static void LoadVertexes_UDMF(map_t *map)
{
    numvertexes = array_size(map->udmf_vertexes);
    vertexes = arena_alloc_num(world_arena, vertex_t, numvertexes);

    for (size_t i = 0; i < numvertexes; i++)
    {
        fixed_t x = DoubleToFixed(map->udmf_vertexes[i].x);
        fixed_t y = DoubleToFixed(map->udmf_vertexes[i].y);

        vertexes[i].r_x = vertexes[i].x = x;
        vertexes[i].r_y = vertexes[i].y = y;
    }
}

static void LoadSectors_UDMF(map_t *map)
{
    numsectors = array_size(map->udmf_sectors);
    sectors = arena_alloc_num(world_arena, sector_t, numsectors);

    for (size_t i = 0; i < numsectors; i++)
    {
        sector_t *s = &sectors[i];
        UDMF_Sector_t *us = &map->udmf_sectors[i];

        SectorInitProperties(s);

        s->floorheight = IntToFixed(us->heightfloor);
        s->ceilingheight = IntToFixed(us->heightceiling);
        s->floorpic = R_FlatNumForName(us->texturefloor);
        s->ceilingpic = R_FlatNumForName(us->textureceiling);
        s->lightlevel = us->lightlevel;
        s->special = us->special;
        s->tag = us->tag;

        s->flags = us->flags;
        s->lightfloor = us->lightfloor;
        s->lightceiling = us->lightceiling;

        s->floor_rotation = FixedToAngle(DoubleToFixed(us->rotationfloor));
        s->ceiling_rotation = FixedToAngle(DoubleToFixed(us->rotationceiling));

        s->floor_xoffs = DoubleToFixed(us->xpanningfloor);
        s->floor_yoffs = DoubleToFixed(us->ypanningfloor);
        s->ceiling_xoffs = DoubleToFixed(us->xpanningceiling);
        s->ceiling_yoffs = DoubleToFixed(us->ypanningceiling);

        SectorInitInterpolation(s);

        s->colormap = R_ColormapNumForName(us->colormap);
        s->tint = R_ColormapNumForName(us->tint);
        s->tintceiling = R_ColormapNumForName(us->tintceiling);
        s->tintfloor = R_ColormapNumForName(us->tintfloor);

        if (us->scroll_floor_type && (us->scroll_floor_x || us->scroll_floor_y))
        {
            Add_EESectorScroller(us->scroll_floor_type, i, false,
                                 us->scroll_floor_x, us->scroll_floor_y);
        }

        if (us->scroll_ceil_type && (us->scroll_ceil_x || us->scroll_ceil_y))
        {
            Add_EESectorScroller(us->scroll_floor_type, i, true,
                                 us->scroll_floor_x, us->scroll_floor_y);
        }

        if (us->scrollfloormode && (us->xscrollfloor || us->yscrollfloor))
        {
            Add_ParamSectorScroller(us->scrollfloormode, i, false,
                                    DoubleToFixed(us->xscrollfloor),
                                    DoubleToFixed(us->yscrollfloor));
        }

        if (us->scrollceilingmode && (us->xscrollceiling || us->yscrollceiling))
        {
            Add_ParamSectorScroller(us->scrollceilingmode, i, true,
                                    DoubleToFixed(us->xscrollceiling),
                                    DoubleToFixed(us->yscrollceiling));
        }
    }
}

static void AllocateSideDefs_UDMF(map_t *map)
{
    numsides = array_size(map->udmf_sidedefs);
    sides = arena_alloc_num(world_arena, side_t, numsides);
}

static void LoadLineDefs_UDMF(map_t *map)
{
    numlines = array_size(map->udmf_linedefs);
    lines = arena_alloc_num(world_arena, line_t, numlines);

    for (size_t i = 0; i < numlines; i++)
    {
        line_t *l = &lines[i];
        UDMF_Linedef_t *ul = &map->udmf_linedefs[i];

        if (ul->v1_id >= numvertexes)
        {
            I_Error("linedef %zu's vertex 1 index is out of range", i);
        }
        l->v1 = &vertexes[ul->v1_id];

        if (ul->v1_id >= numvertexes)
        {
            I_Error("linedef %zu's vertex 2 index is out of range", i);
        }
        l->v2 = &vertexes[ul->v2_id];

        if (ul->sidefront >= numsides)
        {
            I_Error("linedef %zu's frontside index is out of range", i);
        }
        l->sidenum[0] = ul->sidefront;

        if (ul->sideback >= numsides)
        {
            I_Error("linedef %zu's backside index is out of range", i);
        }
        l->sidenum[1] = ul->sideback;

        l->flags = ul->flags;
        l->special = ul->special;
        l->id = ul->id;
        l->args[0] = ul->args[0];
        l->args[1] = ul->args[1];
        l->args[2] = ul->args[2];
        l->args[3] = ul->args[3];
        l->args[4] = ul->args[4];

        // Custom automap line style
        l->amls = ul->amls;
        if (l->amls < amls_Default || l->amls >= AMLS_COUNT)
        {
            l->amls = amls_Default;
        }

        // Translucency and special effects support
        int32_t lump = W_CheckNumForName(ul->tranmap);

        if (lump == NO_INDEX && ul->alpha < 1.0)
        {
            const int32_t alpha = (int32_t)floor(ul->alpha * 100.0);
            l->tranmap = GetNormalTranMap(alpha);
        }

        if (lump != NO_INDEX && W_LumpLength(lump) == tranmap_lump_length)
        {
            l->tranmap = W_CacheLumpNum(lump, PU_CACHE);
        }

        LinedefCalculateProperties(l);
    }
}

static void LoadSideDefs_UDMF(map_t *map)
{
    for (size_t i = 0; i < numsides; i++)
    {
        side_t *s = &sides[i];
        UDMF_Sidedef_t *us = &map->udmf_sidedefs[i];

        SidedefInitProperties(s);
        if (us->sector_id >= numsectors)
        {
            I_Error("sidedef %zu's sector index is out of range", i);
        }
        s->sector = &sectors[us->sector_id];
        s->textureoffset = IntToFixed(us->offsetx);
        s->rowoffset = IntToFixed(us->offsety);

        s->offsetx_top = DoubleToFixed(us->offsetx_top);
        s->offsety_top = DoubleToFixed(us->offsety_top);
        s->offsetx_mid = DoubleToFixed(us->offsetx_mid);
        s->offsety_mid = DoubleToFixed(us->offsety_mid);
        s->offsetx_bottom = DoubleToFixed(us->offsetx_bottom);
        s->offsety_bottom = DoubleToFixed(us->offsety_bottom);
        SidedefInitInterpolation(s);

        s->flags = us->flags;
        s->light = us->light;
        s->light_top = us->light_top;
        s->light_mid = us->light_mid;
        s->light_bottom = us->light_bottom;

        s->tint = R_ColormapNumForName(us->tint);

        if (us->xscroll || us->yscroll)
        {
            Add_ScrollerStatic(sc_side, i, DoubleToFixed(us->xscroll),
                               DoubleToFixed(us->yscroll));
        }

        if (us->xscrolltop || us->yscrolltop)
        {
            Add_ScrollerStatic(sc_side_top, i, DoubleToFixed(us->xscrolltop),
                               DoubleToFixed(us->yscrolltop));
        }

        if (us->xscrollmid || us->yscrollmid)
        {
            Add_ScrollerStatic(sc_side_mid, i, DoubleToFixed(us->xscrollmid),
                               DoubleToFixed(us->yscrollmid));
        }

        if (us->xscrollbottom || us->yscrollbottom)
        {
            Add_ScrollerStatic(sc_side_bottom, i,
                               DoubleToFixed(us->xscrollbottom),
                               DoubleToFixed(us->yscrollbottom));
        }

        ProcessSideDefs(s, us->texturebottom, us->texturemiddle,
                        us->texturetop);
    }
}

void P_LoadThings_UDMF(map_t *map)
{
    for (size_t i = 0; i < array_size(map->udmf_things); i++)
    {
        UDMF_Thing_t *ut = &map->udmf_things[i];

        // Do spawn all other stuff.
        mapthing_t mt = {0};
        mt.x = DoubleToFixed(ut->x);
        mt.y = DoubleToFixed(ut->y);
        mt.height = DoubleToFixed(ut->height);
        mt.angle = CLAMP(ut->angle, 0, 360);
        mt.type = ut->type;
        mt.options = ut->options;

        mt.tid = ut->tid;
        mt.special = ut->special;
        mt.args[0] = ut->args[0];
        mt.args[1] = ut->args[1];
        mt.args[2] = ut->args[2];
        mt.args[3] = ut->args[3];
        mt.args[4] = ut->args[4];

        mt.health = DoubleToFixed(ut->health);
        mt.tint = R_ColormapNumForName(ut->tint);

        // Translucency and special effects support
        int32_t lump = W_CheckNumForName(ut->tranmap);

        if (lump == NO_INDEX && ut->alpha < 1.0)
        {
            const int32_t alpha = (int32_t)floor(ut->alpha * 100.0);
            mt.tranmap = GetNormalTranMap(alpha);
        }

        if (lump != NO_INDEX && W_LumpLength(lump) == tranmap_lump_length)
        {
            mt.tranmap = W_CacheLumpNum(lump, PU_CACHE);
        }

        ProcessMapThing(&mt);
    }
}

//-----------------------------------------------------------------------------

static boolean LoadReject(map_t *map)
{
    int32_t lumpnum = map->reject;
    size_t totallines = GroupLines(map);

    // Calculate the size that the REJECT lump *should* be.
    size_t minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.
    size_t lumplen = W_LumpLengthWithName(lumpnum, "REJECT");

    boolean ret;

    if (lumplen >= minlength)
    {
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
        ret = false;
    }
    else
    {
        unsigned int padvalue;

        rejectmatrix = Z_Malloc(minlength, PU_LEVEL, (void **)&rejectmatrix);

        if (W_LumpExists(lumpnum))
        {
            W_ReadLumpSize(lumpnum, rejectmatrix, minlength);
        }

        //!
        // @category mod
        //
        // Pad the remaining REJECT table space with 0xff.
        //

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0x00;
        }

        memset(rejectmatrix + lumplen, padvalue, minlength - lumplen);

        if (demo_compatibility && overflow[emu_reject].enabled)
        {
            unsigned int rejectpad[4] = {
                0,       // Size
                0,       // Part of z_zone block header
                50,      // PU_LEVEL
                0x1d4a11 // DOOM_CONST_ZONEID
            };

            overflow[emu_reject].triggered = true;

            rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

            // Copy values from rejectpad into the destination array.

            byte *dest = rejectmatrix + lumplen;

            unsigned int byte_num;
            size_t pad_size = sizeof(rejectpad);

            for (size_t i = 0; i < (minlength - lumplen) && i < pad_size; ++i)
            {
                byte_num = i % 4;
                *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
                ++dest;
            }
        }

        ret = true;
    }

    return ret;
}

//-----------------------------------------------------------------------------

void P_LoadMap_Doom(map_t *map)
{
    // the original implementation used only low precision math
    P_PointOnLineSide = P_PointOnLineSide_Classic;
    P_PointOnDivlineSide = P_PointOnDivlineSide_Classic;

    // note: most of this ordering is important
    LoadVertexes_Doom(map);
    LoadSectors_Doom(map);
    AllocateSideDefs_Doom(map); // <- This needs Sectors
    LoadLineDefs_Doom(map);     // <- this needs allocated Sides
    LoadSideDefs_Doom(map);     // <- this needs Lines
    PostProcessLineDefs(map);   // <- this needs Sides

    map->bmap_format = P_LoadBlockMap(map);

    // clang-format off
    switch (map->bsp_format)
    {
        case BSP_NANO:
            BSP_BuildNodes();
            break;
        case BSP_XNOD: case BSP_ZNOD:
        case BSP_XGLN: case BSP_ZGLN:
        case BSP_XGL2: case BSP_ZGL2:
        case BSP_XGL3: case BSP_ZGL3:
            P_LoadBSPTree_ZDBSP(map);
            break;
        case BSP_DEEPBSPV4:
            P_LoadSubsectors_DeePBSPV4(map);
            P_LoadNodes_DeePBSPV4(map);
            P_LoadSegs_DeePBSPV4(map);
            break;
        case BSP_DOOMBSP:
            P_LoadSubsectors(map);
            P_LoadNodes(map);
            P_LoadSegs(map);
            break;
    }
    // clang-format on

    map->reject_built = LoadReject(map);
}

void P_LoadMap_UDMF(map_t *map)
{
    // udmf requires higher precision math
    P_PointOnLineSide = P_PointOnLineSide_Precise;
    P_PointOnDivlineSide = P_PointOnDivlineSide_Precise;

    if (map->znodes < 0)
    {
        I_Error("Could not find ZNODES lump for UDMF map: %s.",
                lumpinfo[map->label].name);
    }

    if (map->bsp_format == BSP_NANO)
    {
        I_Error("Invalid format found on ZNODES lump for UDMF map: %s",
                lumpinfo[map->label].name);
    }

    P_ClearMemory_UDMF(map);
    P_ParseTextMap(map);

    // note: most of this ordering is important
    LoadVertexes_UDMF(map);
    LoadSectors_UDMF(map);
    AllocateSideDefs_UDMF(map); // <- This needs Sectors
    LoadLineDefs_UDMF(map);     // <- this needs allocated Sides
    LoadSideDefs_UDMF(map);     // <- this needs Lines
    PostProcessLineDefs(map);   // <- this needs Sides

    map->bmap_format = P_LoadBlockMap(map);
    P_LoadBSPTree_ZDBSP(map);
    map->reject_built = LoadReject(map);

    P_ClearMemory_UDMF(map);
}
