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

// Arena allocator, inspired by https://nullprogram.com/blog/2023/09/27/


#include "m_arena.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "m_array.h"

typedef struct
{
    void **ptrs;
    size_t size;
} block_t;

struct arena_s
{
    char *buffer;

    char *beg;
    char *end;

    block_t *blocks;
};

void *M_ArenaAlloc(arena_t *arena, size_t count, size_t size, size_t align)
{
    block_t *block;
    array_foreach(block, arena->blocks)
    {
        if (block->size == size && array_size(block->ptrs))
        {
            return array_pop(block->ptrs);
        }
    }

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

void M_FreeBlock(arena_t *arena, void *ptr, size_t size)
{
    block_t *block;
    array_foreach(block, arena->blocks)
    {
        if (block->size == size)
        {
            array_push(block->ptrs, ptr);
            return;
        }
    }

    block_t new_block = {.ptrs = NULL, .size = size};
    array_push(new_block.ptrs, ptr);
    array_push(arena->blocks, new_block);
}

arena_t *M_InitArena(size_t cap)
{
    arena_t *arena = calloc(1, sizeof(*arena));

    arena->buffer = malloc(cap);
    arena->beg = arena->buffer;
    arena->end = arena->beg + cap;

    return arena;
}

static void FreeBlocks(arena_t *arena)
{
    block_t *block;
    array_foreach(block, arena->blocks)
    {
        array_free(block->ptrs);
    }
    array_free(block);
}

void M_ClearArena(arena_t *arena)
{
    arena->beg = arena->buffer;
    FreeBlocks(arena);
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
    FreeBlocks(arena);
}

void M_FreeArenaCopy(arena_copy_t *copy)
{
    free(copy->buffer);
    free(copy);
}
