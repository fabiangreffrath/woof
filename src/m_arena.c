//
// Copyright(C) 2024 Roman Fomin
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

// Arena allocator, inspired by https://nullprogram.com/blog/2023/09/27/


#include "m_arena.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"

struct arena_s
{
    char *buffer;

    char *beg;
    char *end;
};

void *M_ArenaAlloc(arena_t *arena, size_t count, size_t size, size_t align)
{
    ptrdiff_t padding = -(uintptr_t)arena->beg & (align - 1);
    ptrdiff_t available = arena->end - arena->beg - padding;

    if (available < 0 || count > available / size)
    {
        I_Error("Out of memory"); // one possible out-of-memory policy
    }

    void *p = arena->beg + padding;
    arena->beg += padding + count * size;
    return p;
}

arena_t *M_InitArena(size_t cap)
{
    arena_t *arena = calloc(1, sizeof(*arena));

    arena->buffer = malloc(cap);
    arena->beg = arena->buffer;
    arena->end = arena->beg + cap;

    return arena;
}

void M_ClearArena(arena_t *arena)
{
    arena->beg = arena->buffer;
}

struct arena_copy_s
{
    char *buffer;
    size_t size;
};

arena_copy_t *M_CopyArena(const arena_t *arena)
{
    arena_copy_t *copy = calloc(1, sizeof(*copy));

    ptrdiff_t size = arena->beg - arena->buffer;
    copy->size = size;
    copy->buffer = malloc(size);
    memcpy(copy->buffer, arena->buffer, size);

    return copy;
}

void M_RestoreArena(arena_t *arena, const arena_copy_t *copy)
{
    arena->beg = arena->buffer + copy->size;
    memcpy(arena->buffer, copy->buffer, copy->size);
}

void M_FreeArenaCopy(arena_copy_t *copy)
{
    free(copy->buffer);
    free(copy);
}
