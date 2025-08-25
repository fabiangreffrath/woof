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

#include "i_region.h"
#include "i_system.h"

#define M_ARRAY_INIT_CAPACITY 32
#include "m_array.h"

typedef struct
{
    void **ptrs;
    size_t size;
    size_t align;
} block_t;

struct arena_s
{
    char *buffer;
    size_t reserve;

    char *beg;
    char *end;

    block_t *deleted;
};

void *M_ArenaAlloc(arena_t *arena, size_t count, size_t size, size_t align)
{
    int deleted_size = array_size(arena->deleted);
    for (int i = 0; i < deleted_size; ++i)
    {
        block_t *block = &arena->deleted[i];

        if (block->size == size && block->align == align
            && array_size(block->ptrs))
        {
            return array_pop(block->ptrs);
        }
    }

    ptrdiff_t padding = -(uintptr_t)arena->beg & (align - 1);
    ptrdiff_t available = arena->end - arena->beg - padding;

    if (available < 0 || count > available / size)
    {
        ptrdiff_t buffer_size = arena->end - arena->buffer;
        if (buffer_size * 2 > arena->reserve)
        {
            I_Error("Out of memory");
        }
        void *buffer = malloc(buffer_size);
        memcpy(buffer, arena->buffer, buffer_size);
        if (!I_DecommitRegion(arena->buffer, buffer_size))
        {
            I_Error("Failed to decommit region.");
        }
        if (!I_CommitRegion(arena->buffer, buffer_size * 2))
        {
            I_Error("Failed to commit region.");
        }
        memcpy(arena->buffer, buffer, buffer_size);
        free(buffer);
        arena->end = arena->buffer + buffer_size * 2;
    }

    void *ptr = arena->beg + padding;
    arena->beg += padding + count * size;
    return ptr;
}

void M_ArenaFree(arena_t *arena, void *ptr, size_t size, size_t align)
{
    int deleted_size = array_size(arena->deleted);
    for (int i = 0; i < deleted_size; ++i)
    {
        block_t *block = &arena->deleted[i];

        if (block->size == size && block->align == align)
        {
            array_push(block->ptrs, ptr);
            return;
        }
    }

    block_t block = {.ptrs = NULL, .size = size, .align = align};
    array_push(block.ptrs, ptr);
    array_push(arena->deleted, block);
}

arena_t *M_ArenaInit(size_t reserve, size_t commit)
{
    arena_t *arena = calloc(1, sizeof(*arena));

    arena->reserve = reserve;
    arena->buffer = I_ReserveRegion(reserve);
    if (!arena->buffer)
    {
        I_Error("Failed to reserve region.");
    }
    if (!I_CommitRegion(arena->buffer, commit))
    {
        I_Error("Failed to commit region.");
    }
    arena->beg = arena->buffer;
    arena->end = arena->beg + commit;

    return arena;
}

static void FreeBlocks(block_t *blocks)
{
    block_t *block;
    array_foreach(block, blocks)
    {
        array_free(block->ptrs);
    }
    array_free(blocks);
}

void M_ArenaClear(arena_t *arena)
{
    arena->beg = arena->buffer;

    FreeBlocks(arena->deleted);
    arena->deleted = NULL;
}

struct arena_copy_s
{
    char *buffer;
    size_t size;

    block_t *deleted;
};

static block_t *CopyBlocks(const block_t *from)
{
    int numblocks = array_size(from);
    if (!numblocks)
    {
        return NULL;
    }

    block_t *to = NULL;
    array_grow(to, numblocks);
    array_ptr(to)->size = numblocks;

    for (int i = 0; i < numblocks; ++i)
    {
        to[i].ptrs = NULL;
        to[i].size = from[i].size;
        to[i].align = from[i].align;

        int numptrs = array_size(from[i].ptrs);
        if (numptrs)
        {
            array_grow(to[i].ptrs, numptrs);
            memcpy(to[i].ptrs, from[i].ptrs, numptrs * sizeof(void *));
            array_ptr(to[i].ptrs)->size = numptrs;
        }
    }

    return to;
}

arena_copy_t *M_ArenaCopy(const arena_t *arena)
{
    arena_copy_t *copy = calloc(1, sizeof(*copy));

    ptrdiff_t size = arena->beg - arena->buffer;
    copy->size = size;
    copy->buffer = malloc(size);
    memcpy(copy->buffer, arena->buffer, size);

    copy->deleted = CopyBlocks(arena->deleted);

    return copy;
}

void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy)
{
    arena->beg = arena->buffer + copy->size;
    memcpy(arena->buffer, copy->buffer, copy->size);

    FreeBlocks(arena->deleted);
    arena->deleted = CopyBlocks(copy->deleted);
}

void M_ArenaFreeCopy(arena_copy_t *copy)
{
    FreeBlocks(copy->deleted);
    free(copy->buffer);
    free(copy);
}
