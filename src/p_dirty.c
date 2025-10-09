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
#include "r_defs.h"

#define M_ARRAY_INIT_CAPACITY 64
#include "m_array.h"

line_t **dirty_lines = NULL;
partial_line_t *clean_lines = NULL;
side_t **dirty_sides = NULL;
partial_side_t *clean_sides = NULL;

void P_DirtyLine(line_t *line)
{
    partial_line_t partial_line = {
        .frontsector = line->frontsector,
        .backsector = line->backsector,
        .special = line->special
    };
    array_push(clean_lines, partial_line);
    line->dirty = true;
    array_push(dirty_lines, line);
}

void P_DirtySide(side_t *side)
{
    partial_side_t partial_side = {
        .textureoffset = side->textureoffset,
        .rowoffset = side->rowoffset,
        .toptexture = side->toptexture,
        .bottomtexture = side->bottomtexture,
        .midtexture = side->midtexture
    };
    array_push(clean_sides, partial_side);
    side->dirty = true;
    array_push(dirty_sides, side);
}

void P_ClearDirtyArrays(void)
{
    array_clear(dirty_lines);
    array_clear(clean_lines);
    array_clear(dirty_sides);
    array_clear(clean_sides);
}
