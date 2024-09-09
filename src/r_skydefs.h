
#ifndef R_SKYDEFS_H
#define R_SKYDEFS_H

typedef enum
{
    SkyType_Normal,
    SkyType_Fire,
    SkyType_WithForeground,
} skytype_t;

typedef struct fire_s
{
    int *palette;
    double updatetime;
} fire_t;

typedef struct
{
    const char *name;
    double mid;
    double scrollx;
    double scrolly;
    double scalex;
    double scaley;
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
