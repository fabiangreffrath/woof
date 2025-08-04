//
// Copyright(C) 2025 Roman Fomin
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

#ifndef MEM_REGION_H
#define MEM_REGION_H

#include <stddef.h>

#include "doomtype.h"

void *I_ReserveRegion(size_t size);
boolean I_ReleaseRegion(void *ptr, size_t size);
boolean I_CommitRegion(void *ptr, size_t size);

#endif
