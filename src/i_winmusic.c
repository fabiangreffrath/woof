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
//      Windows native MIDI


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "doomtype.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc2.h"
#include "memio.h"
#include "mus2mid.h"
#include "midifile.h"

int winmm_reset_type = 2;
int winmm_reset_delay = 100;
int winmm_reverb_level = 40;
int winmm_chorus_level = 0;

static byte gs_reset[] = {
    0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7
};

static byte gm_system_on[] = {
    0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7
};

static byte gm2_system_on[] = {
    0xF0, 0x7E, 0x7F, 0x09, 0x03, 0xF7
};

static byte xg_system_on[] = {
    0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7
};

static byte master_volume_msg[] = {
    0xF0, 0x7F, 0x7F, 0x04, 0x01, 0x7F, 0x7F, 0xF7
};

#define DEFAULT_MASTER_VOLUME 16383
static unsigned int master_volume = DEFAULT_MASTER_VOLUME;
static int last_volume = -1;
static float volume_factor = 1.0f;
static boolean update_volume = false;

static DWORD timediv;
static DWORD tempo;

static HMIDISTRM hMidiStream;
static MIDIHDR MidiStreamHdr;
static HANDLE hBufferReturnEvent;
static HANDLE hExitEvent;
static HANDLE hPlayerThread;

// This is a reduced Windows MIDIEVENT structure for MEVT_F_SHORT
// type of events.

typedef struct
{
    DWORD dwDeltaTime;
    DWORD dwStreamID; // always 0
    DWORD dwEvent;
} native_event_t;

typedef struct
{
    midi_track_iter_t *iter;
    int absolute_time;
    boolean empty;
} win_midi_track_t;

typedef struct
{
    midi_file_t *file;
    win_midi_track_t *tracks;
    int current_time;
    boolean looping;
} win_midi_song_t;

static win_midi_song_t song;

#define BUFFER_INITIAL_SIZE 1024

typedef struct
{
    byte *data;
    size_t size;
    size_t position;
} buffer_t;

static buffer_t buffer;

// Maximum of 4 events in the buffer for faster volume updates.

#define STREAM_MAX_EVENTS 4

#define MAKE_EVT(a, b, c, d) ((DWORD)((a) | ((b) << 8) | ((c) << 16) | ((d) << 24)))

#define PADDED_SIZE(x) (((x) + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1))

static boolean initial_playback = false;

// Message box for midiStream errors.

static void MidiError(const char *prefix, DWORD dwError)
{
    char szErrorBuf[MAXERRORLENGTH];
    MMRESULT mmr;

    mmr = midiOutGetErrorText(dwError, (LPSTR) szErrorBuf, MAXERRORLENGTH);
    if (mmr == MMSYSERR_NOERROR)
    {
        char *msg = M_StringJoin(prefix, ": ", szErrorBuf, NULL);
        MessageBox(NULL, msg, "midiStream Error", MB_ICONEXCLAMATION);
        free(msg);
    }
    else
    {
        fprintf(stderr, "%s: Unknown midiStream error.\n", prefix);
    }
}

// midiStream callback.

static void CALLBACK MidiStreamProc(HMIDIOUT hMidi, UINT uMsg,
                                    DWORD_PTR dwInstance, DWORD_PTR dwParam1,
                                    DWORD_PTR dwParam2)
{
    if (uMsg == MOM_DONE)
    {
        SetEvent(hBufferReturnEvent);
    }
}

static void AllocateBuffer(const size_t size)
{
    MIDIHDR *hdr = &MidiStreamHdr;
    MMRESULT mmr;

    if (buffer.data)
    {
        mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
        if (mmr != MMSYSERR_NOERROR)
        {
            MidiError("midiOutUnprepareHeader", mmr);
        }
    }

    buffer.size = PADDED_SIZE(size);
    buffer.data = I_Realloc(buffer.data, buffer.size);

    hdr->lpData = (LPSTR)buffer.data;
    hdr->dwBytesRecorded = 0;
    hdr->dwBufferLength = buffer.size;
    mmr = midiOutPrepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiOutPrepareHeader", mmr);
    }
}

static void WriteBufferPad(void)
{
    int padding = PADDED_SIZE(buffer.position);
    memset(buffer.data + buffer.position, 0, padding - buffer.position);
    buffer.position = padding;
}

static void WriteBuffer(const byte *ptr, size_t size)
{
    if (buffer.position + size >= buffer.size)
    {
        AllocateBuffer(size + buffer.size * 2);
    }

    memcpy(buffer.data + buffer.position, ptr, size);
    buffer.position += size;
}

static void StreamOut(void)
{
    MIDIHDR *hdr = &MidiStreamHdr;
    MMRESULT mmr;

    hdr->lpData = (LPSTR)buffer.data;
    hdr->dwBytesRecorded = buffer.position;

    mmr = midiStreamOut(hMidiStream, hdr, sizeof(MIDIHDR));
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamOut", mmr);
    }
}

static void SendShortMsg(int time, int status, int channel, int param1, int param2)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(status | channel, param1, param2, MEVT_SHORTMSG);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
}

static void SendLongMsg(int time, const byte *ptr, int length)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(length, 0, 0, MEVT_LONGMSG);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
    WriteBuffer(ptr, length);
    WriteBufferPad();
}

static void SendNOPMsg(int time)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(0, 0, 0, MEVT_NOP);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
}

static void SendDelayMsg(void)
{
    // Convert ms to ticks (see "Standard MIDI Files 1.0" page 14).
    int ticks = (float)winmm_reset_delay * 1000 * timediv / tempo + 0.5f;

    SendNOPMsg(ticks);
}

static void UpdateTempo(int time)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(tempo, 0, 0, MEVT_TEMPO);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
}

static void UpdateVolume(int time)
{
    int volume = master_volume * volume_factor + 0.5f;
    master_volume_msg[5] = volume & 0x7F;
    master_volume_msg[6] = (volume >> 7) & 0x7F;
    SendLongMsg(time, master_volume_msg, sizeof(master_volume_msg));
}

static void ResetDevice(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // midiOutReset() sends "all notes off" and "reset all controllers."
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);

        // Reset pitch bend sensitivity to +/- 2 semitones and 0 cents.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_MSB, 2);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 127);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 127);

        // Reset channel volume and pan.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_MAIN_VOLUME, 100);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_PAN, 64);

        // Reset instrument.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_MSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_LSB, 0);
        SendShortMsg(0, MIDI_EVENT_PROGRAM_CHANGE, i, 0, 0);
    }

    // Send SysEx reset message.
    switch (winmm_reset_type)
    {
        case 1: // GS Reset
            SendLongMsg(0, gs_reset, sizeof(gs_reset));
            break;

        case 2: // GM System On
            SendLongMsg(0, gm_system_on, sizeof(gm_system_on));
            break;

        case 3: // GM2 System On
            SendLongMsg(0, gm2_system_on, sizeof(gm2_system_on));
            break;

        case 4: // XG System On
            SendLongMsg(0, xg_system_on, sizeof(xg_system_on));
            break;

        default: // None
            break;
    }

    // Send delay after reset.
    if (winmm_reset_delay > 0)
    {
        SendDelayMsg();
    }

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Reset reverb and chorus.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_REVERB, winmm_reverb_level);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_CHORUS, winmm_chorus_level);
    }
}

static boolean IsSysExReset(const byte *msg, int length)
{
    if (length < 5)
    {
        return false;
    }

    switch (msg[0])
    {
        case 0x41: // Roland
            switch (msg[2])
            {
                case 0x42: // GS
                    switch (msg[3])
                    {
                        case 0x12: // DT1
                            if (length == 10 &&
                                msg[4] == 0x00 &&  // Address MSB
                                msg[5] == 0x00 &&  // Address
                                msg[6] == 0x7F &&  // Address LSB
                              ((msg[7] == 0x00 &&  // Data     (MODE-1)
                                msg[8] == 0x01) || // Checksum (MODE-1)
                               (msg[7] == 0x01 &&  // Data     (MODE-2)
                                msg[8] == 0x00)))  // Checksum (MODE-2)
                            {
                                // SC-88 System Mode Set
                                // 41 <dev> 42 12 00 00 7F 00 01 F7 (MODE-1)
                                // 41 <dev> 42 12 00 00 7F 01 00 F7 (MODE-2)
                                return true;
                            }
                            else if (length == 10 &&
                                     msg[4] == 0x40 && // Address MSB
                                     msg[5] == 0x00 && // Address
                                     msg[6] == 0x7F && // Address LSB
                                     msg[7] == 0x00 && // Data (GS Reset)
                                     msg[8] == 0x41)   // Checksum
                            {
                                // GS Reset
                                // 41 <dev> 42 12 40 00 7F 00 41 F7
                                return true;
                            }
                            break;
                    }
                    break;
            }
            break;

        case 0x43: // Yamaha
            switch (msg[2])
            {
                case 0x2B: // TG300
                    if (length == 9 &&
                        msg[3] == 0x00 && // Start Address b20 - b14
                        msg[4] == 0x00 && // Start Address b13 - b7
                        msg[5] == 0x7F && // Start Address b6 - b0
                        msg[6] == 0x00 && // Data
                        msg[7] == 0x01)   // Checksum
                    {
                        // TG300 All Parameter Reset
                        // 43 <dev> 2B 00 00 7F 00 01 F7
                        return true;
                    }
                    break;

                case 0x4C: // XG
                    if (length == 8 &&
                        msg[3] == 0x00 && // Address High
                        msg[4] == 0x00 && // Address Mid
                        msg[5] == 0x7E && // Address Low
                        msg[6] == 0x00)   // Data
                    {
                        // XG System On
                        // 43 <dev> 4C 00 00 7E 00 F7
                        return true;
                    }
                    else if (length == 8 &&
                             msg[3] == 0x00 && // Address High
                             msg[4] == 0x00 && // Address Mid
                             msg[5] == 0x7F && // Address Low
                             msg[6] == 0x00)   // Data
                    {
                        // XG All Parameter Reset
                        // 43 <dev> 4C 00 00 7F 00 F7
                        return true;
                    }
                    break;
            }
            break;

        case 0x7E: // Universal Non-Real Time
            switch (msg[2])
            {
                case 0x09: // General Midi
                    if (length == 5 &&
                       (msg[3] == 0x01 || // GM System On
                        msg[3] == 0x02 || // GM System Off
                        msg[3] == 0x03))  // GM2 System On
                    {
                        // GM System On/Off, GM2 System On
                        // 7E <dev> 09 01 F7
                        // 7E <dev> 09 02 F7
                        // 7E <dev> 09 03 F7
                        return true;
                    }
                    break;
            }
            break;
    }
    return false;
}

static boolean IsMasterVolume(const byte *msg, int length, unsigned int *volume)
{
    // General Midi (7F <dev> 04 01 <lsb> <msb> F7)
    if (length == 7 &&
        msg[0] == 0x7F && // Universal Real Time
        msg[2] == 0x04 && // Device Control
        msg[3] == 0x01)   // Master Volume
    {
        *volume = (msg[4] & 0x7F) | ((msg[5] & 0x7F) << 7);
        return true;
    }

    // Roland (41 <dev> 42 12 40 00 04 <vol> <sum> F7)
    if (length == 10 &&
        msg[0] == 0x41 && // Roland
        msg[2] == 0x42 && // GS
        msg[3] == 0x12 && // DT1
        msg[4] == 0x40 && // Address MSB
        msg[5] == 0x00 && // Address
        msg[6] == 0x04)   // Address LSB
    {
        *volume = DEFAULT_MASTER_VOLUME * (msg[7] & 0x7F) / 127;
        return true;
    }

    // Yamaha (43 <dev> 4C 00 00 04 <vol> F7)
    if (length == 8 &&
        msg[0] == 0x43 && // Yamaha
        msg[2] == 0x4C && // XG
        msg[3] == 0x00 && // Address High
        msg[4] == 0x00 && // Address Mid
        msg[5] == 0x04)   // Address Low
    {
        *volume = DEFAULT_MASTER_VOLUME * (msg[6] & 0x7F) / 127;
        return true;
    }

    return false;
}

static void SendSysExMsg(int time, const byte *data, int length)
{
    native_event_t native_event;
    const byte event_type = MIDI_EVENT_SYSEX;

    if (IsMasterVolume(data, length, &master_volume))
    {
        // Found a master volume message in the MIDI file. Take this new
        // value and scale it by the user's volume slider.
        UpdateVolume(time);
        return;
    }

    // Send the SysEx message.
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(length + sizeof(byte), 0, 0, MEVT_LONGMSG);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
    WriteBuffer(&event_type, sizeof(byte));
    WriteBuffer(data, length);
    WriteBufferPad();

    if (IsSysExReset(data, length))
    {
        // SysEx reset also resets master volume. Take the default master
        // volume and scale it by the user's volume slider.
        master_volume = DEFAULT_MASTER_VOLUME;
        UpdateVolume(0);
    }
}

static void FillBuffer(void)
{
    int i;
    int num_events;

    buffer.position = 0;

    if (initial_playback)
    {
        initial_playback = false;

        master_volume = DEFAULT_MASTER_VOLUME;
        ResetDevice();
        StreamOut();
        return;
    }

    if (update_volume)
    {
        update_volume = false;

        UpdateVolume(0);
        StreamOut();
        return;
    }

    for (num_events = 0; num_events < STREAM_MAX_EVENTS; )
    {
        midi_event_t *event;
        int min_time = INT_MAX;
        int idx = -1;
        int delta_time;

        // Look for an event with a minimal delta time.
        for (i = 0; i < MIDI_NumTracks(song.file); ++i)
        {
            int time = 0;

            if (song.tracks[i].empty)
            {
                continue;
            }

            time = song.tracks[i].absolute_time + MIDI_GetDeltaTime(song.tracks[i].iter);

            if (time < min_time)
            {
                min_time = time;
                idx = i;
            }
        }

        // No more MIDI events left.
        if (idx == -1)
        {
            if (song.looping && song.current_time)
            {
                for (i = 0; i < MIDI_NumTracks(song.file); ++i)
                {
                    song.tracks[i].absolute_time = 0;
                    MIDI_RestartIterator(song.tracks[i].iter);
                    song.tracks[i].empty = false;
                }
                song.current_time = 0;
                continue;
            }
            else
            {
                break;
            }
        }

        song.tracks[idx].absolute_time = min_time;

        if (!MIDI_GetNextEvent(song.tracks[idx].iter, &event))
        {
            song.tracks[idx].empty = true;
            continue;
        }

        delta_time = min_time - song.current_time;

        switch ((int)event->event_type)
        {
            case MIDI_EVENT_META:
                switch (event->data.meta.type)
                {
                    case MIDI_META_SET_TEMPO:
                        tempo = MAKE_EVT(event->data.meta.data[2],
                            event->data.meta.data[1], event->data.meta.data[0], 0);
                        UpdateTempo(delta_time);
                        break;

                    case MIDI_META_END_OF_TRACK:
                        SendNOPMsg(delta_time);
                        break;

                    default:
                        continue;
                }
                break;

            case MIDI_EVENT_CONTROLLER:
            case MIDI_EVENT_NOTE_OFF:
            case MIDI_EVENT_NOTE_ON:
            case MIDI_EVENT_AFTERTOUCH:
            case MIDI_EVENT_PITCH_BEND:
                SendShortMsg(delta_time, event->event_type, event->data.channel.channel,
                    event->data.channel.param1, event->data.channel.param2);
                break;

            case MIDI_EVENT_PROGRAM_CHANGE:
            case MIDI_EVENT_CHAN_AFTERTOUCH:
                SendShortMsg(delta_time, event->event_type, event->data.channel.channel,
                    event->data.channel.param1, 0);
                break;

            case MIDI_EVENT_SYSEX:
                SendSysExMsg(delta_time, event->data.sysex.data, event->data.sysex.length);
                song.current_time = min_time;
                StreamOut();
                return;

            default:
                continue;
        }

        num_events++;
        song.current_time = min_time;
    }

    if (num_events)
    {
        StreamOut();
    }
}

// The Windows API documentation states: "Applications should not call any
// multimedia functions from inside the callback function, as doing so can
// cause a deadlock." We use thread to avoid possible deadlocks.

static DWORD WINAPI PlayerProc(void)
{
    HANDLE events[2] = { hBufferReturnEvent, hExitEvent };

    while (1)
    {
        switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
        {
            case WAIT_OBJECT_0:
                FillBuffer();
                break;

            case WAIT_OBJECT_0 + 1:
                return 0;
        }
    }
    return 0;
}

static boolean I_WIN_InitMusic(void)
{
    UINT MidiDevice = MIDI_MAPPER;
    MMRESULT mmr;

    mmr = midiStreamOpen(&hMidiStream, &MidiDevice, (DWORD)1,
                         (DWORD_PTR)MidiStreamProc, (DWORD_PTR)NULL,
                         CALLBACK_FUNCTION);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamOpen", mmr);
        return false;
    }

    AllocateBuffer(BUFFER_INITIAL_SIZE);

    hBufferReturnEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    return true;
}

static void I_WIN_SetMusicVolume(int volume)
{
    if (last_volume == volume)
    {
        // Ignore holding key down in volume menu.
        return;
    }

    last_volume = volume;

    volume_factor = sqrtf((float)volume / 15);

    update_volume = true;
}

static void I_WIN_StopSong(void *handle)
{
    MMRESULT mmr;

    if (hPlayerThread)
    {
        SetEvent(hExitEvent);
        WaitForSingleObject(hPlayerThread, INFINITE);

        CloseHandle(hPlayerThread);
        hPlayerThread = NULL;
    }

    mmr = midiStreamStop(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamStop", mmr);
    }
}

static void I_WIN_PlaySong(void *handle, boolean looping)
{
    MMRESULT mmr;

    song.looping = looping;

    hPlayerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PlayerProc,
                                 0, 0, 0);
    SetThreadPriority(hPlayerThread, THREAD_PRIORITY_TIME_CRITICAL);

    initial_playback = true;

    update_volume = true;

    SetEvent(hBufferReturnEvent);

    mmr = midiStreamRestart(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamRestart", mmr);
    }
}

static void I_WIN_PauseSong(void *handle)
{
    MMRESULT mmr;

    mmr = midiStreamPause(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamPause", mmr);
    }
}

static void I_WIN_ResumeSong(void *handle)
{
    MMRESULT mmr;

    mmr = midiStreamRestart(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamRestart", mmr);
    }
}

static void *I_WIN_RegisterSong(void *data, int len)
{
    int i;
    int num_tracks;

    MIDIPROPTIMEDIV prop_timediv;
    MIDIPROPTEMPO prop_tempo;
    MMRESULT mmr;

    if (IsMid(data, len))
    {
        song.file = MIDI_LoadFile(data, len);
    }
    else
    {
        // Assume a MUS file and try to convert
        MEMFILE *instream;
        MEMFILE *outstream;
        void *outbuf;
        size_t outbuf_len;

        instream = mem_fopen_read(data, len);
        outstream = mem_fopen_write();

        if (mus2mid(instream, outstream) == 0)
        {
            mem_get_buf(outstream, &outbuf, &outbuf_len);
            song.file = MIDI_LoadFile(outbuf, outbuf_len);
        }
        else
        {
            song.file = NULL;
        }

        mem_fclose(instream);
        mem_fclose(outstream);
    }

    if (song.file == NULL)
    {
        fprintf(stderr, "I_WIN_RegisterSong: Failed to load MID.\n");
        return NULL;
    }

    prop_timediv.cbStruct = sizeof(MIDIPROPTIMEDIV);
    prop_timediv.dwTimeDiv = MIDI_GetFileTimeDivision(song.file);
    mmr = midiStreamProperty(hMidiStream, (LPBYTE)&prop_timediv,
                             MIDIPROP_SET | MIDIPROP_TIMEDIV);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamProperty", mmr);
        return NULL;
    }
    timediv = prop_timediv.dwTimeDiv;

    // Set initial tempo.
    prop_tempo.cbStruct = sizeof(MIDIPROPTIMEDIV);
    prop_tempo.dwTempo = 500000; // 120 BPM
    mmr = midiStreamProperty(hMidiStream, (LPBYTE)&prop_tempo,
                             MIDIPROP_SET | MIDIPROP_TEMPO);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamProperty", mmr);
        return NULL;
    }
    tempo = prop_tempo.dwTempo;

    num_tracks = MIDI_NumTracks(song.file);

    song.tracks = malloc(num_tracks * sizeof(win_midi_track_t));

    for (i = 0; i < num_tracks; ++i)
    {
        song.tracks[i].iter = MIDI_IterateTrack(song.file, i);
        song.tracks[i].absolute_time = 0;
        song.tracks[i].empty = false;
    }

    ResetEvent(hBufferReturnEvent);
    ResetEvent(hExitEvent);

    return (void *)1;
}

static void I_WIN_UnRegisterSong(void *handle)
{
    if (song.tracks)
    {
        int i;
        for (i = 0; i < MIDI_NumTracks(song.file); ++i)
        {
            MIDI_FreeIterator(song.tracks[i].iter);
            song.tracks[i].iter = NULL;
        }
        free(song.tracks);
        song.tracks = NULL;
    }
    if (song.file)
    {
        MIDI_FreeFile(song.file);
        song.file = NULL;
    }
    song.current_time = 0;
    song.looping = false;
}

static void I_WIN_ShutdownMusic(void)
{
    MMRESULT mmr;

    I_WIN_StopSong(NULL);
    I_WIN_UnRegisterSong(NULL);

    // Reset device at shutdown.
    buffer.position = 0;
    ResetDevice();
    StreamOut();
    mmr = midiStreamRestart(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamRestart", mmr);
    }
    WaitForSingleObject(hBufferReturnEvent, INFINITE);

    mmr = midiStreamClose(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamClose", mmr);
    }

    hMidiStream = NULL;

    if (buffer.data)
    {
        free(buffer.data);
        buffer.data = NULL;
    }
    buffer.size = 0;
    buffer.position = 0;

    CloseHandle(hBufferReturnEvent);
    CloseHandle(hExitEvent);
}

music_module_t music_win_module =
{
    I_WIN_InitMusic,
    I_WIN_ShutdownMusic,
    I_WIN_SetMusicVolume,
    I_WIN_PauseSong,
    I_WIN_ResumeSong,
    I_WIN_RegisterSong,
    I_WIN_PlaySong,
    I_WIN_StopSong,
    I_WIN_UnRegisterSong,
};
