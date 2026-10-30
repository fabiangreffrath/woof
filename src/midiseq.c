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
//      MIDI/MUS song sequencing and native MIDI output.

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_timer.h"
#include "m_config.h"
#include "memio.h"
#include "midifallback.h"
#include "midifile.h"
#include "midiseq.h"
#include "midiout.h"
#include "mus2mid.h"

//---------------------------------------------------------------------------
// Settings
//---------------------------------------------------------------------------

enum
{
    RESET_TYPE_NO_SYSEX,
    RESET_TYPE_GM,
    RESET_TYPE_GS,
    RESET_TYPE_XG,
    RESET_NUM,
};

enum
{
    COMP_VANILLA,
    COMP_STANDARD,
    COMP_FULL,
    COMP_NUM,
};

static int midi_complevel = COMP_STANDARD;
static int midi_reset_type = RESET_TYPE_GM;
static int midi_reset_delay = -1;
static boolean midi_ctf = true;
static boolean midi_sysex_volume = false;

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
// Native MIDI output
//---------------------------------------------------------------------------

static boolean channel_used[MIDI_CHANNELS_PER_TRACK];
static byte channel_volume[MIDI_CHANNELS_PER_TRACK];
static uint16_t master_volume;
static float volume_factor = 1.0f;

// Bank select is dropped for devices that run in General MIDI mode.

static boolean AllowBankSelect(void)
{
    switch (midi_complevel)
    {
        case COMP_VANILLA:
            return false;

        case COMP_FULL:
            return true;

        default:
            return midi_reset_type != RESET_TYPE_GM;
    }
}

// Sends a channel message with the second parameter overridden to zero. Only
// use this function for events that require special handling.

static void SendChannelMsgZero(const midi_event_t *event)
{
    const byte message[] = {event->event_type | event->data.channel.channel,
                            event->data.channel.param1, 0};
    MIDI_SendShortMsg(message, sizeof(message));
    channel_used[event->data.channel.channel] = true;
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

    channel_used[event->data.channel.channel] = true;
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
    channel_used[channel] = true;
}

static void SendBankSelectMSB(byte channel, byte value)
{
    if (AllowBankSelect())
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
    channel_used[channel] = true;
}

static void SendBankSelectLSB(byte channel, byte value)
{
    if (AllowBankSelect())
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
    channel_used[channel] = true;
}

// Writes an RPN message set to NULL (0x7F). Prevents accidental data entry.

static void SendNullRPN(const midi_event_t *event)
{
    const byte channel = event->data.channel.channel;
    SendControlChange(channel, MIDI_CONTROLLER_RPN_LSB, MIDI_RPN_NULL);
    SendControlChange(channel, MIDI_CONTROLLER_RPN_MSB, MIDI_RPN_NULL);
    channel_used[channel] = true;
}

// Writes a master volume message. The value is scaled by the volume slider.

static void SendMasterVolumeMsg(uint16_t volume)
{
    uint32_t scaled_volume = lroundf((float)volume * volume_factor);
    scaled_volume = MIN(scaled_volume, MIDI_DEFAULT_MASTER_VOLUME);

    const byte lsb = scaled_volume & 0x7F;
    const byte msb = (scaled_volume >> 7) & 0x7F;
    const byte data[] = {0xF0, 0x7F, 0x7F, 0x04, 0x01, lsb, msb, 0xF7};
    MIDI_SendLongMsg(data, sizeof(data));

    master_volume = volume;
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
    if (midi_sysex_volume)
    {
        SendChannelMsg(event, true);
    }
    else
    {
        SendManualVolumeMsg(event->data.channel.channel,
                            event->data.channel.param2);
    }

    channel_used[event->data.channel.channel] = true;
}

// Sets each channel to its saved volume level, scaled by the volume slider.

void MIDISeq_UpdateVolume(void)
{
    if (midi_sysex_volume)
    {
        SendMasterVolumeMsg(master_volume);
    }
    else
    {
        for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; i++)
        {
            SendManualVolumeMsg(i, channel_volume[i]);
        }
    }
}

// Sets each channel to the default volume level, scaled by the volume slider.

static void ResetVolume(void)
{
    if (midi_sysex_volume)
    {
        SendMasterVolumeMsg(MIDI_DEFAULT_MASTER_VOLUME);
    }
    else
    {
        for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; i++)
        {
            SendManualVolumeMsg(i, MIDI_DEFAULT_VOLUME);
        }
    }
}

// Writes "notes off" and "sound off" messages for each channel. Some devices
// may support only one or the other. Held notes (sustained, etc.) are released
// to prevent hanging notes.

void MIDISeq_SendNotesSoundOff(void)
{
    for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        SendControlChange(i, MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
        SendControlChange(i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);
    }
}

// Resets commonly used controllers. This is only for a reset type of "No SysEx"
// for devices that don't support SysEx resets.

static void ResetNoSysEx(void)
{
    for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
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
    for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
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

void MIDISeq_ResetDevice(boolean startup)
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
    if (startup)
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

    memset(channel_used, 0, sizeof(channel_used));
}

void MIDISeq_SetVolumeFactor(float factor)
{
    volume_factor = factor;
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
            memset(channel_used, 0, sizeof(channel_used));
            break;

        case MIDI_SYSEX_MASTER_VOLUME:
            if (midi_sysex_volume)
            {
                const uint16_t volume =
                    ((data[6] & 0x7F) << 7) | (data[5] & 0x7F);
                SendMasterVolumeMsg(volume);
            }
            else
            {
                MIDI_SendLongMsg(data, length);
            }
            break;

        case MIDI_SYSEX_MASTER_VOLUME_ROLAND:
            if (midi_sysex_volume)
            {
                const uint16_t volume = (data[8] & 0x7F) << 7;
                SendMasterVolumeMsg(volume);
            }
            else
            {
                MIDI_SendLongMsg(data, length);
            }
            break;

        case MIDI_SYSEX_MASTER_VOLUME_YAMAHA:
            if (midi_sysex_volume)
            {
                const uint16_t volume = (data[7] & 0x7F) << 7;
                SendMasterVolumeMsg(volume);
            }
            else
            {
                MIDI_SendLongMsg(data, length);
            }
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
            if (midi_sysex_volume)
            {
                MIDI_SendLongMsg(data, length);
            }
            else
            {
                // Replace SysEx part level message with channel volume message.
                SendManualVolumeMsg(event->data.sysex.channel, data[8]);
            }
            channel_used[event->data.sysex.channel] = true;
            break;
    }
}

// Native MIDI processing for the vanilla (DMX MPU-401) compatibility level.

static void ProcessEvent_Vanilla(const midi_event_t *event)
{
    switch (event->event_type)
    {
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

// Native MIDI processing for the standard and full compatibility levels.

static void ProcessEvent_Standard(const midi_event_t *event)
{
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
                    SendVolumeMsg(event);
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
            SendProgramChange(event->data.channel.channel,
                              event->data.channel.param1);
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

void MIDISeq_NativeEventHandler(const midi_event_t *event)
{
    if (event->event_type == MIDI_EVENT_SYSEX)
    {
        SendSysExMsg(event);
        return;
    }

    if (midi_complevel == COMP_VANILLA)
    {
        ProcessEvent_Vanilla(event);
    }
    else
    {
        ProcessEvent_Standard(event);
    }
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
            if (midi_complevel != COMP_VANILLA)
            {
                CheckFFLoop(event);
            }
            break;

        default:
            break;
    }
}

// Passes a channel event to the backend.

static void DispatchEvent(const midi_event_t *event)
{
    channel_used[event->data.channel.channel] = true;

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
// events of excluded tracks are handled here, the rest is passed to the
// backend.

static void ProcessEvent(const midi_event_t *event, midi_track_t *track)
{
    switch (event->event_type)
    {
        case MIDI_EVENT_META:
            ProcessMetaEvent(event, track);
            return;

        case MIDI_EVENT_SYSEX:
            if (midi_complevel == COMP_FULL)
            {
                event_handler(event);
            }
            return;

        default:
            break;
    }

    if (midi_complevel != COMP_VANILLA)
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
    BIND_NUM(midi_complevel, COMP_STANDARD, 0, COMP_NUM - 1,
        "[Native MIDI] Compatibility level (0 = Vanilla; 1 = Standard; 2 = Full)");
    BIND_NUM(midi_reset_type, RESET_TYPE_GM, 0, RESET_NUM - 1,
        "[Native MIDI] Reset type (0 = No SysEx; 1 = GM; 2 = GS; 3 = XG)");
    BIND_NUM(midi_reset_delay, -1, -1, 2000,
        "[Native MIDI] Delay after reset (-1 = Auto; 0 = None; 1-2000 = Milliseconds)");
    BIND_BOOL(midi_ctf, true,
        "[Native MIDI] Fix invalid instruments by emulating SC-55 capital tone fallback");
    BIND_BOOL(midi_sysex_volume, false,
        "[Native MIDI] Use SysEx messages to control volume");
}
