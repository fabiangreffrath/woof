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

#define VIEWDIST (ORIGWIDTH / 2) // Distance from projection plane to POV.
#define FIXED_TO_ALFLOAT(x) ((ALfloat)(((double)x) / FRACUNIT))

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
    boolean point_source;
} oal_source_params_t;

static boolean I_3D_InitSound(void)
{
    return I_OAL_InitSound(true);
}

static boolean I_3D_ReinitSound(void)
{
    return I_OAL_ReinitSound(true);
}

static void CalcPitchAngle(const player_t *player, angle_t *pitch)
{
    fixed_t lookdir, slope;

    // Same as R_SetupFrame() in r_main.c.
    lookdir = player->lookdir / MLOOKUNIT + player->recoilpitch;

    if (lookdir == 0)
    {
        *pitch = 0;
        return;
    }

    if (lookdir > LOOKDIRMAX)
        lookdir = LOOKDIRMAX;
    else if (lookdir < -LOOKDIRMAX)
        lookdir = -LOOKDIRMAX;

    slope = (lookdir << FRACBITS) / VIEWDIST;
    if (slope < 0)
    {
        *pitch = (ANGLE_MAX - tantoangle[-slope >> DBITS]) >> ANGLETOFINESHIFT;
    }
    else
    {
        *pitch = tantoangle[slope >> DBITS] >> ANGLETOFINESHIFT;
    }
}

static void CalcListenerParams(const mobj_t *listener, oal_listener_params_t *lis)
{
    const player_t *player = listener->player;
    const angle_t yaw = listener->angle >> ANGLETOFINESHIFT;
    angle_t pitch;

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

    CalcPitchAngle(player, &pitch);
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
        lis->orientation[0] = FIXED_TO_ALFLOAT(finecosine[yaw] * finesine[pitch]);
        lis->orientation[1] = FIXED_TO_ALFLOAT(finecosine[pitch]);
        lis->orientation[2] = FIXED_TO_ALFLOAT(-finesine[yaw] * finesine[pitch]);

        // "Up" vector after yaw and pitch rotations.
        lis->orientation[3] = FIXED_TO_ALFLOAT(-finecosine[yaw] * finecosine[pitch]);
        lis->orientation[4] = FIXED_TO_ALFLOAT(finesine[pitch]);
        lis->orientation[5] = FIXED_TO_ALFLOAT(finesine[yaw] * finecosine[pitch]);
    }
}

static void CalcSourceParams(const mobj_t *listener, const mobj_t *source,
                             oal_source_params_t *src)
{
    src->position[0] = FIXED_TO_ALFLOAT(source->x);
    src->position[2] = FIXED_TO_ALFLOAT(-source->y);

    // Treat monsters and projectiles as point sources.
    src->point_source = (source->info && source->info->actualheight);

    if (src->point_source)
    {
        // Set vertical position to middle of sprite.
        src->position[1] = FIXED_TO_ALFLOAT(source->z + (source->info->actualheight >> 1));

        if (oal_use_doppler)
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
    else
    {
        // This is a door, switch, lift, etc. and doesn't have a proper vertical
        // position. Similar to vanilla Doom, make sure the player can hear this
        // at any vertical position (e.g. tall lifts).
        
        // Set the source's vertical position to the listener's vertical position.
        src->position[1] = FIXED_TO_ALFLOAT(listener->player->viewz);

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
                         fixed_t *dist)
{
    const fixed_t adx = abs((listener->x >> FRACBITS) - (source->x >> FRACBITS));
    const fixed_t ady = abs((listener->y >> FRACBITS) - (source->y >> FRACBITS));
    const fixed_t adz = abs((listener->z >> FRACBITS) - (source->z >> FRACBITS));
    fixed_t distxy;

    CalcHypotenuse(adx, ady, &distxy);
    CalcHypotenuse(distxy, adz, dist);
}

static boolean CalcVolumePriority(const mobj_t *listener, const mobj_t *source,
                                  int *vol, int *pri)
{
    fixed_t dist;
    int pri_volume;

    CalcDistance(listener, source, &dist);

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
                                      int chanvol, int *vol, int *sep, int *pri,
                                      int channel)
{
    oal_listener_params_t lis;
    oal_source_params_t src;

    if (!ScaleVolume(chanvol, vol))
    {
        return false;
    }

    if (!source || source == players[displayplayer].mo || !listener ||
        !listener->player)
    {
        I_OAL_DeferUpdates();
        I_OAL_ResetSource2D(channel);
        return true;
    }

    if (!CalcVolumePriority(listener, source, vol, pri))
    {
        return false;
    }

    CalcSourceParams(listener, source, &src);
    CalcListenerParams(listener, &lis);

    I_OAL_DeferUpdates();
    I_OAL_ResetSource3D(channel, src.point_source);
    I_OAL_AdjustSource3D(channel, src.position, src.velocity);
    I_OAL_AdjustListener3D(lis.position, lis.velocity, lis.orientation);

    return true;
}

static void I_3D_UpdateSoundParams(int channel, int volume, int separation)
{
    I_OAL_SetVolume(channel, volume);
    I_OAL_ProcessUpdates();
}

const sound_module_t sound_3d_module =
{
    I_3D_InitSound,
    I_3D_ReinitSound,
    I_OAL_AllowReinitSound,
    I_OAL_UpdateUserSoundSettings,
    I_OAL_CacheSound,
    I_3D_AdjustSoundParams,
    I_3D_UpdateSoundParams,
    I_OAL_StartSound,
    I_OAL_StopSound,
    I_OAL_SoundIsPlaying,
    I_OAL_ShutdownSound,
};
