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

#include "wi_interlvl.h"

#include "doomdef.h"
#include "doomtype.h"
#include "i_printf.h"
#include "w_wad.h"
#include "z_zone.h"

#define M_ARRAY_MALLOC(size) Z_Malloc((size), PU_LEVEL, NULL)
#define M_ARRAY_REALLOC(ptr, size) Z_Realloc((ptr), (size), PU_LEVEL, NULL)
#define M_ARRAY_FREE(ptr) Z_Free((ptr))
#include "m_array.h"

#include "m_json.h"

static boolean ParseCondition(json_t *json, interlevelcond_t *out)
{
    json_t *condition = JS_GetObject(json, "condition");
    if (!JS_IsNumber(condition))
    {
        return false;
    }
    out->condition = JS_GetInteger(condition);

    json_t *param = JS_GetObject(json, "param");
    if (!JS_IsNumber(param))
    {
        return false;
    }
    out->param = JS_GetInteger(param);

    return true;
}

static boolean ParseFrame(json_t *json, interlevelframe_t *out)
{
    json_t *image_lump = JS_GetObject(json, "image");
    if (!JS_IsString(image_lump))
    {
        return false;
    }
    out->image_lump = Z_StrDup(JS_GetString(image_lump), PU_LEVEL);

    json_t *type = JS_GetObject(json, "type");
    if (!JS_IsNumber(type))
    {
        return false;
    }
    out->type = JS_GetInteger(type);

    json_t *duration = JS_GetObject(json, "duration");
    if (!JS_IsNumber(duration))
    {
        return false;
    }
    out->duration = JS_GetNumber(duration) * TICRATE;

    json_t *maxduration = JS_GetObject(json, "maxduration");
    if (!JS_IsNumber(maxduration))
    {
        return false;
    }
    out->maxduration = JS_GetNumber(maxduration) * TICRATE;

    return true;
}

static boolean ParseAnim(json_t *json, interlevelanim_t *out)
{
    json_t *js_frames = JS_GetObject(json, "frames");
    json_t *js_frame = NULL;
    interlevelframe_t *frames = NULL;

    JS_ArrayForEach(js_frame, js_frames)
    {
        interlevelframe_t frame = {0};
        if (ParseFrame(js_frame, &frame))
        {
            array_push(frames, frame);
        }
    }
    out->frames = frames;

    json_t *x_pos = JS_GetObject(json, "x");
    json_t *y_pos = JS_GetObject(json, "y");
    if (!JS_IsNumber(x_pos) || !JS_IsNumber(y_pos))
    {
        return false;
    }
    out->x_pos = JS_GetInteger(x_pos);
    out->y_pos = JS_GetInteger(y_pos);

    json_t *js_conditions = JS_GetObject(json, "conditions");
    json_t *js_condition = NULL;
    interlevelcond_t *conditions = NULL;

    JS_ArrayForEach(js_condition, js_conditions)
    {
        interlevelcond_t condition = {0};
        if (ParseCondition(js_condition, &condition))
        {
            array_push(conditions, condition);
        }
    }
    out->conditions = conditions;

    return true;
}

static void ParseLevelLayer(json_t *json, interlevellayer_t *out)
{
    json_t *js_anims = JS_GetObject(json, "anims");
    json_t *js_anim = NULL;
    interlevelanim_t *anims = NULL;

    JS_ArrayForEach(js_anim, js_anims)
    {
        interlevelanim_t anim = {0};
        if (ParseAnim(js_anim, &anim))
        {
            array_push(anims, anim);
        }
    }
    out->anims = anims;

    json_t *js_conditions = JS_GetObject(json, "conditions");
    json_t *js_condition = NULL;
    interlevelcond_t *conditions = NULL;

    JS_ArrayForEach(js_condition, js_conditions)
    {
        interlevelcond_t condition = {0};
        if (ParseCondition(js_condition, &condition))
        {
            array_push(conditions, condition);
        }
    }
    out->conditions = conditions;
}

interlevel_t *WI_ParseInterlevel(const char *lumpname)
{
    json_t *json = JS_Open("interlevel", (version_t){1, 0, 0},
                           W_CacheLumpName(lumpname, PU_CACHE));
    if (json == NULL)
    {
        JS_Close(json);
        return NULL;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data))
    {
        JS_Close(json);
        return NULL;
    }

    json_t *music = JS_GetObject(data, "music");
    json_t *backgroundimage = JS_GetObject(data, "backgroundimage");

    if (!JS_IsString(music) || !JS_IsString(backgroundimage))
    {
        JS_Close(json);
        return NULL;
    }

    interlevel_t *out = Z_Calloc(1, sizeof(*out), PU_LEVEL, NULL);

    out->music_lump = Z_StrDup(JS_GetString(music), PU_LEVEL);
    out->background_lump = Z_StrDup(JS_GetString(backgroundimage), PU_LEVEL);

    json_t *js_layers = JS_GetObject(data, "layers");
    json_t *js_layer = NULL;
    interlevellayer_t *layers = NULL;

    JS_ArrayForEach(js_layer, js_layers)
    {
        interlevellayer_t layer = {0};
        ParseLevelLayer(js_layer, &layer);
        array_push(layers, layer);
    }
    out->layers = layers;

    JS_Close(json);
    return out;
}
