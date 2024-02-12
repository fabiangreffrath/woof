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

// Growable arrays of homogeneous values of any type, similar to a std::vector
// in C++. The array must be initialised to NULL. Inspired by
// https://github.com/nothings/stb/blob/master/stb_ds.h
//
// array_push(), array_grow() and array_free() may change the buffer pointer,
// and any previously-taken pointers should be considered invalidated.

#include <stddef.h>
#include <stdlib.h>

#include "i_system.h"

#ifndef M_ARRAY_INIT_CAPACITY
 #define M_ARRAY_INIT_CAPACITY 8
#endif

typedef struct
{
    int capacity;
    int size;
    char buffer[];
} m_array_buffer_t;

inline static m_array_buffer_t *array_ptr(const void *v)
{
    return (m_array_buffer_t *)((char *)v - offsetof(m_array_buffer_t, buffer));
}

inline static int array_size(const void *v)
{
    return v ? array_ptr(v)->size : 0;
}

inline static int array_capacity(const void *v)
{
    return v ? array_ptr(v)->capacity : 0;
}

inline static void array_clear(const void *v)
{
    if (v)
    {
        array_ptr(v)->size = 0;
    }
}

#define array_grow(v, n) \
    ((v) = M_ArrayGrow((v), sizeof(*(v)), n))

#define array_push(v, e) \
    do \
    { \
        if (!(v)) \
        { \
            (v) = M_ArrayGrow((v), sizeof(*(v)), M_ARRAY_INIT_CAPACITY); \
        } \
        else if (array_ptr((v))->size == array_ptr((v))->capacity) \
        { \
            (v) = M_ArrayGrow((v), sizeof(*(v)), array_ptr((v))->capacity); \
        } \
        (v)[array_ptr((v))->size++] = (e); \
    } while (0)

#define array_free(v) \
    do \
    { \
        if (v) \
        { \
            free(array_ptr((v))); \
            (v) = NULL; \
        } \
    } while (0)

inline static void *M_ArrayGrow(void *v, size_t esize, int n)
{
    m_array_buffer_t *p;

    if (v)
    {
        p = array_ptr(v);
        p = I_Realloc(p, sizeof(m_array_buffer_t) + (p->capacity + n) * esize);
        p->capacity += n;
    }
    else
    {
        p = malloc(sizeof(m_array_buffer_t) + n * esize);
        p->capacity = n;
        p->size = 0;
    }

    return p->buffer;
}
