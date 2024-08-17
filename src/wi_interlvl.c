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

#include <stdlib.h>
#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"

#include "cjson/cJSON.h"

static boolean ParseCondition(cJSON *json, interlevelcond_t *out)
{
    cJSON *condition = cJSON_GetObjectItemCaseSensitive(json, "condition");
    if (!cJSON_IsNumber(condition))
    {
        return false;
    }
    out->condition = condition->valueint;

    cJSON *param = cJSON_GetObjectItemCaseSensitive(json, "param");
    if (!cJSON_IsNumber(param))
    {
        return false;
    }
    out->param = param->valueint;

    return true;
}

static boolean ParseFrame(cJSON *json, interlevelframe_t *out)
{
    cJSON *image_lump = cJSON_GetObjectItemCaseSensitive(json, "image");
    if (!cJSON_IsString(image_lump))
    {
        return false;
    }
    out->image_lump = M_StringDuplicate(image_lump->valuestring);

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsNumber(type))
    {
        return false;
    }
    out->type = type->valueint;

    cJSON *duration = cJSON_GetObjectItemCaseSensitive(json, "duration");
    if (!cJSON_IsNumber(duration))
    {
        return false;
    }
    out->duration = duration->valueint;

    cJSON *maxduration = cJSON_GetObjectItemCaseSensitive(json, "maxduration");
    if (!cJSON_IsNumber(maxduration))
    {
        return false;
    }
    out->maxduration = maxduration->valueint;

    return true;
}

static boolean ParseAnim(cJSON *json, interlevelanim_t *out)
{
    cJSON *js_frames = cJSON_GetObjectItemCaseSensitive(json, "frames");
    cJSON *js_frame = NULL;
    interlevelframe_t *frames = NULL;

    cJSON_ArrayForEach(js_frame, js_frames)
    {
        interlevelframe_t frame = {0};
        if (ParseFrame(js_frame, &frame))
        {
            array_push(frames, frame);
        }
    }
    out->frames = frames;

    cJSON *x_pos = cJSON_GetObjectItemCaseSensitive(json, "x");
    cJSON *y_pos = cJSON_GetObjectItemCaseSensitive(json, "y");
    if (!cJSON_IsNumber(x_pos) || !cJSON_IsNumber(y_pos))
    {
        return false;
    }
    out->x_pos = x_pos->valueint;
    out->y_pos = y_pos->valueint;

    cJSON *js_conditions = cJSON_GetObjectItemCaseSensitive(json, "conditions");
    cJSON *js_condition = NULL;
    interlevelcond_t *conditions = NULL;

    cJSON_ArrayForEach(js_condition, js_conditions)
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

static void ParseLevelLayer(cJSON *json, interlevellayer_t *out)
{
    cJSON *js_anims = cJSON_GetObjectItemCaseSensitive(json, "anims");
    cJSON *js_anim = NULL;
    interlevelanim_t *anims = NULL;

    cJSON_ArrayForEach(js_anim, js_anims)
    {
        interlevelanim_t anim = {0};
        if (ParseAnim(js_anim, &anim))
        {
            array_push(anims, anim);
        }
    }
    out->anims = anims;

    cJSON *js_conditions = cJSON_GetObjectItemCaseSensitive(json, "conditions");
    cJSON *js_condition = NULL;
    interlevelcond_t *conditions = NULL;

    cJSON_ArrayForEach(js_condition, js_conditions)
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
    interlevel_t *out = calloc(1, sizeof(*out));

    cJSON *json = cJSON_Parse(W_CacheLumpName(lumpname, PU_CACHE));
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            I_Printf(VB_ERROR, "Error before: %s\n", error_ptr);
        }
        free(out);
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data))
    {
        free(out);
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *music = cJSON_GetObjectItemCaseSensitive(data, "music");
    cJSON *backgroundimage = cJSON_GetObjectItemCaseSensitive(data, "backgroundimage");

    if (!cJSON_IsString(music) || !cJSON_IsString(backgroundimage))
    {
        free(out);
        cJSON_Delete(json);
        return NULL;
    }

    out->music_lump = M_StringDuplicate(music->valuestring);
    out->background_lump = M_StringDuplicate(backgroundimage->valuestring);

    cJSON *js_layers = cJSON_GetObjectItemCaseSensitive(data, "layers");
    cJSON *js_layer = NULL;

    if (!cJSON_IsNull(js_layers))
    {
        interlevellayer_t *anim_layers = NULL;

        cJSON_ArrayForEach(js_layer, js_layers)
        {
            interlevellayer_t anim_layer = {0};
            ParseLevelLayer(js_layer, &anim_layer);
            array_push(anim_layers, anim_layer);
        }
        out->anim_layers = anim_layers;
    }

    cJSON_Delete(json);
    return out;
}

void WI_FreeInterLevel(interlevel_t *in)
{
    free(in->music_lump);
    free(in->background_lump);

    for (int i = 0; i < array_size(in->anim_layers); ++i)
    {
        for (int j = 0; j < array_size(in->anim_layers[i].anims); ++j)
        {
            for (int k = 0; k < array_size(in->anim_layers[i].anims[j].frames); ++k)
            {
                free(in->anim_layers[i].anims[j].frames[k].image_lump);
            }
            array_free(in->anim_layers[i].anims[j].frames);
            array_free(in->anim_layers[i].anims[j].conditions);
        }
        array_free(in->anim_layers[i].anims);
        array_free(in->anim_layers[i].conditions);
    }
    array_free(in->anim_layers);
}
