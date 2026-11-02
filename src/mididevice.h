//
// Copyright(C) 2022 ceski
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
//
// DESCRIPTION:
//      MIDI output compatibility layer. Maps sequencer events to the messages
//      a MIDI device expects, including the DMX, General MIDI and SC-55
//      quirks.

#ifndef MIDIDEVICE_H
#define MIDIDEVICE_H

#include "doomtype.h"
#include "midifile.h"

// Receives raw MIDI messages. Realtime output to a hardware device also needs
// the delays between reset messages; streaming backends leave the delay
// callbacks NULL and receive everything at once.

typedef struct
{
    void (*ShortMsg)(const byte *message, unsigned int length);
    void (*LongMsg)(const byte *message, unsigned int length);
    void (*DelayUS)(uint64_t us);
    void (*DelayMS)(int ms);
} midi_output_sink_t;

void MIDIDevice_SetSink(const midi_output_sink_t *sink);

// Emulates a MIDI device: rewrites and filters the event stream according to
// the compatibility level, capital tone fallback, volume and reset settings.
// Can be installed as MIDISeq event handler.

void MIDIDevice_HandleEvent(const midi_event_t *event);

// Resets the device. Call this function before each song starts and once at
// shut down.

void MIDIDevice_Reset(boolean startup);

void MIDIDevice_SendNotesSoundOff(void);

void MIDIDevice_SetVolumeFactor(float factor);

void MIDIDevice_UpdateVolume(void);

void MIDIDevice_BindVariables(void);

#endif
