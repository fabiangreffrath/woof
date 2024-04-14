//
// Copyright(C) 2024 Roman Fomin
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

#ifndef MIDIOUT_H
#define MIDIOUT_H

#include "doomtype.h"

void MIDI_SendShortMsg(const byte *message, unsigned int length);

void MIDI_SendLongMsg(const byte *message, unsigned int length);

int MIDI_CountDevices(void);

const char *MIDI_GetDeviceName(int device);

boolean MIDI_OpenDevice(int device);

void MIDI_CloseDevice(void);

#endif
