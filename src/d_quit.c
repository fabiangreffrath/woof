//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2022 Fabian Greffrath
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
//    Exit functions.
//


#include <SDL3/SDL.h>

#include "doomstat.h"
#include "g_game.h"
#include "m_array.h"
#include "m_config.h"
#include "w_wad.h"

//
// I_Quit
//

void I_Quit(void)
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    SDL_Quit();

    W_Close();
}

void I_QuitFirst(void)
{
    if (demorecording)
    {
        G_CheckDemoStatus();
    }
}

void I_QuitLast(void)
{
    M_SaveDefaults();
}
