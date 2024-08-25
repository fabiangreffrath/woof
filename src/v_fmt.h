//
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2019 Simon Judd
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

#ifndef V_FMT_H
#define V_FMT_H

#include "doomtype.h"
#include "z_zone.h"
#include "w_wad.h"

struct patch_s *V_LinearToTransPatch(const byte *data, int width, int height,
                                     int *output_size, int color_key,
                                     pu_tag tag, void **user);

struct patch_s *V_CachePatchNum(int lump, pu_tag tag);

inline static struct patch_s *V_CachePatchName(const char *name, pu_tag tag)
{
    return V_CachePatchNum(W_GetNumForName(name), tag);
}

void *V_CacheFlatNum(int lump, pu_tag tag);

int V_LumpSize(int lump);

#endif
