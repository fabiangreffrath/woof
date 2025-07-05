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
#include "r_defs.h"

typedef int skyindex_t;

typedef enum
{
    SkyType_Indetermined = -1,
    SkyType_Normal,
    SkyType_Fire,
    SkyType_WithForeground,
} skytype_t;

typedef struct
{
    int texture;
    byte color; // TODO: FIXME -- consider defining a `byte *texturecolor`
                // to not need to calculate one each draw function call,
                // for the sake of animated skies
    fixed_t mid;
    fixed_t scrollx, scrolly;
    fixed_t scalex,  scaley;
    fixed_t currx,   curry;
    fixed_t prevx,   prevy;
} skytex_t;

typedef struct sky_s
{
    skytype_t type;

    // Type 0, 1, 2 -- Shared base
    skytex_t background;

    // Type 1 -- Fire
    byte*   fire;
    byte*   palette;
    int32_t updatetime;
    int32_t tics_left;

    // Type 2 -- With Foreground
    skytex_t foreground;

    // Sky color for higher FOVs
    byte color;

    // Sky transfer line specials
    side_t *side;
    boolean usedefaultmid;
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
