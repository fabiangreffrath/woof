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
//
// DESCRIPTION: Growable arrays of homogeneous values of any type, similar to a
// std::vector in C++. The array must be initialised to NULL. Inspired by
// https://github.com/nothings/stb/blob/master/stb_ds.h
//
// array_push(), array_grow() and array_free() may change the buffer pointer,
// and any previously-taken pointers should be considered invalidated.

void *M_ArrayIncreaseCapacity(void *v, size_t esize, size_t n);
void *M_ArrayIncreaseSize(void *v, size_t esize);

size_t array_size(void *v);
void   array_clear(void *v);
void   array_free(void *v);

#define array_grow(v, n) \
    ((v) = M_ArrayIncreaseCapacity((v), sizeof(*(v)), n))

#define array_push(v, e) \
    do \
    { \
        (v) = M_ArrayIncreaseSize((v), sizeof(*(v))); \
        (v)[array_size((v)) - 1] = (e); \
    } while (0)
