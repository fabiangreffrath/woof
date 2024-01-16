//
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
//      System interface for 3D sound.
//

#include <stdio.h>
#include <stdlib.h>

#include "doomstat.h"
#include "i_oalsound.h"
#include "r_data.h"
#include "r_main.h"
#include "p_setup.h"

#define FIXED_TO_ALFLOAT(x) ((ALfloat)(FIXED2DOUBLE(x)))

typedef struct oal_listener_params_s
{
    ALfloat position[3];
    ALfloat velocity[3];
    ALfloat orientation[6];
} oal_listener_params_t;

typedef struct oal_source_params_s
{
    ALfloat position[3];
    ALfloat velocity[3];
    boolean use_3d;
    boolean point_source;
    fixed_t z;
} oal_source_params_t;

static oal_source_params_t src;

static int CalcFinePitch(const player_t *player)
{
    fixed_t pitch;

    if (player->pitch == 0)
    {
        return 0;
    }

    // Flip sign due to right-hand rule.
    pitch = -player->pitch;
    if (pitch < 0)
    {
        return ((ANGLE_MAX + pitch) >> ANGLETOFINESHIFT);
    }
    else
    {
        return (pitch >> ANGLETOFINESHIFT);
    }
}

static void CalcListenerParams(const mobj_t *listener, oal_listener_params_t *lis)
{
    const player_t *player = listener->player;
    const int yaw = listener->angle >> ANGLETOFINESHIFT;
    const int pitch = CalcFinePitch(player);

    // Doom to OpenAL space: {x, y, z} to {x, z, -y}

    lis->position[0] = FIXED_TO_ALFLOAT(listener->x);
    lis->position[1] = FIXED_TO_ALFLOAT(player->viewz);
    lis->position[2] = FIXED_TO_ALFLOAT(-listener->y);

    if (oal_use_doppler)
    {
        lis->velocity[0] = FIXED_TO_ALFLOAT(listener->momx) * TICRATE;
        lis->velocity[1] = FIXED_TO_ALFLOAT(listener->momz) * TICRATE;
        lis->velocity[2] = FIXED_TO_ALFLOAT(-listener->momy) * TICRATE;
    }
    else
    {
        lis->velocity[0] = 0.0f;
        lis->velocity[1] = 0.0f;
        lis->velocity[2] = 0.0f;
    }

    if (pitch == 0)
    {
        // "At" vector after yaw rotation.
        lis->orientation[0] = FIXED_TO_ALFLOAT(finecosine[yaw]);
        lis->orientation[1] = 0.0f;
        lis->orientation[2] = FIXED_TO_ALFLOAT(-finesine[yaw]);

        // "Up" vector doesn't change.
        lis->orientation[3] = 0.0f;
        lis->orientation[4] = 1.0f;
        lis->orientation[5] = 0.0f;
    }
    else
    {
        // "At" vector after yaw and pitch rotations.
        lis->orientation[0] = FIXED_TO_ALFLOAT(FixedMul(finecosine[yaw], finecosine[pitch]));
        lis->orientation[1] = FIXED_TO_ALFLOAT(-finesine[pitch]);
        lis->orientation[2] = FIXED_TO_ALFLOAT(-FixedMul(finesine[yaw], finecosine[pitch]));

        // "Up" vector after yaw and pitch rotations.
        lis->orientation[3] = FIXED_TO_ALFLOAT(FixedMul(finecosine[yaw], finesine[pitch]));
        lis->orientation[4] = FIXED_TO_ALFLOAT(finecosine[pitch]);
        lis->orientation[5] = FIXED_TO_ALFLOAT(-FixedMul(finesine[yaw], finesine[pitch]));
    }
}

static void CalcSourceParams(const mobj_t *source, oal_source_params_t *src)
{
    // Doom to OpenAL space: {x, y, z} to {x, z, -y}

    src->position[0] = FIXED_TO_ALFLOAT(source->x);
    src->position[1] = FIXED_TO_ALFLOAT(src->z);
    src->position[2] = FIXED_TO_ALFLOAT(-source->y);

    // Doppler effect only applies to monsters and projectiles.
    if (oal_use_doppler && src->point_source)
    {
        src->velocity[0] = FIXED_TO_ALFLOAT(source->momx) * TICRATE;
        src->velocity[1] = FIXED_TO_ALFLOAT(source->momz) * TICRATE;
        src->velocity[2] = FIXED_TO_ALFLOAT(-source->momy) * TICRATE;
    }
    else
    {
        src->velocity[0] = 0.0f;
        src->velocity[1] = 0.0f;
        src->velocity[2] = 0.0f;
    }
}

static void CalcHypotenuse(fixed_t adx, fixed_t ady, fixed_t *dist)
{
    if (ady > adx)
    {
        const fixed_t temp = adx;
        adx = ady;
        ady = temp;
    }

    if (adx)
    {
        const int slope = FixedDiv(ady, adx) >> DBITS;
        const int angle = tantoangle[slope] >> ANGLETOFINESHIFT;
        *dist = FixedDiv(adx, finecosine[angle]);
    }
    else
    {
        *dist = 0;
    }
}

static void CalcDistance(const mobj_t *listener, const mobj_t *source,
                         oal_source_params_t *src, fixed_t *dist)
{
    const fixed_t adx = abs((listener->x >> FRACBITS) - (source->x >> FRACBITS));
    const fixed_t ady = abs((listener->y >> FRACBITS) - (source->y >> FRACBITS));
    fixed_t distxy;

    CalcHypotenuse(adx, ady, &distxy);

    // Treat monsters and projectiles as point sources.
    src->point_source = (source->thinker.function.p1 != (actionf_p1)P_DegenMobjThinker &&
                        source->info && source->actualheight);

    if (src->point_source)
    {
        fixed_t adz;
        // Vertical distance is from player's view to middle of source's sprite.
        src->z = source->z + (source->actualheight >> 1);
        adz = abs((listener->player->viewz >> FRACBITS) - (src->z >> FRACBITS));
        CalcHypotenuse(distxy, adz, dist);
    }
    else
    {
        // The source is a door, switch, lift, etc. and doesn't have a proper
        // vertical position. Ignore vertical distance like vanilla Doom.
        src->z = listener->player->viewz;
        *dist = distxy;
    }
}

static boolean CalcVolumePriority(fixed_t dist, int *vol, int *pri)
{
    int pri_volume;

    if (dist == 0)
    {
        return true;
    }
    else if (dist >= (S_CLIPPING_DIST >> FRACBITS))
    {
        return false;
    }
    else if (dist <= (S_CLOSE_DIST >> FRACBITS))
    {
        pri_volume = *vol;
    }
    else if (dist > S_ATTENUATOR)
    {
        // OpenAL inverse distance model never reaches zero volume. Gradually
        // ramp down the volume as the distance approaches the limit.
        pri_volume = *vol * ((S_CLIPPING_DIST >> FRACBITS) - dist) /
                            (S_CLOSE_DIST >> FRACBITS);
        *vol = pri_volume;
    }
    else
    {
        // Range where OpenAL inverse distance model applies. Calculate volume
        // for priority bookkeeping but let OpenAL handle the real volume.
        // Simplify formula for OAL_ROLLOFF_FACTOR = 1:
        pri_volume = *vol * (S_CLOSE_DIST >> FRACBITS) / dist;
    }

    // Decrease priority with volume attenuation.
    *pri += (127 - pri_volume);

    if (*pri > 255)
        *pri = 255;

    return (pri_volume > 0);
}

static boolean ScaleVolume(int chanvol, int *vol)
{
    *vol = (snd_SfxVolume * chanvol) / 15;

    if (*vol < 1)
        return false;
    else if (*vol > 127)
        *vol = 127;

    return true;
}

static boolean I_3D_AdjustSoundParams(const mobj_t *listener, const mobj_t *source,
                                      int chanvol, int *vol, int *sep, int *pri)
{
    fixed_t dist;

    if (!ScaleVolume(chanvol, vol))
    {
        return false;
    }

    if (!source || source == players[displayplayer].mo || !listener ||
        !listener->player)
    {
        src.use_3d = false;
        return true;
    }

    CalcDistance(listener, source, &src, &dist);

    if (!CalcVolumePriority(dist, vol, pri))
    {
        return false;
    }

    src.use_3d = true;
    CalcSourceParams(source, &src);

    return true;
}

static void I_3D_UpdateSoundParams(int channel, int volume, int separation)
{
    if (src.use_3d)
    {
        I_OAL_UpdateSourceParams(channel, src.position, src.velocity);
    }

    I_OAL_SetVolume(channel, volume);
}

static void I_3D_UpdateListenerParams(const mobj_t *listener)
{
    oal_listener_params_t lis;

    if (!listener || !listener->player)
    {
        return;
    }

    CalcListenerParams(listener, &lis);
    I_OAL_UpdateListenerParams(lis.position, lis.velocity, lis.orientation);
}

static boolean I_3D_StartSound(int channel, sfxinfo_t *sfx, int pitch)
{
    if (src.use_3d)
    {
        I_OAL_ResetSource3D(channel, src.point_source);
    }
    else
    {
        I_OAL_ResetSource2D(channel);
    }

    return I_OAL_StartSound(channel, sfx, pitch);
}

const sound_module_t sound_3d_module =
{
    I_OAL_InitSound,
    I_OAL_ReinitSound,
    I_OAL_AllowReinitSound,
    I_OAL_UpdateUserSoundSettings,
    I_OAL_CacheSound,
    I_3D_AdjustSoundParams,
    I_3D_UpdateSoundParams,
    I_3D_UpdateListenerParams,
    I_3D_StartSound,
    I_OAL_StopSound,
    I_OAL_SoundIsPlaying,
    I_OAL_ShutdownSound,
    I_OAL_ShutdownModule,
    I_OAL_DeferUpdates,
    I_OAL_ProcessUpdates,
};
