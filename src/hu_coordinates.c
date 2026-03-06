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
#include "st_sbardef.h"
#include "st_widgets.h"
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
    const double mx = FixedToDouble(mo->momx);
    const double my = FixedToDouble(mo->momy);

    return sqrt(mx * mx + my * my);
}

static double CalcDistance(const mobj_t *mo)
{
    const double dx = FixedToDouble(mo->x - mo->oldx);
    const double dy = FixedToDouble(mo->y - mo->oldy);

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

static void BuildString(sbe_widget_t *widget, char *buf, int len,
                        int pos)
{
    if (WIDGET_WIDTH > pos)
    {
        M_snprintf(buf + pos, len - pos, "%-*s", WIDGET_WIDTH - pos, "");
    }

    ST_AddLine(widget, buf);
}

static void FixedToString(sbe_widget_t *widget, const char *label,
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

    BuildString(widget, buf, len, pos);
}

static void AngleToString(sbe_widget_t *widget, const char *label,
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

    BuildString(widget, buf, len, pos);
}

static void MagnitudeToString(sbe_widget_t *widget, const char *label,
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

    BuildString(widget, buf, len, pos);
}

static void ComponentToString(sbe_widget_t *widget, const char *label,
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

    BuildString(widget, buf, len, pos);
}

void HU_BuildCoordinatesEx(sbe_widget_t *widget, const mobj_t *mo)
{
    int pos;
    double magnitude;
    crange_idx_e color;

    widget->font = widget->default_font;

    #define LINE_SIZE 60

    // Coordinates.

    static char line1[LINE_SIZE];
    pos = M_snprintf(line1, sizeof(line1), GREEN_S);
    FixedToString(widget, "X", mo->x, line1, sizeof(line1), pos);
    static char line2[LINE_SIZE];
    pos = M_snprintf(line2, sizeof(line2), GREEN_S);
    FixedToString(widget, "Y", mo->y, line2, sizeof(line2), pos);
    static char line3[LINE_SIZE];
    pos = M_snprintf(line3, sizeof(line3), GREEN_S);
    FixedToString(widget, "Z", mo->z, line3, sizeof(line3), pos);
    static char line4[LINE_SIZE];
    pos = M_snprintf(line4, sizeof(line4), GREEN_S);
    AngleToString(widget, "A", mo->angle, line4, sizeof(line4), pos);
    ST_AddLine(widget, " ");

    // "Momentum" per tic.

    magnitude = CalcMomentum(mo);
    color = BLOCK_COLOR(magnitude, THRESH_M1, THRESH_M2, THRESH_M3);
    static char line5[LINE_SIZE];
    pos = M_snprintf(line5, sizeof(line5), "\x1b%c", '0' + color);
    MagnitudeToString(widget, "M", magnitude, line5, sizeof(line5), pos);
    static char line6[LINE_SIZE];
    pos = M_snprintf(line6, sizeof(line6), "\x1b%c", '0' + color);
    ComponentToString(widget, "X", mo->momx, line6, sizeof(line6), pos);
    static char line7[LINE_SIZE];
    pos = M_snprintf(line7, sizeof(line7), "\x1b%c", '0' + color);
    ComponentToString(widget, "Y", mo->momy, line7, sizeof(line7), pos);
    ST_AddLine(widget, " ");

    // Distance per tic.

    magnitude = CalcDistance(mo);
    color = BLOCK_COLOR(magnitude, THRESH_D1, THRESH_D2, THRESH_D3);
    static char line8[LINE_SIZE];
    pos = M_snprintf(line8, sizeof(line8), "\x1b%c", '0' + color);
    MagnitudeToString(widget, "D", magnitude, line8, sizeof(line8), pos);
    static char line9[LINE_SIZE];
    pos = M_snprintf(line9, sizeof(line9), "\x1b%c", '0' + color);
    ComponentToString(widget, "X", mo->x - mo->oldx, line9, sizeof(line9), pos);
    static char line10[LINE_SIZE];
    pos = M_snprintf(line10, sizeof(line10), "\x1b%c", '0' + color);
    ComponentToString(widget, "Y", mo->y - mo->oldy, line10, sizeof(line10), pos);
}
