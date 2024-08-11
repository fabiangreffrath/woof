//
// Copyright(C) 2024 ceski
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
// DESCRIPTION:
//      Vector math functions.
//

#ifndef __M_VECTOR__
#define __M_VECTOR__

#include <math.h>

#include "doomtype.h"

typedef struct
{
    float x;
    float y;
    float z;
} vec;

typedef struct
{
    float w;
    float x;
    float y;
    float z;
} quat;

//
// Adds two vectors.
//
inline static vec vec_add(const vec *a, const vec *b)
{
    return (vec){a->x + b->x, a->y + b->y, a->z + b->z};
}

//
// Subtracts two vectors.
//
inline static vec vec_subtract(const vec *a, const vec *b)
{
    return (vec){a->x - b->x, a->y - b->y, a->z - b->z};
}

//
// Cross product of two vectors.
//
inline static vec vec_crossproduct(const vec *a, const vec *b)
{
    return (vec){a->y * b->z - a->z * b->y,
                 a->z * b->x - a->x * b->z,
                 a->x * b->y - a->y * b->x};
}

//
// Dot product of two vectors.
//
inline static float vec_dotproduct(const vec *a, const vec *b)
{
    return (a->x * b->x + a->y * b->y + a->z * b->z);
}

//
// Returns the negative of the input vector.
//
inline static vec vec_negative(const vec *v)
{
    return (vec){-v->x, -v->y, -v->z};
}

//
// Multiplies a vector by a scalar value.
//
inline static vec vec_scale(const vec *v, float scalar)
{
    return (vec){v->x * scalar, v->y * scalar, v->z * scalar};
}

//
// Clamps each vector component.
//
inline static vec vec_clamp(float min, float max, const vec *v)
{
    return (vec){BETWEEN(min, max, v->x),
                 BETWEEN(min, max, v->y),
                 BETWEEN(min, max, v->z)};
}

//
// Vector length squared.
//
inline static float vec_lengthsquared(const vec *v)
{
    return (v->x * v->x + v->y * v->y + v->z * v->z);
}

//
// Vector magnitude.
//
inline static float vec_length(const vec *v)
{
    return sqrtf(vec_lengthsquared(v));
}

//
// Normalizes a vector using a given magnitude.
//
inline static vec vec_normalize_mag(const vec *v, float mag)
{
    return (mag > 0.0f ? vec_scale(v, 1.0f / mag) : *v);
}

//
// Normalizes a vector.
//
inline static vec vec_normalize(const vec *v)
{
    return vec_normalize_mag(v, vec_length(v));
}

//
// Is this a zero vector?
//
inline static boolean is_zero_vec(const vec *v)
{
    return (v->x == 0.0f && v->y == 0.0f && v->z == 0.0f);
}

//
// Returns the conjugate of a unit quaternion (same as its inverse).
//
inline static quat quat_inverse(const quat *q)
{
    return (quat){q->w, -q->x, -q->y, -q->z};
}

//
// Converts a vector to a quaternion.
//
inline static quat vec_to_quat(const vec *v)
{
    return (quat){0.0f, v->x, v->y, v->z};
}

//
// Multiplies two quaternions.
//
inline static quat quat_multiply(const quat *a, const quat *b)
{
    return (quat){a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z,
                  a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y,
                  a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x,
                  a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w};
}

//
// Returns a unit quaternion from an angle and vector.
//
inline static quat angle_axis(float angle, const vec *v)
{
    vec temp = vec_normalize(v);
    temp = vec_scale(&temp, sinf(angle * 0.5f));
    return (quat){cosf(angle * 0.5f), temp.x, temp.y, temp.z};
}

//
// Rotates a vector by a unit quaternion.
//
inline static vec vec_rotate(const vec *v, const quat *q)
{
    const quat q_inv = quat_inverse(q);
    const quat v_quat = vec_to_quat(v);
    quat temp = quat_multiply(q, &v_quat);
    temp = quat_multiply(&temp, &q_inv);
    return (vec){temp.x, temp.y, temp.z};
}

//
// Linear interpolation between two vectors.
//
inline static vec vec_lerp(const vec *a, const vec *b, float factor)
{
    vec temp = vec_subtract(b, a);
    temp = vec_scale(&temp, factor);
    return vec_add(a, &temp);
}

#endif
