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
    int size;
    int align;
} block_t;

struct arena_s
{
    char *buffer;
    int reserve;

    char *beg;
    char *end;

    block_t *deleted;
};

void *M_ArenaAlloc(arena_t *arena, int size, int align)
{
    array_foreach_type(block, arena->deleted, block_t)
    {
        if (block->size == size && block->align == align)
        {
            if (array_size(block->ptrs))
            {
                void *ptr = array_pop(block->ptrs);
                memset(ptr, 0, size);
                return ptr;
            }
            else
            {
                break;
            }
        }
    }

    ptrdiff_t padding = (align - ((uintptr_t)arena->beg & (align - 1))) & (align - 1);

    ptrdiff_t available = arena->end - arena->beg - padding;

    while (available < 0 || size > available)
    {
        ptrdiff_t buffer_size = arena->end - arena->buffer;
        ptrdiff_t new_buffer_size = buffer_size * 2;
        if (new_buffer_size > arena->reserve)
        {
            I_Error("Out of memory");
        }

        void *buffer = malloc(buffer_size);
        memcpy(buffer, arena->buffer, buffer_size);
        if (!I_DecommitRegion(arena->buffer, buffer_size))
        {
            I_Error("Failed to decommit region.");
        }
        if (!I_CommitRegion(arena->buffer, new_buffer_size))
        {
            I_Error("Failed to commit region.");
        }
        memcpy(arena->buffer, buffer, buffer_size);
        free(buffer);

        arena->end = arena->buffer + new_buffer_size;
        available = arena->end - arena->beg - padding;
    }

    void *ptr = arena->beg + padding;
    arena->beg += padding + size;

    memset(ptr, 0, size);

    return ptr;
}

void M_ArenaFree(arena_t *arena, void *ptr, int size, int align)
{
    array_foreach_type(block, arena->deleted, block_t)
    {
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

arena_t *M_ArenaInit(int reserve, int commit)
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
    array_foreach_type(block, blocks, block_t)
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

arena_copy_t *M_ArenaCreateCopy(const arena_t *arena)
{
    arena_copy_t *copy = calloc(1, sizeof(*copy));

    ptrdiff_t size = arena->beg - arena->buffer;
    copy->size = size;
    copy->buffer = malloc(size);
    memcpy(copy->buffer, arena->buffer, size);

    copy->deleted = NULL;
    array_foreach_type(block, arena->deleted, block_t)
    {
        block_t new_block = {.ptrs = NULL, .size = block->size, .align = block->align};
        array_copy(new_block.ptrs, block->ptrs);
        array_push(copy->deleted, new_block);
    }

    return copy;
}

arena_copy_t *M_ArenaCopy(arena_copy_t *copy, const arena_t *arena)
{
    if (!copy)
    {
        return M_ArenaCreateCopy(arena);
    }

    ptrdiff_t size = arena->beg - arena->buffer;
    if (copy->size < size)
    {
        copy->size = size;
        copy->buffer = I_Realloc(copy->buffer, copy->size);
    }
    memcpy(copy->buffer, arena->buffer, size);

    if (copy->deleted)
    {
        FreeBlocks(copy->deleted);
    }
    copy->deleted = NULL;
    array_foreach_type(block, arena->deleted, block_t)
    {
        block_t new_block = {.ptrs = NULL, .size = block->size, .align = block->align};
        array_copy(new_block.ptrs, block->ptrs);
        array_push(copy->deleted, new_block);
    }

    return copy;
}

void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy)
{
    arena->beg = arena->buffer + copy->size;
    memcpy(arena->buffer, copy->buffer, copy->size);

    FreeBlocks(arena->deleted);
    arena->deleted = NULL;
    array_foreach_type(block, copy->deleted, block_t)
    {
        block_t new_block = {.ptrs = NULL, .size = block->size, .align = block->align};
        array_copy(new_block.ptrs, block->ptrs);
        array_push(arena->deleted, new_block);
    }
}

void M_ArenaFreeCopy(arena_copy_t *copy)
{
    FreeBlocks(copy->deleted);
    free(copy->buffer);
    free(copy);
}
