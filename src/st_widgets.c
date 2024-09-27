//
// Copyright(C) 2024 Roman Fomin
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

#include "d_player.h"
#include "doomstat.h"
#include "m_misc.h"
#include "st_sbardef.h"
#include "i_timer.h"
#include "v_video.h"

#define GRAY_S  "\x1b\x32"
#define GREEN_S "\x1b\x33"
#define BROWN_S "\x1b\x34"
#define GOLD_S  "\x1b\x35"
#define RED_S   "\x1b\x36"
#define BLUE_S  "\x1b\x37"

void UpdateMessage(sbe_widget_t *widget, player_t *player)
{
    if (!player->message)
    {
        widget->string = "";
        return;
    }

    static char string[120];
    static int duration_left;

    if (player->message[0])
    {
        duration_left = widget->duration;
        M_StringCopy(string, player->message, sizeof(string));
        player->message[0] = '\0';
    }

    if (duration_left == 0)
    {
        string[0] = '\0';
    }
    else
    {
        --duration_left;
    }

    widget->string = string;
}

void UpdateSecretMessage(sbe_widget_t *widget, player_t *player)
{
    static char string[80];
    static int duration_left;

    if (player->secretmessage)
    {
        duration_left = widget->duration;
        M_StringCopy(string, player->secretmessage, sizeof(string));
        player->secretmessage = NULL;
    }

    if (duration_left == 0)
    {
        string[0] = '\0';
    }
    else
    {
        --duration_left;
    }

    widget->string = string;
}

void UpdateMonSec(sbe_widget_t *widget)
{
    static char string[120];

    string[0] = '\0';

    int fullkillcount = 0;
    int fullitemcount = 0;
    int fullsecretcount = 0;
    int kill_percent_count = 0;

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        if (playeringame[i])
        {
            fullkillcount += players[i].killcount - players[i].maxkilldiscount;
            fullitemcount += players[i].itemcount;
            fullsecretcount += players[i].secretcount;
            kill_percent_count += players[i].killcount;
        }
    }

    if (respawnmonsters)
    {
        fullkillcount = kill_percent_count;
        max_kill_requirement = totalkills;
    }

    int killcolor = (fullkillcount >= max_kill_requirement) ? '0' + CR_BLUE1
                                                            : '0' + CR_GRAY;
    int secretcolor =
        (fullsecretcount >= totalsecret) ? '0' + CR_BLUE1 : '0' + CR_GRAY;
    int itemcolor =
        (fullitemcount >= totalitems) ? '0' + CR_BLUE1 : '0' + CR_GRAY;

    M_snprintf(string, sizeof(string),
        RED_S "K \x1b%c%d/%d " RED_S "I \x1b%c%d/%d " RED_S "S \x1b%c%d/%d",
        killcolor, fullkillcount, max_kill_requirement,
        itemcolor, fullitemcount, totalitems,
        secretcolor, fullsecretcount, totalsecret);

    widget->string = string;
}

void UpdateStTime(sbe_widget_t *widget, player_t *player)
{
    static char string[80];

    string[0] = '\0';

    int offset = 0;

    if (time_scale != 100)
    {
        offset +=
            M_snprintf(string, sizeof(string), BLUE_S "%d%% ", time_scale);
    }

    if (totalleveltimes)
    {
        const int time = (totalleveltimes + leveltime) / TICRATE;

        offset += M_snprintf(string + offset, sizeof(string) - offset,
                             GREEN_S "%d:%02d ", time / 60, time % 60);
    }

    if (!player->btuse_tics)
    {
        M_snprintf(string + offset, sizeof(string) - offset,
                   GRAY_S "%d:%05.2f\t", leveltime / TICRATE / 60,
                   (float)(leveltime % (60 * TICRATE)) / TICRATE);
    }
    else
    {
        M_snprintf(string + offset, sizeof(string) - offset,
                   GOLD_S "U %d:%05.2f\t", player->btuse / TICRATE / 60,
                   (float)(player->btuse % (60 * TICRATE)) / TICRATE);
    }

    widget->string = string;
}
