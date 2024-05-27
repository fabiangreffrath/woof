//
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
//      Game Input Utility Functions
//

#include <math.h>

#include "g_game.h"

//
// Side Movement
//

static int RoundSide_Strict(double side)
{
    return lround(side * 0.5) * 2; // Even values only.
}

static int RoundSide_Full(double side)
{
    return lround(side);
}

int (*G_RoundSide)(double side);

void G_UpdateSideMove(void)
{
    if (strictmode || (netgame && !solonet))
    {
        G_RoundSide = RoundSide_Strict;
        sidemove = default_sidemove;
    }
    else
    {
        G_RoundSide = RoundSide_Full;
        sidemove = autostrafe50 ? forwardmove : default_sidemove;
    }
}
