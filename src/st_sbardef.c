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

#include "st_sbardef.h"

#include <stdlib.h>

#include "doomdef.h"
#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_json.h"
#include "m_misc.h"
#include "m_swap.h"
#include "r_data.h"
#include "r_defs.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

static numberfont_t *numberfonts;
static hudfont_t *hudfonts;

static const char *sbn_names[] = {
   [sbn_health] = "health",
   [sbn_armor] = "armor",
   [sbn_frags] = "frags",
   [sbn_ammo] = "ammo",
   [sbn_ammoselected] = "ammoselected",
   [sbn_maxammo] = "maxammo",
   [sbn_weaponammo] = "weaponammo",
   [sbn_weaponmaxammo] = "weaponmaxammo"
};

static const char *item_names[] = {
    [item_messageonly] = "messageonly",
    [item_bluecard] = "bluecard",
    [item_yellowcard] = "yellowcard",
    [item_redcard] = "redcard",
    [item_blueskull] = "blueskull",
    [item_yellowskull] = "yellowskull",
    [item_redskull] = "redskull",
    [item_backpack] = "backpack",
    [item_healthbonus] = "healthbonus",
    [item_stimpack] = "stimpack",
    [item_medikit] = "medikit",
    [item_soulsphere] = "soulsphere",
    [item_megasphere] = "megasphere",
    [item_armorbonus] = "armorbonus",
    [item_greenarmor] = "greenarmor",
    [item_bluearmor] = "bluearmor",
    [item_areamap] = "areamap",
    [item_lightamp] = "lightamp",
    [item_berserk] = "berserk",
    [item_invisibility] = "invisibility",
    [item_radsuit] = "radsuit",
    [item_invulnerability] = "invulnerability",
};

static const char *session_names[] = {
    "singleplayer",
    "cooperative", 
    "deathmatch",
};

static const char *gamemode_names[] = {
    [shareware] = "shareware",
    [registered] = "registered",
    [commercial] = "commercial",
    [retail] = "retail",
    [indetermined] = "indetermined" 
};

static const char *widgetenabled_names[] = {
    [sbw_monsec] = "monsec",
    [sbw_time] = "time",
    [sbw_coord] = "coord"
};

static const char *sbc_names[] = {
    [sbc_weaponowned] = "weaponowned",
    [sbc_weaponselected] = "weaponselected",
    [sbc_weaponnotselected] = "weaponnotselected",
    [sbc_weaponhasammo] = "weaponhasammo",
    [sbc_selectedweaponhasammo] = "selectedweaponhasammo",
    [sbc_selectedweaponammotype] = "selectedweaponammotype",
    [sbc_weaponslotowned] = "weaponslotowned",
    [sbc_weaponslotnotowned] = "weaponslotnotowned",
    [sbc_weaponslotselected] = "weaponslotselected",
    [sbc_weaponslotnotselected] = "weaponslotnotselected",
    [sbc_itemowned] = "itemowned",
    [sbc_itemnotowned] = "itemnotowned",
    [sbc_featurelevelgreaterequal] = "featurelevelgreaterequal",
    [sbc_featurelevelless] = "featurelevelless",
    [sbc_sessiontypeequal] = "sessiontypeequal",
    [sbc_sessiontypenotequal] = "sessiontypenotequal",
    [sbc_modeeequal] = "modeeequal",
    [sbc_modenotequal] = "modenotequal",
    [sbc_hudmodeequal] = "hudmodeequal",
    [sbc_widgetmode] = "widgetmode",
    [sbc_widgetenabled] = "widgetenabled",
    [sbc_widgetdisabled] = "widgetdisabled",
    [sbc_weaponnotowned] = "weaponnotowned",
    [sbc_healthgreaterequal] = "healthgreaterequal",
    [sbc_healthless] = "healthless",
    [sbc_armorgreaterequal] = "armorgreaterequal",
    [sbc_armorless] = "armorless",
    [sbc_widescreenequal] = "widescreenequal",
};

static const char *sbw_names[] = {
    [sbw_monsec] = "monsec",
    [sbw_time] = "time",
    [sbw_coord] = "coord",
    [sbw_fps] = "fps",
    [sbw_rate] = "rate",
    [sbw_cmd] = "cmd",
    [sbw_speed] = "speed",
    [sbw_message] = "message",
    [sbw_announce] = "announce",
    [sbw_chat] = "chat",
    [sbw_title] = "title",
};

static int CheckNameInternal(const char *name, const char *array[], int size)
{
    for (int i = 0; i < size; ++i)
    {
        if (!strcasecmp(name, array[i]))
        {
            return i;
        }
    }
    return -1;
}

#define CheckName(name, array) CheckNameInternal(name, array, arrlen(array))

static int CheckNamedValueInternal(json_t *json, const char *array[], int size)
{
    if (JS_IsNumber(json))
    {
        return JS_GetNumber(json);
    }
    else if (JS_IsString(json))
    {
        const char *name = JS_GetString(json);
        return CheckNameInternal(name, array, size);
    }
    return -1;
}

#define CheckNamedValue(json, array) \
    CheckNamedValueInternal(json, array, arrlen(array))

static boolean ParseSbarCondition(json_t *json, sbarcondition_t *out)
{
    json_t *condition = JS_GetObject(json, "condition");
    out->condition = CheckNamedValue(condition, sbc_names);
    if (out->condition == sbc_none)
    {
        return false;
    }

    json_t *param = JS_GetObject(json, "param");
    if (JS_IsNumber(param))
    {
        out->param = JS_GetNumber(param);
    }
    else if (JS_IsString(param))
    {
        const char *name = JS_GetString(param);
        switch (out->condition)
        {
            case sbc_itemowned:
            case sbc_itemnotowned:
                out->param = CheckName(name, item_names);
                break;
            case sbc_sessiontypeequal:
            case sbc_sessiontypenotequal:
                out->param = CheckName(name, session_names);
                break;
            case sbc_modeeequal:
            case sbc_modenotequal:
                out->param = CheckName(name, gamemode_names);
                break;
            case sbc_widgetenabled:
            case sbc_widgetdisabled:
                out->param = CheckName(name, widgetenabled_names);
                break;
            default:
                return false;
        }
        if (out->param == -1)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

static boolean ParseSbarFrame(json_t *json, sbarframe_t *out)
{
    json_t *lump = JS_GetObject(json, "lump");
    if (!JS_IsString(lump))
    {
        return false;
    }
    out->patch_name = M_StringDuplicate(JS_GetString(lump));

    json_t *duration = JS_GetObject(json, "duration");
    if (!JS_IsNumber(duration))
    {
        return false;
    }
    out->duration = JS_GetNumber(duration) * TICRATE;
    return true;
}

static boolean ParseSbarElem(json_t *json, sbarelem_t *out);

static boolean ParseSbarElemType(json_t *json, sbarelementtype_t type,
                                 sbarelem_t *out)
{
    out->type = type;

    json_t *x_pos = JS_GetObject(json, "x");
    json_t *y_pos = JS_GetObject(json, "y");
    json_t *alignment = JS_GetObject(json, "alignment");
    if (!JS_IsNumber(x_pos) || !JS_IsNumber(y_pos) || !JS_IsNumber(alignment))
    {
        return false;
    }
    out->x_pos = JS_GetInteger(x_pos);
    out->y_pos = JS_GetInteger(y_pos);
    out->alignment = JS_GetInteger(alignment);

    const char *tranmap = JS_GetStringValue(json, "tranmap");
    if (tranmap)
    {
        out->tranmap = W_CacheLumpName(tranmap, PU_STATIC);
    }

    json_t *translucency = JS_GetObject(json, "translucency");
    if (JS_IsBoolean(translucency) && JS_GetBoolean(translucency))
    {
        out->tranmap = main_tranmap;
    }

    const char *translation = JS_GetStringValue(json, "translation");
    out->cr = translation ? V_CRByName(translation) : CR_NONE;
    out->crboom = CR_NONE;

    json_t *js_conditions = JS_GetObject(json, "conditions");
    json_t *js_condition = NULL;
    JS_ArrayForEach(js_condition, js_conditions)
    {
        sbarcondition_t condition = {0};
        if (ParseSbarCondition(js_condition, &condition))
        {
            array_push(out->conditions, condition);
        }
    }

    json_t *js_children = JS_GetObject(json, "children");
    json_t *js_child = NULL;
    JS_ArrayForEach(js_child, js_children)
    {
        sbarelem_t elem = {0};
        if (ParseSbarElem(js_child, &elem))
        {
            array_push(out->children, elem);
        }
    }

    switch (type)
    {
        case sbe_graphic:
            {
                sbe_graphic_t *graphic = calloc(1, sizeof(*graphic));
                const char *patch = JS_GetStringValue(json, "patch");
                if (!patch)
                {
                    free(graphic);
                    return false;
                }
                graphic->patch_name = M_StringDuplicate(patch);
                out->subtype.graphic = graphic;
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = calloc(1, sizeof(*animation));
                json_t *js_frames = JS_GetObject(json, "frames");
                json_t *js_frame = NULL;
                JS_ArrayForEach(js_frame, js_frames)
                {
                    sbarframe_t frame = {0};
                    if (ParseSbarFrame(js_frame, &frame))
                    {
                        array_push(animation->frames, frame);
                    }
                }
                out->subtype.animation = animation;
            }
            break;

        case sbe_number:
        case sbe_percent:
            {
                sbe_number_t *number = calloc(1, sizeof(*number));
                const char *font_name = JS_GetStringValue(json, "font");
                if (!font_name)
                {
                    free(number);
                    return false;
                }
                json_t *type = JS_GetObject(json, "type");
                number->type = CheckNamedValue(type, sbn_names);
                json_t *param = JS_GetObject(json, "param");
                json_t *maxlength = JS_GetObject(json, "maxlength");
                if (number->type == sbn_none || !JS_IsNumber(param)
                    || !JS_IsNumber(maxlength))
                {
                    free(number);
                    return false;
                }
                number->param = JS_GetInteger(param);
                number->maxlength = JS_GetInteger(maxlength);

                numberfont_t *font;
                array_foreach(font, numberfonts)
                {
                    if (!strcmp(font->name, font_name))
                    {
                        number->font = font;
                        break;
                    }
                }
                out->subtype.number = number;
            }
            break;

        case sbe_widget:
             {
                sbe_widget_t *widget = calloc(1, sizeof(*widget));
                const char *font_name = JS_GetStringValue(json, "font");
                if (!font_name)
                {
                    free(widget);
                    return false;
                }

                json_t *type = JS_GetObject(json, "type");
                widget->type = CheckNamedValue(type, sbw_names);
                if (widget->type == sbw_none)
                {
                    free(widget);
                    return false;
                }

                hudfont_t *font;
                array_foreach(font, hudfonts)
                {
                    if (!strcmp(font->name, font_name))
                    {
                        widget->default_font = widget->font = font;
                        break;
                    }
                }

                switch (widget->type)
                {
                    case sbw_message:
                    case sbw_announce:
                        widget->duration =
                            JS_GetNumberValue(json, "duration") * TICRATE;
                        break;
                    case sbw_monsec:
                    case sbw_coord:
                        {
                            json_t *vertical = JS_GetObject(json, "vertical");
                            if (JS_IsBoolean(vertical))
                            {
                                widget->vertical = JS_GetBoolean(vertical);
                            }
                        }
                        break;
                    default:
                        break;
                }

                out->subtype.widget = widget;
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = calloc(1, sizeof(*face));
                out->subtype.face = face;
            }
            break;

        default:
            break;
    }
    return true;
}

static const char *sbe_names[] =
{
    [sbe_canvas] = "canvas",
    [sbe_graphic] = "graphic",
    [sbe_animation] = "animation",
    [sbe_face] = "face",
    [sbe_facebackground] = "facebackground",
    [sbe_number] = "number",
    [sbe_percent] = "percent",
    [sbe_widget] = "widget",
    [sbe_carousel] = "carousel"
};

static boolean ParseSbarElem(json_t *json, sbarelem_t *out)
{
    for (sbarelementtype_t type = sbe_none + 1; type < sbe_max; ++type)
    {
        json_t *obj = JS_GetObject(json, sbe_names[type]);
        if (obj && JS_IsObject(obj))
        {
            return ParseSbarElemType(obj, type, out);
        }
    }

    return false;
}

static boolean ParseNumberFont(json_t *json, numberfont_t *out)
{
    json_t *name = JS_GetObject(json, "name");
    if (!JS_IsString(name))
    {
        return false;
    }
    out->name = M_StringDuplicate(JS_GetString(name));

    const char *stem = JS_GetStringValue(json, "stem");
    if (!stem)
    {
        return false;
    }

    json_t *type = JS_GetObject(json, "type");
    if (!JS_IsNumber(type))
    {
        return false;
    }
    out->type = JS_GetInteger(type);

    char lump[9] = {0};
    int found;
    int maxwidth = 0;
    int maxheight = 0;

    for (int num = 0; num < 10; ++num)
    {
        M_snprintf(lump, sizeof(lump), "%sNUM%d", stem, num);
        found = W_CheckNumForName(lump);
        if (found < 0)
        {
            I_Printf(VB_ERROR, "SBARDEF: patch \"%s\" not found", lump);
            continue;
        }
        out->numbers[num] = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->numbers[num]->width));
        maxheight = MAX(maxheight, SHORT(out->numbers[num]->height));
    }

    out->maxheight = maxheight;

    M_snprintf(lump, sizeof(lump), "%sMINUS", stem);
    found = W_CheckNumForName(lump);
    if (found >= 0)
    {
        out->minus = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->minus->width));
    }

    M_snprintf(lump, sizeof(lump), "%sPRCNT", stem);
    found = W_CheckNumForName(lump);
    if (found >= 0)
    {
        out->percent = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->percent->width));
    }

    switch (out->type)
    {
        case sbf_mono0:
            out->monowidth = SHORT(out->numbers[0]->width);
            break;
        case sbf_monomax:
            out->monowidth = maxwidth;
            break;
        default:
            break;
    }

    return true;
}

static void LoadHUDFont(hudfont_t *out)
{
    char lump[9] = {0};
    int found;
    int maxwidth = 0;
    int maxheight = 0;

    for (int i = 0; i < HU_FONTSIZE; ++i)
    {
        M_snprintf(lump, sizeof(lump), "%s%03d", out->stem, i + HU_FONTSTART);
        found = W_CheckNumForName(lump);
        if (found < 0)
        {
            out->characters[i] = NULL;
            continue;
        }
        out->characters[i] = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->characters[i]->width));
        maxheight = MAX(maxheight, SHORT(out->characters[i]->height));
    }

    out->maxheight = maxheight;

    switch (out->type)
    {
        case sbf_mono0:
            out->monowidth = SHORT(out->characters[0]->width);
            break;
        case sbf_monomax:
            out->monowidth = maxwidth;
            break;
        default:
            break;
    }
}

static boolean ParseHUDFont(json_t *json, hudfont_t *out)
{
    json_t *name = JS_GetObject(json, "name");
    if (!JS_IsString(name))
    {
        return false;
    }
    out->name = M_StringDuplicate(JS_GetString(name));

    const char *stem = JS_GetStringValue(json, "stem");
    if (!stem)
    {
        return false;
    }
    out->stem = M_StringDuplicate(stem);

    json_t *type = JS_GetObject(json, "type");
    if (!JS_IsNumber(type))
    {
        return false;
    }
    out->type = JS_GetInteger(type);

    LoadHUDFont(out);

    return true;
}

hudfont_t *LoadSTCFN(void)
{
    hudfont_t *font;
    array_foreach(font, hudfonts)
    {
        if (!strcasecmp(font->stem, "STCFN"))
        {
            return font;
        }
    }
    font = calloc(1, sizeof(*font));
    font->stem = "STCFN";
    font->type = sbf_proportional;
    LoadHUDFont(font);
    return font;
}

static boolean ParseStatusBar(json_t *json, statusbar_t *out)
{
    json_t *height = JS_GetObject(json, "height");
    json_t *fullscreenrender = JS_GetObject(json, "fullscreenrender");
    if (!JS_IsNumber(height) || !JS_IsBoolean(fullscreenrender))
    {
        return false;
    }
    out->height = JS_GetInteger(height);
    out->fullscreenrender = JS_GetBoolean(fullscreenrender);

    const char *fillflat = JS_GetStringValue(json, "fillflat");
    out->fillflat = fillflat ? M_StringDuplicate(fillflat) : NULL;

    json_t *js_children = JS_GetObject(json, "children");
    json_t *js_child = NULL;
    JS_ArrayForEach(js_child, js_children)
    {
        sbarelem_t elem = {0};
        if (ParseSbarElem(js_child, &elem))
        {
            array_push(out->children, elem);
        }
    }

    return true;
}

sbardef_t *ST_ParseSbarDef(void)
{
    json_t *json = JS_Open("SBARDEF", "statusbar", (version_t){1, 1, 0});
    if (json == NULL)
    {
        return NULL;
    }

    boolean load_defaults = false;
    version_t v = {0};
    JS_GetVersion(json, &v);
    if (v.major == 1 && v.minor < 1)
    {
        load_defaults = true;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_ERROR, "SBARDEF: no data");
        JS_Close("SBARDEF");
        return NULL;
    }

    sbardef_t *out = calloc(1, sizeof(*out));

    json_t *js_numberfonts = JS_GetObject(data, "numberfonts");
    json_t *js_numberfont = NULL;

    JS_ArrayForEach(js_numberfont, js_numberfonts)
    {
        numberfont_t numberfont = {0};
        if (ParseNumberFont(js_numberfont, &numberfont))
        {
            array_push(numberfonts, numberfont);
        }
    }

    json_t *js_hudfonts = JS_GetObject(data, "hudfonts");
    json_t *js_hudfont = NULL;

    JS_ArrayForEach(js_hudfont, js_hudfonts)
    {
        hudfont_t hudfont = {0};
        if (ParseHUDFont(js_hudfont, &hudfont))
        {
            array_push(hudfonts, hudfont);
        }
    }

    json_t *js_statusbars = JS_GetObject(data, "statusbars");
    json_t *js_statusbar = NULL;

    JS_ArrayForEach(js_statusbar, js_statusbars)
    {
        statusbar_t statusbar = {0};
        if (ParseStatusBar(js_statusbar, &statusbar))
        {
            array_push(out->statusbars, statusbar);
        }
    }

    JS_Close("SBARDEF");

    if (!load_defaults)
    {
        return out;
    }

    json = JS_Open("SBHUDDEF", "hud", (version_t){1, 0, 0});
    if (json == NULL)
    {
        free(out);
        return NULL;
    }

    data = JS_GetObject(json, "data");

    js_hudfonts = JS_GetObject(data, "hudfonts");
    js_hudfont = NULL;

    JS_ArrayForEach(js_hudfont, js_hudfonts)
    {
        hudfont_t hudfont = {0};
        if (ParseHUDFont(js_hudfont, &hudfont))
        {
            array_push(hudfonts, hudfont);
        }
    }

    statusbar_t *statusbar;
    array_foreach(statusbar, out->statusbars)
    {
        json_t *js_widgets = JS_GetObject(data, "widgets");
        json_t *js_widget = NULL;

        JS_ArrayForEach(js_widget, js_widgets)
        {
            sbarelem_t elem = {0};
            if (ParseSbarElem(js_widget, &elem))
            {
                elem.y_pos += (statusbar->height - SCREENHEIGHT);
                array_push(statusbar->children, elem);
            }
        }
    }

    JS_Close("SBHUDDEF");

    return out;
}
