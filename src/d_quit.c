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

#include <errno.h>

#include "SDL.h"

#include "doomstat.h"
#include "i_system.h"
#include "m_misc.h"
#include "g_game.h"
#include "m_io.h"
#include "w_wad.h"
#include "i_glob.h"

//
// I_Quit
//

void I_Quit(void)
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    SDL_Quit();
}

void I_QuitFirst(void)
{
    if (demorecording)
    {
        G_CheckDemoStatus();
    }
}

extern char **tempdirs;

void I_QuitLast(void)
{
    int i;

    M_SaveDefaults();

    if (!tempdirs)
    {
        return;
    }

    W_CloseFileDescriptors();

    for (i = 0; tempdirs[i]; ++i)
    {
        glob_t *glob;
        const char *filename;

        glob = I_StartMultiGlob(tempdirs[i], GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                                "*.*", NULL);
        for (;;)
        {
            filename = I_NextGlob(glob);
            if (filename == NULL)
            {
                break;
            }

            if (M_remove(filename))
            {
                printf("Failed to delete temporary file: %s (%s)\n",
                        filename, strerror(errno));
            }
        }

        I_EndGlob(glob);

        if (M_rmdir(tempdirs[i]))
        {
            printf("Failed to delete temporary directory: %s (%s)\n",
                    tempdirs[i], strerror(errno));
        }
    }
}
