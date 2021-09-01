//
//  Copyright(C) 2021 Roman Fomin
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Windows native MIDI

#ifndef __I_WINMUSIC__
#define __I_WINMUSIC__

#include "doomtype.h"

boolean I_WIN_InitMusic(void);
void I_WIN_PlaySong(boolean looping);
void I_WIN_StopSong();
void I_WIN_SetMusicVolume(int volume);
void I_WIN_RegisterSong(void *data, int len);
void I_WIN_UnRegisterSong();

extern boolean win_midi_registered;

#endif
