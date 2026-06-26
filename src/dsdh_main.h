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

#ifndef DSDH_MAIN_H
#define DSDH_MAIN_H

void DSDH_Init(void);

int DSDH_StateTranslate(int frame_number);
int DSDH_StatesGetNewIndex(void);

int DSDH_ThingTranslate(int thing_number);
int DSDH_MobjInfoGetNewIndex(void);

int DSDH_SoundTranslate(int sfx_number);
int DSDH_SoundsGetNewIndex(void);

int DSDH_SpriteTranslate(int sprite_number);
int DSDH_SpritesGetNewIndex(void);

#endif
