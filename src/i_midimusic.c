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
//      Native MIDI backend.

#include <math.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_thread.h"
#include "i_timer.h"
#include "m_array.h"
#include "m_config.h"
#include "midiout.h"
#include "midiseq.h"

typedef enum
{
    STATE_STARTUP,
    STATE_PLAYING,
    STATE_PAUSING,
    STATE_STOPPED,
    STATE_PAUSED
} midi_state_t;

static thread_t *player_thread_handle;
static mutex_t *music_lock;
static atomicint_t player_thread_running;

static boolean music_initialized;

static midi_state_t midi_state;
static midi_state_t old_state;

static void *song_data;
static int song_length;
static boolean song_looping;

// Wall clock of the current song, excluding paused time.

static uint64_t start_time, pause_time;

static void RestartTimer(uint64_t offset)
{
    if (pause_time)
    {
        start_time += (I_GetTimeUS() - pause_time);
        pause_time = 0;
    }
    else
    {
        start_time = I_GetTimeUS() - offset;
    }
}

static void PauseTimer(void)
{
    pause_time = I_GetTimeUS();
}

static uint64_t CurrentTime(void)
{
    return I_GetTimeUS() - start_time;
}

static int PlayerThread(void *unused)
{
    I_SetCurrentThreadPriority();

    midi_position_t position;
    boolean pending = false;
    boolean sleep = false;

    while (I_AtomicLoad(&player_thread_running))
    {
        if (sleep)
        {
            I_SleepUS(500);
            sleep = false;
        }

        // The MIDI thread must have exclusive access to shared resources until
        // the end of the current loop iteration or when the thread exits.

        I_MutexLock(music_lock);

        switch (midi_state)
        {
            case STATE_STARTUP:
                if (!MIDISeq_RegisterSong(song_data, song_length))
                {
                    midi_state = STATE_STOPPED;
                    break;
                }
                MIDISeq_ResetDevice(true);
                MIDISeq_Play(song_looping);
                RestartTimer(0);
                midi_state = STATE_PLAYING;
                break;

            case STATE_PLAYING:
                if (!pending)
                {
                    if (!MIDISeq_NextEvent(&position))
                    {
                        midi_state = STATE_STOPPED;
                        break;
                    }
                    pending = true;
                }

                if ((int64_t)position.time_us - (int64_t)CurrentTime() > 1000)
                {
                    sleep = true;
                    break;
                }

                MIDISeq_SendEvent(&position);
                pending = false;
                break;

            case STATE_PAUSING:
                // Send notes/sound off to prevent hanging notes.
                MIDISeq_SendNotesSoundOff();
                PauseTimer();
                midi_state = STATE_PAUSED;
                break;

            case STATE_STOPPED:
            case STATE_PAUSED:
                sleep = true;
                break;
        }

        I_MutexUnlock(music_lock);
    }

    return 0;
}

static const char **midi_devices = NULL;

static void GetDevices(void)
{
    if (array_size(midi_devices))
    {
        return;
    }

    int num_devs = MIDI_CountDevices();

    for (int i = 0; i < num_devs; ++i)
    {
        const char *name = MIDI_GetDeviceName(i);
        if (name)
        {
            array_push(midi_devices, name);
        }
    }
}

static boolean I_MID_InitMusic(int device)
{
    GetDevices();

    if (!array_size(midi_devices))
    {
        I_Printf(VB_ERROR, "I_MID_InitMusic: No devices found.");
        return false;
    }

    if (device >= array_size(midi_devices))
    {
        return false;
    }

    if (!MIDI_OpenDevice(device))
    {
        return false;
    }

    MIDISeq_SetEventHandler(MIDISeq_NativeEventHandler);

    music_initialized = true;

    I_Printf(VB_INFO, "MIDI Init: Using '%s'.", midi_devices[device]);

    return true;
}

static int midi_gain = 100;

// MIDI CC#7 volume formula (GM Level 1 Developer Guidelines, page 9).
#define MIDI_DB_TO_GAIN(db) powf(10.0f, (db) / 40.0f)

static void UpdateVolumeFactor(int volume)
{
    MIDISeq_SetVolumeFactor(volume / 15.0f * MIDI_DB_TO_GAIN(midi_gain));
}

static void I_MID_SetMusicVolume(int volume)
{
    if (!I_AtomicLoad(&player_thread_running))
    {
        UpdateVolumeFactor(volume);
        return;
    }

    I_MutexLock(music_lock);
    UpdateVolumeFactor(volume);
    MIDISeq_UpdateVolume();
    I_MutexUnlock(music_lock);
}

static void I_MID_StopSong(void *handle)
{
    if (!music_initialized || !I_AtomicLoad(&player_thread_running))
    {
        return;
    }

    I_AtomicExchange(&player_thread_running, 0);
    I_ThreadJoin(player_thread_handle);
    I_MutexDestroy(music_lock);

    // Send notes/sound off to prevent hanging notes.
    MIDISeq_SendNotesSoundOff();
}

static void I_MID_PlaySong(void *handle, boolean looping)
{
    if (!music_initialized)
    {
        return;
    }

    I_AtomicExchange(&player_thread_running, 1);
    song_looping = looping;
    midi_state = STATE_STARTUP;
    music_lock = I_MutexCreate();
    player_thread_handle = I_ThreadCreate(PlayerThread, "MIDI", NULL);
}

static void I_MID_PauseSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    I_MutexLock(music_lock);
    old_state = midi_state;
    midi_state = STATE_PAUSING;
    I_MutexUnlock(music_lock);
}

static void I_MID_ResumeSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    I_MutexLock(music_lock);
    if (midi_state == STATE_PAUSED)
    {
        RestartTimer(0);
        midi_state = old_state;
    }
    I_MutexUnlock(music_lock);
}

static const char *music_format = "Unknown";

static void *I_MID_RegisterSong(void *data, int len)
{
    if (!music_initialized)
    {
        return NULL;
    }

    if (IsMid(data, len))
    {
        music_format = "MIDI (Native)";
    }
    else if (IsMus(data, len))
    {
        music_format = "MUS (Native)";
    }
    else
    {
        return NULL;
    }

    song_data = data;
    song_length = len;

    return (void *)1;
}

static void I_MID_UnRegisterSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    MIDISeq_UnregisterSong();

    song_data = NULL;
    song_length = 0;
}

static void I_MID_ShutdownMusic(void)
{
    if (!music_initialized)
    {
        return;
    }

    I_MID_StopSong(NULL);
    I_MID_UnRegisterSong(NULL);

    MIDISeq_ResetDevice(false);

    MIDI_CloseDevice();

    music_initialized = false;
}

static const char **I_MID_DeviceList(void)
{
    GetDevices();

    return midi_devices;
}

static midiplayertype_t I_MID_MidiPlayerType(void)
{
    return midiplayer_native;
}

static const char *I_MID_MusicFormat(void)
{
    return music_format;
}

static void I_MID_BindVariables(void)
{
    MIDISeq_BindVariables();
    BIND_NUM(midi_gain, 0, -20, 0, "[Native MIDI] Gain [dB]");
}

music_module_t music_mid_module =
{
    I_MID_InitMusic,
    I_MID_ShutdownMusic,
    I_MID_SetMusicVolume,
    I_MID_PauseSong,
    I_MID_ResumeSong,
    I_MID_RegisterSong,
    I_MID_PlaySong,
    I_MID_StopSong,
    I_MID_UnRegisterSong,
    I_MID_DeviceList,
    I_MID_BindVariables,
    I_MID_MidiPlayerType,
    I_MID_MusicFormat,
};
