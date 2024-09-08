
#include "r_skydefs.h"

#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"

#include "cjson/cJSON.h"

static boolean ParseFire(cJSON *json, fire_t *out)
{
    cJSON *updatetime = cJSON_GetObjectItemCaseSensitive(json, "updatetime");
    if (!cJSON_IsNumber(updatetime))
    {
        return false;
    }
    out->updatetime = updatetime->valuedouble;

    cJSON *palette = cJSON_GetObjectItemCaseSensitive(json, "palette");
    if (!cJSON_IsArray(palette))
    {
        return false;
    }
    int size = cJSON_GetArraySize(palette);
    for (int i = 0; i < size; ++i)
    {
        cJSON *color = cJSON_GetArrayItem(palette, i);
        array_push(out->palette, color->valueint);
    }

    return true;
}

static boolean ParseSkyTex(cJSON *json, skytex_t *out)
{
    cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (!cJSON_IsString(name))
    {
        return false;
    }
    out->name = M_StringDuplicate(name->valuestring);

    cJSON *mid = cJSON_GetObjectItemCaseSensitive(json, "mid");
    cJSON *scrollx = cJSON_GetObjectItemCaseSensitive(json, "scrollx");
    cJSON *scrolly = cJSON_GetObjectItemCaseSensitive(json, "scrolly");
    cJSON *scalex = cJSON_GetObjectItemCaseSensitive(json, "scalex");
    cJSON *scaley = cJSON_GetObjectItemCaseSensitive(json, "scaley");
    if (!cJSON_IsNumber(mid) 
        || !cJSON_IsNumber(scrollx) || !cJSON_IsNumber(scrolly)
        || !cJSON_IsNumber(scalex)  || !cJSON_IsNumber(scaley))
    {
        return false;
    }
    out->mid = mid->valuedouble;
    out->scrollx = scrollx->valuedouble;
    out->scrolly = scrolly->valuedouble;
    out->scalex = scalex->valuedouble;
    out->scaley = scaley->valuedouble;

    return true;
}

static boolean ParseSky(cJSON *json, sky_t *out)
{
    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsNumber(type))
    {
        return false;
    }
    out->type = type->valueint;

    skytex_t background = {0};
    if (!ParseSkyTex(json, &background))
    {
        return false;
    }
    out->background = background;

    cJSON *js_fire = cJSON_GetObjectItemCaseSensitive(json, "fire");
    fire_t fire = {0};
    if (!ParseFire(js_fire, &fire))
    {
        return false;
    }
    out->fire = fire;

    cJSON *js_foreground = cJSON_GetObjectItemCaseSensitive(json, "foreground");
    skytex_t foreground = {0};
    ParseSkyTex(js_foreground, &foreground);
    out->foreground = foreground;

    return true;
}

static boolean ParseFlatMap(cJSON *json, flatmap_t *out)
{
    cJSON *flat = cJSON_GetObjectItemCaseSensitive(json, "flat");
    out->flat = M_StringDuplicate(flat->valuestring);
    cJSON *sky = cJSON_GetObjectItemCaseSensitive(json, "sky");
    out->sky = M_StringDuplicate(sky->valuestring);
    return true;
}

skydef_t *R_ParseSkyDefs(void)
{
    int lumpnum = W_CheckNumForName("SKYDEFS");
    if (lumpnum < 0)
    {
        return NULL;
    }

    cJSON *json = cJSON_Parse(W_CacheLumpNum(lumpnum, PU_CACHE));
    if (json == NULL)
    {
        I_Printf(VB_ERROR, "JSON: Error parsing SKYDEFS");
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data))
    {
        cJSON_Delete(json);
        return NULL;
    }

    skydef_t *out = calloc(1, sizeof(*out));

    cJSON *js_skies = cJSON_GetObjectItemCaseSensitive(data, "skies");
    cJSON *js_sky = NULL;
    cJSON_ArrayForEach(js_sky, js_skies)
    {
        sky_t sky = {0};
        if (ParseSky(js_sky, &sky))
        {
            array_push(out->skies, sky);
        }
    }

    cJSON *js_flatmapping = cJSON_GetObjectItemCaseSensitive(data, "flatmapping");
    cJSON *js_flatmap = NULL;
    cJSON_ArrayForEach(js_flatmap, js_flatmapping)
    {
        flatmap_t flatmap = {0};
        if (ParseFlatMap(js_flatmap, &flatmap))
        {
            array_push(out->flatmapping, flatmap);
        }
    }

    cJSON_Delete(json);
    return out;
}
