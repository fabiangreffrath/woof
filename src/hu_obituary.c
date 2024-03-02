//
//  Copyright (C) 2023 Fabian Greffrath
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:  Obituaries
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "d_deh.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_obituary.h"
#include "info.h"
#include "m_misc2.h"
#include "net_client.h"
#include "p_mobj.h"

int show_obituary_messages;
int hudcolor_obituary;

// [FG] gender-neutral pronouns

struct
{
    const char *const from;
    const char *const to;
} static const pronouns[] = {
    {"%g", "they"   },
    {"%h", "them"   },
    {"%p", "their"  },
    {"%s", "theirs" },
    {"%r", "they're"},
};

static char *playerstr[] = {
    "Player 1",
    "Player 2",
    "Player 3",
    "Player 4",
};

void HU_InitObituaries(void)
{
    // [FG] TODO only the server knows the names of all clients,
    //           but at least we know ours...

    playerstr[consoleplayer] = net_player_name;
}

static inline char *StrReplace(char *str, const char *from, const char *to)
{
    if (strstr(str, from) != NULL)
    {
        char *newstr;
        newstr = M_StringReplace(str, from, to);
        free(str);
        str = newstr;
    }

    return str;
}

void HU_Obituary(mobj_t *target, mobj_t *source, method_t mod)
{
    int i;
    char *ob = s_OB_DEFAULT, *str;

    if (target->player->mo != target)
    {
        ob = s_OB_VOODOO;
    }
    else if (target == source)
    {
        ob = s_OB_KILLEDSELF;
    }
    else if (source)
    {
        if (mod == MOD_Telefrag)
        {
            ob = (source->player) ? s_OB_MPTELEFRAG : s_OB_MONTELEFRAG;
        }
        else if (source->player)
        {
            switch (source->player->readyweapon)
            {
                case wp_fist:
                    ob = s_OB_MPFIST;
                    break;
                case wp_pistol:
                    ob = s_OB_MPPISTOL;
                    break;
                case wp_shotgun:
                    ob = s_OB_MPSHOTGUN;
                    break;
                case wp_chaingun:
                    ob = s_OB_MPCHAINGUN;
                    break;
                case wp_missile:
                    ob = s_OB_MPROCKET;
                    break;
                case wp_plasma:
                    ob = s_OB_MPPLASMARIFLE;
                    break;
                case wp_bfg:
                    ob = s_OB_MPBFG_BOOM;
                    break;
                case wp_chainsaw:
                    ob = s_OB_MPCHAINSAW;
                    break;
                case wp_supershotgun:
                    ob = s_OB_MPSSHOTGUN;
                    break;
                default:
                    ob = s_OB_MPDEFAULT;
                    break;
            }
        }
        else
        {
            switch (source->type)
            {
                case MT_POSSESSED:
                    ob = s_OB_ZOMBIE;
                    break;
                case MT_SHOTGUY:
                    ob = s_OB_SHOTGUY;
                    break;
                case MT_VILE:
                    ob = s_OB_VILE;
                    break;
                case MT_UNDEAD:
                    ob = (mod == MOD_Melee) ? s_OB_UNDEADHIT : s_OB_UNDEAD;
                    break;
                case MT_FATSO:
                    ob = s_OB_FATSO;
                    break;
                case MT_CHAINGUY:
                    ob = s_OB_CHAINGUY;
                    break;
                case MT_SKULL:
                    ob = s_OB_SKULL;
                    break;
                case MT_TROOP:
                    ob = (mod == MOD_Melee) ? s_OB_IMPHIT : s_OB_IMP;
                    break;
                case MT_HEAD:
                    ob = (mod == MOD_Melee) ? s_OB_CACOHIT : s_OB_CACO;
                    break;
                case MT_SERGEANT:
                    ob = s_OB_DEMONHIT;  // [FG] melee only
                    break;
                case MT_SHADOWS:
                    ob = s_OB_SPECTREHIT;  // [FG] melee only
                    break;
                case MT_BRUISER:
                    ob = (mod == MOD_Melee) ? s_OB_BARONHIT : s_OB_BARON;
                    break;
                case MT_KNIGHT:
                    ob = (mod == MOD_Melee) ? s_OB_KNIGHTHIT : s_OB_KNIGHT;
                    break;
                case MT_SPIDER:
                    ob = s_OB_SPIDER;
                    break;
                case MT_BABY:
                    ob = s_OB_BABY;
                    break;
                case MT_CYBORG:
                    ob = s_OB_CYBORG;
                    break;
                case MT_WOLFSS:
                    ob = s_OB_WOLFSS;
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        switch (mod)
        {
            case MOD_Slime:
                ob = s_OB_SLIME;
                break;
            case MOD_Lava:
                ob = s_OB_LAVA;
                break;
            case MOD_Crush:
                ob = s_OB_CRUSH;
                break;
            default:
                break;
        }
    }

    str = M_StringDuplicate(ob);

    for (i = 0; i < arrlen(pronouns); i++)
    {
        str = StrReplace(str, pronouns[i].from, pronouns[i].to);
    }

    if (source && source->player)
    {
        str = StrReplace(str, "%k", playerstr[source->player - players]);
    }

    str = StrReplace(str, "%o", playerstr[target->player - players]);

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
        {
            break;
        }

        doomprintf(&players[i], MESSAGES_OBITUARY, "\x1b%c%s",
                   '0' + hudcolor_obituary, str);
    }

    free(str);
}
