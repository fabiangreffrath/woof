//
// Copyright(C) 2020 by Ryan Krafnick
// Copyright(C) 2025 by Guilherme Miranda
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
//	Doom Speed Demos Archive, demo analysis
//

#include <stdio.h>

#include "g_analysis.h"

#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_io.h"

boolean demo_analysis = false;
demo_analysis_t analysis = {0};

// TODO: update to support arbitrarily any weapons, ID24hacked
inline boolean G_IsWeapon(mobj_t *thing)
{
    switch (thing->sprite)
    {
        case SPR_BFUG:
        case SPR_MGUN:
        case SPR_CSAW:
        case SPR_LAUN:
        case SPR_PLAS:
        case SPR_SHOT:
        case SPR_SGN2:
            return true;
        default:
            return false;
    }
}

void G_ResetAnalysis(void)
{
    analysis.pacifist = true;
    analysis.reality = true;
    analysis.almost_reality = true;
    analysis.reborn = false;
    analysis.missed_monsters = 0;
    analysis.missed_secrets = 0;
    analysis.missed_weapons = 0;
    analysis.tyson = true;
    analysis.kill_100 = true;
    analysis.secret_100 = true;
    analysis.any_counted_monsters = false;
    analysis.any_monsters = false;
    analysis.any_secrets = false;
    analysis.any_weapons = false;
    analysis.stroller = true;
    analysis.turbo = false;
    analysis.collector = true;
}

void G_WriteAnalysis(void)
{
    if (!demo_analysis)
    {
        return;
    }

    FILE *fstream = M_fopen("analysis.txt", "w");

    if (!fstream)
    {
        I_Printf(VB_WARNING, "Unable to open analysis.txt for writing!\n");
        return;
    }

    if (analysis.reality)
    {
        analysis.almost_reality = false;
    }
    if (!analysis.pacifist)
    {
        analysis.stroller = false;
    }
    if (!analysis.any_weapons)
    {
        analysis.collector = false;
    }

    analysis.nomo = nomonsters > 0;
    analysis.respawn = respawnparm > 0;
    analysis.fast = fastparm > 0;

    const char *category = G_DetectCategory();
    const int is_signed = -1; // Woof does not support ExCmdDemo

    fprintf(fstream, "skill %d\n", gameskill + 1);
    fprintf(fstream, "nomonsters %d\n", analysis.nomo);
    fprintf(fstream, "respawn %d\n", analysis.respawn);
    fprintf(fstream, "fast %d\n", analysis.fast);
    fprintf(fstream, "pacifist %d\n", analysis.pacifist);
    fprintf(fstream, "stroller %d\n", analysis.stroller);
    fprintf(fstream, "reality %d\n", analysis.reality);
    fprintf(fstream, "almost_reality %d\n", analysis.almost_reality);
    fprintf(fstream, "reborn %d\n", analysis.reborn);
    fprintf(fstream, "100k %d\n", analysis.kill_100);
    fprintf(fstream, "100s %d\n", analysis.secret_100);
    fprintf(fstream, "missed_monsters %d\n", analysis.missed_monsters);
    fprintf(fstream, "missed_secrets %d\n", analysis.missed_secrets);
    fprintf(fstream, "weapon_collector %d\n", analysis.collector);
    fprintf(fstream, "tyson_weapons %d\n", analysis.tyson);
    fprintf(fstream, "turbo %d\n", analysis.turbo);
    fprintf(fstream, "solo_net %d\n", solonet);
    fprintf(fstream, "coop_spawns %d\n", coopspawns);
    fprintf(fstream, "category %s\n", category);
    fprintf(fstream, "signature %d\n", is_signed);

    fclose(fstream);

    return;
}

const char *G_DetectCategory(void)
{
    const boolean satisfies_max = analysis.missed_monsters == 0
                                  && analysis.secret_100
                                  && (analysis.any_secrets
                                     || analysis.any_counted_monsters);

    const boolean satisfies_respawn = analysis.secret_100
                                      && analysis.kill_100
                                      && analysis.any_monsters
                                      && (analysis.any_secrets
                                         || analysis.any_counted_monsters);

    const boolean satisfies_tyson = analysis.missed_monsters == 0
                                    && analysis.tyson
                                    && analysis.any_counted_monsters;

    const boolean satisfies_100s = analysis.any_secrets
                                   && analysis.secret_100;

    if (analysis.turbo || coopspawns || solonet || analysis.reborn)
    {
        return "Other";
    }

    if (gameskill == sk_hard)
    {
        if (analysis.nomo && !analysis.respawn && !analysis.fast)
        {
            if (satisfies_100s)
            {
                return "NoMo 100S";
            }

            return "NoMo";
        }

        if (analysis.respawn && !analysis.nomo && !analysis.fast)
        {
            if (satisfies_respawn)
            {
                return "UV Respawn";
            }

            return "Other";
        }

        if (analysis.fast && !analysis.nomo && !analysis.respawn)
        {
            if (satisfies_max)
            {
                return "UV Fast";
            }

            return "Other";
        }

        if (analysis.nomo || analysis.respawn || analysis.fast)
        {
            return "Other";
        }

        if (satisfies_max)
        {
            return "UV Max";
        }
        if (satisfies_tyson)
        {
            return "UV Tyson";
        }
        if (analysis.any_monsters && analysis.stroller)
        {
            return "Stroller";
        }
        if (analysis.any_monsters && analysis.pacifist)
        {
            return "Pacifist";
        }

        return "UV Speed";
    }
    else if (gameskill == sk_nightmare)
    {
        if (nomonsters)
        {
            return "Other";
        }
        if (satisfies_100s)
        {
            return "NM 100S";
        }

        return "NM Speed";
    }

    return "Other";
}
