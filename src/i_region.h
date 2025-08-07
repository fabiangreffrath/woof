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

// Wrapper for the virtual memory API, similar to the "region" crate in Rust.

#ifndef I_REGION_H
#define I_REGION_H

#include "doomtype.h"

void *I_ReserveRegion(size_t size);
boolean I_ReleaseRegion(void *ptr, size_t size);
boolean I_CommitRegion(void *ptr, size_t size);
boolean I_DecommitRegion(void *ptr, size_t size);

#endif
