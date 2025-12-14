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

#include "p_dirty.h"

#include "doomstat.h"
#include "doomtype.h"
#include "r_defs.h"

#define M_ARRAY_INIT_CAPACITY 64
#include "m_array.h"

dirty_line_t *dirty_lines = NULL;
dirty_side_t *dirty_sides = NULL;

void P_DirtyLine(line_t *line)
{
    line->dirty = true;

    dirty_line_t dirty_line = {
        .line = line,
        .clean_line.special = line->special
    };
    array_push(dirty_lines, dirty_line);
}

void P_DirtySide(side_t *side)
{
    side->dirty = true;

    dirty_side_t dirty_side = {
        .side = side,
        .clean_side.textureoffset = side->textureoffset,
        .clean_side.rowoffset = side->rowoffset,
        .clean_side.toptexture = side->toptexture,
        .clean_side.bottomtexture = side->bottomtexture,
        .clean_side.midtexture = side->midtexture
    };
    array_push(dirty_sides, dirty_side);
}

void P_CleanLine(dirty_line_t *dl)
{
    dl->line->dirty = false;

    dl->line->special = dl->clean_line.special;
}

void P_CleanSide(dirty_side_t *ds)
{
    ds->side->dirty = false;

    ds->side->textureoffset = ds->clean_side.textureoffset;
    ds->side->rowoffset = ds->clean_side.rowoffset;
    ds->side->toptexture = ds->clean_side.toptexture;
    ds->side->bottomtexture = ds->clean_side.bottomtexture;
    ds->side->midtexture = ds->clean_side.midtexture;
}

void P_ClearDirtyArrays(void)
{
    array_clear(dirty_lines);
    array_clear(dirty_sides);
}

typedef struct
{
    dirty_line_t *dirty_lines;
    dirty_side_t *dirty_sides;

    int episode;
    int map;
} dirty_t;

static dirty_t *levels;

void P_ArchiveDirtyArraysCurrentLevel(void)
{
    dirty_t level = {0};
    level.map = gamemap;
    level.episode = gameepisode;

    array_copy(level.dirty_lines, dirty_lines);
    array_copy(level.dirty_sides, dirty_sides);

    array_push(levels, level);
}

boolean P_UnArchiveDirtyArrays(int episode, int map)
{
    for (int i = 0; i < array_size(levels); ++i)
    {
        dirty_t *level = &levels[i];

        if (level->map == map && level->episode == episode)
        {
            array_copy(dirty_lines, level->dirty_lines);
            array_foreach_type(dl, dirty_lines, dirty_line_t)
            {
                dl->line->dirty = true;
            }

            array_copy(dirty_sides, level->dirty_sides);
            array_foreach_type(ds, dirty_sides, dirty_side_t)
            {
                ds->side->dirty = true;
            }

            array_free(level->dirty_lines);
            array_free(level->dirty_sides);
            array_delete(levels, i);
            return true;
        }
    }

    return false;
}
