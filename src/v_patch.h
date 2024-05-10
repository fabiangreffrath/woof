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

#include "doomtype.h"
#include "z_zone.h"
#include "w_wad.h"

#define NO_COLOR_KEY (-1)

struct patch_s *V_LinearToTransPatch(const byte *data, int width, int height,
                                     int color_key, pu_tag tag, void **user);

struct patch_s *V_CacheLumpNum(int lump, pu_tag tag);

#define V_CacheLumpName(name, tag) V_CacheLumpNum(W_GetNumForName(name), (tag))
