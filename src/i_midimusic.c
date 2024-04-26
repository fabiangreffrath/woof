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
//

#include "SDL.h"

#include <math.h>
#include <stdlib.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_timer.h"
#include "m_array.h"
#include "memio.h"
#include "midiout.h"
#include "midifallback.h"
#include "midifile.h"
#include "mus2mid.h"

static SDL_Thread *player_thread_handle;
static SDL_mutex *music_lock;
static SDL_atomic_t player_thread_running;

static boolean music_initialized;

// Tempo control variables

static unsigned int ticks_per_beat;
static unsigned int us_per_beat;

enum
{
    COMP_VANILLA,
    COMP_STANDARD,
    COMP_FULL,
};

enum
{
    RESET_TYPE_NO_SYSEX,
    RESET_TYPE_GM,
    RESET_TYPE_GS,
    RESET_TYPE_XG,
};

int midi_complevel = COMP_STANDARD;
int midi_reset_type = RESET_TYPE_GM;
int midi_reset_delay = -1;
boolean midi_ctf = true;

static const byte gm_system_on[] =
{
    0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7
};

static const byte gs_reset[] =
{
    0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7
};

static const byte xg_system_on[] =
{
    0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7
};

static const byte ff_loopStart[] =
{
    'l', 'o', 'o', 'p', 'S', 't', 'a', 'r', 't'
};

static const byte ff_loopEnd[] =
{
    'l', 'o', 'o', 'p', 'E', 'n', 'd'
};

static boolean allow_bank_select;

static byte channel_volume[MIDI_CHANNELS_PER_TRACK];
static float volume_factor = 0.0f;

typedef enum
{
    STATE_STARTUP,
    STATE_PLAYING,
    STATE_PAUSING,
    STATE_WAITING,
    STATE_STOPPED,
    STATE_PAUSED
} midi_state_t;

static midi_state_t midi_state;

static midi_state_t old_state;

#define EMIDI_DEVICE (1U << EMIDI_DEVICE_GENERAL_MIDI)

typedef struct
{
    midi_track_iter_t *iter;
    unsigned int elapsed_time;
    unsigned int saved_elapsed_time;
    boolean end_of_track;
    boolean saved_end_of_track;
    unsigned int emidi_device_flags;
    boolean emidi_designated;
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
} midi_song_t;

static midi_song_t song;

typedef struct
{
    midi_event_t *event;
    midi_track_t *track;
} midi_position_t;

static uint64_t start_time, pause_time;

static uint64_t TicksToUS(uint32_t ticks)
{
    return (uint64_t)ticks * us_per_beat / ticks_per_beat;
}

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

// Sends a channel message with the second parameter overridden to zero. Only
// use this function for events that require special handling.

static void SendChannelMsgZero(const midi_event_t *event)
{
    const byte message[] = {event->event_type | event->data.channel.channel,
                            event->data.channel.param1, 0};
    MIDI_SendShortMsg(message, sizeof(message));
}

static void SendChannelMsg(const midi_event_t *event, boolean use_param2)
{
    if (use_param2)
    {
        const byte message[] = {event->event_type | event->data.channel.channel,
                                event->data.channel.param1,
                                event->data.channel.param2};
        MIDI_SendShortMsg(message, sizeof(message));
    }
    else
    {
        const byte message[] = {event->event_type | event->data.channel.channel,
                                event->data.channel.param1};
        MIDI_SendShortMsg(message, sizeof(message));
    }
}

static void SendControlChange(byte channel, byte number, byte value)
{
    const byte message[] = {MIDI_EVENT_CONTROLLER | channel, number, value};
    MIDI_SendShortMsg(message, sizeof(message));
}

// Writes a MIDI program change message. If applicable, emulates capital tone
// fallback (CTF) to fix invalid instruments.

static void SendProgramChange(byte channel, byte program)
{
    byte message[] = {MIDI_EVENT_PROGRAM_CHANGE | channel, program};

    if (midi_ctf)
    {
        const midi_fallback_t fallback = MIDI_ProgramFallback(channel, program);

        if (fallback.type == FALLBACK_DRUMS)
        {
            message[1] = fallback.value;
        }
        else if (fallback.type == FALLBACK_BANK_MSB)
        {
            SendControlChange(channel, MIDI_CONTROLLER_BANK_SELECT_MSB,
                              fallback.value);
        }
    }

    MIDI_SendShortMsg(message, sizeof(message));
}

static void SendBankSelectMSB(byte channel, byte value)
{
    if (allow_bank_select)
    {
        if (midi_ctf)
        {
            MIDI_UpdateBankMSB(channel, value);
        }
    }
    else
    {
        value = 0;
    }

    SendControlChange(channel, MIDI_CONTROLLER_BANK_SELECT_MSB, value);
}

static void SendBankSelectLSB(byte channel, byte value)
{
    if (allow_bank_select)
    {
        if (midi_ctf)
        {
            const midi_fallback_t fallback =
                MIDI_BankLSBFallback(channel, value);

            if (fallback.type == FALLBACK_BANK_LSB)
            {
                value = fallback.value;
            }
        }
    }
    else
    {
        value = 0;
    }

    SendControlChange(channel, MIDI_CONTROLLER_BANK_SELECT_LSB, value);
}

// Writes an RPN message set to NULL (0x7F). Prevents accidental data entry.

static void SendNullRPN(const midi_event_t *event)
{
    const byte channel = event->data.channel.channel;
    SendControlChange(channel, MIDI_CONTROLLER_RPN_LSB, MIDI_RPN_NULL);
    SendControlChange(channel, MIDI_CONTROLLER_RPN_MSB, MIDI_RPN_NULL);
}

static void UpdateTempo(const midi_event_t *event)
{
    byte *data = event->data.meta.data;
    us_per_beat = (data[0] << 16) | (data[1] << 8) | data[2];
    RestartTimer(TicksToUS(song.elapsed_time));
}

// Writes a MIDI volume message. The value is scaled by the volume slider.

static void SendManualVolumeMsg(byte channel, byte volume)
{
    unsigned int scaled_volume;

    scaled_volume = lroundf((float)volume * volume_factor);
    scaled_volume = MIN(scaled_volume, 127);

    SendControlChange(channel, MIDI_CONTROLLER_VOLUME_MSB, scaled_volume);

    channel_volume[channel] = volume;
}

static void SendVolumeMsg(const midi_event_t *event)
{
    SendManualVolumeMsg(event->data.channel.channel,
                        event->data.channel.param2);
}

// Sets each channel to its saved volume level, scaled by the volume slider.

static void UpdateVolume(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        SendManualVolumeMsg(i, channel_volume[i]);
    }
}

// Sets each channel to the default volume level, scaled by the volume slider.

static void ResetVolume(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        SendManualVolumeMsg(i, MIDI_DEFAULT_VOLUME);
    }
}

// Writes "notes off" and "sound off" messages for each channel. Some devices
// may support only one or the other. Held notes (sustained, etc.) are released
// to prevent hanging notes.

static void SendNotesSoundOff(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        SendControlChange(i, MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
        SendControlChange(i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);
    }
}

// Writes "reset all controllers" message for each channel. Despite the name,
// this only resets some controllers (see MIDI Recommended Practice RP-015).

static void ResetControllers(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; i++)
    {
        SendControlChange(i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
    }
}

// Resets commonly used controllers. This is only for a reset type of "No SysEx"
// for devices that don't support SysEx resets.

static void ResetNoSysEx(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Reset commonly used controllers.
        SendControlChange(i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
        SendControlChange(i, MIDI_CONTROLLER_PAN, 64);
        SendControlChange(i, MIDI_CONTROLLER_BANK_SELECT_MSB, 0);
        SendControlChange(i, MIDI_CONTROLLER_BANK_SELECT_LSB, 0);
        SendProgramChange(i, 0);
        SendControlChange(i, MIDI_CONTROLLER_REVERB, 40);
        SendControlChange(i, MIDI_CONTROLLER_CHORUS, 0);
    }
}

// Resets the pitch bend sensitivity for each channel. This must be sent during
// a reset due to an MS GS Wavetable Synth bug.

static void ResetPitchBendSensitivity(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Set RPN MSB/LSB to pitch bend sensitivity.
        SendControlChange(i, MIDI_CONTROLLER_RPN_LSB, MIDI_RPN_PITCH_BEND_SENS_LSB);
        SendControlChange(i, MIDI_CONTROLLER_RPN_MSB, MIDI_RPN_MSB);

        // Reset pitch bend sensitivity to +/- 2 semitones and 0 cents.
        SendControlChange(i, MIDI_CONTROLLER_DATA_ENTRY_MSB, 2);
        SendControlChange(i, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);

        // Set RPN MSB/LSB to null value after data entry.
        SendControlChange(i, MIDI_CONTROLLER_RPN_LSB, MIDI_RPN_NULL);
        SendControlChange(i, MIDI_CONTROLLER_RPN_MSB, MIDI_RPN_NULL);
    }
}

// Wait time based on bytes sent to the device. For compatibility with hardware
// devices (e.g. SC-55).

static void ResetDelayBytes(uint32_t length)
{
    if (midi_reset_delay == -1)
    {
        // MIDI transfer period is 320 us per byte (MIDI 1.0 Electrical Spec
        // Update CA-033, page 2).
        I_SleepUS(320 * length);
    }
}

// Resets the MIDI device. Call this function before each song starts and once
// at shut down.

static void ResetDevice(void)
{
    // For notes/sound off called prior to this function.
    ResetDelayBytes(96);

    if (midi_ctf)
    {
        MIDI_ResetFallback();
    }

    switch (midi_reset_type)
    {
        case RESET_TYPE_NO_SYSEX:
            ResetNoSysEx();
            ResetDelayBytes(320);
            break;

        case RESET_TYPE_GS:
            MIDI_SendLongMsg(gs_reset, sizeof(gs_reset));
            ResetDelayBytes(sizeof(gs_reset));
            break;

        case RESET_TYPE_XG:
            MIDI_SendLongMsg(xg_system_on, sizeof(xg_system_on));
            ResetDelayBytes(sizeof(xg_system_on));
            break;

        default:
            MIDI_SendLongMsg(gm_system_on, sizeof(gm_system_on));
            ResetDelayBytes(sizeof(gm_system_on));
            break;
    }

    // Roland/Yamaha require an additional ~50 ms to execute a SysEx reset.
    if (midi_reset_delay == -1 && midi_reset_type != RESET_TYPE_NO_SYSEX)
    {
        I_Sleep(60);
    }

    // MS GS Wavetable Synth doesn't reset pitch bend sensitivity.
    ResetPitchBendSensitivity();
    ResetDelayBytes(288);

    // Reset volume (initial playback or on shutdown if no SysEx reset).
    // Scale by slider on initial playback, max on shutdown.
    if (midi_state == STATE_STARTUP)
    {
        ResetVolume();
        ResetDelayBytes(48);
    }
    else if (midi_reset_type == RESET_TYPE_NO_SYSEX)
    {
        volume_factor = 1.0f;
        ResetVolume();
        ResetDelayBytes(48);
    }

    // Send delay after reset. This is for hardware devices only (e.g. SC-55).
    if (midi_reset_delay > 0)
    {
        I_Sleep(midi_reset_delay);
    }
}

static void SendSysExMsg(const midi_event_t *event)
{
    const byte *data = event->data.sysex.data;
    const unsigned int length = event->data.sysex.length;

    switch (event->data.sysex.type)
    {
        case MIDI_SYSEX_RESET:
            if (midi_ctf)
            {
                MIDI_ResetFallback();
            }
            MIDI_SendLongMsg(data, length);
            ResetVolume();
            break;

        case MIDI_SYSEX_RHYTHM_PART:
            if (midi_ctf)
            {
                MIDI_UpdateDrumMap(event->data.sysex.channel, data[8]);
            }
            // Fall through.

        case MIDI_SYSEX_OTHER:
            MIDI_SendLongMsg(data, length);
            break;

        case MIDI_SYSEX_PART_LEVEL:
            // Replace SysEx part level message with channel volume message.
            SendManualVolumeMsg(event->data.sysex.channel, data[8]);
            break;
    }
}

// Sets a Final Fantasy or RPG Maker loop point.

static void SetLoopPoint(void)
{
    unsigned int i;

    for (i = 0; i < song.num_tracks; ++i)
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

static void SendEMIDI(const midi_event_t *event, midi_track_t *track)
{
    unsigned int i;
    unsigned int flag;
    int count;

    switch (event->data.channel.param1)
    {
        case EMIDI_CONTROLLER_TRACK_DESIGNATION:
            if (track->elapsed_time < ticks_per_beat)
            {
                flag = event->data.channel.param2;

                if (flag == EMIDI_DEVICE_ALL)
                {
                    track->emidi_device_flags = UINT_MAX;
                    track->emidi_designated = true;
                }
                else if (flag <= EMIDI_DEVICE_ULTRASOUND)
                {
                    track->emidi_device_flags |= 1U << flag;
                    track->emidi_designated = true;
                }
            }
            break;

        case EMIDI_CONTROLLER_TRACK_EXCLUSION:
            if (song.rpg_loop)
            {
                SetLoopPoint();
            }
            else if (track->elapsed_time < ticks_per_beat)
            {
                flag = event->data.channel.param2;

                if (!track->emidi_designated)
                {
                    track->emidi_device_flags = UINT_MAX;
                    track->emidi_designated = true;
                }

                if (flag <= EMIDI_DEVICE_ULTRASOUND)
                {
                    track->emidi_device_flags &= ~(1U << flag);
                }
            }
            break;

        case EMIDI_CONTROLLER_PROGRAM_CHANGE:
            if (track->emidi_program || track->elapsed_time < ticks_per_beat)
            {
                track->emidi_program = true;
                SendProgramChange(event->data.channel.channel,
                                  event->data.channel.param2);
            }
            break;

        case EMIDI_CONTROLLER_VOLUME:
            if (track->emidi_volume || track->elapsed_time < ticks_per_beat)
            {
                track->emidi_volume = true;
                SendVolumeMsg(event);
            }
            break;

        case EMIDI_CONTROLLER_LOOP_BEGIN:
            count = event->data.channel.param2;
            count = (count == 0) ? (-1) : count;
            track->emidi_loop_count = count;
            MIDI_SetLoopPoint(track->iter);
            break;

        case EMIDI_CONTROLLER_LOOP_END:
            if (event->data.channel.param2 == EMIDI_LOOP_FLAG)
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
            break;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN:
            count = event->data.channel.param2;
            count = (count == 0) ? (-1) : count;
            for (i = 0; i < song.num_tracks; ++i)
            {
                song.tracks[i].emidi_loop_count = count;
                MIDI_SetLoopPoint(song.tracks[i].iter);
                song.tracks[i].saved_end_of_track = song.tracks[i].end_of_track;
                song.tracks[i].saved_elapsed_time = song.tracks[i].elapsed_time;
            }
            song.saved_elapsed_time = song.elapsed_time;
            break;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_END:
            if (event->data.channel.param2 == EMIDI_LOOP_FLAG)
            {
                for (i = 0; i < song.num_tracks; ++i)
                {
                    if (song.tracks[i].emidi_loop_count != 0)
                    {
                        MIDI_RestartAtLoopPoint(song.tracks[i].iter);
                        song.tracks[i].end_of_track = song.tracks[i].saved_end_of_track;
                        song.tracks[i].elapsed_time = song.tracks[i].saved_elapsed_time;
                        song.elapsed_time = song.saved_elapsed_time;
                        RestartTimer(TicksToUS(song.elapsed_time));
                    }

                    if (song.tracks[i].emidi_loop_count > 0)
                    {
                        song.tracks[i].emidi_loop_count--;
                    }
                }
            }
            break;
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
            if (midi_complevel != COMP_VANILLA)
            {
                CheckFFLoop(event);
            }
            break;

        default:
            break;
    }
}

// ProcessEvent function for vanilla (DMX MPU-401) compatibility level. Do not
// call this function directly. See the ProcessEvent function pointer.

static void ProcessEvent_Vanilla(const midi_event_t *event, midi_track_t *track)
{
    switch (event->event_type)
    {
        case MIDI_EVENT_META:
            ProcessMetaEvent(event, track);
            break;

        case MIDI_EVENT_CONTROLLER:
            switch (event->data.channel.param1)
            {
                case MIDI_CONTROLLER_BANK_SELECT_MSB:
                    // DMX has broken bank select support and runs in GM mode.
                    // Actual behavior (MSB is okay):
                    //SendBankSelectMSB(event->data.channel.channel,
                    //                  event->data.channel.param2);
                    SendBankSelectMSB(event->data.channel.channel, 0);
                    break;

                case MIDI_CONTROLLER_BANK_SELECT_LSB:
                    // DMX has broken bank select support and runs in GM mode.
                    // Actual behavior (LSB is sent as MSB!):
                    //SendBankSelectMSB(event->data.channel.channel,
                    //                  event->data.channel.param2);
                    SendBankSelectLSB(event->data.channel.channel, 0);
                    break;

                case MIDI_CONTROLLER_MODULATION:
                case MIDI_CONTROLLER_PAN:
                case MIDI_CONTROLLER_EXPRESSION:
                case MIDI_CONTROLLER_HOLD1_PEDAL:
                case MIDI_CONTROLLER_SOFT_PEDAL:
                case MIDI_CONTROLLER_REVERB:
                case MIDI_CONTROLLER_CHORUS:
                    SendChannelMsg(event, true);
                    break;

                case MIDI_CONTROLLER_VOLUME_MSB:
                    SendVolumeMsg(event);
                    break;

                case MIDI_CONTROLLER_RESET_ALL_CTRLS:
                    // MS GS Wavetable Synth resets volume if param2 isn't zero.
                    // Per MIDI 1.0 Spec, param2 must be zero.
                    SendChannelMsgZero(event);
                    break;

                case MIDI_CONTROLLER_ALL_SOUND_OFF:
                case MIDI_CONTROLLER_ALL_NOTES_OFF:
                    // Per MIDI 1.0 Spec, param2 must be zero.
                    SendChannelMsgZero(event);
                    break;

                default:
                    break;
            }
            break;

        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_PITCH_BEND:
            SendChannelMsg(event, true);
            break;

        case MIDI_EVENT_PROGRAM_CHANGE:
            SendProgramChange(event->data.channel.channel,
                              event->data.channel.param1);
            break;

        default:
            break;
    }
}

// ProcessEvent function for standard and full MIDI compatibility levels. Do not
// call this function directly. See the ProcessEvent function pointer.

static void ProcessEvent_Standard(const midi_event_t *event,
                                  midi_track_t *track)
{
    switch ((int)event->event_type)
    {
        case MIDI_EVENT_SYSEX:
            if (midi_complevel == COMP_FULL)
            {
                SendSysExMsg(event);
            }
            return;

        case MIDI_EVENT_META:
            ProcessMetaEvent(event, track);
            return;
    }

    if (track->emidi_designated && (EMIDI_DEVICE & ~track->emidi_device_flags))
    {
        return;
    }

    switch (event->event_type)
    {
        case MIDI_EVENT_CONTROLLER:
            switch (event->data.channel.param1)
            {
                case MIDI_CONTROLLER_MODULATION:
                case MIDI_CONTROLLER_DATA_ENTRY_MSB:
                case MIDI_CONTROLLER_PAN:
                case MIDI_CONTROLLER_EXPRESSION:
                case MIDI_CONTROLLER_DATA_ENTRY_LSB:
                case MIDI_CONTROLLER_HOLD1_PEDAL:
                case MIDI_CONTROLLER_SOFT_PEDAL:
                case MIDI_CONTROLLER_REVERB:
                case MIDI_CONTROLLER_CHORUS:
                case MIDI_CONTROLLER_POLY_MODE_OFF:
                    SendChannelMsg(event, true);
                    break;

                case MIDI_CONTROLLER_VOLUME_MSB:
                    if (!track->emidi_volume)
                    {
                        SendVolumeMsg(event);
                    }
                    break;

                case MIDI_CONTROLLER_VOLUME_LSB:
                    break;

                case MIDI_CONTROLLER_BANK_SELECT_MSB:
                    SendBankSelectMSB(event->data.channel.channel,
                                      event->data.channel.param2);
                    break;

                case MIDI_CONTROLLER_BANK_SELECT_LSB:
                    SendBankSelectLSB(event->data.channel.channel,
                                      event->data.channel.param2);
                    break;

                case MIDI_CONTROLLER_NRPN_LSB:
                case MIDI_CONTROLLER_NRPN_MSB:
                    if (midi_complevel == COMP_FULL)
                    {
                        SendChannelMsg(event, true);
                    }
                    else
                    {
                        // MS GS Wavetable Synth nulls RPN for any NRPN.
                        SendNullRPN(event);
                    }
                    break;

                case MIDI_CONTROLLER_RPN_LSB:
                    switch (event->data.channel.param2)
                    {
                        case MIDI_RPN_PITCH_BEND_SENS_LSB:
                        case MIDI_RPN_FINE_TUNING_LSB:
                        case MIDI_RPN_COARSE_TUNING_LSB:
                        case MIDI_RPN_NULL:
                            SendChannelMsg(event, true);
                            break;

                        default:
                            if (midi_complevel == COMP_FULL)
                            {
                                SendChannelMsg(event, true);
                            }
                            else
                            {
                                // MS GS Wavetable Synth ignores other RPNs.
                                SendNullRPN(event);
                            }
                            break;
                    }
                    break;

                case MIDI_CONTROLLER_RPN_MSB:
                    switch (event->data.channel.param2)
                    {
                        case MIDI_RPN_MSB:
                        case MIDI_RPN_NULL:
                            SendChannelMsg(event, true);
                            break;

                        default:
                            if (midi_complevel == COMP_FULL)
                            {
                                SendChannelMsg(event, true);
                            }
                            else
                            {
                                // MS GS Wavetable Synth ignores other RPNs.
                                SendNullRPN(event);
                            }
                            break;
                    }
                    break;

                case EMIDI_CONTROLLER_TRACK_DESIGNATION:
                case EMIDI_CONTROLLER_TRACK_EXCLUSION:
                case EMIDI_CONTROLLER_PROGRAM_CHANGE:
                case EMIDI_CONTROLLER_VOLUME:
                case EMIDI_CONTROLLER_LOOP_BEGIN:
                case EMIDI_CONTROLLER_LOOP_END:
                case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN:
                case EMIDI_CONTROLLER_GLOBAL_LOOP_END:
                    SendEMIDI(event, track);
                    break;

                case MIDI_CONTROLLER_RESET_ALL_CTRLS:
                    // MS GS Wavetable Synth resets volume if param2 isn't zero.
                    // Per MIDI 1.0 Spec, param2 must be zero.
                    SendChannelMsgZero(event);
                    break;

                case MIDI_CONTROLLER_ALL_SOUND_OFF:
                case MIDI_CONTROLLER_ALL_NOTES_OFF:
                case MIDI_CONTROLLER_POLY_MODE_ON:
                    // Per MIDI 1.0 Spec, param2 must be zero.
                    SendChannelMsgZero(event);
                    break;

                case MIDI_CONTROLLER_OMNI_MODE_OFF:
                case MIDI_CONTROLLER_OMNI_MODE_ON:
                    if (midi_complevel == COMP_FULL)
                    {
                        // Per MIDI 1.0 Spec, param2 must be zero.
                        SendChannelMsgZero(event);
                    }
                    break;

                default:
                    if (midi_complevel == COMP_FULL)
                    {
                        SendChannelMsg(event, true);
                    }
                    break;
            }
            break;

        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_PITCH_BEND:
            SendChannelMsg(event, true);
            break;

        case MIDI_EVENT_PROGRAM_CHANGE:
            if (!track->emidi_program)
            {
                SendProgramChange(event->data.channel.channel,
                                  event->data.channel.param1);
            }
            break;

        case MIDI_EVENT_AFTERTOUCH:
            if (midi_complevel == COMP_FULL)
            {
                SendChannelMsg(event, true);
            }
            break;

        case MIDI_EVENT_CHAN_AFTERTOUCH:
            if (midi_complevel == COMP_FULL)
            {
                SendChannelMsg(event, false);
            }
            break;

        default:
            break;
    }
}

// Function pointer determined by the desired MIDI compatibility level. Set
// during initialization by the main thread, then called from the MIDI thread
// only.

static void (*ProcessEvent)(const midi_event_t *event,
                            midi_track_t *track) = ProcessEvent_Standard;

// Restarts a song that uses a Final Fantasy or RPG Maker loop point.

static void RestartLoop(void)
{
    unsigned int i;

    for (i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartAtLoopPoint(song.tracks[i].iter);
        song.tracks[i].end_of_track = song.tracks[i].saved_end_of_track;
        song.tracks[i].elapsed_time = song.tracks[i].saved_elapsed_time;
    }
    song.elapsed_time = song.saved_elapsed_time;
    RestartTimer(TicksToUS(song.elapsed_time));
}

// Restarts a song that uses standard looping.

static void RestartTracks(void)
{
    unsigned int i;

    for (i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartIterator(song.tracks[i].iter);
        song.tracks[i].elapsed_time = 0;
        song.tracks[i].end_of_track = false;
        song.tracks[i].emidi_device_flags = 0;
        song.tracks[i].emidi_designated = false;
        song.tracks[i].emidi_program = false;
        song.tracks[i].emidi_volume = false;
        song.tracks[i].emidi_loop_count = 0;
    }
    song.elapsed_time = 0;
    RestartTimer(0);
}

// Get the next event from the MIDI file, process it or return if the delta
// time is > 0.

static midi_state_t NextEvent(midi_position_t *position)
{
    midi_event_t *event = NULL;
    midi_track_t *track = NULL;
    unsigned int min_time = UINT_MAX;
    unsigned int delta_time;

    // Find next event across all tracks.
    for (int i = 0; i < song.num_tracks; ++i)
    {
        if (!song.tracks[i].end_of_track)
        {
            unsigned int time = song.tracks[i].elapsed_time
                                + MIDI_GetDeltaTime(song.tracks[i].iter);
            if (time < min_time)
            {
                min_time = time;
                track = &song.tracks[i];
            }
        }
    }

    // No more events. Restart or stop song.
    if (track == NULL)
    {
        if (song.elapsed_time)
        {
            if (song.ff_restart || song.rpg_loop)
            {
                song.ff_restart = false;
                RestartLoop();
                return STATE_PLAYING;
            }
            else if (song.looping)
            {
                ResetControllers();
                RestartTracks();
                return STATE_PLAYING;
            }
        }
        return STATE_STOPPED;
    }

    track->elapsed_time = min_time;
    delta_time = min_time - song.elapsed_time;
    song.elapsed_time = min_time;

    if (!MIDI_GetNextEvent(track->iter, &event))
    {
        track->end_of_track = true;
        return STATE_PLAYING;
    }

    // Restart FF loop after sending all events that share same ticks_per_beat.
    if (song.ff_restart && MIDI_GetDeltaTime(track->iter) > 0)
    {
        song.ff_restart = false;
        RestartLoop();
        return STATE_PLAYING;
    }

    if (!delta_time)
    {
        ProcessEvent(event, track);
        return STATE_PLAYING;
    }

    position->track = track;
    position->event = event;
    return STATE_WAITING;
}

static boolean RegisterSong(void)
{
    if (IsMid(song.lump_data, song.lump_length))
    {
        song.file = MIDI_LoadFile(song.lump_data, song.lump_length);
    }
    else
    {
        // Assume a MUS file and try to convert
        MEMFILE *instream;
        MEMFILE *outstream;
        void *outbuf;
        size_t outbuf_len;

        instream = mem_fopen_read(song.lump_data, song.lump_length);
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
        I_Printf(VB_ERROR, "I_MID_RegisterSong: Failed to load MID.");
        return false;
    }

    ticks_per_beat = MIDI_GetFileTimeDivision(song.file);
    us_per_beat = MIDI_DEFAULT_TEMPO;

    song.num_tracks = MIDI_NumTracks(song.file);
    song.tracks = calloc(song.num_tracks, sizeof(midi_track_t));
    for (uint16_t i = 0; i < song.num_tracks; i++)
    {
        song.tracks[i].iter = MIDI_IterateTrack(song.file, i);
    }

    song.rpg_loop = MIDI_RPGLoop(song.file);
    return true;
}

static int PlayerThread(void *unused)
{
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);

    midi_position_t position;
    boolean sleep = false;

    while (SDL_AtomicGet(&player_thread_running))
    {
        if (sleep)
        {
            I_SleepUS(500);
            sleep = false;
        }

        // The MIDI thread must have exclusive access to shared resources until
        // the end of the current loop iteration or when the thread exits.

        SDL_LockMutex(music_lock);

        switch (midi_state)
        {
            case STATE_STARTUP:
                if (!RegisterSong())
                {
                    midi_state = STATE_STOPPED;
                    break;
                }
                ResetDevice();
                midi_state = STATE_PLAYING;
                RestartTimer(0);
                break;

            case STATE_PLAYING:
                midi_state = NextEvent(&position);
                break;

            case STATE_PAUSING:
                // Send notes/sound off to prevent hanging notes.
                SendNotesSoundOff();
                PauseTimer();
                midi_state = STATE_PAUSED;
                break;

            case STATE_WAITING:
                {
                    int64_t remaining_time =
                        TicksToUS(song.elapsed_time) - CurrentTime();
                    if (remaining_time > 1000)
                    {
                        sleep = true;
                        break;
                    }
                    if (remaining_time > 0)
                    {
                        I_SleepUS(remaining_time);
                    }
                    ProcessEvent(position.event, position.track);
                    midi_state = STATE_PLAYING;
                }
                break;

            case STATE_STOPPED:
            case STATE_PAUSED:
                sleep = true;
                break;
        }

        SDL_UnlockMutex(music_lock);
    }

    return 0;
}

const char **midi_devices = NULL;

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

    switch (midi_complevel)
    {
        case COMP_VANILLA:
            ProcessEvent = ProcessEvent_Vanilla;
            allow_bank_select = false;
            break;

        case COMP_STANDARD:
            ProcessEvent = ProcessEvent_Standard;
            allow_bank_select = (midi_reset_type != RESET_TYPE_GM);
            break;

        case COMP_FULL:
            ProcessEvent = ProcessEvent_Standard;
            allow_bank_select = true;
            break;
    }

    music_initialized = true;

    I_Printf(VB_INFO, "MIDI Init: Using '%s'.", midi_devices[device]);

    return true;
}

static void I_MID_SetMusicVolume(int volume)
{
    static int last_volume = -1;

    if (last_volume == volume)
    {
        // Ignore holding key down in volume menu.
        return;
    }
    last_volume = volume;

    if (!SDL_AtomicGet(&player_thread_running))
    {
        volume_factor = sqrtf((float)volume / 15.0f);
        return;
    }

    SDL_LockMutex(music_lock);
    volume_factor = sqrtf((float)volume / 15.0f);
    UpdateVolume();
    SDL_UnlockMutex(music_lock);
}

static void I_MID_StopSong(void *handle)
{
    if (!music_initialized || !SDL_AtomicGet(&player_thread_running))
    {
        return;
    }

    SDL_AtomicSet(&player_thread_running, 0);
    SDL_WaitThread(player_thread_handle, NULL);
    SDL_DestroyMutex(music_lock);

    // Send notes/sound off to prevent hanging notes.
    SendNotesSoundOff();
}

static void I_MID_PlaySong(void *handle, boolean looping)
{
    if (!music_initialized)
    {
        return;
    }

    SDL_AtomicSet(&player_thread_running, 1);
    song.looping = looping;
    midi_state = STATE_STARTUP;
    music_lock = SDL_CreateMutex();
    player_thread_handle = SDL_CreateThread(PlayerThread, NULL, NULL);
}

static void I_MID_PauseSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    SDL_LockMutex(music_lock);
    old_state = midi_state;
    midi_state = STATE_PAUSING;
    SDL_UnlockMutex(music_lock);
}

static void I_MID_ResumeSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    SDL_LockMutex(music_lock);
    if (midi_state == STATE_PAUSED)
    {
        RestartTimer(0);
        midi_state = old_state;
    }
    SDL_UnlockMutex(music_lock);
}

static void *I_MID_RegisterSong(void *data, int len)
{
    if (!music_initialized)
    {
        return NULL;
    }

    if (!IsMid(data, len) && !IsMus(data, len))
    {
        return NULL;
    }

    song.lump_data = data;
    song.lump_length = len;

    return (void *)1;
}

static void I_MID_UnRegisterSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    if (song.tracks)
    {
        unsigned int i;
        for (i = 0; i < song.num_tracks; ++i)
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
    song.lump_data = NULL;
    song.lump_length = 0;
    song.elapsed_time = 0;
    song.saved_elapsed_time = 0;
    song.num_tracks = 0;
    song.looping = false;
    song.ff_loop = false;
    song.ff_restart = false;
    song.rpg_loop = false;
}

static void I_MID_ShutdownMusic(void)
{
    if (!music_initialized)
    {
        return;
    }

    I_MID_StopSong(NULL);
    I_MID_UnRegisterSong(NULL);

    ResetDevice();

    MIDI_CloseDevice();

    music_initialized = false;
}

static const char **I_MID_DeviceList(void)
{
    GetDevices();

    return midi_devices;
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
};
