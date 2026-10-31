//
// Copyright(C) 2021-2022 Roman Fomin
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
//      MIDI/MUS song sequencing.

#ifndef MIDISEQ_H
#define MIDISEQ_H

#include "doomtype.h"
#include "midifile.h"

typedef struct midi_track_s midi_track_t;

// The compatibility level also changes how the song is sequenced: the vanilla
// level ignores EMIDI and Final Fantasy loop markers.

typedef enum
{
    MIDI_COMP_VANILLA,
    MIDI_COMP_STANDARD,
    MIDI_COMP_FULL,
    MIDI_COMP_NUM,
} midi_complevel_t;

// The next song event and the absolute playback time at which it must be
// played. The sequencer does not own a clock, the caller drives it: wall clock
// time for realtime output, a sample clock for streaming synths.

typedef struct
{
    midi_event_t *event;
    midi_track_t *track;
    uint64_t time_us;
} midi_position_t;

// Receives every playable event: channel messages and SysEx. Meta events,
// EMIDI control messages and events of tracks excluded by EMIDI are handled by
// the sequencer itself and never reach the handler. SysEx filtering (e.g. by
// compatibility level) is up to the handler.

typedef void (*midi_event_handler_t)(const midi_event_t *event);

void MIDISeq_SetEventHandler(midi_event_handler_t handler);

int MIDISeq_GetComplevel(void);

// Song loading. RegisterSong() unregisters the previous song.

boolean MIDISeq_RegisterSong(void *data, int length);

void MIDISeq_UnregisterSong(void);

// Playback control. Play() rewinds the song to the beginning.
//
// A backend drives the sequencer with its own clock:
//
//     MIDISeq_Play(looping);
//
//     while (MIDISeq_NextEvent(&position))
//     {
//         WaitUntil(position.time_us);
//         MIDISeq_SendEvent(&position);
//     }
//
// Realtime output waits on the wall clock, streaming synths render the frames
// between the current position and position.time_us.

void MIDISeq_Play(boolean looping);

boolean MIDISeq_NextEvent(midi_position_t *position);

void MIDISeq_SendEvent(const midi_position_t *position);

void MIDISeq_BindVariables(void);

#endif
