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

#include "st_widgets.h"

#include <math.h>
#include <string.h>

#include "dstrings.h"
#include "d_event.h"
#include "d_deh.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_command.h"
#include "hu_coordinates.h"
#include "hu_obituary.h"
#include "i_video.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_misc.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_main.h"
#include "r_voxel.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "i_timer.h"
#include "v_video.h"
#include "u_mapinfo.h"

boolean       show_messages;
boolean       show_toggle_messages;
boolean       show_pickup_messages;

secretmessage_t hud_secret_message; // "A secret is revealed!" message
widgetstate_t hud_level_stats;
widgetstate_t hud_level_time;
boolean       hud_time_use;
widgetstate_t hud_player_coords;
widgetstate_t hud_widget_font;

static boolean hud_map_announce;
static boolean message_colorized;

//jff 2/16/98 hud supported automap colors added
int hudcolor_titl;  // color range of automap level title
int hudcolor_xyco;  // color range of new coords on automap

boolean chat_on;

void ST_ClearLines(sbe_widget_t *widget)
{
    array_clear(widget->lines);
}

void ST_AddLine(sbe_widget_t *widget, const char *string)
{
    widgetline_t line = { .string = string };
    array_push(widget->lines, line);
}

static void SetLine(sbe_widget_t *widget, const char *string)
{
    array_clear(widget->lines);
    widgetline_t line = { .string = string };
    array_push(widget->lines, line);
}

static char message_string[HU_MAXLINELENGTH];

static boolean message_review;

static void UpdateMessage(sbe_widget_t *widget, player_t *player)
{
    if (!player->message)
    {
        ST_ClearLines(widget);
        return;
    }

    static char string[120];
    static int duration_left;
    static boolean overwrite = true;
    static boolean messages_enabled = true;

    if (messages_enabled)
    {
        if (message_string[0])
        {
            duration_left = widget->duration;
            M_StringCopy(string, message_string, sizeof(string));
            message_string[0] = '\0';
            overwrite = false;
        }
        else if (player->message && player->message[0] && overwrite)
        {
            duration_left = widget->duration;
            M_StringCopy(string, player->message, sizeof(string));
            player->message[0] = '\0';
        }
        else if (message_review)
        {
            message_review = false;
            duration_left = widget->duration;
        }
    }

    if (messages_enabled != show_messages)
    {
        messages_enabled = show_messages;
    }

    if (duration_left == 0)
    {
        ST_ClearLines(widget);
        overwrite = true;
    }
    else
    {
        SetLine(widget, string);
        --duration_left;
    }
}

static void UpdateSecretMessage(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!hud_secret_message)
    {
        return;
    }

    static char string[80];
    static int duration_left;

    if (player->secretmessage)
    {
        duration_left = widget->duration;
        M_StringCopy(string, player->secretmessage, sizeof(string));
        player->secretmessage = NULL;
    }

    if (duration_left > 0)
    {
        ST_AddLine(widget, string);
        --duration_left;
    }
}

// key tables
// jff 5/10/98 french support removed, 
// as it was not being used and couldn't be easily tested
//

static const char shiftxform[] =
{
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '<', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    ')', // shift-0
    '!', // shift-1
    '@', // shift-2
    '#', // shift-3
    '$', // shift-4
    '%', // shift-5
    '^', // shift-6
    '&', // shift-7
    '*', // shift-8
    '(', // shift-9
    ':',
    ':', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

typedef struct
{
    char string[HU_MAXLINELENGTH];
    int pos;
} chatline_t;

static chatline_t lines[MAXPLAYERS];

static void ClearChatLine(chatline_t *line)
{
    line->pos = 0;
    line->string[0] = '\0';
}

static boolean AddKeyToChatLine(chatline_t *line, char ch)
{
    if (ch >= ' ' && ch <= '_')
    {
        if (line->pos == HU_MAXLINELENGTH - 1)
        {
            return false;
        }
        line->string[line->pos++] = ch;
        line->string[line->pos] = '\0';
    }
    else if (ch == KEY_BACKSPACE) // phares
    {
        if (line->pos == 0)
        {
            return false;
        }
        else
        {
            line->string[--line->pos] = '\0';
        }
    }
    else if (ch != KEY_ENTER) // phares
    {
        return false; // did not eat key
    }

    return true; // ate the key
}

#define HU_BROADCAST 5

char **player_names[] =
{
    &s_HUSTR_PLRGREEN,
    &s_HUSTR_PLRINDIGO,
    &s_HUSTR_PLRBROWN,
    &s_HUSTR_PLRRED
};

void ST_UpdateChatMessage(void)
{
    static char chat_dest[MAXPLAYERS];

    for (int p = 0; p < MAXPLAYERS; p++)
    {
        if (!playeringame[p])
        {
            continue;
        }

        char ch = players[p].cmd.chatchar;
        if (p != consoleplayer && ch)
        {
            if (ch <= HU_BROADCAST)
            {
                chat_dest[p] = ch;
            }
            else
            {
                if (ch >= 'a' && ch <= 'z')
                {
                    ch = (char)shiftxform[(unsigned char)ch];
                }

                if (AddKeyToChatLine(&lines[p], ch) && ch == KEY_ENTER)
                {
                    if (lines[p].pos && (chat_dest[p] == consoleplayer + 1
                                         || chat_dest[p] == HU_BROADCAST))
                    {
                        M_snprintf(message_string, sizeof(message_string),
                            "%s%s", *player_names[p], lines[p].string);

                        S_StartSoundPitch(0,
                                          gamemode == commercial ? sfx_radio
                                                                 : sfx_tink,
                                          PITCH_NONE);
                    }
                    ClearChatLine(&lines[p]);
                }
            }
            players[p].cmd.chatchar = 0;
        }
    }
}

static const char *chat_macros[] = {
    HUSTR_CHATMACRO0, HUSTR_CHATMACRO1, HUSTR_CHATMACRO2, HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4, HUSTR_CHATMACRO5, HUSTR_CHATMACRO6, HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8, HUSTR_CHATMACRO9
};

#define QUEUESIZE   128

static char chatchars[QUEUESIZE];
static int  head = 0;
static int  tail = 0;

//
// QueueChatChar()
//
// Add an incoming character to the circular chat queue
//
// Passed the character to queue, returns nothing
//

static void QueueChatChar(char ch)
{
    if (((head + 1) & (QUEUESIZE - 1)) == tail)
    {
        displaymsg("%s", HUSTR_MSGU);
    }
    else
    {
        chatchars[head++] = ch;
        head &= QUEUESIZE - 1;
    }
}

//
// ST_DequeueChatChar()
//
// Remove the earliest added character from the circular chat queue
//
// Passed nothing, returns the character dequeued
//

char ST_DequeueChatChar(void)
{
    char ch;

    if (head != tail)
    {
        ch = chatchars[tail++];
        tail &= QUEUESIZE - 1;
    }
    else
    {
        ch = 0;
    }

    return ch;
}

static chatline_t chatline;

boolean ST_MessagesResponder(event_t *ev)
{
    static char lastmessage[HU_MAXLINELENGTH + 1];

    boolean eatkey = false;
    static boolean shiftdown = false;
    static boolean altdown = false;
    int ch;
    int numplayers;

    static int num_nobrainers = 0;

    ch = (ev->type == ev_keydown) ? ev->data1.i : 0;

    numplayers = 0;
    for (int p = 0; p < MAXPLAYERS; p++)
    {
        numplayers += playeringame[p];
    }

    if (ev->data1.i == KEY_RSHIFT)
    {
        shiftdown = ev->type == ev_keydown;
        return false;
    }

    if (ev->data1.i == KEY_RALT)
    {
        altdown = ev->type == ev_keydown;
        return false;
    }

    if (M_InputActivated(input_chat_backspace))
    {
        ch = KEY_BACKSPACE;
    }

    if (!chat_on)
    {
        if (M_InputActivated(input_chat_enter)) // phares
        {
            //jff 2/26/98 toggle list of messages
            message_review = true;
            eatkey = true;
        }
        else if (demoplayback) // killough 10/02/98: no chat if demo playback
        {
            eatkey = false;
        }
        else if (netgame && M_InputActivated(input_chat))
        {
            eatkey = chat_on = true;
            ClearChatLine(&chatline);
            QueueChatChar(HU_BROADCAST);
        }
        else if (netgame && numplayers > 2) // killough 11/98: simplify
        {
            for (int p = 0; p < MAXPLAYERS; p++)
            {
                if (M_InputActivated(input_chat_dest0 + p))
                {
                    if (p == consoleplayer)
                    {
                        displaymsg("%s",
                                   ++num_nobrainers < 3 ? HUSTR_TALKTOSELF1
                                   : num_nobrainers < 6 ? HUSTR_TALKTOSELF2
                                   : num_nobrainers < 9 ? HUSTR_TALKTOSELF3
                                   : num_nobrainers < 32
                                       ? HUSTR_TALKTOSELF4
                                       : HUSTR_TALKTOSELF5);
                    }
                    else if (playeringame[p])
                    {
                        eatkey = chat_on = true;
                        ClearChatLine(&chatline);
                        QueueChatChar((char)(p + 1));
                        break;
                    }
                }
            }
        }
    } // jff 2/26/98 no chat functions if message review is displayed
    else
    {
        if (M_InputActivated(input_chat_enter))
        {
            ch = KEY_ENTER;
        }

        // send a macro
        if (altdown)
        {
            ch = ch - '0';
            if (ch < 0 || ch > 9)
            {
                return false;
            }
            const char *macromessage = chat_macros[ch];

            // kill last message with a '\n'
            QueueChatChar(KEY_ENTER); // DEBUG!!!                // phares

            // send the macro message
            while (*macromessage)
            {
                QueueChatChar(*macromessage++);
            }
            QueueChatChar(KEY_ENTER); // phares

            // leave chat mode and notify that it was sent
            chat_on = false;
            M_StringCopy(lastmessage, chat_macros[ch], sizeof(lastmessage));
            displaymsg("%s", lastmessage);
            eatkey = true;
        }
        else
        {
            if (shiftdown || (ch >= 'a' && ch <= 'z'))
            {
                ch = shiftxform[ch];
            }
            eatkey = AddKeyToChatLine(&chatline, ch);
            if (eatkey)
            {
                QueueChatChar(ch);
            }

            if (ch == KEY_ENTER) // phares
            {
                chat_on = false;
                if (chatline.pos)
                {
                    M_StringCopy(lastmessage, chatline.string,
                                 sizeof(lastmessage));
                    displaymsg("%s", lastmessage);
                }
            }
            else if (ch == KEY_ESCAPE) // phares
            {
                chat_on = false;
            }
        }
    }
    return eatkey;
}

static void UpdateChat(sbe_widget_t *widget)
{
    static char string[HU_MAXLINELENGTH + 1];

    string[0] = '\0';

    if (chat_on)
    {
        M_StringCopy(string, chatline.string, sizeof(string));

        if (leveltime & 16)
        {
            M_StringConcat(string, "_", sizeof(string));
        }
    }

    SetLine(widget, string);
}

static boolean IsVanillaMap(int e, int m)
{
    if (gamemode == commercial)
    {
        return (e == 1 && m > 0 && m <= 32);
    }
    else
    {
        return (e > 0 && e <= 4 && m > 0 && m <= 9);
    }
}

#define HU_TITLE  (*mapnames[(gameepisode - 1) * 9 + gamemap - 1])
#define HU_TITLE2 (*mapnames2[gamemap - 1])
#define HU_TITLEP (*mapnamesp[gamemap - 1])
#define HU_TITLET (*mapnamest[gamemap - 1])

static char title_string[HU_MAXLINELENGTH];

void ST_ResetTitle(void)
{
    char string[120];
    string[0] = '\0';

    char *s;

    if (gamemapinfo && gamemapinfo->levelname)
    {
        if (gamemapinfo->label)
        {
            s = gamemapinfo->label;
        }
        else
        {
            s = gamemapinfo->mapname;
        }

        if (s == gamemapinfo->mapname || U_CheckField(s))
        {
            M_snprintf(string, sizeof(string), "%s: ", s);
        }
        s = gamemapinfo->levelname;
    }
    else if (gamestate == GS_LEVEL)
    {
        if (IsVanillaMap(gameepisode, gamemap))
        {
            s = (gamemode != commercial)     ? HU_TITLE
                : (gamemission == pack_tnt)  ? HU_TITLET
                : (gamemission == pack_plut) ? HU_TITLEP
                                             : HU_TITLE2;
        }
        // WADs like pl2.wad have a MAP33, and rely on the layout in the
        // Vanilla executable, where it is possible to overflow the end of one
        // array into the next.
        else if (gamemode == commercial && gamemap >= 33 && gamemap <= 35)
        {
            s = (gamemission == doom2)       ? (*mapnamesp[gamemap - 33])
                : (gamemission == pack_plut) ? (*mapnamest[gamemap - 33])
                                             : "";
        }
        else
        {
            // initialize the map title widget with the generic map lump name
            s = MapName(gameepisode, gamemap);
        }
    }
    else
    {
        s = "";
    }

    char *n;

    // [FG] cap at line break
    if ((n = strchr(s, '\n')))
    {
        *n = '\0';
    }

    M_StringConcat(string, s, sizeof(string));

    title_string[0] = '\0';
    M_snprintf(title_string, sizeof(title_string), "\x1b%c%s" ORIG_S,
               '0' + hudcolor_titl, string);

    if (hud_map_announce && leveltime == 0)
    {
        if (gamemapinfo && U_CheckField(gamemapinfo->author))
            displaymsg("%s by %s", string, gamemapinfo->author);
        else
            displaymsg("%s", string);
    }
}

static void UpdateTitle(sbe_widget_t *widget)
{
    SetLine(widget, title_string);
}

static boolean WidgetEnabled(widgetstate_t state)
{
    if (automapactive && !(state & HUD_WIDGET_AUTOMAP))
    {
        return false;
    }
    else if (!automapactive && !(state & HUD_WIDGET_HUD))
    {
        return false;
    }
    return true;
}

static void ForceDoomFont(sbe_widget_t *widget)
{
    if (WidgetEnabled(hud_widget_font))
    {
        widget->font = stcfnt;
    }
    else
    {
        widget->font = widget->default_font;
    }
}

static void UpdateCoord(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (strictmode)
    {
        return;
    }

    if (hud_player_coords == HUD_WIDGET_ADVANCED)
    {
        HU_BuildCoordinatesEx(widget, player->mo);
        return;
    }

    if (!WidgetEnabled(hud_player_coords))
    {
        return;
    }

    ForceDoomFont(widget);

    fixed_t x, y, z; // killough 10/98:
    void AM_Coordinates(const mobj_t *, fixed_t *, fixed_t *, fixed_t *);

    // killough 10/98: allow coordinates to display non-following pointer
    AM_Coordinates(player->mo, &x, &y, &z);

    static char string[80];

    // jff 2/16/98 output new coord display
    M_snprintf(string, sizeof(string),
               "\x1b%cX " GRAY_S "%d \x1b%cY " GRAY_S "%d \x1b%cZ " GRAY_S "%d",
               '0' + hudcolor_xyco, x >> FRACBITS, '0' + hudcolor_xyco,
               y >> FRACBITS, '0' + hudcolor_xyco, z >> FRACBITS);

    ST_AddLine(widget, string);
}

typedef enum
{
    STATSFORMAT_RATIO,
    STATSFORMAT_BOOLEAN,
    STATSFORMAT_PERCENT,
    STATSFORMAT_REMAINING,
    STATSFORMAT_COUNT
} statsformat_t;

static statsformat_t hud_stats_format;

static void StatsFormatFunc_Ratio(char *buffer, size_t size, const int count,
                                  const int total)
{
    M_snprintf(buffer, size, "%d/%d", count, total);
}

static void StatsFormatFunc_Boolean(char *buffer, size_t size, const int count,
                                    const int total)
{
    M_snprintf(buffer, size, "%s", (count >= total) ? "YES" : "NO");
}

static void StatsFormatFunc_Percent(char *buffer, size_t size, const int count,
                                    const int total)
{
    M_snprintf(buffer, size, "%d%%", !total ? 100 : count * 100 / total);
}

static void StatsFormatFunc_Remaining(char *buffer, size_t size,
                                      const int count, const int total)
{
    M_snprintf(buffer, size, "%d", total - count);
}

static void StatsFormatFunc_Count(char *buffer, size_t size, const int count,
                                  const int total)
{
    M_snprintf(buffer, size, "%d", count);
}

typedef void (*statsformatfunc_t)(char *buffer, size_t size, const int count,
                                  const int total);

static const statsformatfunc_t StatsFormatFuncs[] = {
    StatsFormatFunc_Ratio,     StatsFormatFunc_Boolean, StatsFormatFunc_Percent,
    StatsFormatFunc_Remaining, StatsFormatFunc_Count,
};

static void UpdateMonSec(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_stats))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[120];

    const int cr_blue = (widget->font == stcfnt) ? CR_BLUE2 : CR_BLUE1;

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

    int killcolor = (fullkillcount >= max_kill_requirement) ? '0' + cr_blue
                                                            : '0' + CR_GRAY;
    int secretcolor =
        (fullsecretcount >= totalsecret) ? '0' + cr_blue : '0' + CR_GRAY;
    int itemcolor =
        (fullitemcount >= totalitems) ? '0' + cr_blue : '0' + CR_GRAY;

    char kill_str[16], item_str[16], secret_str[16];

    statsformatfunc_t StatsFormatFunc = StatsFormatFuncs[hud_stats_format];

    StatsFormatFunc(kill_str, sizeof(kill_str), fullkillcount, max_kill_requirement);
    StatsFormatFunc(item_str, sizeof(item_str), fullitemcount, totalitems);
    StatsFormatFunc(secret_str, sizeof(secret_str), fullsecretcount, totalsecret);

    M_snprintf(string, sizeof(string),
        RED_S "K \x1b%c%s " RED_S "I \x1b%c%s " RED_S "S \x1b%c%s",
        killcolor, kill_str,
        itemcolor, item_str,
        secretcolor, secret_str);

    ST_AddLine(widget, string);
}

static void UpdateDM(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_stats))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[120];

    const int cr_blue = (widget->font == stcfnt) ? CR_BLUE2 : CR_BLUE1;

    int offset = 0;

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        int result = 0, others = 0;

        if (!playeringame[i])
        {
            continue;
        }

        for (int p = 0; p < MAXPLAYERS; ++p)
        {
            if (!playeringame[p])
            {
                continue;
            }

            if (i != p)
            {
                result += players[i].frags[p];
                others -= players[p].frags[i];
            }
            else
            {
                result -= players[i].frags[p];
            }
        }

        offset += M_snprintf(string + offset, sizeof(string) - offset,
                             "\x1b%c%d/%d ", (i == displayplayer) ?
                             '0' + cr_blue : '0' + CR_GRAY, result, others);
    }

    ST_AddLine(widget, string);
}

static void UpdateStTime(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_time))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[80];

    int offset = 0;

    if (time_scale != 100)
    {
        offset +=
            M_snprintf(string, sizeof(string), "%s%d%% ",
                       (widget->font == stcfnt) ? BLUE2_S : BLUE1_S, time_scale);
    }

    if (levelTimer == true)
    {
        const int time = levelTimeCount / TICRATE;

        offset += M_snprintf(string + offset, sizeof(string) - offset,
                             BROWN_S "%d:%02d ", time / 60, time % 60);
    }
    else if (totalleveltimes)
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
        player->btuse_tics--;
    }

    ST_AddLine(widget, string);
}

static void UpdateFPS(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!(player->cheats & CF_SHOWFPS))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[20];
    M_snprintf(string, sizeof(string), GRAY_S "%d " GREEN_S "FPS", fps);
    ST_AddLine(widget, string);
}

static void UpdateRate(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!(player->cheats & CF_RENDERSTATS))
    {
        return;
    }

    static char line1[80];
    M_snprintf(line1, sizeof(line1),
               GRAY_S "Sprites %4d Segs %4d Visplanes %4d   " GREEN_S
                      "FPS %3d %dx%d",
               rendered_vissprites, rendered_segs, rendered_visplanes,
               fps, video.width, video.height);
    ST_AddLine(widget, line1);

    if (voxels_rendering)
    {
        static char line2[60];
        M_snprintf(line2, sizeof(line2), GRAY_S " Voxels %4d",
                   rendered_voxels);
        ST_AddLine(widget, line2);
    }
}

int speedometer;

static void UpdateSpeed(sbe_widget_t *widget, player_t *player)
{
    if (speedometer <= 0)
    {
        SetLine(widget, "");
        return;
    }

    ForceDoomFont(widget);

    static const double factor[] = {TICRATE, 2.4003, 525.0 / 352.0};
    static const char *units[] = {"ups", "km/h", "mph"};
    const int type = speedometer - 1;
    const mobj_t *mo = player->mo;
    const double dx = FIXED2DOUBLE(mo->x - mo->oldx);
    const double dy = FIXED2DOUBLE(mo->y - mo->oldy);
    const double dz = FIXED2DOUBLE(mo->z - mo->oldz);
    const double speed = sqrt(dx * dx + dy * dy + dz * dz) * factor[type];

    static char string[60];
    M_snprintf(string, sizeof(string), GRAY_S "%.*f " GREEN_S "%s",
               type && speed ? 1 : 0, speed, units[type]);
    SetLine(widget, string);
}

static void UpdateCmd(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!STRICTMODE(hud_command_history))
    {
        return;
    }

    HU_BuildCommandHistory(widget);
}

// [crispy] print a bar indicating demo progress at the bottom of the screen
boolean ST_DemoProgressBar(boolean force)
{
    const int progress = video.unscaledw * playback_tic / playback_totaltics;
    static int old_progress = 0;

    if (old_progress < progress)
    {
        old_progress = progress;
    }
    else if (!force)
    {
        return false;
    }

    V_FillRect(0, SCREENHEIGHT - 2, progress, 1, v_darkest_color);
    V_FillRect(0, SCREENHEIGHT - 1, progress, 1, v_lightest_color);

    return true;
}

struct
{
    char **str;
    const int cr;
    const char *col;
} static const colorize_strings[] = {
    // [Woof!] colorize keycard and skull key messages
    {&s_GOTBLUECARD,     CR_BLUE2, " blue "  },
    {&s_GOTBLUESKUL,     CR_BLUE2, " blue "  },
    {&s_GOTREDCARD,      CR_RED,   " red "   },
    {&s_GOTREDSKULL,     CR_RED,   " red "   },
    {&s_GOTYELWCARD,     CR_GOLD,  " yellow "},
    {&s_GOTYELWSKUL,     CR_GOLD,  " yellow "},
    {&s_PD_BLUEC,        CR_BLUE2, " blue "  },
    {&s_PD_BLUEK,        CR_BLUE2, " blue "  },
    {&s_PD_BLUEO,        CR_BLUE2, " blue "  },
    {&s_PD_BLUES,        CR_BLUE2, " blue "  },
    {&s_PD_REDC,         CR_RED,   " red "   },
    {&s_PD_REDK,         CR_RED,   " red "   },
    {&s_PD_REDO,         CR_RED,   " red "   },
    {&s_PD_REDS,         CR_RED,   " red "   },
    {&s_PD_YELLOWC,      CR_GOLD,  " yellow "},
    {&s_PD_YELLOWK,      CR_GOLD,  " yellow "},
    {&s_PD_YELLOWO,      CR_GOLD,  " yellow "},
    {&s_PD_YELLOWS,      CR_GOLD,  " yellow "},

    // [Woof!] colorize multi-player messages
    {&s_HUSTR_PLRGREEN,  CR_GREEN, "Green: " },
    {&s_HUSTR_PLRINDIGO, CR_GRAY,  "Indigo: "},
    {&s_HUSTR_PLRBROWN,  CR_BROWN, "Brown: " },
    {&s_HUSTR_PLRRED,    CR_RED,   "Red: "   },
};

static char* PrepareColor(const char *str, const char *col)
{
    char *str_replace, col_replace[16];

    M_snprintf(col_replace, sizeof(col_replace),
               ORIG_S "%s" ORIG_S, col);
    str_replace = M_StringReplace(str, col, col_replace);

    return str_replace;
}

static void UpdateColor(char *str, int cr)
{
    int i;
    int len = strlen(str);

    if (!message_colorized)
    {
        cr = CR_ORIG;
    }

    for (i = 0; i < len; ++i)
    {
        if (str[i] == '\x1b' && i + 1 < len)
        {
          str[i + 1] = '0'+cr;
          break;
        }
    }
}

void ST_InitWidgets(void)
{
    // [Woof!] prepare player messages for colorization
    for (int i = 0; i < arrlen(colorize_strings); i++)
    {
        *colorize_strings[i].str =
            PrepareColor(*colorize_strings[i].str, colorize_strings[i].col);
    }

    ST_ResetMessageColors();
}

void ST_ResetMessageColors(void)
{
    int i;

    for (i = 0; i < arrlen(colorize_strings); i++)
    {
        UpdateColor(*colorize_strings[i].str, colorize_strings[i].cr);
    }
}

sbarelem_t *st_time_elem = NULL, *st_cmd_elem = NULL;

boolean message_centered;
sbarelem_t *st_msg_elem = NULL;

static void ForceCenterMessage(sbarelem_t *elem)
{
    static sbaralignment_t default_alignment;
    if (!st_msg_elem)
    {
        default_alignment = elem->alignment;
        st_msg_elem = elem;
    }

    elem->alignment = message_centered ? sbe_h_middle : default_alignment;
}

void ST_UpdateWidget(sbarelem_t *elem, player_t *player)
{
    sbe_widget_t *widget = elem->subtype.widget;

    switch (widget->type)
    {
        case sbw_message:
            ForceCenterMessage(elem);
            UpdateMessage(widget, player);
            break;
        case sbw_chat:
            UpdateChat(widget);
            break;
        case sbw_secret:
            UpdateSecretMessage(widget, player);
            break;
        case sbw_title:
            UpdateTitle(widget);
            break;

        case sbw_monsec:
            if (deathmatch)
                UpdateDM(widget);
            else
                UpdateMonSec(widget);
            break;
        case sbw_time:
            st_time_elem = elem;
            UpdateStTime(widget, player);
            break;
        case sbw_coord:
            UpdateCoord(widget, player);
            break;
        case sbw_fps:
            UpdateFPS(widget, player);
            break;
        case sbw_rate:
            UpdateRate(widget, player);
            break;
        case sbw_cmd:
            st_cmd_elem = elem;
            UpdateCmd(widget);
            break;
        case sbw_speed:
            UpdateSpeed(widget, player);
            break;
        default:
            break;
    }
}

void ST_BindHUDVariables(void)
{
  M_BindNum("hud_level_stats", &hud_level_stats, NULL,
            HUD_WIDGET_OFF, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
            ss_stat, wad_no,
            "Show level stats (kills, items, and secrets) widget (1 = On automap; "
            "2 = On HUD; 3 = Always)");
  M_BindNum("hud_stats_format", &hud_stats_format, NULL,
            STATSFORMAT_RATIO, STATSFORMAT_RATIO, STATSFORMAT_COUNT,
            ss_stat, wad_no,
            "Format of level stats (0 = Ratio; 1 = Boolean; 2 = Percent; 3 = Remaining; 4 = Count)");
  M_BindNum("hud_level_time", &hud_level_time, NULL,
            HUD_WIDGET_OFF, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
            ss_stat, wad_no,
            "Show level time widget (1 = On automap; 2 = On HUD; 3 = Always)");
  M_BindNum("hud_player_coords", &hud_player_coords, NULL,
            HUD_WIDGET_AUTOMAP, HUD_WIDGET_OFF, HUD_WIDGET_ADVANCED,
            ss_stat, wad_no,
            "Show player coordinates widget (1 = On automap; 2 = On HUD; 3 = Always; 4 = Advanced)");
  M_BindBool("hud_command_history", &hud_command_history, NULL, false, ss_stat,
             wad_no, "Show command history widget");
  BIND_NUM(hud_command_history_size, 10, 1, HU_MAXMESSAGES,
           "Number of commands to display for command history widget");
  BIND_BOOL(hud_hide_empty_commands, true,
            "Hide empty commands from command history widget");
  M_BindBool("hud_time_use", &hud_time_use, NULL, false, ss_stat, wad_no,
             "Show split time when pressing the use-button");
  M_BindNum("hud_widget_font", &hud_widget_font, NULL,
            HUD_WIDGET_OFF, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
            ss_stat, wad_no,
            "Use standard Doom font for widgets (1 = On automap; 2 = On HUD; 3 "
            "= Always)");

  M_BindNum("hudcolor_titl", &hudcolor_titl, NULL,
            CR_GOLD, CR_BRICK, CR_NONE, ss_none, wad_yes,
            "Color range used for automap level title");
  M_BindNum("hudcolor_xyco", &hudcolor_xyco, NULL,
            CR_GREEN, CR_BRICK, CR_NONE, ss_none, wad_yes,
            "Color range used for automap coordinates");

  BIND_BOOL(show_messages, true, "Show messages");
  M_BindNum("hud_secret_message", &hud_secret_message, NULL,
            SECRETMESSAGE_ON, SECRETMESSAGE_OFF, SECRETMESSAGE_COUNT,
            ss_stat, wad_no,
            "Announce revealed secrets (0 = Off; 1 = On; 2 = Count)");
  M_BindBool("hud_map_announce", &hud_map_announce, NULL,
            false, ss_stat, wad_no, "Announce map titles");
  M_BindBool("show_toggle_messages", &show_toggle_messages, NULL,
            true, ss_stat, wad_no, "Show toggle messages");
  M_BindBool("show_pickup_messages", &show_pickup_messages, NULL,
             true, ss_stat, wad_no, "Show pickup messages");
  M_BindBool("show_obituary_messages", &show_obituary_messages, NULL,
             true, ss_stat, wad_no, "Show obituaries");
  BIND_NUM(hudcolor_obituary, CR_GRAY, CR_BRICK, CR_NONE,
           "Color range used for obituaries");
  M_BindBool("message_centered", &message_centered, NULL,
             false, ss_stat, wad_no, "Center messages horizontally");
  M_BindBool("message_colorized", &message_colorized, NULL,
             false, ss_stat, wad_no, "Colorize player messages");

#define BIND_CHAT(num)                                                     \
    M_BindStr("chatmacro" #num, &chat_macros[(num)], HUSTR_CHATMACRO##num, \
              wad_yes, "Chat string associated with " #num " key")

  BIND_CHAT(0);
  BIND_CHAT(1);
  BIND_CHAT(2);
  BIND_CHAT(3);
  BIND_CHAT(4);
  BIND_CHAT(5);
  BIND_CHAT(6);
  BIND_CHAT(7);
  BIND_CHAT(8);
  BIND_CHAT(9);
}
