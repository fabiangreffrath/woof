//
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
//      MIDI instrument fallback support
//

#include <string.h>

#include "midifallback.h"
#include "i_printf.h"

static const byte presets[128][128] =
{
    [1] = // Variation #1
    {
        [38] = 1,
        [57] = 1,
        [60] = 1,
        [80] = 1,
        [81] = 1,
        [98] = 1,
        [102] = 1,
        [104] = 1,
        [120] = 1,
        [121] = 1,
        [122] = 1,
        [123] = 1,
        [124] = 1,
        [125] = 1,
        [126] = 1,
        [127] = 1,
    },
    [2] = // Variation #2
    {
        [102] = 2,
        [120] = 2,
        [122] = 2,
        [123] = 2,
        [124] = 2,
        [125] = 2,
        [126] = 2,
        [127] = 2,
        },
    [3] = // Variation #3
    {
        [122] = 3,
        [123] = 3,
        [124] = 3,
        [125] = 3,
        [126] = 3,
        [127] = 3,
    },
    [4] = // Variation #4
    {
        [122] = 4,
        [124] = 4,
        [125] = 4,
        [126] = 4,
    },
    [5] = // Variation #5
    {
        [122] = 5,
        [124] = 5,
        [125] = 5,
        [126] = 5,
    },
    [6] = // Variation #6
    {
        [125] = 6,
    },
    [7] = // Variation #7
    {
        [125] = 7,
    },
    [8] = // Variation #8
    {
        [0] = 8,
        [1] = 8,
        [2] = 8,
        [3] = 8,
        [4] = 8,
        [5] = 8,
        [6] = 8,
        [11] = 8,
        [12] = 8,
        [14] = 8,
        [16] = 8,
        [17] = 8,
        [19] = 8,
        [21] = 8,
        [24] = 8,
        [25] = 8,
        [26] = 8,
        [27] = 8,
        [28] = 8,
        [30] = 8,
        [31] = 8,
        [38] = 8,
        [39] = 8,
        [40] = 8,
        [48] = 8,
        [50] = 8,
        [61] = 8,
        [62] = 8,
        [63] = 8,
        [80] = 8,
        [81] = 8,
        [107] = 8,
        [115] = 8,
        [116] = 8,
        [117] = 8,
        [118] = 8,
        [125] = 8,
    },
    [9] = // Variation #9
    {
        [14] = 9,
        [118] = 9,
        [125] = 9,
    },
    [16] = // Variation #16
    {
        [0] = 16,
        [4] = 16,
        [5] = 16,
        [6] = 16,
        [16] = 16,
        [19] = 16,
        [24] = 16,
        [25] = 16,
        [28] = 16,
        [39] = 16,
        [62] = 16,
        [63] = 16,
    },
    [24] = // Variation #24
    {
        [4] = 24,
        [6] = 24,
    },
    [32] = // Variation #32
    {
        [16] = 32,
        [17] = 32,
        [24] = 32,
        [52] = 32,
    },
};

static const byte drums[128] =
{
    [8] = 8,
    [16] = 16,
    [24] = 24,
    [25] = 25,
    [32] = 32,
    [40] = 40,
    [48] = 48,
    [56] = 56,
};

static byte bank_msb[MIDI_CHANNELS_PER_TRACK];
static byte drum_map[MIDI_CHANNELS_PER_TRACK];

void MIDI_ResetFallback(void)
{
    memset(bank_msb, 0, sizeof(bank_msb));
    memset(drum_map, 0, sizeof(drum_map));

    // Channel 10 (index 9) is set to drum map 1 by default.
    drum_map[9] = 1;
}

void MIDI_UpdateBankMSB(byte idx, byte value)
{
    bank_msb[idx] = value;
}

void MIDI_UpdateDrumMap(byte idx, byte value)
{
    drum_map[idx] = value;
}

midi_fallback_t MIDI_BankLSBFallback(byte idx, byte value)
{
    midi_fallback_t fallback;

    if (value == 0)
    {
        // Bank select LSB is already zero. No fallback required.
        fallback.type = FALLBACK_NONE;
        fallback.value = 0;
    }
    else
    {
        // Bank select LSB is not supported.
        fallback.type = FALLBACK_BANK_LSB;
        fallback.value = 0;

        I_Printf(VB_DEBUG, "midifallback: ch=%d [lsb=%d] to [lsb=%d]",
                 idx, value, fallback.value);
    }

    return fallback;
}

midi_fallback_t MIDI_ProgramFallback(byte idx, byte program)
{
    midi_fallback_t fallback;

    if (drum_map[idx] == 0) // Normal channel
    {
        const byte variation = bank_msb[idx];

        if (variation == 0 || variation == presets[variation][program])
        {
            // Found a capital or variation. No fallback required.
            fallback.type = FALLBACK_NONE;
            fallback.value = 0;
        }
        else
        {
            fallback.type = FALLBACK_BANK_MSB;

            if (variation > 63 || program > 119)
            {
                // Fall to capital.
                fallback.value = 0;
            }
            else
            {
                // Fall to sub-capital (next multiple of 8).
                fallback.value = presets[variation & ~7][program];
            }

            I_Printf(VB_DEBUG, "midifallback: ch=%d pc=%d [msb=%d] to [msb=%d]",
                     idx, program, variation, fallback.value);
        }
    }
    else // Drums Channel
    {
        if (program == 0 || program == drums[program])
        {
            // Found a drum set. No fallback required.
            fallback.type = FALLBACK_NONE;
            fallback.value = 0;
        }
        else
        {
            // Fall to sub-drum set (next multiple of 8).
            fallback.type = FALLBACK_DRUMS;
            fallback.value = drums[program & ~7];

            I_Printf(VB_DEBUG, "midifallback: ch=%d (drums) [pc=%d] to [pc=%d]",
                     idx, program, fallback.value);
        }
    }

    return fallback;
}
