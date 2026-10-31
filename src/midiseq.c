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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_sound.h"
#include "m_config.h"
#include "memio.h"
#include "midifile.h"
#include "midiseq.h"
#include "mus2mid.h"

//---------------------------------------------------------------------------
// Settings
//---------------------------------------------------------------------------

// The compatibility level also changes how the song is sequenced: the vanilla
// level ignores EMIDI and Final Fantasy loop markers.

static int midi_complevel = MIDI_COMP_STANDARD;

static const byte ff_loopStart[] =
{
    'l', 'o', 'o', 'p', 'S', 't', 'a', 'r', 't'
};

static const byte ff_loopEnd[] =
{
    'l', 'o', 'o', 'p', 'E', 'n', 'd'
};

#define EMIDI_DEVICE EMIDI_DEVICE_GENERAL_MIDI

//---------------------------------------------------------------------------
// Tempo
//---------------------------------------------------------------------------

static unsigned int ticks_per_beat = 1;
static unsigned int us_per_beat = MIDI_DEFAULT_TEMPO;

static uint64_t TicksToUS(unsigned int ticks)
{
    return (uint64_t)ticks * us_per_beat / ticks_per_beat;
}

static void UpdateTempo(const midi_event_t *event)
{
    const byte *data = event->data.meta.data;
    us_per_beat = (data[0] << 16) | (data[1] << 8) | data[2];
}

//---------------------------------------------------------------------------
// Song state
//---------------------------------------------------------------------------

struct midi_track_s
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
};

typedef struct
{
    midi_file_t *file;
    midi_track_t *tracks;
    unsigned int num_tracks;
    unsigned int elapsed_time;
    unsigned int saved_elapsed_time;
    uint64_t time_us;
    boolean looping;
    boolean ff_loop;
    boolean ff_restart;
    boolean rpg_loop;
} midi_song_t;

static midi_song_t song;

static midi_event_handler_t event_handler;

// Channels that received events. Reset controllers and volume are sent for
// these channels when the song loops.

static boolean channel_used[MIDI_CHANNELS_PER_TRACK];

int MIDISeq_GetComplevel(void)
{
    return midi_complevel;
}

// Sets a Final Fantasy or RPG Maker loop point.

static void SetLoopPoint(void)
{
    if (song.num_tracks == 0)
    {
        return;
    }

    for (unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_SetLoopPoint(song.tracks[i].iter);
        song.tracks[i].saved_end_of_track = song.tracks[i].end_of_track;
        song.tracks[i].saved_elapsed_time = song.tracks[i].elapsed_time;
    }
    song.saved_elapsed_time = song.elapsed_time;
}

// Checks if the MIDI meta message contains a Final Fantasy loop marker.

static void CheckFFLoop(const midi_event_t *event)
{
    if (event->data.meta.length == sizeof(ff_loopStart)
        && !memcmp(event->data.meta.data, ff_loopStart, sizeof(ff_loopStart)))
    {
        SetLoopPoint();
        song.ff_loop = true;
    }
    else if (song.ff_loop && event->data.meta.length == sizeof(ff_loopEnd)
             && !memcmp(event->data.meta.data, ff_loopEnd, sizeof(ff_loopEnd)))
    {
        song.ff_restart = true;
    }
}

static void ProcessMetaEvent(const midi_event_t *event, midi_track_t *track)
{
    switch (event->data.meta.type)
    {
        case MIDI_META_END_OF_TRACK:
            track->end_of_track = true;
            break;

        case MIDI_META_SET_TEMPO:
            UpdateTempo(event);
            break;

        case MIDI_META_MARKER:
            if (midi_complevel != MIDI_COMP_VANILLA)
            {
                CheckFFLoop(event);
            }
            break;

        default:
            break;
    }
}

// Passes a playable event to the backend.

static void DispatchEvent(const midi_event_t *event)
{
    if (event->event_type < MIDI_EVENT_SYSEX)
    {
        channel_used[event->data.channel.channel] = true;
    }
    else if (event->event_type == MIDI_EVENT_SYSEX
             && event->data.sysex.type == MIDI_SYSEX_RESET)
    {
        // The device state has been reset, do not reset it again on a loop.
        memset(channel_used, 0, sizeof(channel_used));
    }

    event_handler(event);
}

static void SendEMIDIProgramChange(const midi_event_t *event)
{
    midi_event_t program = *event;

    program.event_type = MIDI_EVENT_PROGRAM_CHANGE;
    program.data.channel.param1 = event->data.channel.param2;

    DispatchEvent(&program);
}

static void SendEMIDIVolume(const midi_event_t *event)
{
    midi_event_t volume = *event;

    volume.event_type = MIDI_EVENT_CONTROLLER;
    volume.data.channel.param1 = MIDI_CONTROLLER_VOLUME_MSB;
    volume.data.channel.param2 = event->data.channel.param2;

    DispatchEvent(&volume);
}

// Handles EMIDI control messages. Returns true if the event was consumed.

static boolean ProcessEMIDI(const midi_event_t *event, midi_track_t *track)
{
    const unsigned int param2 = event->data.channel.param2;
    int count;

    switch (event->data.channel.param1)
    {
        case EMIDI_CONTROLLER_TRACK_DESIGNATION:
            return true;

        case EMIDI_CONTROLLER_TRACK_EXCLUSION:
            if (song.rpg_loop)
            {
                SetLoopPoint();
            }
            return true;

        case EMIDI_CONTROLLER_PROGRAM_CHANGE:
            if (track->emidi_program)
            {
                SendEMIDIProgramChange(event);
            }
            return true;

        case EMIDI_CONTROLLER_VOLUME:
            if (track->emidi_volume)
            {
                SendEMIDIVolume(event);
            }
            return true;

        case EMIDI_CONTROLLER_LOOP_BEGIN:
            count = (param2 == 0) ? -1 : param2;
            track->emidi_loop_count = count;
            MIDI_SetLoopPoint(track->iter);
            return true;

        case EMIDI_CONTROLLER_LOOP_END:
            if (param2 == EMIDI_LOOP_FLAG)
            {
                if (track->emidi_loop_count != 0)
                {
                    MIDI_RestartAtLoopPoint(track->iter);
                }

                if (track->emidi_loop_count > 0)
                {
                    track->emidi_loop_count--;
                }
            }
            return true;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN:
            count = (param2 == 0) ? -1 : param2;
            for (unsigned int i = 0; i < song.num_tracks; ++i)
            {
                song.tracks[i].emidi_loop_count = count;
                MIDI_SetLoopPoint(song.tracks[i].iter);
                song.tracks[i].saved_end_of_track =
                    song.tracks[i].end_of_track;
                song.tracks[i].saved_elapsed_time =
                    song.tracks[i].elapsed_time;
            }
            song.saved_elapsed_time = song.elapsed_time;
            return true;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_END:
            if (param2 == EMIDI_LOOP_FLAG)
            {
                for (unsigned int i = 0; i < song.num_tracks; ++i)
                {
                    if (song.tracks[i].emidi_loop_count != 0)
                    {
                        MIDI_RestartAtLoopPoint(song.tracks[i].iter);
                        song.tracks[i].end_of_track =
                            song.tracks[i].saved_end_of_track;
                        song.tracks[i].elapsed_time =
                            song.tracks[i].saved_elapsed_time;
                        song.elapsed_time = song.saved_elapsed_time;
                    }

                    if (song.tracks[i].emidi_loop_count > 0)
                    {
                        song.tracks[i].emidi_loop_count--;
                    }
                }
            }
            return true;
    }

    return false;
}

// Sequencer level event processing. Meta events, EMIDI control messages and
// events of excluded tracks are handled here, everything else is passed to the
// backend.

static void ProcessEvent(const midi_event_t *event, midi_track_t *track)
{
    if (event->event_type == MIDI_EVENT_META)
    {
        ProcessMetaEvent(event, track);
        return;
    }

    if (midi_complevel != MIDI_COMP_VANILLA)
    {
        if (!track->emidi_include_track)
        {
            return;
        }

        if (event->event_type == MIDI_EVENT_CONTROLLER)
        {
            if (ProcessEMIDI(event, track))
            {
                return;
            }

            if (event->data.channel.param1 == MIDI_CONTROLLER_VOLUME_MSB
                && track->emidi_volume)
            {
                return;
            }
        }
        else if (event->event_type == MIDI_EVENT_PROGRAM_CHANGE
                 && track->emidi_program)
        {
            return;
        }
    }

    DispatchEvent(event);
}

void MIDISeq_SetEventHandler(midi_event_handler_t handler)
{
    event_handler = handler;
}

void MIDISeq_SendEvent(const midi_position_t *position)
{
    ProcessEvent(position->event, position->track);
}

//---------------------------------------------------------------------------
// Sequencer
//---------------------------------------------------------------------------

// Restarts a song that uses a Final Fantasy or RPG Maker loop point.

static void RestartLoop(void)
{
    if (song.num_tracks == 0)
    {
        return;
    }

    for (unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartAtLoopPoint(song.tracks[i].iter);
        song.tracks[i].end_of_track = song.tracks[i].saved_end_of_track;
        song.tracks[i].elapsed_time = song.tracks[i].saved_elapsed_time;
    }
    song.elapsed_time = song.saved_elapsed_time;
}

// Resets the controllers and volume for each channel, similar to vanilla Doom.

static void ResetControllersVolume(void)
{
    midi_event_t event = {0};

    event.event_type = MIDI_EVENT_CONTROLLER;
    event.data.channel.param2 = 0;

    for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; i++)
    {
        if (channel_used[i])
        {
            event.data.channel.channel = i;
            event.data.channel.param1 = MIDI_CONTROLLER_RESET_ALL_CTRLS;
            event_handler(&event);

            event.data.channel.param1 = MIDI_CONTROLLER_VOLUME_MSB;
            event.data.channel.param2 = MIDI_DEFAULT_VOLUME;
            event_handler(&event);

            channel_used[i] = false;
        }
    }
}

// Restarts a song that uses standard looping.

static void RestartTracks(void)
{
    for (unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartIterator(song.tracks[i].iter);
        song.tracks[i].elapsed_time = 0;
        song.tracks[i].end_of_track = false;
        song.tracks[i].emidi_loop_count = 0;
    }
    song.elapsed_time = 0;
}

void MIDISeq_Play(boolean looping)
{
    song.looping = looping;
    song.saved_elapsed_time = 0;
    song.time_us = 0;
    song.ff_loop = false;
    song.ff_restart = false;

    memset(channel_used, 0, sizeof(channel_used));

    RestartTracks();
}

// Get the next event from the MIDI file. Events with a delta time of zero are
// processed immediately, the first event with a positive delta time is stored
// in the position together with the time it must be played at.

boolean MIDISeq_NextEvent(midi_position_t *position)
{
    for (;;)
    {
        midi_event_t *event = NULL;
        midi_track_t *track = NULL;
        unsigned int min_time = UINT_MAX;
        unsigned int delta_ticks;

        // Find next event across all tracks.
        for (unsigned int i = 0; i < song.num_tracks; ++i)
        {
            if (!song.tracks[i].end_of_track)
            {
                const unsigned int time =
                    song.tracks[i].elapsed_time
                    + MIDI_GetDeltaTime(song.tracks[i].iter);

                if (time < min_time)
                {
                    min_time = time;
                    track = &song.tracks[i];
                }
            }
        }

        // No more events. Restart or stop the song.
        if (track == NULL)
        {
            if (song.elapsed_time)
            {
                if (song.ff_restart || song.rpg_loop)
                {
                    song.ff_restart = false;
                    RestartLoop();
                    continue;
                }
                else if (song.looping)
                {
                    ResetControllersVolume();
                    RestartTracks();
                    continue;
                }
            }
            return false;
        }

        // The playback time is accumulated from the event deltas, so it keeps
        // running across loop restarts and tempo changes.
        const uint64_t previous_time_us = TicksToUS(song.elapsed_time);

        track->elapsed_time = min_time;
        delta_ticks = min_time - song.elapsed_time;
        song.elapsed_time = min_time;

        if (!MIDI_GetNextEvent(track->iter, &event))
        {
            track->end_of_track = true;
            continue;
        }

        // Restart FF loop after sending all events that share same
        // ticks_per_beat.
        if (song.ff_restart && MIDI_GetDeltaTime(track->iter) > 0)
        {
            song.ff_restart = false;
            RestartLoop();
            continue;
        }

        if (!delta_ticks)
        {
            ProcessEvent(event, track);
            continue;
        }

        song.time_us += TicksToUS(min_time) - previous_time_us;

        position->event = event;
        position->track = track;
        position->time_us = song.time_us;

        return true;
    }
}

static void InitEMIDI(void)
{
    for (unsigned int i = 0; i < song.num_tracks; i++)
    {
        midi_track_t *track = &song.tracks[i];
        unsigned int elapsed_time = 0;
        boolean designated = false;

        for (;;)
        {
            midi_event_t *event;

            elapsed_time += MIDI_GetDeltaTime(track->iter);

            if (elapsed_time >= ticks_per_beat)
            {
                break;
            }

            if (!MIDI_GetNextEvent(track->iter, &event))
            {
                break;
            }

            if (event->event_type == MIDI_EVENT_CONTROLLER)
            {
                const unsigned int dev = event->data.channel.param2;

                switch (event->data.channel.param1)
                {
                    case EMIDI_CONTROLLER_TRACK_DESIGNATION:
                        if (dev == EMIDI_DEVICE_ALL || dev == EMIDI_DEVICE)
                        {
                            designated = true;
                            track->emidi_include_track = true;
                        }
                        else if (!designated)
                        {
                            designated = true;
                            track->emidi_include_track = false;
                        }
                        break;

                    case EMIDI_CONTROLLER_TRACK_EXCLUSION:
                        if (dev == EMIDI_DEVICE_ALL || dev == EMIDI_DEVICE)
                        {
                            track->emidi_include_track = false;
                        }
                        break;

                    case EMIDI_CONTROLLER_PROGRAM_CHANGE:
                        track->emidi_program = true;
                        break;

                    case EMIDI_CONTROLLER_VOLUME:
                        track->emidi_volume = true;
                        break;

                    default:
                        break;
                }
            }
        }

        MIDI_RestartIterator(song.tracks[i].iter);
    }
}

void MIDISeq_UnregisterSong(void)
{
    if (song.tracks)
    {
        for (unsigned int i = 0; i < song.num_tracks; ++i)
        {
            MIDI_FreeIterator(song.tracks[i].iter);
        }
        free(song.tracks);
    }

    if (song.file)
    {
        MIDI_FreeFile(song.file);
    }

    memset(&song, 0, sizeof(song));

    ticks_per_beat = 1;
    us_per_beat = MIDI_DEFAULT_TEMPO;
}

boolean MIDISeq_RegisterSong(void *data, int length)
{
    MIDISeq_UnregisterSong();

    if (IsMid(data, length))
    {
        song.file = MIDI_LoadFile(data, length);
    }
    else
    {
        // Assume a MUS file and try to convert
        MEMFILE *instream = mem_fopen_read(data, length);
        MEMFILE *outstream = mem_fopen_write();
        void *outbuf;
        size_t outbuf_len;

        if (mus2mid(instream, outstream) == 0)
        {
            mem_get_buf(outstream, &outbuf, &outbuf_len);
            song.file = MIDI_LoadFile(outbuf, outbuf_len);
        }

        mem_fclose(instream);
        mem_fclose(outstream);
    }

    if (song.file == NULL)
    {
        I_Printf(VB_ERROR, "MIDISeq_RegisterSong: Failed to load song.");
        return false;
    }

    ticks_per_beat = MIDI_GetFileTimeDivision(song.file);
    us_per_beat = MIDI_DEFAULT_TEMPO;

    song.num_tracks = MIDI_NumTracks(song.file);
    song.tracks = calloc(song.num_tracks, sizeof(midi_track_t));

    for (unsigned int i = 0; i < song.num_tracks; ++i)
    {
        song.tracks[i].iter = MIDI_IterateTrack(song.file, i);
        song.tracks[i].emidi_include_track = true;
    }

    song.rpg_loop = MIDI_RPGLoop(song.file);

    if (!song.rpg_loop)
    {
        InitEMIDI();
    }

    return true;
}

void MIDISeq_BindVariables(void)
{
    BIND_NUM(midi_complevel, MIDI_COMP_STANDARD, 0, MIDI_COMP_NUM - 1,
        "[Native MIDI] Compatibility level (0 = Vanilla; 1 = Standard; 2 = Full)");
}
