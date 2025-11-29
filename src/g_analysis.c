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

int demo_analysis = false;

boolean demo_pacifist = true;
boolean demo_reality = true;
boolean demo_almost_reality = true;
boolean demo_reborn = false;
int demo_missed_monsters = 0;
int demo_missed_secrets = 0;
int demo_missed_weapons = 0;
boolean demo_tyson_weapons = true;
boolean demo_100k = true;
boolean demo_100s = true;
boolean demo_any_counted_monsters = false;
boolean demo_any_monsters = false;
boolean demo_any_secrets = false;
boolean demo_any_weapons = false;
boolean demo_stroller = true;
boolean demo_nomo = false;
boolean demo_respawn = false;
boolean demo_fast = false;
boolean demo_turbo = false;
boolean demo_weapon_collector = true;

// TODO?
// DSDA's -track params are not supported

/*
int demo_kills_on_map = 0;
boolean demo_100k_on_map = false;
boolean demo_100k_note_shown = false;
boolean demo_pacifist_note_shown = false;
boolean demo_reality_note_shown = false;
boolean demo_almost_reality_note_shown = false;
*/

void G_ResetAnalysis(void)
{
    demo_pacifist = true;
    demo_reality = true;
    demo_almost_reality = true;
    demo_reborn = false;
    demo_missed_monsters = 0;
    demo_missed_secrets = 0;
    demo_missed_weapons = 0;
    demo_tyson_weapons = true;
    demo_100k = true;
    demo_100s = true;
    demo_any_counted_monsters = false;
    demo_any_monsters = false;
    demo_any_secrets = false;
    demo_any_weapons = false;
    demo_stroller = true;
    demo_turbo = false;
    demo_weapon_collector = true;
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

    if (demo_reality)
    {
        demo_almost_reality = false;
    }
    if (!demo_pacifist)
    {
        demo_stroller = false;
    }
    if (!demo_any_weapons)
    {
        demo_weapon_collector = false;
    }

    demo_nomo = nomonsters > 0;
    demo_respawn = respawnparm > 0;
    demo_fast = fastparm > 0;

    const char *category = G_DetectCategory();
    const int is_signed = -1; // Woof does not support ExCmdDemo

    fprintf(fstream, "skill %d\n", gameskill + 1);
    fprintf(fstream, "nomonsters %d\n", demo_nomo);
    fprintf(fstream, "respawn %d\n", demo_respawn);
    fprintf(fstream, "fast %d\n", demo_fast);
    fprintf(fstream, "pacifist %d\n", demo_pacifist);
    fprintf(fstream, "stroller %d\n", demo_stroller);
    fprintf(fstream, "reality %d\n", demo_reality);
    fprintf(fstream, "almost_reality %d\n", demo_almost_reality);
    fprintf(fstream, "reborn %d\n", demo_reborn);
    fprintf(fstream, "100k %d\n", demo_100k);
    fprintf(fstream, "100s %d\n", demo_100s);
    fprintf(fstream, "missed_monsters %d\n", demo_missed_monsters);
    fprintf(fstream, "missed_secrets %d\n", demo_missed_secrets);
    fprintf(fstream, "weapon_collector %d\n", demo_weapon_collector);
    fprintf(fstream, "tyson_weapons %d\n", demo_tyson_weapons);
    fprintf(fstream, "turbo %d\n", demo_turbo);
    fprintf(fstream, "solo_net %d\n", solonet);
    fprintf(fstream, "coop_spawns %d\n", coopspawns);
    fprintf(fstream, "category %s\n", category);
    fprintf(fstream, "signature %d\n", is_signed);

    fclose(fstream);

    return;
}

const char *G_DetectCategory(void)
{
    const boolean satisfies_max = (demo_missed_monsters == 0
                                   && demo_100s
                                   && (demo_any_secrets
                                      || demo_any_counted_monsters));

    const boolean satisfies_respawn = (demo_100s
                                       && demo_100k
                                       && demo_any_monsters
                                       && (demo_any_secrets
                                          || demo_any_counted_monsters));

    const boolean satisfies_tyson = (demo_missed_monsters == 0
                                     && demo_tyson_weapons
                                     && demo_any_counted_monsters);

    const boolean satisfies_100s = demo_any_secrets
                                   && demo_100s;

    if (demo_turbo || coopspawns || solonet || demo_reborn)
    {
        return "Other";
    }

    if (gameskill == sk_hard)
    {
        if (demo_nomo && !demo_respawn && !demo_fast)
        {
            if (satisfies_100s)
            {
                return "NoMo 100S";
            }

            return "NoMo";
        }

        if (demo_respawn && !demo_nomo && !demo_fast)
        {
            if (satisfies_respawn)
            {
                return "UV Respawn";
            }

            return "Other";
        }

        if (demo_fast && !demo_nomo && !demo_respawn)
        {
            if (satisfies_max)
            {
                return "UV Fast";
            }

            return "Other";
        }

        if (demo_nomo || demo_respawn || demo_fast)
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
        if (demo_any_monsters && demo_stroller)
        {
            return "Stroller";
        }
        if (demo_any_monsters && demo_pacifist)
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
