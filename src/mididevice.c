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

#include <math.h>

#include "doomtype.h"
#include "m_config.h"
#include "mididevice.h"
#include "midifallback.h"
#include "midifile.h"
#include "midiseq.h"

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

//---------------------------------------------------------------------------
// Output
//---------------------------------------------------------------------------

static const midi_output_sink_t *sink;

static boolean FullComplevel(void)
{
    return MIDISeq_GetComplevel() == MIDI_COMP_FULL;
}

static void SendShortMsg(const byte *message, unsigned int length)
{
    if (sink && sink->ShortMsg)
    {
        sink->ShortMsg(message, length);
    }
}

static void SendLongMsg(const byte *message, unsigned int length)
{
    if (sink && sink->LongMsg)
    {
        sink->LongMsg(message, length);
    }
}

// Realtime output waits for the device to process the messages sent so far.
// Streaming backends receive everything at once and have no delay callbacks.

static void DelayUS(uint64_t us)
{
    if (sink && sink->DelayUS)
    {
        sink->DelayUS(us);
    }
}

static void DelayMS(int ms)
{
    if (sink && sink->DelayMS)
    {
        sink->DelayMS(ms);
    }
}

void MIDIDevice_SetSink(const midi_output_sink_t *output_sink)
{
    sink = output_sink;
}

//---------------------------------------------------------------------------
// Volume
//---------------------------------------------------------------------------

static byte channel_volume[MIDI_CHANNELS_PER_TRACK];
static uint16_t master_volume;
static float volume_factor = 1.0f;

// Bank select is dropped for devices that run in General MIDI mode.

static boolean AllowBankSelect(void)
{
    switch (MIDISeq_GetComplevel())
    {
        case MIDI_COMP_VANILLA:
            return false;

        case MIDI_COMP_FULL:
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
    SendShortMsg(message, sizeof(message));
}

static void SendChannelMsg(const midi_event_t *event, boolean use_param2)
{
    if (use_param2)
    {
        const byte message[] = {event->event_type | event->data.channel.channel,
                                event->data.channel.param1,
                                event->data.channel.param2};
        SendShortMsg(message, sizeof(message));
    }
    else
    {
        const byte message[] = {event->event_type | event->data.channel.channel,
                                event->data.channel.param1};
        SendShortMsg(message, sizeof(message));
    }
}

static void SendControlChange(byte channel, byte number, byte value)
{
    const byte message[] = {MIDI_EVENT_CONTROLLER | channel, number, value};
    SendShortMsg(message, sizeof(message));
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

    SendShortMsg(message, sizeof(message));
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
}

// Writes an RPN message set to NULL (0x7F). Prevents accidental data entry.

static void SendNullRPN(const midi_event_t *event)
{
    const byte channel = event->data.channel.channel;
    SendControlChange(channel, MIDI_CONTROLLER_RPN_LSB, MIDI_RPN_NULL);
    SendControlChange(channel, MIDI_CONTROLLER_RPN_MSB, MIDI_RPN_NULL);
}

// Writes a master volume message. The value is scaled by the volume slider.

static void SendMasterVolumeMsg(uint16_t volume)
{
    uint32_t scaled_volume = lroundf((float)volume * volume_factor);
    scaled_volume = MIN(scaled_volume, MIDI_DEFAULT_MASTER_VOLUME);

    const byte lsb = scaled_volume & 0x7F;
    const byte msb = (scaled_volume >> 7) & 0x7F;
    const byte data[] = {0xF0, 0x7F, 0x7F, 0x04, 0x01, lsb, msb, 0xF7};
    SendLongMsg(data, sizeof(data));

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
}

// Sets each channel to its saved volume level, scaled by the volume slider.

void MIDIDevice_UpdateVolume(void)
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

void MIDIDevice_SendNotesSoundOff(void)
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
        DelayUS((uint64_t)length * 320);
    }
}

// Resets the MIDI device. Call this function before each song starts and once
// at shut down.

void MIDIDevice_Reset(boolean startup)
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
            SendLongMsg(gs_reset, sizeof(gs_reset));
            ResetDelayBytes(sizeof(gs_reset));
            break;

        case RESET_TYPE_XG:
            SendLongMsg(xg_system_on, sizeof(xg_system_on));
            ResetDelayBytes(sizeof(xg_system_on));
            break;

        default:
            SendLongMsg(gm_system_on, sizeof(gm_system_on));
            ResetDelayBytes(sizeof(gm_system_on));
            break;
    }

    // Roland/Yamaha require an additional ~50 ms to execute a SysEx reset.
    if (midi_reset_delay == -1 && midi_reset_type != RESET_TYPE_NO_SYSEX)
    {
        DelayMS(60);
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
        DelayMS(midi_reset_delay);
    }
}

void MIDIDevice_SetVolumeFactor(float factor)
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
            SendLongMsg(data, length);
            ResetVolume();
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
                SendLongMsg(data, length);
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
                SendLongMsg(data, length);
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
                SendLongMsg(data, length);
            }
            break;

        case MIDI_SYSEX_RHYTHM_PART:
            if (midi_ctf)
            {
                MIDI_UpdateDrumMap(event->data.sysex.channel, data[8]);
            }
            // Fall through.

        case MIDI_SYSEX_OTHER:
            SendLongMsg(data, length);
            break;

        case MIDI_SYSEX_PART_LEVEL:
            if (midi_sysex_volume)
            {
                SendLongMsg(data, length);
            }
            else
            {
                // Replace SysEx part level message with channel volume message.
                SendManualVolumeMsg(event->data.sysex.channel, data[8]);
            }
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
                    if (FullComplevel())
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
                            if (FullComplevel())
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
                            if (FullComplevel())
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
                    if (FullComplevel())
                    {
                        // Per MIDI 1.0 Spec, param2 must be zero.
                        SendChannelMsgZero(event);
                    }
                    break;

                default:
                    if (FullComplevel())
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
            if (FullComplevel())
            {
                SendChannelMsg(event, true);
            }
            break;

        case MIDI_EVENT_CHAN_AFTERTOUCH:
            if (FullComplevel())
            {
                SendChannelMsg(event, false);
            }
            break;

        default:
            break;
    }
}

void MIDIDevice_HandleEvent(const midi_event_t *event)
{
    switch (event->event_type)
    {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            SendSysExMsg(event);
            return;

        case MIDI_EVENT_META:
            return;

        default:
            break;
    }

    if (MIDISeq_GetComplevel() == MIDI_COMP_VANILLA)
    {
        ProcessEvent_Vanilla(event);
    }
    else
    {
        ProcessEvent_Standard(event);
    }
}

void MIDIDevice_BindVariables(void)
{
    BIND_NUM(midi_reset_type, RESET_TYPE_GM, 0, RESET_NUM - 1,
        "[Native MIDI] Reset type (0 = No SysEx; 1 = GM; 2 = GS; 3 = XG)");
    BIND_NUM(midi_reset_delay, -1, -1, 2000,
        "[Native MIDI] Delay after reset (-1 = Auto; 0 = None; 1-2000 = Milliseconds)");
    BIND_BOOL(midi_ctf, true,
        "[Native MIDI] Fix invalid instruments by emulating SC-55 capital tone fallback");
    BIND_BOOL(midi_sysex_volume, false,
        "[Native MIDI] Use SysEx messages to control volume");
}
