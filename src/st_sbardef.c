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
#include "r_defs.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

static boolean ParseSbarCondition(json_t *json, sbarcondition_t *out)
{
    json_t *condition = JS_GetObject(json, "condition");
    json_t *param = JS_GetObject(json, "param");
    if (!JS_IsNumber(condition) || !JS_IsNumber(param))
    {
        return false;
    }
    out->condition = JS_GetInteger(condition);
    out->param = JS_GetInteger(param);

    return true;
}

static boolean ParseSbarFrame(json_t *json, sbarframe_t *out)
{
    json_t *lump = JS_GetObject(json, "lump");
    if (!JS_IsString(lump))
    {
        return false;
    }
    out->patch_name = JS_GetString(lump);

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
    out->elemtype = type;

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

    out->tranmap = JS_GetStringValue(json, "tranmap");
    out->translation = JS_GetStringValue(json, "translation");

    out->cr = out->translation ? V_CRByName(out->translation) : CR_NONE;
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
                json_t *patch = JS_GetObject(json, "patch");
                if (!JS_IsString(patch))
                {
                    return false;
                }
                out->patch_name = JS_GetString(patch);
            }
            break;

        case sbe_animation:
            {
                json_t *js_frames = JS_GetObject(json, "frames");
                json_t *js_frame = NULL;
                JS_ArrayForEach(js_frame, js_frames)
                {
                    sbarframe_t frame = {0};
                    if (ParseSbarFrame(js_frame, &frame))
                    {
                        array_push(out->frames, frame);
                    }
                }
            }
            break;

        case sbe_number:
        case sbe_percent:
            {
                json_t *font = JS_GetObject(json, "font");
                if (!JS_IsString(font))
                {
                    return false;
                }
                out->font_name = JS_GetString(font);

                json_t *type = JS_GetObject(json, "type");
                json_t *param = JS_GetObject(json, "param");
                json_t *maxlength = JS_GetObject(json, "maxlength");
                if (!JS_IsNumber(type) || !JS_IsNumber(param)
                    || !JS_IsNumber(maxlength))
                {
                    return false;
                }
                out->numtype = JS_GetInteger(type);
                out->numparam = JS_GetInteger(param);
                out->maxlength = JS_GetInteger(maxlength);
            }
            break;

        case sbe_widget:
             {
                json_t *font = JS_GetObject(json, "font");
                if (!JS_IsString(font))
                {
                    return false;
                }
                out->font_name = JS_GetString(font);

                json_t *type = JS_GetObject(json, "type");
                if (!JS_IsNumber(type))
                {
                    return false;
                }
                out->widgettype = JS_GetInteger(type);
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
    [sbe_widget] = "widget"
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
    json_t *stem = JS_GetObject(json, "stem");
    if (!JS_IsString(name) || !JS_IsString(stem))
    {
        return false;
    }
    out->name = JS_GetString(name);
    out->stem = JS_GetString(stem);

    json_t *type = JS_GetObject(json, "type");
    if (!JS_IsNumber(type))
    {
        return false;
    }
    out->type = JS_GetInteger(type);

    char lump[9] = {0};
    int found;
    int maxwidth = 0;

    for (int num = 0; num < 10; ++num)
    {
        M_snprintf(lump, sizeof(lump), "%sNUM%d", out->stem, num);
        found = W_CheckNumForName(lump);
        if (found < 0)
        {
            I_Printf(VB_ERROR, "SBARDEF: patch \"%s\" not found", lump);
            return false;
        }
        out->numbers[num] = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->numbers[num]->width));
    }

    M_snprintf(lump, sizeof(lump), "%sMINUS", out->stem);
    found = W_CheckNumForName(lump);
    if (found >= 0)
    {
        out->minus = V_CachePatchNum(found, PU_STATIC);
        maxwidth = MAX(maxwidth, SHORT(out->minus->width));
    }

    M_snprintf(lump, sizeof(lump), "%sPRCNT", out->stem);
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

static boolean ParseHUDFont(json_t *json, hudfont_t *out)
{
    json_t *name = JS_GetObject(json, "name");
    json_t *stem = JS_GetObject(json, "stem");
    if (!JS_IsString(name) || !JS_IsString(stem))
    {
        return false;
    }
    out->name = JS_GetString(name);
    out->stem = JS_GetString(stem);

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

    return true;
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

    out->fillflat = JS_GetStringValue(json, "fillflat");

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
    json_t *json = JS_Open("SBARDEF", "statusbar", (version_t){1, 0, 0});
    if (json == NULL)
    {
        return NULL;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_ERROR, "SBARDEF: no data");
        JS_Close(json);
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
            array_push(out->numberfonts, numberfont);
        }
    }

    json_t *js_hudfonts = JS_GetObject(data, "hudfonts");
    json_t *js_hudfont = NULL;

    JS_ArrayForEach(js_hudfont, js_hudfonts)
    {
        hudfont_t hudfont = {0};
        if (ParseHUDFont(js_hudfont, &hudfont))
        {
            array_push(out->hudfonts, hudfont);
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

    return out;
}
