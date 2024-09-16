
#ifndef ST_SBARDEF_H
#define ST_SBARDEF_H

#include "doomtype.h"
#include "r_defs.h"

typedef enum
{
    sbf_none = -1,
    sbf_mono0,
    sbf_monomax,
    sbf_proportional,
} numfonttype_t;

typedef enum
{
    sbc_none = -1,
    sbc_weaponowned,
    sbc_weaponselected,
    sbc_weaponnotselected,
    sbc_weaponhasammo,
    sbc_selectedweaponhasammo,
    sbc_selectedweaponammotype,
    sbc_weaponslotowned,
    sbc_weaponslotnotowned,
    sbc_weaponslotselected,
    sbc_weaponslotnotselected,
    sbc_itemowned,
    sbc_itemnotowned,
    sbc_featurelevelgreaterequal,
    sbc_featurelevelless,
    sbc_sessiontypeeequal,
    sbc_sessiontypenotequal,
    sbc_modeeequal,
    sbc_modenotequal,
    sbc_hudmodeequal,

    sbc_max,
} sbarconditiontype_t;

typedef struct
{
    sbarconditiontype_t condition;
    int param;
} sbarcondition_t;

typedef enum
{
    sbn_none = -1,
    sbn_health,
    sbn_armor,
    sbn_frags,
    sbn_ammo,
    sbn_ammoselected,
    sbn_maxammo,
    sbn_weaponammo,
    sbn_weaponmaxammo,

    sbn_max,
} sbarnumbertype_t;

typedef enum
{
    sbe_none = -1,
    sbe_canvas,
    sbe_graphic,
    sbe_animation,
    sbe_face,
    sbe_facebackground,
    sbe_number,
    sbe_percent,

    sbe_max,
} sbarelementtype_t;

typedef enum
{
    sbe_h_left = 0x00,
    sbe_h_middle = 0x01,
    sbe_h_right = 0x02,

    sbe_h_mask = 0x03,

    sbe_v_top = 0x00,
    sbe_v_middle = 0x04,
    sbe_v_bottom = 0x08,

    sbe_v_mask = 0x0C,
} sbaralignment_t;

typedef struct
{
    const char *lump;
    int duration;
} sbarframe_t;

typedef struct sbarelem_s sbarelem_t;

struct sbarelem_s
{
    sbarelementtype_t elemtype;
    int x_pos;
    int y_pos;
    sbaralignment_t alignment;
    const char *tranmap;
    const char *translation;
    sbarcondition_t *conditions;
    sbarelem_t *children;

    // graphic
    patch_t *patch;

    // animation
    sbarframe_t *frames;

    // number, percent
    const char *font;
    sbarnumbertype_t numbertype;
    int param;
    int maxlength;
};

typedef struct
{
    int height;
    boolean fullscreenrender;
    const char *fillflat;
    sbarelem_t *children;
} statusbar_t;

typedef struct sbarnumfont_t
{
    const char *name;
    numfonttype_t type;
    const char *stem;
    int monowidth;
    patch_t *numbers[10];
    patch_t *percent;
    patch_t *minus;
} numberfont_t;

typedef struct
{
    numberfont_t *numberfonts;
    statusbar_t *statusbars;
} sbardefs_t;

sbardefs_t *ST_ParseSbarDef(void);

#endif
