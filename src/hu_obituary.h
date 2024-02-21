//
//  Copyright (C) 2023 Fabian Greffrath
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
// DESCRIPTION:  Obituaries
//
//-----------------------------------------------------------------------------

#ifndef __HU_OBITUARY_H__
#define __HU_OBITUARY_H__

struct mobj_s;

typedef enum
{
    MOD_None,
    MOD_Slime,
    MOD_Lava,
    MOD_Crush,
    MOD_Telefrag,
    MOD_Melee,
} method_t;

void HU_InitObituaries(void);
void HU_Obituary(struct mobj_s *target, struct mobj_s *source, method_t mod);

extern int show_obituary_messages;
extern int hudcolor_obituary;

#endif
