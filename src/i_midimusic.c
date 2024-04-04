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
#include "rtmidi/rtmidi_c.h"

#include <math.h>
#include <stdlib.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_timer.h"
#include "m_array.h"
#include "memio.h"
#include "midifallback.h"
#include "midifile.h"
#include "mus2mid.h"

static struct RtMidiWrapper *midiout;

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
    RESET_TYPE_NONE,
    RESET_TYPE_GM,
    RESET_TYPE_GS,
    RESET_TYPE_XG,
};

const char *midi_device = "";
int midi_complevel = COMP_STANDARD;
int midi_reset_type = RESET_TYPE_GM;
int midi_reset_delay = 0;

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

static boolean use_fallback;

#define DEFAULT_VOLUME 100
static byte channel_volume[MIDI_CHANNELS_PER_TRACK];
static float volume_factor = 0.0f;

typedef enum
{
    STATE_STARTUP,
    STATE_STOPPING,
    STATE_EXIT,
    STATE_PLAYING,
    STATE_WAITING,
    STATE_PAUSED
} midi_state_t;

static midi_state_t midi_state;

#define EMIDI_DEVICE (1U << EMIDI_DEVICE_GENERAL_MIDI)

static const char **ports = NULL;

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

static boolean CheckError(const char *func)
{
    if (!midiout->ok)
    {
        I_Printf(VB_ERROR, "%s: %s", func, midiout->msg);
        return true;
    }
    return false;
}

static void SendShortMsg(byte status, byte channel, byte param1, byte param2)
{
    byte message[3];

    message[0] = status | channel;
    message[1] = param1;
    message[2] = param2;

    if (rtmidi_out_send_message(midiout, message, sizeof(message)) < 0)
    {
        I_Printf(VB_ERROR, "SendShortMsg: %s", midiout->msg);
    }
}

static void SendChannelMsg(const midi_event_t *event, boolean use_param2)
{
    SendShortMsg(event->event_type, event->data.channel.channel,
                 event->data.channel.param1,
                 use_param2 ? event->data.channel.param2 : 0);
}

static void SendLongMsg(const byte *ptr, unsigned int length)
{
    if (rtmidi_out_send_message(midiout, ptr, length) < 0)
    {
        I_Printf(VB_ERROR, "SendLongMsg: %s", midiout->msg);
    }
}

// Writes an RPN message set to NULL (0x7F). Prevents accidental data entry.

static void SendNullRPN(const midi_event_t *event)
{
    const byte channel = event->data.channel.channel;
    SendShortMsg(MIDI_EVENT_CONTROLLER, channel, MIDI_CONTROLLER_RPN_LSB,
                 MIDI_RPN_NULL);
    SendShortMsg(MIDI_EVENT_CONTROLLER, channel, MIDI_CONTROLLER_RPN_MSB,
                 MIDI_RPN_NULL);
}

// static void SendNOPMsg(unsigned int delta_time)
// {
//     ;
// }

static uint64_t TicksToUS(unsigned int ticks)
{
    return ((uint64_t)ticks * us_per_beat) / ticks_per_beat;
}

static void UpdateTempo(const midi_event_t *event)
{
    byte *data = event->data.meta.data;
    us_per_beat = (data[0] << 16) | (data[1] << 8) | data[2];
}

// Writes a MIDI volume message. The value is scaled by the volume slider.

static void SendManualVolumeMsg(byte channel, byte volume)
{
    unsigned int scaled_volume;

    scaled_volume = volume * volume_factor + 0.5f;

    if (scaled_volume > 127)
    {
        scaled_volume = 127;
    }

    SendShortMsg(MIDI_EVENT_CONTROLLER, channel, MIDI_CONTROLLER_VOLUME_MSB,
                 scaled_volume);

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
        SendManualVolumeMsg(i, DEFAULT_VOLUME);
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
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);
    }
}

// Resets commonly used controllers. This is only for a reset type of "none" for
// devices that don't support SysEx resets.

static void ResetControllers(void)
{
    int i;

    for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Reset commonly used controllers.
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_PAN, 64);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_MSB, 0);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_LSB, 0);
        SendShortMsg(MIDI_EVENT_PROGRAM_CHANGE, i, 0, 0);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_REVERB, 40);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_CHORUS, 0);
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
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 0);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 0);

        // Reset pitch bend sensitivity to +/- 2 semitones and 0 cents.
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_MSB, 2);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);

        // Set RPN MSB/LSB to null value after data entry.
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 127);
        SendShortMsg(MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 127);
    }
}

// Resets the MIDI device. Call this function before each song starts and once
// at shut down.

static void ResetDevice(void)
{
    MIDI_ResetFallback();
    use_fallback = false;

    switch (midi_reset_type)
    {
        case RESET_TYPE_NONE:
            ResetControllers();
            break;

        case RESET_TYPE_GS:
            SendLongMsg(gs_reset, sizeof(gs_reset));
            use_fallback = (midi_complevel != COMP_VANILLA);
            break;

        case RESET_TYPE_XG:
            SendLongMsg(xg_system_on, sizeof(xg_system_on));
            break;

        default:
            SendLongMsg(gm_system_on, sizeof(gm_system_on));
            break;
    }

    // MS GS Wavetable Synth doesn't reset pitch bend sensitivity.
    ResetPitchBendSensitivity();

    // Reset volume (initial playback or on shutdown if no SysEx reset).
    // Scale by slider on initial playback, max on shutdown.
    if (midi_state == STATE_STARTUP)
    {
        ResetVolume();
    }
    else if (midi_reset_type == RESET_TYPE_NONE)
    {
        volume_factor = 1.0f;
        ResetVolume();
    }

    // Send delay after reset. This is for hardware devices only (e.g. SC-55).
    if (midi_reset_delay > 0)
    {
        I_Sleep(midi_reset_delay);
    }
}

// Normally, volume is controlled by channel volume messages. Roland defined a
// special SysEx message called "part level" that is equivalent to this. MS GS
// Wavetable Synth ignores these messages, but other MIDI devices support them.
// Returns true if there is a match.

static boolean IsPartLevel(const byte *msg, unsigned int length)
{
    if (length == 10 &&
        msg[0] == 0x41 && // Roland
        msg[2] == 0x42 && // GS
        msg[3] == 0x12 && // DT1
        msg[4] == 0x40 && // Address MSB
        msg[5] >= 0x10 && // Address
        msg[5] <= 0x1F && // Address
        msg[6] == 0x19 && // Address LSB
        msg[9] == 0xF7)   // SysEx EOX
    {
        const byte checksum = 128 - ((int)msg[4] + msg[5] + msg[6] + msg[7]) % 128;

        if (msg[8] == checksum)
        {
            // GS Part Level (aka Channel Volume)
            // 41 <dev> 42 12 40 <ch> 19 <vol> <sum> F7
            return true;
        }
    }

    return false;
}

// Checks if the current SysEx message matches any known SysEx reset message.
// Returns true if there is a match.

static boolean IsSysExReset(const byte *msg, unsigned int length)
{
    if (length < 5)
    {
        return false;
    }

    switch (msg[0])
    {
        case 0x41:  // Roland
            switch (msg[2])
            {
                case 0x42:  // GS
                    switch (msg[3])
                    {
                        case 0x12:  // DT1
                            if (length == 10 &&
                                msg[4] == 0x00 &&    // Address MSB
                                msg[5] == 0x00 &&    // Address
                                msg[6] == 0x7F &&    // Address LSB
                                ((msg[7] == 0x00 &&  // Data     (MODE-1)
                                  msg[8] == 0x01)
                                 ||                  // Checksum (MODE-1)
                                 (msg[7] == 0x01 &&  // Data     (MODE-2)
                                  msg[8] == 0x00)))  // Checksum (MODE-2)
                            {
                                // SC-88 System Mode Set
                                // 41 <dev> 42 12 00 00 7F 00 01 F7 (MODE-1)
                                // 41 <dev> 42 12 00 00 7F 01 00 F7 (MODE-2)
                                return true;
                            }
                            else if (length == 10 &&
                                     msg[4] == 0x40 &&  // Address MSB
                                     msg[5] == 0x00 &&  // Address
                                     msg[6] == 0x7F &&  // Address LSB
                                     msg[7] == 0x00 &&  // Data (GS Reset)
                                     msg[8] == 0x41)    // Checksum
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

        case 0x43:  // Yamaha
            switch (msg[2])
            {
                case 0x2B:  // TG300
                    if (length == 9 &&
                        msg[3] == 0x00 &&  // Start Address b20 - b14
                        msg[4] == 0x00 &&  // Start Address b13 - b7
                        msg[5] == 0x7F &&  // Start Address b6 - b0
                        msg[6] == 0x00 &&  // Data
                        msg[7] == 0x01)    // Checksum
                    {
                        // TG300 All Parameter Reset
                        // 43 <dev> 2B 00 00 7F 00 01 F7
                        return true;
                    }
                    break;

                case 0x4C:                                // XG
                    if (length == 8 &&
                        msg[3] == 0x00 &&  // Address High
                        msg[4] == 0x00 &&  // Address Mid
                        (msg[5] == 0x7E || // Address Low (System On)
                         msg[5] == 0x7F)
                        &&                 // Address Low (All Parameter Reset)
                        msg[6] == 0x00)    // Data
                    {
                        // XG System On, XG All Parameter Reset
                        // 43 <dev> 4C 00 00 7E 00 F7
                        // 43 <dev> 4C 00 00 7F 00 F7
                        return true;
                    }
                    break;
            }
            break;

        case 0x7E:  // Universal Non-Real Time
            switch (msg[2])
            {
                case 0x09:  // General Midi
                    if (length == 5
                        && (msg[3] == 0x01 ||  // GM System On
                            msg[3] == 0x02 ||  // GM System Off
                            msg[3] == 0x03))   // GM2 System On
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

// Writes a MIDI SysEx message. Call this function from the MIDI thread only,
// with exclusive access to shared resources.

static void SendSysExMsg(const midi_event_t *event)
{
    const byte *data = event->data.sysex.data;
    const unsigned int length = event->data.sysex.length;

    if (IsPartLevel(data, length))
    {
        byte channel;

        // Convert "block number" to a channel number.
        if (data[5] == 0x10) // Channel 10
        {
            channel = 9;
        }
        else if (data[5] < 0x1A) // Channels 1-9
        {
            channel = (data[5] & 0x0F) - 1;
        }
        else // Channels 11-16
        {
            channel = data[5] & 0x0F;
        }

        // Replace SysEx part level message with channel volume message.
        SendManualVolumeMsg(channel, data[7]);
        return;
    }

    // Send the SysEx message.
    SendLongMsg(data, length);

    if (IsSysExReset(data, length))
    {
        // SysEx reset also resets volume. Take the default channel volumes
        // and scale them by the user's volume slider.
        ResetVolume();

        // Disable instrument fallback and give priority to MIDI file. Fallback
        // assumes GS (SC-55 level) and the MIDI file could be GM, GM2, XG, or
        // GS (SC-88 or higher). Preserve the composer's intent.
        if (use_fallback)
        {
            MIDI_ResetFallback();
            use_fallback = false;
        }
    }
}

// Writes a MIDI program change message. If applicable, emulates capital tone
// fallback to fix invalid instruments.

static void SendProgramMsg(byte channel, byte program,
                           const midi_fallback_t *fallback)
{
    switch ((int)fallback->type)
    {
        case FALLBACK_BANK_MSB:
            SendShortMsg(MIDI_EVENT_CONTROLLER, channel,
                         MIDI_CONTROLLER_BANK_SELECT_MSB, fallback->value);
            SendShortMsg(MIDI_EVENT_PROGRAM_CHANGE, channel, program, 0);
            break;

        case FALLBACK_DRUMS:
            SendShortMsg(MIDI_EVENT_PROGRAM_CHANGE, channel, fallback->value, 0);
            break;

        default:
            SendShortMsg(MIDI_EVENT_PROGRAM_CHANGE, channel, program, 0);
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

static void SendEMIDI(const midi_event_t *event, midi_track_t *track,
                      const midi_fallback_t *fallback)
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
            //SendNOPMsg(delta_time);
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
            //SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_PROGRAM_CHANGE:
            if (track->emidi_program || track->elapsed_time < ticks_per_beat)
            {
                track->emidi_program = true;
                SendProgramMsg(event->data.channel.channel,
                               event->data.channel.param2, fallback);
            }
            else
            {
                //SendNOPMsg(delta_time);
            }
            break;

        case EMIDI_CONTROLLER_VOLUME:
            if (track->emidi_volume || track->elapsed_time < ticks_per_beat)
            {
                track->emidi_volume = true;
                SendVolumeMsg(event);
            }
            else
            {
                //SendNOPMsg(delta_time);
            }
            break;

        case EMIDI_CONTROLLER_LOOP_BEGIN:
            count = event->data.channel.param2;
            count = (count == 0) ? (-1) : count;
            track->emidi_loop_count = count;
            MIDI_SetLoopPoint(track->iter);
            // SendNOPMsg(delta_time);
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
            //SendNOPMsg(delta_time);
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
            //SendNOPMsg(delta_time);
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
                    }

                    if (song.tracks[i].emidi_loop_count > 0)
                    {
                        song.tracks[i].emidi_loop_count--;
                    }
                }
            }
            //SendNOPMsg(delta_time);
            break;
    }
}

// Writes a MIDI meta message. Call this function from the MIDI thread only,
// with exclusive access to shared resources.

static void SendMetaMsg(const midi_event_t *event, midi_track_t *track)
{
    switch (event->data.meta.type)
    {
        case MIDI_META_END_OF_TRACK:
            track->end_of_track = true;
            //SendNOPMsg(delta_time);
            break;

        case MIDI_META_SET_TEMPO:
            UpdateTempo(event);
            break;

        case MIDI_META_MARKER:
            if (midi_complevel != COMP_VANILLA)
            {
                CheckFFLoop(event);
            }
            //SendNOPMsg(delta_time);
            break;

        default:
            //SendNOPMsg(delta_time);
            break;
    }
}

// ProcessEvent function for vanilla (DMX MPU-401) compatibility level. Do not
// call this function directly. See the ProcessEvent function pointer.

static boolean ProcessEvent_Vanilla(const midi_event_t *event,
                                    midi_track_t *track)
{
    switch ((int)event->event_type)
    {
        case MIDI_EVENT_SYSEX:
            //SendNOPMsg(delta_time);
            return false;

        case MIDI_EVENT_META:
            SendMetaMsg(event, track);
            break;

        case MIDI_EVENT_CONTROLLER:
            switch (event->data.channel.param1)
            {
                case MIDI_CONTROLLER_BANK_SELECT_MSB:
                case MIDI_CONTROLLER_BANK_SELECT_LSB:
                    // DMX has broken bank select support and runs in GM mode.
                    SendChannelMsg(event, false);
                    break;

                case MIDI_CONTROLLER_MODULATION:
                case MIDI_CONTROLLER_PAN:
                case MIDI_CONTROLLER_EXPRESSION:
                case MIDI_CONTROLLER_HOLD1_PEDAL:
                case MIDI_CONTROLLER_SOFT_PEDAL:
                case MIDI_CONTROLLER_REVERB:
                case MIDI_CONTROLLER_CHORUS:
                case MIDI_CONTROLLER_ALL_SOUND_OFF:
                case MIDI_CONTROLLER_ALL_NOTES_OFF:
                    SendChannelMsg(event, true);
                    break;

                case MIDI_CONTROLLER_VOLUME_MSB:
                    SendVolumeMsg(event);
                    break;

                case MIDI_CONTROLLER_RESET_ALL_CTRLS:
                    // MS GS Wavetable Synth resets volume if param2 isn't zero.
                    SendChannelMsg(event, false);
                    break;

                default:
                    //SendNOPMsg(delta_time);
                    break;
            }
            break;

        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_PITCH_BEND:
            SendChannelMsg(event, true);
            break;

        case MIDI_EVENT_PROGRAM_CHANGE:
            SendChannelMsg(event, false);
            break;

        default:
            //SendNOPMsg(delta_time);
            break;
    }

    return true;
}

// ProcessEvent function for standard and full MIDI compatibility levels. Do not
// call this function directly. See the ProcessEvent function pointer.

static boolean ProcessEvent_Standard(const midi_event_t *event,
                                    midi_track_t *track)
{
    midi_fallback_t fallback = {FALLBACK_NONE, 0};

    if (use_fallback)
    {
        MIDI_CheckFallback(event, &fallback, midi_complevel == COMP_FULL);
    }

    switch ((int)event->event_type)
    {
        case MIDI_EVENT_SYSEX:
            if (midi_complevel == COMP_FULL)
            {
                SendSysExMsg(event);
            }
            else
            {
                //SendNOPMsg(delta_time);
            }
            return false;

        case MIDI_EVENT_META:
            SendMetaMsg(event, track);
            return true;
    }

    if (track->emidi_designated && (EMIDI_DEVICE & ~track->emidi_device_flags))
    {
        // Send NOP if this device has been excluded from this track.
        //SendNOPMsg(delta_time);
        return true;
    }

    switch ((int)event->event_type)
    {
        case MIDI_EVENT_CONTROLLER:
            switch (event->data.channel.param1)
            {
                case MIDI_CONTROLLER_BANK_SELECT_MSB:
                case MIDI_CONTROLLER_MODULATION:
                case MIDI_CONTROLLER_DATA_ENTRY_MSB:
                case MIDI_CONTROLLER_PAN:
                case MIDI_CONTROLLER_EXPRESSION:
                case MIDI_CONTROLLER_DATA_ENTRY_LSB:
                case MIDI_CONTROLLER_HOLD1_PEDAL:
                case MIDI_CONTROLLER_SOFT_PEDAL:
                case MIDI_CONTROLLER_REVERB:
                case MIDI_CONTROLLER_CHORUS:
                case MIDI_CONTROLLER_ALL_SOUND_OFF:
                case MIDI_CONTROLLER_ALL_NOTES_OFF:
                case MIDI_CONTROLLER_POLY_MODE_OFF:
                case MIDI_CONTROLLER_POLY_MODE_ON:
                    SendChannelMsg(event, true);
                    break;

                case MIDI_CONTROLLER_VOLUME_MSB:
                    if (track->emidi_volume)
                    {
                        //SendNOPMsg(delta_time);
                    }
                    else
                    {
                        SendVolumeMsg(event);
                    }
                    break;

                case MIDI_CONTROLLER_VOLUME_LSB:
                    //SendNOPMsg(delta_time);
                    break;

                case MIDI_CONTROLLER_BANK_SELECT_LSB:
                    SendChannelMsg(event,
                                   fallback.type != FALLBACK_BANK_LSB);
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
                    SendEMIDI(event, track, &fallback);
                    break;

                case MIDI_CONTROLLER_RESET_ALL_CTRLS:
                    // MS GS Wavetable Synth resets volume if param2 isn't zero.
                    SendChannelMsg(event, false);
                    break;

                default:
                    if (midi_complevel == COMP_FULL)
                    {
                        SendChannelMsg(event, true);
                    }
                    else
                    {
                        //SendNOPMsg(delta_time);
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
            if (track->emidi_program)
            {
                //SendNOPMsg(delta_time);
            }
            else
            {
                SendProgramMsg(event->data.channel.channel,
                               event->data.channel.param1, &fallback);
            }
            break;

        case MIDI_EVENT_AFTERTOUCH:
            if (midi_complevel == COMP_FULL)
            {
                SendChannelMsg(event, true);
            }
            else
            {
                //SendNOPMsg(delta_time);
            }
            break;

        case MIDI_EVENT_CHAN_AFTERTOUCH:
            if (midi_complevel == COMP_FULL)
            {
                SendChannelMsg(event, false);
            }
            else
            {
                //SendNOPMsg(delta_time);
            }
            break;

        default:
            //SendNOPMsg(delta_time);
            break;
    }

    return true;
}

// Function pointer determined by the desired MIDI compatibility level. Set
// during initialization by the main thread, then called from the MIDI thread
// only. The calling thread must have exclusive access to the shared resources
// in this function.

static boolean (*ProcessEvent)(const midi_event_t *event,
                              midi_track_t *track) = ProcessEvent_Standard;

// Restarts a song that uses a Final Fantasy or RPG Maker loop point. Call this
// function from the MIDI thread only, with exclusive access to shared
// resources.

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
}

// Restarts a song that uses standard looping. Call this function from the MIDI
// thread only, with exclusive access to shared resources.

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
}

// The controllers "EMIDI track exclusion" and "RPG Maker loop point" share the
// same number (CC#111) and are not compatible with each other. As a workaround,
// allow an RPG Maker loop point only if no other EMIDI events are present. Call
// this function from the MIDI thread only, before the song starts, with
// exclusive access to shared resources.

static boolean IsRPGLoop(void)
{
    unsigned int i;
    unsigned int num_rpg_events = 0;
    unsigned int num_emidi_events = 0;
    midi_event_t *event = NULL;

    for (i = 0; i < song.num_tracks; ++i)
    {
        while (MIDI_GetNextEvent(song.tracks[i].iter, &event))
        {
            if (event->event_type == MIDI_EVENT_CONTROLLER)
            {
                switch (event->data.channel.param1)
                {
                    case EMIDI_CONTROLLER_TRACK_EXCLUSION:
                        num_rpg_events++;
                        break;

                    case EMIDI_CONTROLLER_TRACK_DESIGNATION:
                    case EMIDI_CONTROLLER_PROGRAM_CHANGE:
                    case EMIDI_CONTROLLER_VOLUME:
                    case EMIDI_CONTROLLER_LOOP_BEGIN:
                    case EMIDI_CONTROLLER_LOOP_END:
                    case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN:
                    case EMIDI_CONTROLLER_GLOBAL_LOOP_END:
                        num_emidi_events++;
                        break;
                }
            }
        }

        MIDI_RestartIterator(song.tracks[i].iter);
    }

    return (num_rpg_events == 1 && num_emidi_events == 0);
}

// Fills the output buffer with events from the current song and then streams it
// out. Call this function from the MIDI thread only, with exclusive access to
// shared resources.

static boolean GetNextEvent(midi_event_t **event, midi_track_t **track,
                            uint64_t *wait_time)
{
    midi_event_t *local_event = NULL;
    midi_track_t *local_track = NULL;
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
                local_track = &song.tracks[i];
            }
        }
    }

    // No more events. Restart or stop song.
    if (local_track == NULL)
    {
        if (song.elapsed_time)
        {
            if (song.ff_restart || song.rpg_loop)
            {
                song.ff_restart = false;
                RestartLoop();
            }
            else if (song.looping)
            {
                for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
                {
                    SendShortMsg(MIDI_EVENT_CONTROLLER, i,
                                 MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
                }
                RestartTracks();
            }
        }
        return false;
    }

    local_track->elapsed_time = min_time;
    delta_time = min_time - song.elapsed_time;
    song.elapsed_time = min_time;

    if (!MIDI_GetNextEvent(local_track->iter, &local_event))
    {
        local_track->end_of_track = true;
        return false;
    }

    // Restart FF loop after sending all events that share same ticks_per_beat.
    if (song.ff_restart && MIDI_GetDeltaTime(local_track->iter) > 0)
    {
        song.ff_restart = false;
        RestartLoop();
        return false;
    }

    *track = local_track;
    *event = local_event;
    *wait_time = I_GetTimeUS() + TicksToUS(delta_time);

    return true;
}

static int PlayerThread(void *unused)
{
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);

    midi_event_t *event = NULL;
    midi_track_t *track = NULL;
    uint64_t wait_time = 0;

    while (SDL_AtomicGet(&player_thread_running))
    {
        if (wait_time && wait_time - I_GetTimeUS() > 1000)
        {
            I_Sleep(1);
        }

        // The MIDI thread must have exclusive access to shared resources until
        // the end of the current loop iteration or when the thread exits.

        SDL_LockMutex(music_lock);

        switch (midi_state)
        {
            case STATE_STARTUP:
                ResetDevice();
                song.rpg_loop = IsRPGLoop();
                midi_state = STATE_PLAYING;
                break;

            case STATE_EXIT:
                SDL_AtomicSet(&player_thread_running, 0);
                break;

            case STATE_PLAYING:
                wait_time = 0;
                if (GetNextEvent(&event, &track, &wait_time))
                {
                    midi_state = STATE_WAITING;
                }
                break;

            case STATE_STOPPING:
                // Send notes/sound off to prevent hanging notes.
                SendNotesSoundOff();
                midi_state = STATE_EXIT;
                break;

            case STATE_WAITING:
                if (wait_time <= I_GetTimeUS())
                {
                    ProcessEvent(event, track);
                    wait_time = 0;
                    midi_state = STATE_PLAYING;
                    break;
                }
                break;

            case STATE_PAUSED:
                wait_time = I_GetTimeUS() + 5000;
                break;
        }

        SDL_UnlockMutex(music_lock);
    }

    return 0;
}

static void GetDevices(void)
{
    if (array_size(ports))
    {
        return;
    }

    if (!midiout)
    {
        midiout = rtmidi_out_create_default();
    }

    unsigned int num_devs = rtmidi_get_port_count(midiout);

    for (int i = 0; i < num_devs; ++i)
    {
        char *s;
        int len;
        rtmidi_get_port_name(midiout, i, NULL, &len);
        s = malloc(len);
        rtmidi_get_port_name(midiout, i, s, &len);
        array_push(ports, s);
    }

    CheckError("GetDevices");
}

static boolean I_MID_InitMusic(int device)
{
    if (device == DEFAULT_MIDI_DEVICE)
    {
        GetDevices();

        device = 0;

        for (int i = 0; i < array_size(ports); ++i)
        {
            if (!strcasecmp(ports[i], midi_device))
            {
                device = i;
                break;
            }
        }
    }

    if (array_size(ports))
    {
        if (device >= array_size(ports))
        {
            device = 0;
        }
        midi_device = ports[device];
    }

    if (!midiout)
    {
        midiout = rtmidi_out_create_default();
    }

    if (CheckError("I_MID_InitMusic"))
    {
        return false;
    }

    rtmidi_open_port(midiout, device, midi_device);

    if (CheckError("I_MID_InitMusic"))
    {
        return false;
    }

    ProcessEvent = (midi_complevel == COMP_VANILLA) ? ProcessEvent_Vanilla
                                                    : ProcessEvent_Standard;
    MIDI_InitFallback();

    music_initialized = true;

    I_Printf(VB_INFO, "MIDI Init: Using '%s'.", midi_device);

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

    SDL_LockMutex(music_lock);
    volume_factor = sqrtf((float)volume / 15.0f);
    if (song.file)
    {
        UpdateVolume();
    }
    SDL_UnlockMutex(music_lock);
}

static void I_MID_StopSong(void *handle)
{
    if (!music_initialized || !SDL_AtomicGet(&player_thread_running))
    {
        return;
    }

    SDL_LockMutex(music_lock);
    midi_state = STATE_STOPPING;
    SDL_UnlockMutex(music_lock);

    SDL_WaitThread(player_thread_handle, NULL);
    SDL_DestroyMutex(music_lock);
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
    midi_state = STATE_PAUSED;
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
        midi_state = STATE_PLAYING;
    }
    SDL_UnlockMutex(music_lock);
}

static void *I_MID_RegisterSong(void *data, int len)
{
    if (!music_initialized)
    {
        return NULL;
    }

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
        I_Printf(VB_ERROR, "I_MID_RegisterSong: Failed to load MID.");
        return NULL;
    }

    ticks_per_beat = MIDI_GetFileTimeDivision(song.file);

    us_per_beat = 500 * 1000; // Default is 120 bpm.

    song.num_tracks = MIDI_NumTracks(song.file);
    song.tracks = calloc(song.num_tracks, sizeof(midi_track_t));
    for (int i = 0; i < song.num_tracks; ++i)
    {
        song.tracks[i].iter = MIDI_IterateTrack(song.file, i);
    }

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

    // Send notes/sound off prior to reset to prevent volume spikes.
    SendNotesSoundOff();
    ResetDevice();

    rtmidi_close_port(midiout);
    rtmidi_out_free(midiout);
    midiout = NULL;

    music_initialized = false;
}

static const char **I_MID_DeviceList(int *current_device)
{
    static const char **devices = NULL;

    if (devices)
    {
        return devices;
    }

    if (current_device)
    {
        *current_device = 0;
    }

    GetDevices();

    for (int i = 0; i < array_size(ports); ++i)
    {
        if (current_device
            && !strcasecmp(ports[i], midi_device))
        {
            *current_device = i;
        }
    }
    devices = ports;
    return devices;
}

static void I_MID_UpdateMusic(void)
{
    ;
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
    I_MID_UpdateMusic,
    I_MID_StopSong,
    I_MID_UnRegisterSong,
    I_MID_DeviceList,
};
