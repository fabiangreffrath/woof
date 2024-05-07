//
//  Copyright (C) 2023-2024 Fabian Greffrath
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
#include "m_misc.h"
#include "net_client.h"
#include "p_mobj.h"

boolean show_obituary_messages;
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

static const char *playerstr[] = {
    "Player 1",
    "Player 2",
    "Player 3",
    "Player 4",
};

static void AssignObituary(const int type, char *ob, char *ob_m)
{
    if (ob && !mobjinfo[type].obituary)
    {
        mobjinfo[type].obituary = ob;
    }
    if (ob_m && !mobjinfo[type].obituary_melee)
    {
        mobjinfo[type].obituary_melee = ob_m;
    }
}

void HU_InitObituaries(void)
{
    AssignObituary(MT_POSSESSED, s_OB_ZOMBIE,   NULL);
    AssignObituary(MT_SHOTGUY,   s_OB_SHOTGUY,  NULL);
    AssignObituary(MT_VILE,      s_OB_VILE,     NULL);
    AssignObituary(MT_UNDEAD,    s_OB_UNDEAD,   s_OB_UNDEADHIT);
    AssignObituary(MT_FATSO,     s_OB_FATSO,    NULL);
    AssignObituary(MT_CHAINGUY,  s_OB_CHAINGUY, NULL);
    AssignObituary(MT_SKULL,     s_OB_SKULL,    NULL);
    AssignObituary(MT_TROOP,     s_OB_IMP,      s_OB_IMPHIT);
    AssignObituary(MT_HEAD,      s_OB_CACO,     s_OB_CACOHIT);
    AssignObituary(MT_SERGEANT,  NULL,          s_OB_DEMONHIT);
    AssignObituary(MT_SHADOWS,   NULL,          s_OB_SPECTREHIT);
    AssignObituary(MT_BRUISER,   s_OB_BARON,    s_OB_BARONHIT);
    AssignObituary(MT_KNIGHT,    s_OB_KNIGHT,   s_OB_KNIGHTHIT);
    AssignObituary(MT_SPIDER,    s_OB_SPIDER,   NULL);
    AssignObituary(MT_BABY,      s_OB_BABY,     NULL);
    AssignObituary(MT_CYBORG,    s_OB_CYBORG,   NULL);
    AssignObituary(MT_WOLFSS,    s_OB_WOLFSS,   NULL);

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
            const int type = source->type;

            if (type >= 0 && type < num_mobj_types)
            {
                if (mod == MOD_Melee && mobjinfo[type].obituary_melee)
                {
                    ob = mobjinfo[type].obituary_melee;
                }
                else if (mobjinfo[type].obituary)
                {
                    ob = mobjinfo[type].obituary;
                }
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
