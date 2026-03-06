//
// Copyright(C) 2005-2014 Simon Howard
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

#include <stdlib.h>

#include "doomtype.h"
#include "opl.h"
#include "opl3.h"
#include "opl_internal.h"
#include "opl_queue.h"

#define MAX_SOUND_SLICE_TIME 100 /* ms */

#define OPL_SILENCE_THRESHOLD   36  // Allow for a worst case of +/-1 ripple in all 36 operators.
#define OPL_CHIP_TIMEOUT        (OPL_SAMPLE_RATE / 2)   // 0.5 seconds of silence with no key-on
                                                        // should be enough to ensure chip is "off"

typedef struct
{
    unsigned int rate;        // Number of times the timer is advanced per sec.
    unsigned int enabled;     // Non-zero if timer is enabled.
    unsigned int value;       // Last value that was set.
    uint64_t expire_time;     // Calculated time that timer will expire.
} opl_timer_t;

// Queue of callbacks waiting to be invoked.

static opl_callback_queue_t *callback_queue;

// Current time, in us since startup:

static uint64_t current_time;

// If non-zero, playback is currently paused.

static int opl_paused;

// Time offset (in us) due to the fact that callbacks
// were previously paused.

static uint64_t pause_offset;

// OPL software emulator structure.

static opl3_chip opl_chips[OPL_MAX_CHIPS];
static uint32_t opl_chip_keys[OPL_MAX_CHIPS];
static uint32_t opl_chip_timeouts[OPL_MAX_CHIPS];
static int opl_opl3mode;

// Register number that was written.

static int register_num = 0;

// Timers; DBOPL does not do timer stuff itself.

static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = { 3125, 0, 0, 0 };

static int mixing_channels;

// Advance time by the specified number of samples, invoking any
// callback functions as appropriate.

static void AdvanceTime(unsigned int nsamples)
{
    opl_callback_t callback;
    void *callback_data;
    uint64_t us;

    // Advance time.

    us = ((uint64_t) nsamples * OPL_SECOND) / OPL_SAMPLE_RATE;
    current_time += us;

    if (opl_paused)
    {
        pause_offset += us;
    }

    // Are there callbacks to invoke now?  Keep invoking them
    // until there are no more left.

    while (!OPL_Queue_IsEmpty(callback_queue)
        && current_time >= OPL_Queue_Peek(callback_queue) + pause_offset)
    {
        // Pop the callback from the queue to invoke it.

        if (!OPL_Queue_Pop(callback_queue, &callback, &callback_data))
        {
            break;
        }

        callback(callback_data);
    }
}


// When a chip is re-activated after being idle, we need to bring its
// internal global timers back into synch with the main chip to avoid
// possible beating artifacts

static void ResyncChip (int chip)
{
    // Find an active chip to synchronize with; will usually be chip 0
    int sync = -1;
    for (int c = 0; c < OPL_MAX_CHIPS; ++c)
    {
        if (opl_chip_timeouts[c] < OPL_CHIP_TIMEOUT)
        {
            sync = c;
            break;
        }
    }

    if (sync >= 0)
    {
        // Synchronize the LFOs
        opl_chips[chip].vibpos = opl_chips[sync].vibpos;
        opl_chips[chip].tremolopos = opl_chips[sync].tremolopos;
        opl_chips[chip].timer = opl_chips[sync].timer;

        // Synchronize the envelope clock
        opl_chips[chip].eg_state = opl_chips[sync].eg_state;
        opl_chips[chip].eg_timer = opl_chips[sync].eg_timer;
        opl_chips[chip].eg_timerrem = opl_chips[sync].eg_timerrem;
    }
}


// Callback function to fill a new sound buffer:

int OPL_FillBuffer(byte *buffer, int buffer_samples)
{
    unsigned int filled;

    // Repeatedly call the OPL emulator update function until the buffer is
    // full.
    filled = 0;

    while (filled < buffer_samples)
    {
        uint64_t next_callback_time;
        uint64_t nsamples;

        // Work out the time until the next callback waiting in
        // the callback queue must be invoked.  We can then fill the
        // buffer with this many samples.

        if (opl_paused || OPL_Queue_IsEmpty(callback_queue))
        {
            nsamples = buffer_samples - filled;
        }
        else
        {
            next_callback_time = OPL_Queue_Peek(callback_queue) + pause_offset;

            nsamples = (next_callback_time - current_time) * OPL_SAMPLE_RATE;
            nsamples = (nsamples + OPL_SECOND - 1) / OPL_SECOND;

            if (nsamples > buffer_samples - filled)
            {
                nsamples = buffer_samples - filled;
            }
        }

        // Add emulator output to buffer.
        Bit16s *cursor = (Bit16s *)(buffer + filled * 4);
        for (int s = 0; s < nsamples; ++s)
        {
            // Check for chip activations before we generate the sample
            for (int c = 0; c < num_opl_chips; ++c)
            {
                // Reset chip timeout if any channels are active
                if (opl_chip_keys[c])
                {
                    // Resync is necessary if the chip was idle
                    if (opl_chip_timeouts[c] >= OPL_CHIP_TIMEOUT)
                        ResyncChip(c);
                    opl_chip_timeouts[c] = 0;
                }
            }

            // Generate a sample from each chip and mix them
            Bit32s mix[2] = {0, 0};
            for (int c = 0; c < num_opl_chips; ++c)
            {
                // Run the chip if it's active or if it has pending register writes
                if (opl_chip_timeouts[c] < OPL_CHIP_TIMEOUT || (opl_chips[c].writebuf[opl_chips[c].writebuf_cur].reg & 0x200))
                {
                    Bit16s sample[2];
                    OPL3_Generate(&opl_chips[c], sample);
                    mix[0] += sample[0];
                    mix[1] += sample[1];

                    // Reset chip timeout if it breaks the silence threshold
                    if (MAX(abs(sample[0]), abs(sample[1])) > OPL_SILENCE_THRESHOLD)
                        opl_chip_timeouts[c] = 0;
                    else
                        opl_chip_timeouts[c]++;
                }
            }

            cursor[0] = CLAMP(mix[0], -32768, 32767);
            cursor[1] = CLAMP(mix[1], -32768, 32767);
            cursor += 2;
        }

        //OPL3_GenerateStream(&opl_chip, (Bit16s *)(buffer + filled * 4), nsamples);
        filled += nsamples;

        // Invoke callbacks for this point in time.

        AdvanceTime(nsamples);
    }

    return buffer_samples;
}

static void OPL_EMU_Shutdown(void)
{
    OPL_Queue_Destroy(callback_queue);

/*
    if (opl_chip != NULL)
    {
        OPLDestroy(opl_chip);
        opl_chip = NULL;
    }
    */
}

static int OPL_EMU_Init(unsigned int port_base, int num_chips)
{
    opl_paused = 0;
    pause_offset = 0;

    // Queue structure of callbacks to invoke.

    callback_queue = OPL_Queue_Create();
    current_time = 0;

    // Get the mixer frequency, format and number of channels.

    // Only supports AUDIO_S16SYS
    mixing_channels = 2;

    // Create the emulator structure:

    for (int c = 0; c < num_opl_chips; ++c)
        OPL3_Reset(&opl_chips[c], OPL_SAMPLE_RATE);
    opl_opl3mode = 0;

    for (int c = 0; c < OPL_MAX_CHIPS; ++c)
    {
        opl_chip_keys[c] = 0;
        opl_chip_timeouts[c] = OPL_CHIP_TIMEOUT;
    }

    return 1;
}

static unsigned int OPL_EMU_PortRead(int chip, opl_port_t port)
{
    unsigned int result = 0;

    if (port == OPL_REGISTER_PORT_OPL3)
    {
        return 0xff;
    }

    if (chip > 0)
        return result;

    if (timer1.enabled && current_time > timer1.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x40;   // Timer 1 has expired
    }

    if (timer2.enabled && current_time > timer2.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x20;   // Timer 2 has expired
    }

    return result;
}

static void OPLTimer_CalculateEndTime(opl_timer_t *timer)
{
    int tics;

    // If the timer is enabled, calculate the time when the timer
    // will expire.

    if (timer->enabled)
    {
        tics = 0x100 - timer->value;
        timer->expire_time = current_time
                           + ((uint64_t) tics * OPL_SECOND) / timer->rate;
    }
}

static void WriteRegister(int chip, unsigned int reg_num, unsigned int value)
{
    switch (reg_num)
    {
        case OPL_REG_TIMER1:
            // Only allow timers on the first chip
            if (chip == 0)
            {
                timer1.value = value;
                OPLTimer_CalculateEndTime(&timer1);
            }
            break;

        case OPL_REG_TIMER2:
            // Only allow timers on the first chip
            if (chip == 0)
            {
                timer2.value = value;
                OPLTimer_CalculateEndTime(&timer2);
            }
            break;

        case OPL_REG_TIMER_CTRL:
            if (value & 0x80)
            {
                timer1.enabled = 0;
                timer2.enabled = 0;
            }
            else
            {
                if ((value & 0x40) == 0)
                {
                    timer1.enabled = (value & 0x01) != 0;
                    OPLTimer_CalculateEndTime(&timer1);
                }

                if ((value & 0x20) == 0)
                {
                    timer2.enabled = (value & 0x02) != 0;
                    OPLTimer_CalculateEndTime(&timer2);
                }
            }
            break;

        case OPL_REG_NEW:
            // Keep all chips synchronized with the first chip's opl3 mode
            if (chip == 0)
            {
                for (int c = 0; c < num_opl_chips; ++c)
                    OPL3_WriteRegBuffered(&opl_chips[c], reg_num, value);
                opl_opl3mode = value & 0x01;
            }
            break;

        default:
            // Keep track of which channels are keyed-on so we know when the chip is in use
            if ((reg_num & 0xff) >= 0xb0 && (reg_num & 0xff) <= 0xb8)
            {
                uint32_t key_bit = 1 << (((reg_num & 0x100) ? 9 : 0) + (reg_num & 0xf));
                if (value & (1 << 5))
                    opl_chip_keys[chip] |= key_bit;
                else
                    opl_chip_keys[chip] &= ~key_bit;
            }

            OPL3_WriteRegBuffered(&opl_chips[chip], reg_num, value);
            break;
    }
}

static void OPL_EMU_PortWrite(int chip, opl_port_t port, unsigned int value)
{
    if (port == OPL_REGISTER_PORT)
    {
        register_num = value;
    }
    else if (port == OPL_REGISTER_PORT_OPL3)
    {
        register_num = value | 0x100;
    }
    else if (port == OPL_DATA_PORT)
    {
        WriteRegister(chip, register_num, value);
    }
}

static void OPL_EMU_SetCallback(uint64_t us, opl_callback_t callback,
                                void *data)
{
    OPL_Queue_Push(callback_queue, callback, data,
                   current_time - pause_offset + us);
}

static void OPL_EMU_ClearCallbacks(void)
{
    OPL_Queue_Clear(callback_queue);
}

static void OPL_EMU_AdjustCallbacks(float factor)
{
    OPL_Queue_AdjustCallbacks(callback_queue, current_time, factor);
}

opl_driver_t opl_emu_driver =
{
    "Software Emulation",
    OPL_EMU_Init,
    OPL_EMU_Shutdown,
    OPL_EMU_PortRead,
    OPL_EMU_PortWrite,
    OPL_EMU_SetCallback,
    OPL_EMU_ClearCallbacks,
    OPL_EMU_AdjustCallbacks,
};

