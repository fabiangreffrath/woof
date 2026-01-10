//
// Copyright(C) 2026 Roman Fomin
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

#include "am_map.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_json.h"

static mline_t *ParseLines(json_t *json)
{
    mline_t *lines = NULL;

    json_t *js_lines = JS_GetObject(json, "lines");
    json_t *js_line;
    JS_ArrayForEach(js_line, js_lines)
    {
        mline_t line = {0};
        line.a.x = JS_GetNumberValue(js_line, "x1") * FRACUNIT;
        line.a.y = JS_GetNumberValue(js_line, "y1") * FRACUNIT;
        line.b.x = JS_GetNumberValue(js_line, "x2") * FRACUNIT;
        line.b.y = JS_GetNumberValue(js_line, "y2") * FRACUNIT;
        array_push(lines, line);
    }

    return lines;
}

amdef_t *AM_ParseAmDef(void)
{
    json_t *json = JS_Open("AMAPDEF", "automap", (version_t){1, 0, 0});
    if (!json)
    {
        return NULL;
    }

    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_ERROR, "AMAPDEF: no data");
        JS_Close("AMAPDEF");
        return NULL;
    }

    amdef_t *out = calloc(1, sizeof(*out));

    json_t *player = JS_GetObject(data, "player");
    if (JS_IsObject(player))
    {
        out->player = ParseLines(player);
    }
    json_t *player_cheat = JS_GetObject(data, "player_cheat");
    if (JS_IsObject(player_cheat))
    {
        out->player_cheat = ParseLines(player_cheat);
    }
    json_t *thing = JS_GetObject(data, "thing");
    if (JS_IsObject(thing))
    {
        out->thing = ParseLines(thing);
    }
    json_t *key = JS_GetObject(data, "key");
    if (JS_IsObject(key))
    {
        out->key = ParseLines(key);
    }

    JS_Close("AMAPDEF");
    return out;
}
