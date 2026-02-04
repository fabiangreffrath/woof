//
// Copyright(C) 2026 Roman Fomin
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

extern void DSDH_StatesInit(void);
extern void DSDH_MobjInfoInit(void);
extern void DSDH_SoundsInit(void);
extern void DSDH_SpritesInit(void);

void DSDH_Init(void)
{
    DSDH_StatesInit();
    DSDH_MobjInfoInit();
    DSDH_SoundsInit();
    DSDH_SpritesInit();
}
