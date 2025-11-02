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

#define arena_calloc(arena, type) \
    (type *)M_ArenaCalloc(arena, sizeof(type), alignof(type))

#define arena_alloc_num(arena, type, num) \
    (type *)M_ArenaAlloc(arena, sizeof(type) * (num), alignof(type))

void *M_ArenaAlloc(arena_t *arena, int size, int align);
void *M_ArenaCalloc(arena_t *arena, int size, int align);

void arena_free(arena_t *arena, void *ptr);

arena_t *M_ArenaInit(int reserve, int commit);
void M_ArenaClear(arena_t *arena);

typedef struct arena_copy_s arena_copy_t;

arena_copy_t *M_ArenaCopy(const arena_t *arena);
void M_ArenaRestore(arena_t *arena, const arena_copy_t *copy);
void M_ArenaFreeCopy(arena_copy_t *copy);

struct hashmap_s *M_ArenaHashMap(const arena_t *arena);
uintptr_t *M_ArenaTable(const arena_t *arena);

#endif
