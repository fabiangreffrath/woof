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
    int16_t special;
} partial_line_t;

typedef struct
{
    line_t *line;
    partial_line_t clean_line;
} dirty_line_t;

extern dirty_line_t *dirty_lines;

typedef struct 
{
    fixed_t textureoffset;
    fixed_t rowoffset;
    int16_t toptexture;
    int16_t bottomtexture;
    int16_t midtexture;
} partial_side_t;

typedef struct
{
    side_t *side;
    partial_side_t clean_side;
} dirty_side_t;

extern dirty_side_t *dirty_sides;

void P_DirtyLine(line_t *line);
void P_DirtySide(side_t *side);

void P_CleanLine(dirty_line_t *dirty_line);
void P_CleanSide(dirty_side_t *dirty_side);

void P_ClearDirtyArrays(void);

void P_ArchiveDirtyArraysCurrentLevel(void);
boolean P_UnArchiveDirtyArrays(int episode, int map);

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
