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

#include <stdlib.h>

#ifndef M_ARRAY_INIT_CAPACITY
 #define M_ARRAY_INIT_CAPACITY 8
#endif

typedef struct
{
    int capacity;
    int size;
    char buffer[];
} m_array_buffer_t;

m_array_buffer_t *array_ptr(void *v);
int array_size(void *v);
int array_capacity(void *v);

void *M_ArrayGrow(void *v, size_t esize, int n);

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
