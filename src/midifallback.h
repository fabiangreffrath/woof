//
// Copyright(C) 2022 ceski
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
//      MIDI instrument fallback support
//

#ifndef MIDIFALLBACK_H
#define MIDIFALLBACK_H

#include "doomtype.h"
#include "midifile.h"

typedef enum midi_fallback_type_t
{
    FALLBACK_NONE,
    FALLBACK_BANK_MSB,
    FALLBACK_BANK_LSB,
    FALLBACK_DRUMS,
} midi_fallback_type_t;

typedef struct midi_fallback_t
{
    midi_fallback_type_t type;
    byte value;
} midi_fallback_t;

void MIDI_ResetFallback(void);
void MIDI_UpdateBankMSB(byte idx, byte value);
void MIDI_UpdateDrumMap(byte idx, byte value);
midi_fallback_t MIDI_BankLSBFallback(byte idx, byte value);
midi_fallback_t MIDI_ProgramFallback(byte idx, byte program);

#endif // MIDIFALLBACK_H
