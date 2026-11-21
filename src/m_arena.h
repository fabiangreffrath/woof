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

#ifndef M_ARENA_H
#define M_ARENA_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

typedef struct arena_s arena_t;

#define arena_alloc(arena, type) \
    (type *)M_ArenaAlloc(arena, sizeof(type), alignof(type))

#define arena_alloc_num(arena, type, num) \
    (type *)M_ArenaAlloc(arena, sizeof(type) * (num), alignof(type))

void *M_ArenaAlloc(arena_t *arena, size_t size, size_t align);

void arena_free(arena_t *arena, void *ptr);

arena_t *M_ArenaInit(size_t reserve, size_t commit);
void M_ArenaClear(arena_t *arena);

typedef struct arena_copy_s arena_copy_t;

arena_copy_t *M_ArenaCreateCopy(const arena_t *arena);
arena_copy_t *M_ArenaCopy(arena_copy_t *copy, const arena_t *arena);
void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy);
void M_ArenaFreeCopy(arena_copy_t *copy);

// Index of a block in the arena, or -1 if the pointer does not belong
// to this arena. Blocks on a free list keep their index.

int M_ArenaTableIndex(const arena_t *arena, uintptr_t key);

// Number of blocks allocated since the arena was last cleared,
// including blocks currently on a free list.

int M_ArenaTableSize(const arena_t *arena);

// Get an array of pointers to every allocated block, ordered by first
// allocation (that is, by M_ArenaTableIndex()). Blocks on a free list
// are included.

uintptr_t *M_ArenaTable(const arena_t *arena);

#endif
