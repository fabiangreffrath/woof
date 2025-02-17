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

#ifndef M_ARRAY_H
#define M_ARRAY_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"

#ifndef M_ARRAY_INIT_CAPACITY
#  define M_ARRAY_INIT_CAPACITY 8
#endif

#ifndef M_ARRAY_MALLOC
#  define M_ARRAY_MALLOC(size) malloc((size))
#endif

#ifndef M_ARRAY_REALLOC
#  define M_ARRAY_REALLOC(ptr, size) I_Realloc((ptr), (size))
#endif

#ifndef M_ARRAY_FREE
#  define M_ARRAY_FREE(ptr) free((ptr))
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

#define array_grow(v, n) ((v) = M_ArrayGrow(v, sizeof(*(v)), n))

#define array_push(v, e)                                                    \
    do                                                                      \
    {                                                                       \
        if (!(v))                                                           \
        {                                                                   \
            (v) = M_ArrayGrow(v, sizeof(*(v)), M_ARRAY_INIT_CAPACITY);      \
        }                                                                   \
        else if (array_ptr(v)->size == array_ptr(v)->capacity)              \
        {                                                                   \
            (v) = M_ArrayGrow(v, sizeof(*(v)), array_ptr(v)->capacity);     \
        }                                                                   \
        (v)[array_ptr(v)->size++] = (e);                                    \
    } while (0)

#define array_delete_n(v, i, n)                                   \
    do                                                            \
    {                                                             \
        if (v)                                                    \
        {                                                         \
            memmove(&(v)[i], &(v)[i + n],                         \
                    sizeof(*(v)) * (array_ptr(v)->size - n - i)); \
            array_ptr(v)->size -= n;                              \
        }                                                         \
    } while (0)

#define array_delete(v, i) array_delete_n(v, i, 1)

#define array_free(v)                     \
    do                                    \
    {                                     \
        if (v)                            \
        {                                 \
            M_ARRAY_FREE(array_ptr(v));   \
            (v) = NULL;                   \
        }                                 \
    } while (0)

#define array_end(v) ((v) ? (v) + array_ptr(v)->size : (v))

#define array_foreach(ptr, v) \
    for (ptr = (v); ptr < array_end(v); ++ptr)

inline static void *M_ArrayGrow(void *v, size_t esize, int n)
{
    m_array_buffer_t *p;

    if (v)
    {
        p = array_ptr(v);
        p = M_ARRAY_REALLOC(p, sizeof(m_array_buffer_t)
                                   + (p->capacity + n) * esize);
        p->capacity += n;
    }
    else
    {
        p = M_ARRAY_MALLOC(sizeof(m_array_buffer_t) + n * esize);
        p->capacity = n;
        p->size = 0;
    }

    return p->buffer;
}

#endif // M_ARRAY_H
