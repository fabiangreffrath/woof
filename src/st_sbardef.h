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

#ifndef ST_SBARDEF_H
#define ST_SBARDEF_H

#include "doomtype.h"
#include "r_defs.h"
#include "v_video.h"

typedef enum
{
    sbf_none = -1,
    sbf_mono0,
    sbf_monomax,
    sbf_proportional,
} fonttype_t;

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

    // Woof!
    sbc_mode,
    sbc_widgetenabled,
    sbc_widgetdisabled,

    sbc_max,
} sbarconditiontype_t;

typedef struct
{
    sbarconditiontype_t condition;
    int param;
} sbarcondition_t;

typedef enum
{
    sbc_mode_none,
    sbc_mode_automap = 0x01,
    sbc_mode_overlay = 0x02,
    sbc_mode_hud     = 0x04,
} sbc_mode_t;

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

    // Woof!
    sbe_widget,

    sbe_max,
} sbarelementtype_t;

typedef enum
{
    sbw_monsec,
    sbw_time,
    sbw_coord,
    sbw_fps,
    sbw_rate,
    sbw_cmd,
    sbw_speed,

    sbw_message,
    sbw_secret,
    sbw_chat,
    sbw_title,
} sbarwidgettype_t;

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

    // Woof!
    sbe_wide_left = 0x10,
    sbe_wide_right = 0x20,
} sbaralignment_t;

typedef struct
{
    const char *patch_name;
    patch_t *patch;
    int duration;
} sbarframe_t;

typedef struct sbarelem_s sbarelem_t;
typedef struct numberfont_s numberfont_t;
typedef struct hudfont_s hudfont_t;

typedef struct
{
    const char *patch_name;
    patch_t *patch;
} sbe_graphic_t;

typedef struct
{
    sbarframe_t *frames;
    int frame_index;
    int duration_left;
} sbe_animation_t;

typedef struct
{
    const char *font_name;
    numberfont_t *font;
    sbarnumbertype_t type;
    int param;
    int maxlength;
    int value;
    int numvalues;
    int oldvalue;
    int xoffset;
} sbe_number_t;

typedef struct
{
    int faceindex;
    int facecount;
    int oldhealth;
} sbe_face_t;

typedef struct
{
    const char *string;
    int totalwidth;
    int xoffset;
} widgetline_t;

typedef struct sbe_widget_s
{
    sbarwidgettype_t type;
    const char *font_name;
    hudfont_t *font;
    widgetline_t *lines;

    // message
    int duration;
} sbe_widget_t;

struct sbarelem_s
{
    sbarelementtype_t type;
    int x_pos;
    int y_pos;
    sbaralignment_t alignment;
    sbarcondition_t *conditions;
    sbarelem_t *children;

    byte *tranmap;
    crange_idx_e cr;
    crange_idx_e crboom;

    union
    {
        sbe_graphic_t *graphic;
        sbe_animation_t *animation;
        sbe_number_t *number;
        sbe_face_t *face;

        // Woof!
        sbe_widget_t *widget;
    } pointer;
};

typedef struct
{
    int height;
    boolean fullscreenrender;
    const char *fillflat;
    sbarelem_t *children;
} statusbar_t;

struct numberfont_s
{
    const char *name;
    fonttype_t type;
    int monowidth;
    int maxheight;
    patch_t *numbers[10];
    patch_t *percent;
    patch_t *minus;
};

#define HU_FONTSTART    '!'     /* the first font characters */
#define HU_FONTEND      (0x7f)  /*jff 2/16/98 '_' the last font characters */

// Calculate # of glyphs in font.
#define HU_FONTSIZE     (HU_FONTEND - HU_FONTSTART + 1)
#define SPACEWIDTH      4

struct hudfont_s
{
    const char *name;
    fonttype_t type;
    int monowidth;
    int maxheight;
    patch_t *characters[HU_FONTSIZE];
};

typedef struct
{
    numberfont_t *numberfonts;
    hudfont_t *hudfonts;
    statusbar_t *statusbars;
} sbardef_t;

sbardef_t *ST_ParseSbarDef(void);

#endif
