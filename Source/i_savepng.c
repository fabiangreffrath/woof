//
//  Copyright(C) 2020 Fabian Greffrath
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Dynamically load SDL2_Image for PNG screenshots.
//

#include "config.h"

#include <stdlib.h>

#ifdef dl_FOUND
#include <dlfcn.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "i_savepng.h"

savepng_t SavePNG = NULL;

static const char *sdl2_image_libs[] = {
#ifdef dl_FOUND
	"libSDL2_image-2.0.so.0",
	"libSDL2_image-2.0.so",
	"libSDL2_image.so",
#elif _WIN32
	"SDL2_image.dll",
#endif
};

void I_InitSavePNG (void)
{
	int i;

#ifndef _WIN32
	void *sdl2_image_lib = NULL;
	void *savepng_func = NULL;
#else
	HMODULE sdl2_image_lib = NULL;
	FARPROC savepng_func = NULL;
#endif

	for (i = 0; i < sizeof(sdl2_image_libs) / sizeof(*sdl2_image_libs); i++)
	{
#ifdef dl_FOUND
		sdl2_image_lib = dlopen(sdl2_image_libs[i], RTLD_LAZY);
#elif _WIN32
		sdl2_image_lib = LoadLibrary(TEXT(sdl2_image_libs[i]));
#endif
		if (sdl2_image_lib != NULL)
		{
			break;
		}
	}

	if (sdl2_image_lib != NULL)
	{
#ifdef dl_FOUND
		savepng_func = dlsym(sdl2_image_lib, "IMG_SavePNG");
#elif _WIN32
		savepng_func = GetProcAddress(sdl2_image_lib, "IMG_SavePNG");
#endif
	}

	SavePNG = (savepng_t) savepng_func;
}
