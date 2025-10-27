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
#include "m_hashmap.h"

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
    hashmap_t *hashmap;
};

void *M_ArenaAlloc(arena_t *arena, int size, int align)
{
    array_foreach_type(block, arena->deleted, block_t)
    {
        if (block->size == size && block->align == align)
        {
            if (array_size(block->ptrs))
            {
                return array_pop(block->ptrs);
            }
            else
            {
                break;
            }
        }
    }

    ptrdiff_t padding = -(uintptr_t)arena->beg & (align - 1);
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

    hashmap_put(arena->hashmap, (uintptr_t)ptr, size, align);

    return ptr;
}

void *M_ArenaCalloc(arena_t *arena, int size, int align)
{
    void *ptr = M_ArenaAlloc(arena, size, align);
    memset(ptr, 0, size);
    return ptr;
}

void arena_free(arena_t *arena, void *ptr)
{
    int size, align;
    if (!hashmap_get(arena->hashmap, (uintptr_t)ptr, &size, &align))
    {
        I_Error("Freed a pointer not from arena");
    }

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

    arena->hashmap = hashmap_init(1024);

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

    hashmap_free(arena->hashmap);
    arena->hashmap = hashmap_init(1024);
}

struct arena_copy_s
{
    char *buffer;
    size_t size;

    block_t *deleted;
    hashmap_t *hashmap;
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

    for (int i = 0; i < numblocks; ++i)
    {
        to[i].ptrs = NULL;
        to[i].size = from[i].size;
        to[i].align = from[i].align;

        array_copy(to[i].ptrs, from[i].ptrs);
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
    copy->hashmap = M_HashMapCopy(arena->hashmap);

    return copy;
}

void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy)
{
    arena->beg = arena->buffer + copy->size;
    memcpy(arena->buffer, copy->buffer, copy->size);

    FreeBlocks(arena->deleted);
    arena->deleted = CopyBlocks(copy->deleted);
    hashmap_free(arena->hashmap);
    arena->hashmap = M_HashMapCopy(copy->hashmap);
}

void M_ArenaFreeCopy(arena_copy_t *copy)
{
    FreeBlocks(copy->deleted);
    hashmap_free(copy->hashmap);
    free(copy->buffer);
    free(copy);
}

hashmap_t *M_ArenaHashMap(const arena_t *arena)
{
    return arena->hashmap;
}

// Get an ordered array of pointers to the allocated memory.
uintptr_t *M_ArenaTable(const arena_t *arena)
{
    uintptr_t *table = NULL;

    array_resize(table, hashmap_get_size(arena->hashmap));

    hashmap_iterator_t iter = hashmap_iterator(arena->hashmap);
    uintptr_t key;
    int index;
    while (hashmap_next(&iter, &key, &index))
    {
        table[index] = key;
    }

    return table;
}
