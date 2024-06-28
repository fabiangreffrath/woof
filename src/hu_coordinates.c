//
// Copyright(C) 2022 Ryan Krafnick
// Copyright(C) 2024 ceski
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
//
// DESCRIPTION:
//      Advanced Coordinates HUD Widget
//

#include <math.h>

#include "hu_coordinates.h"
#include "m_misc.h"
#include "p_mobj.h"
#include "v_video.h"

#define THRESH_M1 15.11
#define THRESH_M2 19.35
#define THRESH_M3 21.37

#define THRESH_D1 16.67
#define THRESH_D2 21.35
#define THRESH_D3 23.58

#define BLOCK_COLOR(x, a, b, c) \
    (x >= c ? CR_RED : x >= b ? CR_BLUE1 : x >= a ? CR_GREEN : CR_GRAY)

#define WIDGET_WIDTH 17

typedef struct
{
    boolean negative;
    int base;
    int frac;
} split_fixed_t;

typedef struct
{
    int base;
    int frac;
} split_angle_t;

static double CalcMomentum(const mobj_t *mo)
{
    const double mx = FIXED2DOUBLE(mo->momx);
    const double my = FIXED2DOUBLE(mo->momy);

    return sqrt(mx * mx + my * my);
}

static double CalcDistance(const mobj_t *mo)
{
    const double dx = FIXED2DOUBLE(mo->x - mo->oldx);
    const double dy = FIXED2DOUBLE(mo->y - mo->oldy);

    return sqrt(dx * dx + dy * dy);
}

static split_fixed_t SplitFixed(fixed_t x)
{
    split_fixed_t result;

    result.negative = x < 0;
    result.base = x >> FRACBITS;
    result.frac = x & FRACMASK;

    if (result.negative && result.frac)
    {
        result.base++;
        result.frac = FRACUNIT - result.frac;
    }

    return result;
}

static split_angle_t SplitAngle(angle_t x)
{
    split_angle_t result;

    result.base = x >> (FRACBITS + 8);
    result.frac = (x >> FRACBITS) & 0xFF;

    return result;
}

static void BuildString(hu_multiline_t *const w_coord, char *buf, int len,
                        int pos)
{
    if (WIDGET_WIDTH > pos)
    {
        M_snprintf(buf + pos, len - pos, "%-*s", WIDGET_WIDTH - pos, "");
    }

    HUlib_add_string_keep_space(w_coord, buf);
}

static void FixedToString(hu_multiline_t *const w_coord, const char *label,
                          fixed_t x, char *buf, int len, int pos)
{
    const split_fixed_t value = SplitFixed(x);

    if (value.frac)
    {
        const char *sign = (value.negative && !value.base) ? "-" : "";

        pos += M_snprintf(buf + pos, len - pos, "%s: %s%d.%05d", label, sign,
                          value.base, value.frac);
    }
    else
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: %d", label, value.base);
    }

    BuildString(w_coord, buf, len, pos);
}

static void AngleToString(hu_multiline_t *const w_coord, const char *label,
                          angle_t x, char *buf, int len, int pos)
{
    const split_angle_t value = SplitAngle(x);

    if (value.frac)
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: %d.%03d", label,
                          value.base, value.frac);
    }
    else
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: %d", label, value.base);
    }

    BuildString(w_coord, buf, len, pos);
}

static void MagnitudeToString(hu_multiline_t *const w_coord, const char *label,
                              double x, char *buf, int len, int pos)
{
    if (x)
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: %.3f", label, x);
    }
    else
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: 0", label);
    }

    BuildString(w_coord, buf, len, pos);
}

static void ComponentToString(hu_multiline_t *const w_coord, const char *label,
                              fixed_t x, char *buf, int len, int pos)
{
    const split_fixed_t value = SplitFixed(x);

    if (value.frac)
    {
        const char *sign = (value.negative && !value.base) ? "-" : "";

        pos += M_snprintf(buf + pos, len - pos, "%s: %s%d.%03d", label, sign,
                          value.base, 1000 * value.frac / FRACMASK);
    }
    else
    {
        pos += M_snprintf(buf + pos, len - pos, "%s: %d", label, value.base);
    }

    BuildString(w_coord, buf, len, pos);
}

void HU_BuildCoordinatesEx(hu_multiline_t *const w_coord, const mobj_t *mo,
                           char *buf, int len)
{
    int pos;
    double magnitude;
    crange_idx_e color;

    // Coordinates.

    pos = M_snprintf(buf, len, "\x1b%c", '0' + CR_GREEN);
    FixedToString(w_coord, "X", mo->x, buf, len, pos);
    FixedToString(w_coord, "Y", mo->y, buf, len, pos);
    FixedToString(w_coord, "Z", mo->z, buf, len, pos);
    AngleToString(w_coord, "A", mo->angle, buf, len, pos);
    HUlib_add_string_keep_space(w_coord, " ");

    // "Momentum" per tic.

    magnitude = CalcMomentum(mo);
    color = BLOCK_COLOR(magnitude, THRESH_M1, THRESH_M2, THRESH_M3);
    pos = M_snprintf(buf, len, "\x1b%c", '0' + color);
    MagnitudeToString(w_coord, "M", magnitude, buf, len, pos);
    ComponentToString(w_coord, "X", mo->momx, buf, len, pos);
    ComponentToString(w_coord, "Y", mo->momy, buf, len, pos);
    HUlib_add_string_keep_space(w_coord, " ");

    // Distance per tic.

    magnitude = CalcDistance(mo);
    color = BLOCK_COLOR(magnitude, THRESH_D1, THRESH_D2, THRESH_D3);
    pos = M_snprintf(buf, len, "\x1b%c", '0' + color);
    MagnitudeToString(w_coord, "D", magnitude, buf, len, pos);
    ComponentToString(w_coord, "X", mo->x - mo->oldx, buf, len, pos);
    ComponentToString(w_coord, "Y", mo->y - mo->oldy, buf, len, pos);
}
