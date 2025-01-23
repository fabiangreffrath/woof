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

#ifndef R_SKYDEFS_H
#define R_SKYDEFS_H

#include "doomtype.h"
#include "m_fixed.h"

typedef enum
{
    SkyType_Normal,
    SkyType_Fire,
    SkyType_WithForeground,
} skytype_t;

typedef struct fire_s
{
    byte *palette;
    int updatetime;
    int tics_left;
} fire_t;

typedef struct
{
    const char *name;
    double mid;
    fixed_t scrollx;
    fixed_t currx;
    fixed_t prevx;
    fixed_t scrolly;
    fixed_t curry;
    fixed_t prevy;
    fixed_t scalex;
    fixed_t scaley;
} skytex_t;

typedef struct sky_s
{
    skytype_t type;
    skytex_t skytex;
    fire_t fire;
    skytex_t foreground;
} sky_t;

typedef struct
{
    const char *flat;
    const char *sky;
} flatmap_t;

typedef struct
{
    sky_t *skies;
    flatmap_t *flatmapping;
} skydefs_t;

skydefs_t *R_ParseSkyDefs(void);

#endif
