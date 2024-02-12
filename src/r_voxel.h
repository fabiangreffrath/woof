//
// Copyright(C)      2023 Andrew Apted
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

#ifndef __R_VOXEL__
#define __R_VOXEL__

#include "doomtype.h"

void VX_Init (void);

void VX_ClearVoxels (void);

void VX_NearbySprites (void);

struct mobj_s;
boolean VX_ProjectVoxel (struct mobj_s * thing);

struct vissprite_s;
void VX_DrawVoxel (struct vissprite_s * vis);

extern const char ** vxfiles;

extern boolean voxels_rendering, default_voxels_rendering;

void VX_IncreaseMaxDist (void);

void VX_DecreaseMaxDist (void);

void VX_ResetMaxDist (void);

#endif  /* __R_VOXEL__ */
