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
#include <mmreg.h>
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
#include "midifallback.h"

int winmm_reset_type = 2;
int winmm_reset_delay = 0;
int winmm_reverb_level = -1;
int winmm_chorus_level = -1;

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

static boolean use_fallback;

#define DEFAULT_VOLUME 100
static int channel_volume[MIDI_CHANNELS_PER_TRACK];
static int last_volume = -1;
static float volume_factor = 0.0f;
static boolean update_volume = false;

static DWORD timediv;
static DWORD tempo;

static UINT MidiDevice;
static HMIDISTRM hMidiStream;
static MIDIHDR MidiStreamHdr;
static HANDLE hBufferReturnEvent;
static HANDLE hExitEvent;
static HANDLE hPlayerThread;

// MS GS Wavetable Synth Device ID.
static int ms_gs_synth = MIDI_MAPPER;

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

static void SendDelayMsg(int time_ms)
{
    // Convert ms to ticks (see "Standard MIDI Files 1.0" page 14).
    int time_ticks = (float)time_ms * 1000 * timediv / tempo + 0.5f;
    SendNOPMsg(time_ticks);
}

static void UpdateTempo(int time)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID = 0;
    native_event.dwEvent = MAKE_EVT(tempo, 0, 0, MEVT_TEMPO);
    WriteBuffer((byte *)&native_event, sizeof(native_event_t));
}

static void SendVolumeMsg(int time, int channel)
{
    int volume = channel_volume[channel] * volume_factor + 0.5f;
    SendShortMsg(time, MIDI_EVENT_CONTROLLER, channel,
                 MIDI_CONTROLLER_MAIN_VOLUME, volume);
}

static void UpdateVolume(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        SendVolumeMsg(0, i);
    }
}

static void ResetVolume(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        channel_volume[i] = DEFAULT_VOLUME;
        SendVolumeMsg(0, i);
    }
}

static void ResetReverb(void)
{
    int i;
    int reverb = winmm_reverb_level;

    if (reverb == -1 && winmm_reset_type == 0)
    {
        // No reverb specified and no SysEx reset selected. Use GM default.
        reverb = 40;
    }

    if (reverb > -1)
    {
        for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        {
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_REVERB, reverb);
        }
    }
}

static void ResetChorus(void)
{
    int i;
    int chorus = winmm_chorus_level;

    if (chorus == -1 && winmm_reset_type == 0)
    {
        // No chorus specified and no SysEx reset selected. Use GM default.
        chorus = 0;
    }

    if (chorus > -1)
    {
        for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        {
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_CHORUS, chorus);
        }
    }
}

static void ResetDevice(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Stop sound prior to reset to prevent volume spikes.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);
    }

    if (winmm_reset_type == 0) // No SysEx reset
    {
        for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        {
            // Manually reset commonly used controllers.
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_PAN, 64);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_MSB, 0);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_LSB, 0);
            SendShortMsg(0, MIDI_EVENT_PROGRAM_CHANGE, i, 0, 0);
        }
    }
    else // SysEx reset
    {
        if (MidiDevice == ms_gs_synth)
        {
            // MS GS Wavetable Synth lacks "capital tone fallback" in GS mode
            // which can cause silent notes (MAYhem19.wad D_DM2TTL). It also
            // responds to XG System On when it should ignore it. Force GS/GM.
            if (winmm_reset_type == 1)
            {
                SendLongMsg(0, gs_reset, sizeof(gs_reset));
            }
            else
            {
                SendLongMsg(0, gm_system_on, sizeof(gm_system_on));
            }
        }
        else
        {
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
            }
        }
    }

    if (winmm_reset_type == 0 || MidiDevice == ms_gs_synth)
    {
        for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        {
            // MS GS Wavetable Synth doesn't reset pitch bend sensitivity, even
            // when sending a GM/GS reset. Reset to +/- 2 semitones and 0 cents.
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 0);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 0);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_MSB, 2);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 127);
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 127);
        }
    }

    ResetReverb();
    ResetChorus();

    // Reset volume (initial playback or on shutdown if no SysEx reset).
    if (initial_playback || winmm_reset_type == 0)
    {
        // Scale by slider on initial playback, max on shutdown.
        volume_factor = initial_playback ? volume_factor : 1.0f;
        ResetVolume();
    }

    // Send delay after reset.
    if (winmm_reset_delay > 0)
    {
        SendDelayMsg(winmm_reset_delay);
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

static void SendSysExMsg(int time, const byte *data, int length)
{
    native_event_t native_event;
    boolean is_sysex_reset;
    const byte event_type = MIDI_EVENT_SYSEX;

    is_sysex_reset = IsSysExReset(data, length);

    if (is_sysex_reset && MidiDevice == ms_gs_synth)
    {
        // Ignore SysEx reset from MIDI file for MS GS Wavetable Synth.
        SendNOPMsg(time);
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

    if (is_sysex_reset)
    {
        // SysEx reset also resets volume. Take the default channel volumes
        // and scale them by the user's volume slider.
        ResetVolume();

        // Disable instrument fallback and give priorty to MIDI file. Fallback
        // assumes GS (SC-55 level) and the MIDI file could be GM, GM2, XG, or
        // GS (SC-88 or higher). Preserve the composer's intent.
        MIDI_ResetFallback();
        use_fallback = false;
    }
}

static void FillBuffer(void)
{
    int i;
    int num_events;

    buffer.position = 0;

    if (initial_playback)
    {
        ResetDevice();
        StreamOut();

        MIDI_ResetFallback();
        use_fallback = (winmm_reset_type == 1);

        initial_playback = false;
        return;
    }

    if (update_volume)
    {
        update_volume = false;

        UpdateVolume();
        StreamOut();
        return;
    }

    for (num_events = 0; num_events < STREAM_MAX_EVENTS; )
    {
        midi_event_t *event;
        int min_time = INT_MAX;
        int idx = -1;
        int delta_time;
        midi_fallback_t fallback = {FALLBACK_NONE, 0};

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

        if (use_fallback)
        {
            MIDI_CheckFallback(event, &fallback);
        }

        switch ((int)event->event_type)
        {
            case MIDI_EVENT_META:
                switch (event->data.meta.type)
                {
                    case MIDI_META_SET_TEMPO:
                        tempo = MAKE_EVT(event->data.meta.data[2],
                                         event->data.meta.data[1],
                                         event->data.meta.data[0], 0);
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
                if (event->data.channel.param1 == MIDI_CONTROLLER_MAIN_VOLUME)
                {
                    // Adjust channel volume.
                    int volume = event->data.channel.param2;
                    channel_volume[event->data.channel.channel] = volume;
                    SendVolumeMsg(delta_time, event->data.channel.channel);
                    break;
                }
                else if (fallback.type == FALLBACK_BANK_LSB &&
                         event->data.channel.param1 == MIDI_CONTROLLER_BANK_SELECT_LSB)
                {
                    SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER,
                                 event->data.channel.channel,
                                 MIDI_CONTROLLER_BANK_SELECT_LSB,
                                 fallback.value);
                    break;
                }
                // Fall through.
            case MIDI_EVENT_NOTE_OFF:
            case MIDI_EVENT_NOTE_ON:
            case MIDI_EVENT_AFTERTOUCH:
            case MIDI_EVENT_PITCH_BEND:
                SendShortMsg(delta_time, event->event_type,
                             event->data.channel.channel,
                             event->data.channel.param1,
                             event->data.channel.param2);
                break;

            case MIDI_EVENT_PROGRAM_CHANGE:
                if (fallback.type == FALLBACK_BANK_MSB)
                {
                    case FALLBACK_BANK_MSB:
                        SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER,
                                     event->data.channel.channel,
                                     MIDI_CONTROLLER_BANK_SELECT_MSB,
                                     fallback.value);
                        SendShortMsg(0, MIDI_EVENT_PROGRAM_CHANGE,
                                     event->data.channel.channel,
                                     event->data.channel.param1, 0);
                    break;
                }
                else if (fallback.type == FALLBACK_DRUMS)
                {
                    SendShortMsg(delta_time, MIDI_EVENT_PROGRAM_CHANGE,
                                    event->data.channel.channel,
                                    fallback.value, 0);
                    break;
                }
                // Fall through.
            case MIDI_EVENT_CHAN_AFTERTOUCH:
                SendShortMsg(delta_time, event->event_type,
                             event->data.channel.channel,
                             event->data.channel.param1, 0);
                break;

            case MIDI_EVENT_SYSEX:
                SendSysExMsg(delta_time, event->data.sysex.data,
                             event->data.sysex.length);
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

static boolean I_WIN_InitMusic(int device)
{
    MMRESULT mmr;

    MidiDevice = device;

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

    MIDI_InitFallback();

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

    update_volume = (song.file != NULL);
}

static void I_WIN_StopSong(void *handle)
{
    MMRESULT mmr;

    if (!hPlayerThread)
    {
        return;
    }

    SetEvent(hExitEvent);
    WaitForSingleObject(hPlayerThread, INFINITE);
    CloseHandle(hPlayerThread);
    hPlayerThread = NULL;

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

    if (!hMidiStream)
    {
        return;
    }

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

    mmr = midiStreamStop(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamStop", mmr);
    }

    if (buffer.data)
    {
        mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, &MidiStreamHdr,
                                     sizeof(MIDIHDR));
        if (mmr != MMSYSERR_NOERROR)
        {
            MidiError("midiOutUnprepareHeader", mmr);
        }
        free(buffer.data);
        buffer.data = NULL;
        buffer.size = 0;
        buffer.position = 0;
    }

    mmr = midiStreamClose(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamClose", mmr);
    }
    hMidiStream = NULL;

    CloseHandle(hBufferReturnEvent);
    CloseHandle(hExitEvent);
}

int I_WIN_DeviceList(const char* devices[], int size)
{
    int i;
    UINT numdevs = midiOutGetNumDevs();

    for (i = 0; i < numdevs && i < size; ++i)
    {
        MIDIOUTCAPS caps;
        MMRESULT mmr;

        mmr = midiOutGetDevCaps(i, &caps, sizeof(caps));
        if (mmr == MMSYSERR_NOERROR)
        {
            devices[i] = M_StringDuplicate(caps.szPname);

            // is this device MS GS Synth?
            if (caps.wMid == MM_MICROSOFT &&
                caps.wPid == MM_MSFT_GENERIC_MIDISYNTH &&
                caps.wTechnology == MOD_SWSYNTH)
            {
                ms_gs_synth = i;
            }
        }
    }

    return i;
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
    I_WIN_DeviceList,
};
