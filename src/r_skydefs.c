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

#include "r_skydefs.h"

#include "doomdef.h"
#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_json.h"
#include "m_misc.h"
#include "w_wad.h"

static boolean ParseFire(json_t *json, fire_t *out)
{
    json_t *updatetime = JS_GetObject(json, "updatetime");
    if (!JS_IsNumber(updatetime))
    {
        return false;
    }
    out->updatetime = JS_GetNumber(updatetime) * TICRATE;

    json_t *palette = JS_GetObject(json, "palette");
    if (!JS_IsArray(palette))
    {
        return false;
    }
    int size = JS_GetArraySize(palette);
    for (int i = 0; i < size; ++i)
    {
        json_t *color = JS_GetArrayItem(palette, i);
        array_push(out->palette, JS_GetInteger(color));
    }

    return true;
}

static boolean ParseSkyTex(json_t *json, skytex_t *out)
{
    json_t *name = JS_GetObject(json, "name");
    if (!JS_IsString(name))
    {
        return false;
    }
    out->name = M_StringDuplicate(JS_GetString(name));

    json_t *mid = JS_GetObject(json, "mid");
    json_t *scrollx = JS_GetObject(json, "scrollx");
    json_t *scrolly = JS_GetObject(json, "scrolly");
    json_t *scalex = JS_GetObject(json, "scalex");
    json_t *scaley = JS_GetObject(json, "scaley");
    if (!JS_IsNumber(mid)
        || !JS_IsNumber(scrollx) || !JS_IsNumber(scrolly)
        || !JS_IsNumber(scalex)  || !JS_IsNumber(scaley))
    {
        return false;
    }
    out->mid = JS_GetNumber(mid);
    const double ticratescale = 1.0 / TICRATE;
    out->scrollx = (JS_GetNumber(scrollx) * ticratescale) * FRACUNIT;
    out->scrolly = (JS_GetNumber(scrolly) * ticratescale) * FRACUNIT;
    out->scalex = JS_GetNumber(scalex) * FRACUNIT;
    double value = JS_GetNumber(scaley);
    if (value)
    {
        out->scaley = (1.0 / value) * FRACUNIT;
    }
    else
    {
        out->scaley = FRACUNIT;
    }

    return true;
}

static boolean ParseSky(json_t *json, sky_t *out)
{
    json_t *type = JS_GetObject(json, "type");
    if (!JS_IsNumber(type))
    {
        return false;
    }
    out->type = JS_GetInteger(type);

    skytex_t background = {0};
    if (!ParseSkyTex(json, &background))
    {
        return false;
    }
    out->skytex = background;

    json_t *js_fire = JS_GetObject(json, "fire");
    fire_t fire = {0};
    if (!JS_IsNull(js_fire))
    {
        ParseFire(js_fire, &fire);
    }
    out->fire = fire;

    json_t *js_foreground = JS_GetObject(json, "foregroundtex");
    skytex_t foreground = {0};
    if (!JS_IsNull(js_foreground))
    {
        ParseSkyTex(js_foreground, &foreground);
    }
    out->foreground = foreground;

    return true;
}

static boolean ParseFlatMap(json_t *json, flatmap_t *out)
{
    json_t *flat = JS_GetObject(json, "flat");
    out->flat = M_StringDuplicate(JS_GetString(flat));
    json_t *sky = JS_GetObject(json, "sky");
    out->sky = M_StringDuplicate(JS_GetString(sky));
    return true;
}

skydefs_t *R_ParseSkyDefs(void)
{
    int lumpnum = W_CheckNumForName("SKYDEFS");
    if (lumpnum < 0)
    {
        return NULL;
    }

    json_t *json = JS_Open(W_CacheLumpNum(lumpnum, PU_CACHE));
    if (json == NULL)
    {
        I_Printf(VB_ERROR, "JSON: Error parsing SKYDEFS");
        JS_Close(json);
        return NULL;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data))
    {
        JS_Close(json);
        return NULL;
    }

    skydefs_t *out = calloc(1, sizeof(*out));

    json_t *js_skies = JS_GetObject(data, "skies");
    json_t *js_sky = NULL;
    JS_ArrayForEach(js_sky, js_skies)
    {
        sky_t sky = {0};
        if (ParseSky(js_sky, &sky))
        {
            array_push(out->skies, sky);
        }
    }

    json_t *js_flatmapping = JS_GetObject(data, "flatmapping");
    json_t *js_flatmap = NULL;
    JS_ArrayForEach(js_flatmap, js_flatmapping)
    {
        flatmap_t flatmap = {0};
        if (ParseFlatMap(js_flatmap, &flatmap))
        {
            array_push(out->flatmapping, flatmap);
        }
    }

    JS_Close(json);
    return out;
}
