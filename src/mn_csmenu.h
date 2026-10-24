//
//  Copyright (C) 2026 Roman Fomin
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

#ifndef MN_CSMENU_H
#define MN_CSMENU_H

#include "mn_container.h"

extern mn_menu_t csmenu_menu;

extern boolean csmenu_skill;

typedef struct csmenu_s
{
    boolean fastparm;
    boolean respawnparm;
    boolean nomonsters;
    boolean coopspawns;
    boolean pistolstart;
    boolean halfplayerdamage;
    boolean doubleammo;
    boolean aggromonsters;
    int helperdogs;
} csmenu_t;

extern csmenu_t csmenu;

void MN_CSMenuOpen(void);

void MN_CSmenuSelectSkillLevel(void);
void MN_CSMenuStartGame(void);

void MN_BindCSMenuVariables(void);

#endif
