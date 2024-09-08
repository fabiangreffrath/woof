
#ifndef R_SKYDEFS_H
#define R_SKYDEFS_H

#include "m_fixed.h"

typedef enum
{
    sky_normal,
    sky_fire,
    sky_withforeground,
} skytype_t;

typedef struct
{
    int *palette;
    double updatetime;
} fire_t;

typedef struct
{
    const char *name;
    fixed_t mid;
    fixed_t scrollx;
    fixed_t scrolly;
    fixed_t scalex;
    fixed_t scaley;
} skytex_t;

typedef struct
{
    skytype_t type;
    skytex_t background;
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
} skydef_t;

skydef_t *R_ParseSkyDefs(void);

#endif
