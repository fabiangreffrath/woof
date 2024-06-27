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

#ifndef __HU_COORDINATES__
#define __HU_COORDINATES__

#include "doomtype.h"
#include "hu_lib.h"

struct hu_multiline_s;
struct mobj_s;

void HU_BuildCoordinatesEx(struct hu_multiline_s *const w_coord,
                           const struct mobj_s *mo, char *buf, int len);

#endif
