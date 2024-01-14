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

#include "m_array.h"
#include "i_system.h"

m_array_buffer_t *array_ptr(void *v)
{
    return (m_array_buffer_t *)((char *)v - offsetof(m_array_buffer_t, buffer));
}

int array_size(void *v)
{
    return v ? array_ptr(v)->size : 0;
}

int array_capacity(void *v)
{
    return v ? array_ptr(v)->capacity : 0;
}

void *M_ArrayGrow(void *v, size_t esize, int n)
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
