//
// Copyright (C) 1999 by
// id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright(C) 2020-2023 Fabian Greffrath
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

#include <stdlib.h>

#include "doomstat.h"
#include "doomtype.h"
#include "i_oalsound.h"
#include "i_sound.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "r_main.h"
#include "tables.h"

int forceFlipPan;

static boolean I_MBF_AdjustSoundParams(const mobj_t *listener,
                                       const mobj_t *source, int chanvol,
                                       int *vol, int *sep, int *pri)
{
    fixed_t adx, ady, dist;
    angle_t angle;

    // haleyjd 05/29/06: allow per-channel volume scaling
    *vol = (snd_SfxVolume * chanvol) / 15;

    if (*vol < 1)
    {
        return false;
    }
    else if (*vol > 127)
    {
        *vol = 127;
    }

    *sep = NORM_SEP;

    if (!source || source == players[displayplayer].mo)
    {
        return true;
    }

    // haleyjd 08/12/04: we cannot adjust a sound for a NULL listener.
    if (!listener)
    {
        return true;
    }

    // calculate the distance to sound origin
    //  and clip it if necessary
    //
    // killough 11/98: scale coordinates down before calculations start
    // killough 12/98: use exact distance formula instead of approximation

    adx = abs((listener->x >> FRACBITS) - (source->x >> FRACBITS));
    ady = abs((listener->y >> FRACBITS) - (source->y >> FRACBITS));

    if (ady > adx)
    {
        dist = adx, adx = ady, ady = dist;
    }

    dist = adx ? FixedDiv(
               adx, finesine[(tantoangle[FixedDiv(ady, adx) >> DBITS] + ANG90)
                             >> ANGLETOFINESHIFT])
               : 0;

    if (!dist) // killough 11/98: handle zero-distance as special case
    {
        return true;
    }

    if (dist > S_CLIPPING_DIST >> FRACBITS)
    {
        return false;
    }

    if (source->x != players[displayplayer].mo->x
        || source->y != players[displayplayer].mo->y)
    {
        // angle of source to listener
        angle = R_PointToAngle2(listener->x, listener->y, source->x, source->y);

        if (angle <= listener->angle)
        {
            angle += 0xffffffff;
        }
        angle -= listener->angle;
        angle >>= ANGLETOFINESHIFT;

        // stereo separation
        *sep = NORM_SEP - FixedMul(S_STEREO_SWING >> FRACBITS, finesine[angle]);
    }

    // volume calculation
    if (dist > S_CLOSE_DIST >> FRACBITS)
    {
        *vol = *vol * ((S_CLIPPING_DIST >> FRACBITS) - dist) / S_ATTENUATOR;
    }

    // haleyjd 09/27/06: decrease priority with volume attenuation
    *pri = *pri + (127 - *vol);

    if (*pri > 255) // cap to 255
    {
        *pri = 255;
    }

    return (*vol > 0);
}

void I_MBF_UpdateSoundParams(int channel, int volume, int separation)
{
    // SoM 7/1/02: forceFlipPan accounted for here
    if (forceFlipPan)
    {
        separation = 254 - separation;
    }

    I_OAL_SetVolume(channel, volume);
    I_OAL_SetPan(channel, separation);
}

const sound_module_t sound_mbf_module =
{
    I_OAL_InitSound,
    I_OAL_ReinitSound,
    I_OAL_AllowReinitSound,
    I_OAL_CacheSound,
    I_MBF_AdjustSoundParams,
    I_MBF_UpdateSoundParams,
    NULL,
    I_OAL_StartSound,
    I_OAL_StopSound,
    I_OAL_SoundIsPlaying,
    I_OAL_ShutdownSound,
    I_OAL_ShutdownModule,
    I_OAL_DeferUpdates,
    I_OAL_ProcessUpdates,
};
