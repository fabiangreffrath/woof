//
// Copyright(C) 2022 Ryan Krafnick
// Copyright(C) 2024 ceski
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
//      Command History HUD Widget
//

#include <string.h>

#include "d_event.h"
#include "doomstat.h"
#include "hu_command.h"
#include "i_printf.h"
#include "m_misc.h"
#include "v_video.h"

boolean hud_command_history;
int hud_command_history_size;
boolean hud_hide_empty_commands;

typedef struct
{
    signed char forwardmove;
    signed char sidemove;
    short angleturn;
    boolean attack;
    boolean use;
    byte change;
} hud_cmd_t;

typedef struct hud_cmd_item_s
{
    struct hud_cmd_item_s *prev, *next;
    char buf[HU_MAXLINELENGTH];
    hud_cmd_t hud_cmd;
    int count;
} hud_cmd_item_t;

static hud_cmd_item_t hud_cmd_items[HU_MAXMESSAGES];
static hud_cmd_item_t *current = hud_cmd_items;
static boolean lowres_turn_format;

void HU_UpdateTurnFormat(void)
{
    const boolean playdemo = (gameaction == ga_playdemo || demoplayback);
    lowres_turn_format = (!playdemo && lowres_turn) || (playdemo && !longtics);
}

void HU_InitCommandHistory(void)
{
    for (int i = 1; i < HU_MAXMESSAGES; i++)
    {
        hud_cmd_items[i].prev = &hud_cmd_items[i - 1];
        hud_cmd_items[i - 1].next = &hud_cmd_items[i];
    }

    hud_cmd_items[0].prev = &hud_cmd_items[HU_MAXMESSAGES - 1];
    hud_cmd_items[HU_MAXMESSAGES - 1].next = &hud_cmd_items[0];
}

void HU_ResetCommandHistory(void)
{
    for (int i = 0; i < HU_MAXMESSAGES; i++)
    {
        memset(&hud_cmd_items[i].hud_cmd, 0, sizeof(hud_cmd_t));
        hud_cmd_items[i].count = 0;
        hud_cmd_items[i].buf[0] = '\0';
    }
}

static void TicToHudCmd(hud_cmd_t *hud_cmd, const ticcmd_t *cmd)
{
    hud_cmd->forwardmove = cmd->forwardmove;
    hud_cmd->sidemove = cmd->sidemove;

    if (lowres_turn_format)
    {
        hud_cmd->angleturn = (cmd->angleturn + 128) >> 8;
    }
    else
    {
        hud_cmd->angleturn = cmd->angleturn;
    }

    if (cmd->buttons && !(cmd->buttons & BT_SPECIAL))
    {
        hud_cmd->attack = (cmd->buttons & BT_ATTACK) > 0;
        hud_cmd->use = (cmd->buttons & BT_USE) > 0;

        if (cmd->buttons & BT_CHANGE)
        {
            hud_cmd->change =
                1 + ((cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT);
        }
        else
        {
            hud_cmd->change = 0;
        }
    }
    else
    {
        hud_cmd->attack = false;
        hud_cmd->use = false;
        hud_cmd->change = 0;
    }
}

static boolean IsEmpty(const hud_cmd_t *hud_cmd)
{
    return (hud_cmd->forwardmove == 0
            && hud_cmd->sidemove == 0
            && hud_cmd->angleturn == 0
            && hud_cmd->attack == false
            && hud_cmd->use == false
            && hud_cmd->change == 0);
}

static boolean IsEqual(const hud_cmd_t *hud_cmd1, const hud_cmd_t *hud_cmd2)
{
    return (hud_cmd1->forwardmove == hud_cmd2->forwardmove
            && hud_cmd1->sidemove == hud_cmd2->sidemove
            && hud_cmd1->angleturn == hud_cmd2->angleturn
            && hud_cmd1->attack == hud_cmd2->attack
            && hud_cmd1->use == hud_cmd2->use
            && hud_cmd1->change == hud_cmd2->change);
}

static void UpdateHudCmdText(hud_cmd_item_t *cmd_disp)
{
    const hud_cmd_t *hud_cmd = &cmd_disp->hud_cmd;
    char *buf = cmd_disp->buf;
    const int len = sizeof(cmd_disp->buf);
    int i = M_snprintf(buf, len, "\x1b%c", '0' + CR_GRAY);

    if (cmd_disp->count)
    {
        i += M_snprintf(buf + i, len - i, "x%-3d ", cmd_disp->count + 1);
    }
    else
    {
        i += M_snprintf(buf + i, len - i, "     ");
    }

    if (hud_cmd->forwardmove > 0)
    {
        i += M_snprintf(buf + i, len - i, "MF%2d ", hud_cmd->forwardmove);
    }
    else if (hud_cmd->forwardmove < 0)
    {
        i += M_snprintf(buf + i, len - i, "MB%2d ", -hud_cmd->forwardmove);
    }
    else
    {
        i += M_snprintf(buf + i, len - i, "     ");
    }

    if (hud_cmd->sidemove > 0)
    {
        i += M_snprintf(buf + i, len - i, "SR%2d ", hud_cmd->sidemove);
    }
    else if (hud_cmd->sidemove < 0)
    {
        i += M_snprintf(buf + i, len - i, "SL%2d ", -hud_cmd->sidemove);
    }
    else
    {
        i += M_snprintf(buf + i, len - i, "     ");
    }

    if (lowres_turn_format)
    {
        if (hud_cmd->angleturn > 0)
        {
            i += M_snprintf(buf + i, len - i, "TL%2d ", hud_cmd->angleturn);
        }
        else if (hud_cmd->angleturn < 0)
        {
            i += M_snprintf(buf + i, len - i, "TR%2d ", -hud_cmd->angleturn);
        }
        else
        {
            i += M_snprintf(buf + i, len - i, "     ");
        }
    }
    else
    {
        if (hud_cmd->angleturn > 0)
        {
            i += M_snprintf(buf + i, len - i, "TL%5d ", hud_cmd->angleturn);
        }
        else if (hud_cmd->angleturn < 0)
        {
            i += M_snprintf(buf + i, len - i, "TR%5d ", -hud_cmd->angleturn);
        }
        else
        {
            i += M_snprintf(buf + i, len - i, "        ");
        }
    }

    if (hud_cmd->attack)
    {
        i += M_snprintf(buf + i, len - i, "A ");
    }
    else
    {
        i += M_snprintf(buf + i, len - i, "  ");
    }

    if (hud_cmd->use)
    {
        i += M_snprintf(buf + i, len - i, "U ");
    }
    else
    {
        i += M_snprintf(buf + i, len - i, "  ");
    }

    if (hud_cmd->change)
    {
        M_snprintf(buf + i, len - i, "C%d", hud_cmd->change);
    }
    else
    {
        M_snprintf(buf + i, len - i, "  ");
    }
}

void HU_UpdateCommandHistory(const ticcmd_t *cmd)
{
    hud_cmd_t hud_cmd;

    if (!STRICTMODE(hud_command_history))
    {
        return;
    }

    TicToHudCmd(&hud_cmd, cmd);

    if (hud_hide_empty_commands && IsEmpty(&hud_cmd))
    {
        return;
    }

    if (IsEqual(&hud_cmd, &current->hud_cmd))
    {
        current->count++;
    }
    else
    {
        current->count = 0;
        current = current->next;
        current->hud_cmd = hud_cmd;
    }

    UpdateHudCmdText(current);
}

void HU_BuildCommandHistory(hu_multiline_t *const multiline)
{
    hud_cmd_item_t *hud_cmd = current;

    for (int i = 0; i < hud_command_history_size; i++)
    {
        HUlib_add_string_keep_space(multiline, hud_cmd->buf);
        hud_cmd = hud_cmd->prev;
    }
}
