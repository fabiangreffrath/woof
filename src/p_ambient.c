//
// Copyright(C) 2025 ceski
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
//      Ambient sound
//

#include <math.h>

#include "dsdhacked.h"
#include "g_game.h"
#include "i_sound.h"
#include "info.h"
#include "m_random.h"
#include "p_ambient.h"
#include "p_map.h"
#include "p_tick.h"
#include "s_sound.h"
#include "sounds.h"

#define AMB_UPDATE_TICS 7 // 200 ms
#define AMB_KEEP_ALIVE_DIST 330 // 23.57 mu/tic (SR50) * 2 (wallrun) * 7 tics

int zmt_ambientsound = ZMT_UNDEFINED;

static int RandomWaitTics(const ambient_data_t *data)
{
    return (data->min_tics
            + (data->max_tics - data->min_tics) * M_Random() / 255);
}

static float GetEffectiveOffset(ambient_t *ambient)
{
    // Uses the most recent time scale for the last leveltime interval, which
    // is good enough.
    return (ambient->last_offset
            + (float)(leveltime - ambient->last_leveltime) / TICRATE * 100
                  / realtic_clock_rate);
}

void P_GetAmbientSoundParams(ambient_t *ambient, sfxparams_t *params)
{
    params->close_dist = ambient->data.close_dist;
    params->clipping_dist = ambient->data.clipping_dist;
    params->stop_dist = params->clipping_dist + AMB_KEEP_ALIVE_DIST;
    params->volume_scale = ambient->data.volume_scale;
}

float P_GetAmbientSoundOffset(ambient_t *ambient)
{
    if (ambient->data.mode == AMB_MODE_CONTINUOUS)
    {
        const sfxinfo_t *sfx = &S_sfx[ambient->data.sfx_id];
        ambient->offset = GetEffectiveOffset(ambient);
        ambient->offset = fmodf(ambient->offset, sfx->length);

        if (ambient->offset < 0.0f)
        {
            ambient->offset += sfx->length;
        }
    }

    return ambient->offset;
}

boolean P_PlayingAmbientSound(ambient_t *ambient)
{
    return (ambient->active && ambient->playing);
}

void P_StopAmbientSound(ambient_t *ambient)
{
    ambient->active = false;
    ambient->playing = false;
}

void P_MarkAmbientSound(ambient_t *ambient, int handle)
{
    ambient->last_offset = I_GetSoundOffset(handle);
    ambient->last_leveltime = leveltime;
}

void P_EvictAmbientSound(ambient_t *ambient, int handle)
{
    ambient->playing = false;
    P_MarkAmbientSound(ambient, handle);
}

static boolean StartAmbientSound(ambient_t *ambient)
{
    return S_StartAmbientSound(ambient->origin, ambient->data.sfx_id, ambient);
}

static boolean CheckUpdateTics(ambient_t *ambient)
{
    if (--ambient->update_tics > 0)
    {
        return false;
    }

    ambient->update_tics = AMB_UPDATE_TICS;
    return true;
}

static void UpdateContinuous(ambient_t *ambient)
{
    if (!CheckUpdateTics(ambient))
    {
        return;
    }

    if (ambient->playing)
    {
        return;
    }

    if (!ambient->active)
    {
        ambient->active = true;
        ambient->offset = 0.0f;
        ambient->last_offset = 0.0f;
        ambient->last_leveltime = leveltime;
    }

    ambient->playing = StartAmbientSound(ambient);
}

static void UpdateInterval(ambient_t *ambient)
{
    if (--ambient->wait_tics > 0)
    {
        if (!CheckUpdateTics(ambient))
        {
            return;
        }

        if (ambient->active && !ambient->playing)
        {
            const sfxinfo_t *sfx = &S_sfx[ambient->data.sfx_id];
            ambient->offset = GetEffectiveOffset(ambient);

            if (ambient->offset >= 0.0f && ambient->offset < sfx->length)
            {
                ambient->playing = StartAmbientSound(ambient);
            }
            else
            {
                ambient->active = false;
            }
        }
    }
    else
    {
        if (ambient->data.mode == AMB_MODE_RANDOM)
        {
            ambient->wait_tics = RandomWaitTics(&ambient->data);
        }
        else
        {
            ambient->wait_tics = ambient->data.min_tics;
        }

        ambient->update_tics = AMB_UPDATE_TICS;
        ambient->active = true;
        ambient->offset = 0.0f;
        ambient->last_offset = 0.0f;
        ambient->last_leveltime = leveltime;
        ambient->playing = StartAmbientSound(ambient);
    }
}

static void T_AmbientSound(ambient_t *ambient)
{
    if (nosfxparm || !snd_ambient)
    {
        return;
    }

    switch (ambient->data.mode)
    {
        case AMB_MODE_CONTINUOUS:
            UpdateContinuous(ambient);
            break;

        case AMB_MODE_RANDOM:
        case AMB_MODE_PERIODIC:
            UpdateInterval(ambient);
            break;
    }
}

void T_AmbientSoundAdapter(mobj_t *mobj)
{
    T_AmbientSound((ambient_t *)mobj);
}

void P_AddAmbientSoundThinker(mobj_t *mobj, int index)
{
    if (!snd_ambient || !mobj)
    {
        return;
    }

    const ambient_data_t *data = S_GetAmbientData(index);

    if (!data || data->sfx_id == sfx_None || !S_sfx[data->sfx_id].length)
    {
        return;
    }

    ambient_t *ambient = arena_alloc(thinkers_arena, ambient_t);
    ambient->data = *data;

    switch (ambient->data.mode)
    {
        case AMB_MODE_CONTINUOUS:
            ambient->update_tics = AMB_UPDATE_NOW;
            ambient->wait_tics = -1;
            break;

        case AMB_MODE_RANDOM:
            ambient->update_tics = AMB_UPDATE_TICS;
            ambient->wait_tics = RandomWaitTics(&ambient->data);
            break;

        case AMB_MODE_PERIODIC:
            ambient->update_tics = AMB_UPDATE_TICS;
            ambient->wait_tics = ambient->data.min_tics;
            break;
    }

    ambient->active = false;
    ambient->playing = false;
    ambient->offset = 0.0f;
    ambient->last_offset = 0.0f;
    ambient->last_leveltime = 0;

    ambient->source = NULL;
    P_SetTarget(&ambient->source, mobj);
    ambient->origin =
        ambient->data.type == AMB_TYPE_POINT ? ambient->source : NULL;

    ambient->thinker.function.p1 = T_AmbientSoundAdapter;
    P_AddThinker(&ambient->thinker);
}

void P_InitAmbientSoundMobjInfo(void)
{
    mobjinfo_t amb_mobjinfo = {
        .doomednum = 14064,
        .spawnstate = S_NULL,
        .spawnhealth = 1000,
        .seestate = S_NULL,
        .seesound = sfx_None,
        .reactiontime = 8,
        .attacksound = sfx_None,
        .painstate = S_NULL,
        .painchance = 0,
        .painsound = sfx_None,
        .meleestate = S_NULL,
        .missilestate = S_NULL,
        .deathstate = S_NULL,
        .xdeathstate = S_NULL,
        .deathsound = sfx_None,
        .speed = 0,
        .radius = 20 * FRACUNIT,
        .height = 16 * FRACUNIT,
        .mass = 100,
        .damage = 0,
        .activesound = sfx_None,
        .flags = MF_NOBLOCKMAP | MF_NOSECTOR,
        .raisestate = S_NULL,
        .flags2 = 0,
        .infighting_group = IG_DEFAULT,
        .projectile_group = PG_DEFAULT,
        .splash_group = SG_DEFAULT,
        .ripsound = sfx_None,
        .altspeed = NO_ALTSPEED,
        .meleerange = MELEERANGE,
        .bloodcolor = 0,
        .droppeditem = MT_NULL,
        .obituary = NULL,
        .obituary_melee = NULL,
    };

    zmt_ambientsound = dsdh_GetNewMobjInfoIndex();
    mobjinfo[zmt_ambientsound] = amb_mobjinfo;
}
