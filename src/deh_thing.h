//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021 Roman Fomin
// Copyright(C) 2025 Guilherme Miranda
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
//
// Parses "Thing" sections in dehacked files
//

#ifndef DEH_THINS_H
#define DEH_THINS_H

extern int deh_set_blood_color;

extern void DEH_InitMobjInfo(void);

extern void DEH_MobjInfoEnsureCapacity(int limit);
extern int DEH_MobjInfoGetNewIndex(void);

#endif
