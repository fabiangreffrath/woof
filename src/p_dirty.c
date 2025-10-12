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

#include <string.h>

#include "p_dirty.h"

#include "doomstat.h"
#include "doomtype.h"
#include "r_defs.h"

#define M_ARRAY_INIT_CAPACITY 64
#include "m_array.h"

line_t **dirty_lines = NULL;
partial_line_t *clean_lines = NULL;

side_t **dirty_sides = NULL;
partial_side_t *clean_sides = NULL;

void P_DirtyLine(line_t *line)
{
    partial_line_t clean_line = {
        .special = line->special
    };
    array_push(clean_lines, clean_line);
    line->dirty = true;
    array_push(dirty_lines, line);
}

void P_DirtySide(side_t *side)
{
    partial_side_t clean_side = {
        .textureoffset = side->textureoffset,
        .rowoffset = side->rowoffset,
        .toptexture = side->toptexture,
        .bottomtexture = side->bottomtexture,
        .midtexture = side->midtexture
    };
    array_push(clean_sides, clean_side);
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

typedef struct
{
    line_t **dirty_lines;
    partial_line_t *clean_lines;

    side_t **dirty_sides;
    partial_side_t *clean_sides;

    int episode;
    int map;
} dirty_t;

static dirty_t *levels;

#define CopyArray(a, b)                  \
    do                                   \
    {                                    \
        int size = array_size(b);        \
        array_resize(a, size);           \
        memcpy(a, b, sizeof(*(b)) * size); \
    } while (0)

#define SetDirty(v)                    \
    do                                 \
    {                                  \
        int size = array_size(v);      \
        for (int i = 0; i < size; ++i) \
        {                              \
            (v)[i]->dirty = true;      \
        }                              \
    } while (0)

void P_ArchiveDirtyArraysCurrentLevel(void)
{
    dirty_t level = {0};
    level.map = gamemap;
    level.episode = gameepisode;

    CopyArray(level.dirty_lines, dirty_lines);
    CopyArray(level.clean_lines, clean_lines);

    CopyArray(level.dirty_sides, dirty_sides);
    CopyArray(level.clean_sides, clean_sides);

    array_push(levels, level);
}

boolean P_UnArchiveDirtyArrays(int episode, int map)
{
    for (int i = 0; i < array_size(levels); ++i)
    {
        dirty_t *level = &levels[i];

        if (level->map == map && level->episode == episode)
        {
            CopyArray(dirty_lines, level->dirty_lines);
            SetDirty(dirty_lines);
            CopyArray(clean_lines, level->clean_lines);

            CopyArray(dirty_sides, level->dirty_sides);
            SetDirty(dirty_sides);
            CopyArray(clean_sides, level->clean_sides);

            array_free(level->dirty_lines);
            array_free(level->clean_lines);
            array_free(level->dirty_sides);
            array_free(level->clean_sides);
            array_delete(levels, i);
            return true;
        }
    }

    return false;
}
