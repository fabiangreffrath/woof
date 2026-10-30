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

#ifndef MIDISEQ_H
#define MIDISEQ_H

#include "doomtype.h"
#include "midifile.h"

typedef struct
{
    midi_track_iter_t *iter;
    unsigned int elapsed_time;
    unsigned int saved_elapsed_time;
    boolean end_of_track;
    boolean saved_end_of_track;
    boolean emidi_include_track;
    boolean emidi_program;
    boolean emidi_volume;
    int emidi_loop_count;
} midi_track_t;

typedef struct
{
    void *lump_data;
    int lump_length;
    midi_file_t *file;
    midi_track_t *tracks;
    unsigned int elapsed_time;
    unsigned int saved_elapsed_time;
    unsigned int num_tracks;
    boolean looping;
    boolean ff_loop;
    boolean ff_restart;
    boolean rpg_loop;
} midiseq_t;

typedef enum
{
    STATE_STARTUP,
    STATE_PLAYING,
    STATE_PAUSING,
    STATE_WAITING,
    STATE_STOPPED,
    STATE_PAUSED
} midi_state_t;

typedef struct
{
    midi_event_t *event;
    midi_track_t *track;
} midi_position_t;

extern midiseq_t midiseq;

// Tempo control variables

extern unsigned int ticks_per_beat;
extern unsigned int us_per_beat;

extern float volume_factor;

boolean MIDISeq_RegisterSong(void);

extern midi_state_t midi_state;

extern void (*MIDISeq_ProcessEvent)(const midi_event_t *event, midi_track_t *track);

void MIDISeq_ResetDevice(void);

void MIDISeq_SendNotesSoundOff(void);

midi_state_t MIDISeq_NextEvent(midi_position_t *position);

void MIDISeq_SetComplevel(void);

void MIDISeq_UpdateVolume(void);

uint64_t MIDISeq_TicksToUS(uint32_t ticks);

void MIDISeq_RestartTimer(uint64_t offset);

void MIDISeq_PauseTimer(void);

uint64_t MIDISeq_CurrentTime(void);

void MIDISeq_BindVariables(void);

#endif
