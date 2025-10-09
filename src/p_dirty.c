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

#include "r_defs.h"

#define M_ARRAY_INIT_CAPACITY 64
#include "m_array.h"

line_t **dirty_lines = NULL;
line_t *clean_lines = NULL;
side_t **dirty_sides = NULL;
side_t *clean_sides = NULL;

void P_DirtyLine(line_t *line)
{
    array_push(clean_lines, *line);
    line->dirty = true;
    array_push(dirty_lines, line);
}

void P_DirtySide(side_t *side)
{
    array_push(clean_sides, *side);
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
