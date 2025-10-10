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

#ifndef P_DIRTY_H
#define P_DIRTY_H

#include "r_defs.h"

typedef struct
{
    sector_t *frontsector;
    sector_t *backsector;
    int16_t special;
} partial_line_t;

typedef struct
{
    fixed_t textureoffset;
    fixed_t rowoffset;
    int16_t toptexture;
    int16_t bottomtexture;
    int16_t midtexture;
} partial_side_t;

extern line_t **dirty_lines;
extern partial_line_t *clean_lines;

extern side_t **dirty_sides;
extern partial_side_t *clean_sides;

void P_DirtyLine(line_t *line);
void P_DirtySide(side_t *side);

void P_ClearDirtyArrays(void);

inline static line_t *dirty_line(line_t *line)
{
    if (!line->dirty)
    {
        P_DirtyLine(line);
    }
    return line;
}

inline static side_t *dirty_side(side_t *side)
{
    if (!side->dirty)
    {
        P_DirtySide(side);
    }
    return side;
}

#endif
