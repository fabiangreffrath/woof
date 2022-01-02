//
// Copyright(C) 2021 Roman Fomin
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
//      FluidSynth backend

#if defined(HAVE_FLUIDSYNTH)

#ifndef __I_FLMUSIC__
#define __I_FLMUSIC__

void *I_FL_RegisterSong(void *data, int len);

extern void I_FL_InitMusicBackend(void);

#endif

#endif
