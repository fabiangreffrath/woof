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

#include "dsdhacked.h"
#include "info.h"
#include "p_map.h"
#include "p_mobj.h"
#include "sounds.h"

int zmt_ambientsound = ZMT_UNDEFINED;

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
