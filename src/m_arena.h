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

#include <stddef.h>
#include <stdalign.h>

typedef struct arena_s arena_t;

#define arena_alloc(a, n, t) (t *)M_ArenaAlloc(a, n, sizeof(t), alignof(t))

void *M_ArenaAlloc(arena_t *arena, size_t count, size_t size, size_t align);

#define arena_free(a, p, t) M_FreeBlock(a, p, sizeof(t), alignof(t))

void M_FreeBlock(arena_t *arena, void *ptr, size_t size, size_t align);

arena_t *M_InitArena(size_t reserve, size_t commit);
void M_ClearArena(arena_t *arena);

typedef struct arena_copy_s arena_copy_t;

arena_copy_t *M_CopyArena(const arena_t *arena);
void M_RestoreArena(arena_t *arena, const arena_copy_t *copy);
void M_FreeArenaCopy(arena_copy_t *copy);

#endif
