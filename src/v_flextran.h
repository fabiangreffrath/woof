//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//

#include "doomtype.h"

// haleyjd: DOSDoom-style translucency lookup tables

extern unsigned int Col2RGB8[65][256];
extern unsigned int *Col2RGB8_LessPrecision[65];
extern byte RGB32k[32][32][32];

// V_InitFlexTranTable
// Initializes the tables used in Flex translucency calculations
void V_InitFlexTranTable(void);
