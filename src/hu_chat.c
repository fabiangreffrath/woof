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

#include "hu_chat.h"

#include "d_event.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "hu_message.h"
#include "m_input.h"
#include "m_misc.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "st_widgets.h"

boolean chat_on;

static const char shiftxform[] = SHIFTXFORM_ARRAY;

typedef struct
{
    char string[HU_MAXLINELENGTH];
    int pos;
} chatline_t;

// The active input line of the local player.

static chatline_t chatline;

// A player's chat stream is a destination byte (HU_BROADCAST, or the
// recipient's number + 1) followed by the message characters, so the
// destination must survive between characters.

#define HU_BROADCAST 5

static char chat_dest[MAXPLAYERS];

// The lines of the remote players being reassembled.

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

static void ProcessChatChar(int p, char ch)
{
    if (ch <= HU_BROADCAST)
    {
        chat_dest[p] = ch;
        return;
    }

    if (ch >= 'a' && ch <= 'z')
    {
        ch = (char)shiftxform[(unsigned char)ch];
    }

    if (AddKeyToChatLine(&lines[p], ch) && ch == KEY_ENTER)
    {
        if (lines[p].pos && (chat_dest[p] == consoleplayer + 1
                             || chat_dest[p] == HU_BROADCAST))
        {
            char text[HU_MAXLINELENGTH];

            M_snprintf(text, sizeof(text), "%s%s",
                       DEH_StringColorized(strings_players[p]),
                       lines[p].string);

            HU_PostMessage(text);

            S_StartSoundPitch(0,
                              gamemode == commercial ? sfx_radio
                                                     : sfx_tink,
                              PITCH_NONE);
        }
        ClearChatLine(&lines[p]);
    }
}

void HU_ChatCharEvent(int player, char ch)
{
    if (!ch || player < 0 || player >= MAXPLAYERS || player == consoleplayer)
    {
        return;
    }

    ProcessChatChar(player, ch);
}

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
// HU_DequeueChatChar()
//
// Remove the earliest added character from the circular chat queue
//
// Passed nothing, returns the character dequeued
//

char HU_DequeueChatChar(void)
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

boolean HU_MessagesResponder(event_t *ev)
{
    if (ev->type == ev_text)
    {
        return false;
    }

    boolean eatkey = false;
    static boolean shiftdown = false;
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

    if (M_InputActivated(input_chat_backspace))
    {
        ch = KEY_BACKSPACE;
    }

    if (!chat_on)
    {
        if (M_InputActivated(input_msgreview)) // phares
        {
            //jff 2/26/98 toggle list of messages
            ST_ReviewMessages();
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
                        HU_PostMessage(++num_nobrainers < 3 ? HUSTR_TALKTOSELF1
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
                HU_PostMessage(chatline.string);
            }
        }
        else if (ch == KEY_ESCAPE) // phares
        {
            chat_on = false;
        }
    }
    return eatkey;
}

void HU_UpdateChatWidget(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    static char string[HU_MAXLINELENGTH + 1];

    string[0] = '\0';

    if (chat_on)
    {
        M_StringCopy(string, chatline.string, sizeof(string));
        // make sure active chat line is at least one char wide
        M_StringConcat(string, (leveltime & 16) ? "_" : " ", sizeof(string));
        ST_AddLine(widget, string);
    }
}

void HU_ResetChat(void)
{
    chat_on = false;
    ClearChatLine(&chatline);

    for (int p = 0; p < MAXPLAYERS; ++p)
    {
        ClearChatLine(&lines[p]);
        chat_dest[p] = 0;
    }

    head = tail = 0;
}
