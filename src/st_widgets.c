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

#include "dstrings.h"
#include "d_event.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_stuff.h"
#include "m_input.h"
#include "m_misc.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "i_timer.h"
#include "v_video.h"

#define GRAY_S  "\x1b\x32"
#define GREEN_S "\x1b\x33"
#define BROWN_S "\x1b\x34"
#define GOLD_S  "\x1b\x35"
#define RED_S   "\x1b\x36"
#define BLUE_S  "\x1b\x37"

#define HU_MAXLINELENGTH 120

boolean chat_on;

static char message_string[HU_MAXLINELENGTH];

static boolean message_review;

void UpdateMessage(sbe_widget_t *widget, player_t *player)
{
    static char string[120];
    static int duration_left;
    static boolean overwritable = true;
    static boolean messages_enabled = true;

    if (messages_enabled)
    {
        if (message_string[0])
        {
            duration_left = widget->duration;
            M_StringCopy(string, message_string, sizeof(string));
            message_string[0] = '\0';
            overwritable = false;
        }
        else if (player->message && player->message[0] && overwritable)
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
        widget->string = "";
        overwritable = true;
    }
    else
    {
        widget->string = string;
        --duration_left;
    }
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

static boolean AddKeyToLine(chatline_t *line, char ch)
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

void UpdateChatMessage(void)
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

                if (AddKeyToLine(&lines[p], ch) && ch == KEY_ENTER)
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

static void QueueChatChar(char c)
{
    if (((head + 1) & (QUEUESIZE - 1)) == tail)
    {
        displaymsg("%s", HUSTR_MSGU);
    }
    else
    {
        chatchars[head++] = c;
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

boolean MessagesResponder(event_t *ev)
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
            eatkey = AddKeyToLine(&chatline, ch);
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

void UpdateChat(sbe_widget_t *widget)
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
