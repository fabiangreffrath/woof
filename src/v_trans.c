//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Paul Haeberli
// Copyright(C) 2014-2023 Fabian Greffrath
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
//
// Color translation tables
//

#include <math.h>

#include "doomtype.h"
#include "i_video.h"
#include "v_trans.h"
#include "v_video.h"

/*
Date: Sun, 26 Oct 2014 10:36:12 -0700
From: paul haeberli <paulhaeberli@yahoo.com>
Subject: Re: colors and color conversions
To: Fabian Greffrath <fabian@greffrath.com>

Yes, this seems exactly like the solution I was looking for. I just
couldn't find code to do the HSV->RGB conversion. Speaking of the code,
would you allow me to use this code in my software? The Doom source code
is licensed under the GNU GPL, so this code yould have to be under a
compatible license.

    Yes. I'm happy to contribute this code to your project.  GNU GPL or anything
    compatible sounds fine.

Regarding the conversions, the procedure you sent me will leave grays
(r=g=b) untouched, no matter what I set as HUE, right? Is it possible,
then, to also use this routine to convert colors *to* gray?

    You can convert any color to an equivalent grey by setting the saturation
    to 0.0


    - Paul Haeberli
*/

#define CTOLERANCE 0.0001

typedef struct vect
{
    double x;
    double y;
    double z;
} vect;

static void hsv_to_rgb(vect *hsv, vect *rgb)
{
    double h, s, v;

    h = hsv->x;
    s = hsv->y;
    v = hsv->z;
    h *= 360.0;
    if (s < CTOLERANCE)
    {
        rgb->x = v;
        rgb->y = v;
        rgb->z = v;
    }
    else
    {
        int i;
        double f, p, q, t;

        if (h >= 360.0)
        {
            h -= 360.0;
        }
        h /= 60.0;
        i = (int)floor(h);
        f = h - i;
        p = v * (1.0 - s);
        q = v * (1.0 - (s * f));
        t = v * (1.0 - (s * (1.0 - f)));
        switch (i)
        {
            case 0:
                rgb->x = v;
                rgb->y = t;
                rgb->z = p;
                break;
            case 1:
                rgb->x = q;
                rgb->y = v;
                rgb->z = p;
                break;
            case 2:
                rgb->x = p;
                rgb->y = v;
                rgb->z = t;
                break;
            case 3:
                rgb->x = p;
                rgb->y = q;
                rgb->z = v;
                break;
            case 4:
                rgb->x = t;
                rgb->y = p;
                rgb->z = v;
                break;
            case 5:
                rgb->x = v;
                rgb->y = p;
                rgb->z = q;
                break;
        }
    }
}

static void rgb_to_hsv(vect *rgb, vect *hsv)
{
    double h, s, v;
    double cmax, cmin;
    double r, g, b;

    r = rgb->x;
    g = rgb->y;
    b = rgb->z;
    /* find the cmax and cmin of r g b */
    cmax = r;
    cmin = r;
    cmax = (g > cmax ? g : cmax);
    cmin = (g < cmin ? g : cmin);
    cmax = (b > cmax ? b : cmax);
    cmin = (b < cmin ? b : cmin);
    v = cmax; /* value */
    if (cmax > CTOLERANCE)
    {
        s = (cmax - cmin) / cmax;
    }
    else
    {
        s = 0.0;
    }
    if (s < CTOLERANCE)
    {
        h = 0.0;
    }
    else
    {
        double cdelta;
        double rc, gc, bc;

        cdelta = cmax - cmin;
        rc = (cmax - r) / cdelta;
        gc = (cmax - g) / cdelta;
        bc = (cmax - b) / cdelta;
        if (r == cmax)
        {
            h = bc - gc;
        }
        else if (g == cmax)
        {
            h = 2.0 + rc - bc;
        }
        else
        {
            h = 4.0 + gc - rc;
        }
        h = h * 60.0;
        if (h < 0.0)
        {
            h += 360.0;
        }
    }
    hsv->x = h / 360.0;
    hsv->y = s;
    hsv->z = v;
}

byte V_Colorize(byte *playpal, int cr, byte source)
{
    vect rgb, hsv;

    rgb.x = playpal[3 * source + 0] / 255.;
    rgb.y = playpal[3 * source + 1] / 255.;
    rgb.z = playpal[3 * source + 2] / 255.;

    rgb_to_hsv(&rgb, &hsv);

    if (cr == CR_BRIGHT)
    {
        hsv.z *= 1.4;
    }
    else if (cr == CR_GRAY || cr == CR_BLACK || cr == CR_WHITE)
    {
        hsv.y = 0.0;

        if (cr == CR_BLACK)
        {
            hsv.z = 0.5 * hsv.z;
        }
        else if (cr == CR_WHITE)
        {
            hsv.z = 1.0 - 0.5 * hsv.z;
        }
    }
    else
    {
        // [crispy] hack colors to full saturation
        hsv.y = 1.0;

        if (cr == CR_GREEN)
        {
            // hsv.x = 135./360.;
            hsv.x = (144.0 * hsv.z + 120.0 * (1.0 - hsv.z)) / 360.0;
        }
        else if (cr == CR_GOLD)
        {
            // hsv.x = 45./360.;
            // hsv.x = (50. * hsv.z + 30. * (1. - hsv.z))/360.;
            hsv.x = (7.0 + 53.0 * hsv.z) / 360.0;
            hsv.y = 1.0 - 0.4 * hsv.z;
            hsv.z = MIN(hsv.z, 0.2) + 0.8 * hsv.z;
        }
        else if (cr == CR_RED || cr == CR_BRICK)
        {
            hsv.x = 0.0;

            if (cr == CR_BRICK)
            {
                hsv.y = 0.5 * hsv.y;
            }
        }
        else if (cr == CR_BLUE1 || cr == CR_BLUE2)
        {
            hsv.x = 240.0 / 360.0;

            if (cr == CR_BLUE1)
            {
                hsv.y = 1.0 - 0.5 * hsv.z;
                hsv.z = MIN(hsv.z, 0.5) + 0.5 * hsv.z;
            }
        }
        else if (cr == CR_ORANGE || cr == CR_TAN || cr == CR_BROWN)
        {
            hsv.x = 30.0 / 360.0;

            if (cr == CR_TAN)
            {
                hsv.y = 0.5 * hsv.y;
            }
            else if (cr == CR_BROWN)
            {
                hsv.z = 0.5 * hsv.z;
            }
            else
            {
                hsv.y = (hsv.z < 0.6) ? 1.0 : (2.2 - 2.0 * hsv.z);
                hsv.z = MIN(hsv.z, 0.5) + 0.5 * hsv.z;
            }
        }
        else if (cr == CR_YELLOW)
        {
            hsv.x = 60.0 / 360.0;
            hsv.y = 1.0 - 0.85 * hsv.z;
            hsv.z = 1.0;
        }
        else if (cr == CR_PURPLE)
        {
            hsv.x = 300.0 / 360.0;
        }
    }

    hsv_to_rgb(&hsv, &rgb);

    rgb.x *= 255.0;
    rgb.y *= 255.0;
    rgb.z *= 255.0;

    return I_GetNearestColor(playpal, (int)rgb.x, (int)rgb.y, (int)rgb.z);
}
