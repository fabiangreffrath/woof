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

#include <assert.h>
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

// Every block is preceded by this header; the pointer returned to
// callers points just past it. The next header is at a stride derived
// from the size, so the arena can be walked without a separate index.

#define ARENA_ALIGN 16
#define ARENA_MAGIC 0xA5A5C0DEu

typedef struct
{
    uint32_t index;
    uint32_t size;
    uint32_t align;
    uint32_t magic;
} arena_header_t;

static_assert(sizeof(arena_header_t) <= ARENA_ALIGN,
              "arena header does not fit the alignment unit");

struct arena_s
{
    char *buffer;
    int reserve;

    char *beg;
    char *end;

    block_t *deleted;

    uint32_t next_index;
    uint32_t num_blocks;
};

inline static size_t AlignUp(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

inline static size_t BlockStride(size_t size)
{
    return ARENA_ALIGN + AlignUp(size, ARENA_ALIGN);
}

inline static arena_header_t *HeaderOf(const void *ptr)
{
    return (arena_header_t *)((char *)ptr - ARENA_ALIGN);
}

inline static void *DataOf(const arena_header_t *header)
{
    return (char *)header + ARENA_ALIGN;
}

void *M_ArenaAlloc(arena_t *arena, int size, int align)
{
    if (size < 0 || align <= 0 || align > ARENA_ALIGN)
    {
        I_Error("Invalid arena allocation: size %d, align %d", size, align);
    }

    array_foreach_type(block, arena->deleted, block_t)
    {
        if (block->size == size && block->align == align)
        {
            if (array_size(block->ptrs))
            {
                void *ptr = array_pop(block->ptrs);
#ifdef RANGECHECK
                arena_header_t *header = HeaderOf(ptr);
                if (header->magic != ARENA_MAGIC
                    || header->size != (uint32_t)size
                    || header->align != (uint32_t)align)
                {
                    I_Error("Corrupted arena free list");
                }
#endif
                memset(ptr, 0, size);
                return ptr;
            }
            else
            {
                break;
            }
        }
    }

#ifdef RANGECHECK
    if ((uintptr_t)arena->beg & (ARENA_ALIGN - 1))
    {
        I_Error("Arena buffer is not aligned");
    }
#endif

    const size_t needed = BlockStride(size);

    while (arena->end - arena->beg < (ptrdiff_t)needed)
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
    }

    arena_header_t *header = (arena_header_t *)arena->beg;
    header->index = arena->next_index++;
    header->size = size;
    header->align = align;
    header->magic = ARENA_MAGIC;

    void *ptr = DataOf(header);
    arena->beg += needed;
    arena->num_blocks++;

    memset(ptr, 0, size);

    return ptr;
}

void arena_free(arena_t *arena, void *ptr)
{
    const uintptr_t key = (uintptr_t)ptr;

    if (key < (uintptr_t)arena->buffer + ARENA_ALIGN
        || key >= (uintptr_t)arena->beg)
    {
        I_Error("Freed a pointer not from arena");
    }

    arena_header_t *header = HeaderOf(ptr);

    if ((key & (ARENA_ALIGN - 1)) || header->magic != ARENA_MAGIC)
    {
        I_Error("Freed a pointer not from arena");
    }

#ifdef RANGECHECK
    if (header->size > (uint32_t)(arena->beg - (char *)ptr))
    {
        I_Error("Corrupted arena block header");
    }
#endif

    array_foreach_type(block, arena->deleted, block_t)
    {
        if (block->size == (int)header->size
            && block->align == (int)header->align)
        {
            array_push(block->ptrs, ptr);
            return;
        }
    }

    block_t block = {
        .ptrs = NULL,
        .size = (int)header->size,
        .align = (int)header->align
    };
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

    arena->next_index = 0;
    arena->num_blocks = 0;
}

struct arena_copy_s
{
    char *buffer;
    size_t size;

    block_t *deleted;

    uint32_t next_index;
    uint32_t num_blocks;
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

arena_copy_t *M_ArenaCreateCopy(const arena_t *arena)
{
    arena_copy_t *copy = calloc(1, sizeof(*copy));

    size_t size = (size_t)(arena->beg - arena->buffer);
    copy->size = size;

    if (size > 0)
    {
        copy->buffer = malloc(size);
        memcpy(copy->buffer, arena->buffer, size);
    }

    copy->deleted = CopyBlocks(arena->deleted);
    copy->next_index = arena->next_index;
    copy->num_blocks = arena->num_blocks;

    return copy;
}

arena_copy_t *M_ArenaCopy(arena_copy_t *copy, const arena_t *arena)
{
    if (!copy)
    {
        return M_ArenaCreateCopy(arena);
    }

    size_t size = (size_t)(arena->beg - arena->buffer);
    if (copy->size < size)
    {
        copy->buffer = I_Realloc(copy->buffer, size);
    }

    // The arena can shrink when a rollback restores an older keyframe,
    // so the copy's logical size must shrink as well.

    copy->size = size;

    if (size > 0)
    {
        memcpy(copy->buffer, arena->buffer, size);
    }

    if (copy->deleted)
    {
        array_free(copy->deleted);
    }
    copy->deleted = CopyBlocks(arena->deleted);
    copy->next_index = arena->next_index;
    copy->num_blocks = arena->num_blocks;

    return copy;
}

void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy)
{
#ifdef RANGECHECK
    if (copy->size & (ARENA_ALIGN - 1))
    {
        I_Error("Corrupted arena copy size");
    }
#endif

    arena->beg = arena->buffer + copy->size;
    if (copy->size > 0)
    {
        memcpy(arena->buffer, copy->buffer, copy->size);
    }

    FreeBlocks(arena->deleted);
    arena->deleted = CopyBlocks(copy->deleted);
    arena->next_index = copy->next_index;
    arena->num_blocks = copy->num_blocks;
}

void M_ArenaFreeCopy(arena_copy_t *copy)
{
    FreeBlocks(copy->deleted);
    free(copy->buffer);
    free(copy);
}

int M_ArenaTableIndex(const arena_t *arena, uintptr_t key)
{
    if (key < (uintptr_t)(arena->buffer + ARENA_ALIGN)
        || key >= (uintptr_t)arena->beg)
    {
        return -1;
    }

    const arena_header_t *header =
        (const arena_header_t *)((const char *)key - ARENA_ALIGN);

    if (header->magic != ARENA_MAGIC)
    {
        return -1;
    }

    return (int)header->index;
}

int M_ArenaTableSize(const arena_t *arena)
{
    return (int)arena->num_blocks;
}

// Get an array of pointers to every block ever allocated since the
// arena was cleared, including blocks currently on a free list. The
// array is ordered by the block index (first allocation order).

uintptr_t *M_ArenaTable(const arena_t *arena)
{
    uintptr_t *table = calloc(arena->num_blocks, sizeof(*table));

    arena_header_t *header = (arena_header_t *)arena->buffer;

    for (uint32_t i = 0; i < arena->num_blocks; ++i)
    {
#ifdef RANGECHECK
        if ((char *)header + ARENA_ALIGN > arena->beg
            || header->magic != ARENA_MAGIC
            || header->index >= arena->num_blocks)
        {
            I_Error("Corrupted arena block chain");
        }
#endif
        table[header->index] = (uintptr_t)DataOf(header);
        header = (arena_header_t *)((char *)header
                                    + BlockStride(header->size));
    }

#ifdef RANGECHECK
    if ((char *)header != arena->beg)
    {
        I_Error("Corrupted arena block chain");
    }
#endif

    return table;
}
