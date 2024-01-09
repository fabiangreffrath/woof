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

#include <stddef.h>
#include <stdlib.h>
#include "i_system.h"

#define ARRAY_INIT_CAPACITY 8

struct buffer_s
{
    unsigned int capacity;
    unsigned int size;
    char buffer[];
};

#define ARR_PTR(v) \
    ((struct buffer_s *)((char *)(v) - offsetof(struct buffer_s, buffer)))

unsigned int array_size(void *v)
{
    return v ? ARR_PTR(v)->size : 0;
}

unsigned int array_capacity(void *v)
{
    return v ? ARR_PTR(v)->capacity : 0;
}

void array_clear(void *v)
{
    if (v)
    {
        ARR_PTR(v)->size = 0;
    }
}

void array_free(void *v)
{
    if (v)
    {
        free(ARR_PTR(v));
        v = NULL;
    }
}

void *M_ArrayIncreaseCapacity(void *v, size_t esize, unsigned int n)
{
    struct buffer_s *p;

    if (v)
    {
        p = ARR_PTR(v);
        p = I_Realloc(p, sizeof(struct buffer_s) + (p->capacity + n) * esize);
        p->capacity += n;
    }
    else
    {
        p = malloc(sizeof(struct buffer_s) + n * esize);
        p->capacity = n;
        p->size = 0;
    }

    return p->buffer;
}

void *M_ArrayIncreaseSize(void *v, size_t esize)
{
    struct buffer_s *p;

    if (v)
    {
        p = ARR_PTR(v);
        if (p->size == p->capacity)
        {
            v = M_ArrayIncreaseCapacity(v, esize, p->capacity);
            p = ARR_PTR(v);
        }
        p->size++;
    }
    else
    {
        v = M_ArrayIncreaseCapacity(v, esize, ARRAY_INIT_CAPACITY);
        p = ARR_PTR(v);
        p->size++;
    }

    return v;
}
