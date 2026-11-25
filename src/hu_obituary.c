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

#include "d_player.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_message.h"
#include "hu_obituary.h"
#include "info.h"
#include "m_misc.h"
#include "nw_client.h"
#include "p_mobj.h"
#include "v_trans.h"

boolean show_obituary_messages;
int hudcolor_obituary;

// [FG] gender-neutral pronouns, expanded together with the player names so
// that the template is scanned exactly once: a name that happens to contain
// "%o" or a colour escape cannot change the rest of the line.

static char player_names[MAXPLAYERS][MAXPLAYERNAME];

static void CopyPlayerName(char *buffer, size_t size, const char *name)
{
    size_t length = 0;

    // Escape sequences and control characters in a name would leak into the
    // message and its colouring, so only text is kept.

    for (const char *p = name; *p && length + 1 < size; ++p)
    {
        if ((unsigned char)*p < 0x20 || *p == 0x7f)
        {
            continue;
        }

        buffer[length++] = *p;
    }

    buffer[length] = '\0';
}

static void SetPlayerName(int player, const char *name)
{
    CopyPlayerName(player_names[player], sizeof(player_names[player]), name);

    // A player may connect under an empty name; the message still has to
    // name somebody.

    if (!player_names[player][0])
    {
        M_snprintf(player_names[player], sizeof(player_names[player]),
                   "Player %d", player + 1);
    }
}

static void UpdatePlayerNames(void)
{
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        M_snprintf(player_names[i], sizeof(player_names[i]), "Player %d",
                   i + 1);
    }

    // The roster is not part of a recorded demo.

    if (!demoplayback && nw_client_received_wait_data)
    {
        const int num_players = (nw_client_wait_data.num_players < MAXPLAYERS)
                                    ? nw_client_wait_data.num_players
                                    : MAXPLAYERS;

        for (int i = 0; i < num_players; ++i)
        {
            SetPlayerName(i, nw_client_wait_data.player_names[i]);
        }
    }

    // A single player demo shows the name of the player who recorded it,
    // which is the local one; a recorded netgame carries no roster at all,
    // so its players keep the generic names.

    if (nw_player_name && consoleplayer >= 0 && consoleplayer < MAXPLAYERS
        && !(demoplayback && netgame))
    {
        SetPlayerName(consoleplayer, nw_player_name);
    }
}

static void AppendText(char *buffer, size_t size, size_t *length,
                       const char *text)
{
    while (*text && *length + 1 < size)
    {
        buffer[(*length)++] = *text++;
    }

    buffer[*length] = '\0';
}

static void ExpandObituary(const char *ob_text, const char *victim,
                           const char *killer, char *buffer, size_t size)
{
    size_t length = 0;

    buffer[0] = '\0';

    for (const char *p = ob_text; *p; ++p)
    {
        const char *text = NULL;

        if (*p == '%')
        {
            switch (p[1])
            {
                case 'o':
                    text = victim;
                    break;
                case 'k':
                    text = killer;
                    break;
                case 'g':
                    text = "they";
                    break;
                case 'h':
                    text = "them";
                    break;
                case 'p':
                    text = "their";
                    break;
                case 's':
                    text = "theirs";
                    break;
                case 'r':
                    text = "they're";
                    break;
                default:
                    break;
            }

            if (text)
            {
                ++p;
            }
        }

        if (text)
        {
            AppendText(buffer, size, &length, text);
        }
        else if (length + 1 < size)
        {
            buffer[length++] = *p;
            buffer[length] = '\0';
        }
    }
}

static void AssignObituary(const int type, char *ob, char *ob_m)
{
    if (ob && !mobjinfo[type].obituary)
    {
        mobjinfo[type].obituary = DEH_String(ob);
    }
    if (ob_m && !mobjinfo[type].obituary_melee)
    {
        mobjinfo[type].obituary_melee = DEH_String(ob_m);
    }
}

void HU_InitObituaries(void)
{
    AssignObituary(MT_POSSESSED, OB_ZOMBIE,   NULL);
    AssignObituary(MT_SHOTGUY,   OB_SHOTGUY,  NULL);
    AssignObituary(MT_VILE,      OB_VILE,     NULL);
    AssignObituary(MT_UNDEAD,    OB_UNDEAD,   OB_UNDEADHIT);
    AssignObituary(MT_FATSO,     OB_FATSO,    NULL);
    AssignObituary(MT_CHAINGUY,  OB_CHAINGUY, NULL);
    AssignObituary(MT_SKULL,     OB_SKULL,    NULL);
    AssignObituary(MT_TROOP,     OB_IMP,      OB_IMPHIT);
    AssignObituary(MT_HEAD,      OB_CACO,     OB_CACOHIT);
    AssignObituary(MT_SERGEANT,  NULL,        OB_DEMONHIT);
    AssignObituary(MT_SHADOWS,   NULL,        OB_SPECTREHIT);
    AssignObituary(MT_BRUISER,   OB_BARON,    OB_BARONHIT);
    AssignObituary(MT_KNIGHT,    OB_KNIGHT,   OB_KNIGHTHIT);
    AssignObituary(MT_SPIDER,    OB_SPIDER,   NULL);
    AssignObituary(MT_BABY,      OB_BABY,     NULL);
    AssignObituary(MT_CYBORG,    OB_CYBORG,   NULL);
    AssignObituary(MT_WOLFSS,    OB_WOLFSS,   NULL);
}

void HU_Obituary(mobj_t *target, mobj_t *inflictor, mobj_t *source, method_t mod)
{
    const char *ob = OB_DEFAULT;

    if (target->player->mo != target)
    {
        ob = OB_VOODOO;
    }
    else if (target == source)
    {
        ob = OB_KILLEDSELF;

        if (inflictor && inflictor != target
            && mobjinfo[inflictor->type].obituary_self)
        {
            ob = mobjinfo[inflictor->type].obituary_self;
        }
    }
    else if (source)
    {
        if (mod == MOD_Telefrag)
        {
            ob = (source->player) ? OB_MPTELEFRAG : OB_MONTELEFRAG;
        }
        else if (source->player)
        {
            switch (source->player->readyweapon)
            {
                case wp_fist:
                    ob = OB_MPFIST;
                    break;
                case wp_pistol:
                    ob = OB_MPPISTOL;
                    break;
                case wp_shotgun:
                    ob = OB_MPSHOTGUN;
                    break;
                case wp_chaingun:
                    ob = OB_MPCHAINGUN;
                    break;
                case wp_missile:
                    ob = OB_MPROCKET;
                    break;
                case wp_plasma:
                    ob = OB_MPPLASMARIFLE;
                    break;
                case wp_bfg:
                    ob = OB_MPBFG_BOOM;
                    break;
                case wp_chainsaw:
                    ob = OB_MPCHAINSAW;
                    break;
                case wp_supershotgun:
                    ob = OB_MPSSHOTGUN;
                    break;
                default:
                    ob = OB_MPDEFAULT;
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
                ob = OB_SLIME;
                break;
            case MOD_Lava:
                ob = OB_LAVA;
                break;
            case MOD_Crush:
                ob = OB_CRUSH;
                break;
            default:
                break;
        }
    }

    UpdatePlayerNames();

    const char *killer = NULL;

    if (source && source->player)
    {
        killer = player_names[source->player - players];
    }

    char body[HU_MAXLINELENGTH];
    char text[HU_MAXLINELENGTH];

    // Keep room for the colour prefix and the reset sequence, so that a
    // long name cannot cut them off and leak the colour into the rest of
    // the line.

    ExpandObituary(DEH_String(ob), player_names[target->player - players],
                   killer, body, sizeof(body) - sizeof("\x1b" ORIG_S));

    M_snprintf(text, sizeof(text), "\x1b%c%s" ORIG_S,
               '0' + hudcolor_obituary, body);

    HU_Message(NULL, MESSAGES_OBITUARY, "%s", text);
}
