//
// Copyright (C) 1999 by
// id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright(C) 2023 ceski
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
//      System interface for WinMBF sound.
//

#include "i_oalsound.h"
#include "i_sound.h"
#include "r_main.h"

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST (1200 << FRACBITS)

// Distance to origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
//
// killough 12/98: restore original
// #define S_CLOSE_DIST (160<<FRACBITS)

#define S_CLOSE_DIST (200 << FRACBITS)

#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

static boolean I_MBF_AdjustSoundParams(const mobj_t *listener, const mobj_t *source,
                                       int basevolume, int *vol, int *sep, int *pri,
                                       int channel)
{
    fixed_t adx, ady, dist;
    angle_t angle;

    // calculate the distance to sound origin
    //  and clip it if necessary
    //
    // killough 11/98: scale coordinates down before calculations start
    // killough 12/98: use exact distance formula instead of approximation

    adx = abs((listener->x >> FRACBITS) - (source->x >> FRACBITS));
    ady = abs((listener->y >> FRACBITS) - (source->y >> FRACBITS));

    if (ady > adx)
        dist = adx, adx = ady, ady = dist;

    dist = adx ? FixedDiv(adx, finesine[(tantoangle[FixedDiv(ady, adx) >> DBITS]
                                         + ANG90) >> ANGLETOFINESHIFT]) : 0;

    if (!dist)  // killough 11/98: handle zero-distance as special case
    {
        *sep = NORM_SEP;
        *vol = basevolume;
        return *vol > 0;
    }

    if (dist > S_CLIPPING_DIST >> FRACBITS)
        return false;

    // angle of source to listener
    angle = R_PointToAngle2(listener->x, listener->y, source->x, source->y);

    if (angle <= listener->angle)
        angle += 0xffffffff;
    angle -= listener->angle;
    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    *sep = NORM_SEP - FixedMul(S_STEREO_SWING >> FRACBITS, finesine[angle]);

    // volume calculation
    *vol = dist < S_CLOSE_DIST >> FRACBITS ? basevolume :
        basevolume * ((S_CLIPPING_DIST >> FRACBITS) - dist) /
        S_ATTENUATOR;

    // haleyjd 09/27/06: decrease priority with volume attenuation
    *pri = *pri + (127 - *vol);

    if (*pri > 255) // cap to 255
        *pri = 255;

    return *vol > 0;
}

const sound_module_t sound_mbf_module =
{
    I_OAL_InitSound,
    I_OAL_CacheSound,
    I_MBF_AdjustSoundParams,
    I_OAL_UpdateSoundParams2D,
    I_OAL_StartSound,
    I_OAL_StopSound,
    I_OAL_SoundIsPlaying,
    I_OAL_ShutdownSound,
};
