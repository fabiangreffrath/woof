//
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

#include <string.h>

#include "doomstat.h"
#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "p_map.h"
#include "p_mobj.h"
#include "m_argv.h"
#include "sounds.h"

mobjinfo_t *mobjinfo = NULL;
int num_mobj_types;

static int max_thing_number;

static hashmap_t *translate;

void DSDH_MobjInfoInit(void)
{
    num_mobj_types = NUMMOBJTYPES;
    max_thing_number = NUMMOBJTYPES - 1;

    array_resize(mobjinfo, NUMMOBJTYPES);
    memcpy(mobjinfo, original_mobjinfo, NUMMOBJTYPES * sizeof(*mobjinfo));

    // don't want to reorganize info.c structure for a few tweaks...
    for (int i = 0; i < num_mobj_types; ++i)
    {
        // DEHEXTRA
        mobjinfo[i].droppeditem = MT_NULL;
        // MBF21
        mobjinfo[i].flags2           = 0;
        mobjinfo[i].infighting_group = IG_DEFAULT;
        mobjinfo[i].projectile_group = PG_DEFAULT;
        mobjinfo[i].splash_group     = SG_DEFAULT;
        mobjinfo[i].ripsound         = sfx_None;
        mobjinfo[i].altspeed         = NO_ALTSPEED;
        mobjinfo[i].meleerange       = MELEERANGE;
        // Eternity
        mobjinfo[i].bloodcolor = 0;
    }

    // DEHEXTRA
    mobjinfo[MT_WOLFSS].droppeditem    = MT_CLIP;
    mobjinfo[MT_POSSESSED].droppeditem = MT_CLIP;
    mobjinfo[MT_SHOTGUY].droppeditem   = MT_SHOTGUN;
    mobjinfo[MT_CHAINGUY].droppeditem  = MT_CHAINGUN;

    // MBF21
    mobjinfo[MT_VILE].flags2    = MF2_SHORTMRANGE|MF2_DMGIGNORED|MF2_NOTHRESHOLD;
    mobjinfo[MT_UNDEAD].flags2  = MF2_LONGMELEE|MF2_RANGEHALF;
    mobjinfo[MT_FATSO].flags2   = MF2_MAP07BOSS1;
    mobjinfo[MT_BRUISER].flags2 = MF2_E1M8BOSS;
    mobjinfo[MT_SKULL].flags2   = MF2_RANGEHALF;
    mobjinfo[MT_SPIDER].flags2  = MF2_NORADIUSDMG|MF2_RANGEHALF|MF2_FULLVOLSOUNDS|MF2_E3M8BOSS|MF2_E4M8BOSS;
    mobjinfo[MT_BABY].flags2    = MF2_MAP07BOSS2;
    mobjinfo[MT_CYBORG].flags2  = MF2_NORADIUSDMG|MF2_HIGHERMPROB|MF2_RANGEHALF|MF2_FULLVOLSOUNDS|MF2_E2M8BOSS|MF2_E4M6BOSS;

    mobjinfo[MT_BRUISER].projectile_group = PG_BARON;
    mobjinfo[MT_KNIGHT].projectile_group  = PG_BARON;

    mobjinfo[MT_BRUISERSHOT].altspeed = IntToFixed(20);
    mobjinfo[MT_TROOPSHOT].altspeed   = IntToFixed(20);
    mobjinfo[MT_HEADSHOT].altspeed    = IntToFixed(20);

    for (int i = S_SARG_RUN1; i <= S_SARG_PAIN2; ++i)
    {
        states[i].flags |= STATEF_SKILL5FAST;
    }

    // Woof! randomly mirrored death animations
    for (int i = MT_PLAYER; i <= MT_KEEN; ++i)
    {
        switch (i)
        {
            case MT_FIRE:
            case MT_TRACER:
            case MT_SMOKE:
            case MT_FATSHOT:
            case MT_BRUISERSHOT:
            case MT_CYBORG:
                continue;
        }
        mobjinfo[i].flags_extra |= MFX_MIRROREDCORPSE;
    }

    mobjinfo[MT_PUFF].flags_extra  |= MFX_MIRROREDCORPSE;
    mobjinfo[MT_BLOOD].flags_extra |= MFX_MIRROREDCORPSE;

    for (int i = MT_MISC61; i <= MT_MISC69; ++i)
    {
        mobjinfo[i].flags_extra |= MFX_MIRROREDCORPSE;
    }

    mobjinfo[MT_DOGS].flags_extra |= MFX_MIRROREDCORPSE;

    //!
    // @category game
    //
    // Press beta emulation mode (complevel mbf only).
    //
    beta_emulation = M_CheckParm("-beta");
    if (beta_emulation)
    {
        // killough 10/98: beta lost soul has different behavior frames
        mobjinfo[MT_SKULL].spawnstate   = S_BSKUL_STND;
        mobjinfo[MT_SKULL].seestate     = S_BSKUL_RUN1;
        mobjinfo[MT_SKULL].painstate    = S_BSKUL_PAIN1;
        mobjinfo[MT_SKULL].missilestate = S_BSKUL_ATK1;
        mobjinfo[MT_SKULL].deathstate   = S_BSKUL_DIE1;
        mobjinfo[MT_SKULL].damage       = 1;
    }
    // This code causes MT_SCEPTRE and MT_BIBLE to not spawn on the map,
    // which causes desync in Eviternity.wad demos.
#ifdef MBF_STRICT
    else
    {
        mobjinfo[MT_SCEPTRE].doomednum = mobjinfo[MT_BIBLE].doomednum = -1;
    }
#endif
}

int DSDH_ThingTranslate(int thing_number)
{
    max_thing_number = MAX(max_thing_number, thing_number);

    if (thing_number < NUMMOBJTYPES)
    {
        return thing_number;
    }

    if (!translate)
    {
        translate = hashmap_init(256, sizeof(int));
    }

    int *index = hashmap_get(translate, thing_number); 
    if (index)
    {
        return *index;
    }

    int new_index = num_mobj_types;
    hashmap_put(translate, thing_number, &new_index);

    mobjinfo_t mobj = {
        // DEHEXTRA
        .droppeditem = MT_NULL,
        // MBF21
        .infighting_group = IG_DEFAULT,
        .projectile_group = PG_DEFAULT,
        .splash_group     = SG_DEFAULT,
        .altspeed         = NO_ALTSPEED,
        .meleerange       = MELEERANGE
    };
    array_push(mobjinfo, mobj);
    ++num_mobj_types;

    return new_index;
}

int DSDH_MobjInfoGetNewIndex(void)
{
    ++max_thing_number;
    return DSDH_ThingTranslate(max_thing_number);
}
