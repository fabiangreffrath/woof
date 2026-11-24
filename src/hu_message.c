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

#include "hu_message.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "d_player.h"
#include "d_rollback.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "hu_chat.h"
#include "hu_obituary.h"
#include "m_misc.h"
#include "st_widgets.h"

// The events of a tic are kept until that tic is safe to show. A rollback
// resimulates tics that have already produced events, so a slot holds the
// result of the last simulation of its tic, never a log of every attempt.

#define HU_EVENT_TICS 16

typedef struct
{
    int tic;
    boolean has_message;
    boolean has_announce;
    char message[HU_MAXLINELENGTH];
    char announce[HU_MAXLINELENGTH];
    char chat[MAXPLAYERS];
} hu_event_t;

static hu_event_t events[HU_EVENT_TICS];

// The next tic whose events have not been shown yet, or -1 before the
// first dispatch.

static int present_tic = -1;

static char pending_message[HU_MAXLINELENGTH];
static boolean message_pending;

// Feedback of the local player (chat echo, message settings) has its own
// slot, so that a pickup happening in the same frame cannot replace it.

static char posted_message[HU_MAXLINELENGTH];
static boolean posted_pending;

static char pending_announce[HU_MAXLINELENGTH];
static boolean announce_pending;

static int CurrentTic(void)
{
    return gametic / ticdup;
}

static hu_event_t *EventFor(int tic)
{
    hu_event_t *event = &events[tic % HU_EVENT_TICS];

    return event->tic == tic ? event : NULL;
}

// An event of a tic that is already confirmed cannot change anymore, so it
// can be shown right away. Everything else is held back: a resimulation may
// still replace it, or produce no event at all.

static boolean CanPresent(int tic)
{
    if (rollback_resim)
    {
        return false;
    }

    return !rollback_enabled || gamestate != GS_LEVEL || RB_TicConfirmed(tic);
}

static void PostMessage(const char *text)
{
    M_StringCopy(pending_message, text, sizeof(pending_message));
    message_pending = true;
}

static void PostAnnouncement(const char *text)
{
    M_StringCopy(pending_announce, text, sizeof(pending_announce));
    announce_pending = true;
}

static void FileMessage(int tic, const char *text)
{
    hu_event_t *event;

    if (CanPresent(tic))
    {
        PostMessage(text);
        return;
    }

    event = EventFor(tic);

    if (event)
    {
        M_StringCopy(event->message, text, sizeof(event->message));
        event->has_message = true;
    }
}

static void FileAnnouncement(int tic, const char *text)
{
    hu_event_t *event;

    if (CanPresent(tic))
    {
        PostAnnouncement(text);
        return;
    }

    event = EventFor(tic);

    if (event)
    {
        M_StringCopy(event->announce, text, sizeof(event->announce));
        event->has_announce = true;
    }
}

void HU_PostMessage(const char *text)
{
    M_StringCopy(posted_message, text, sizeof(posted_message));
    posted_pending = true;
}

void HU_Message(struct player_s *target, msg_category_t category,
                const char *fmt, ...)
{
    char text[HU_MAXLINELENGTH];
    va_list args;

    if (target && target != &players[displayplayer])
    {
        return;
    }

    if (!show_messages)
    {
        return;
    }

    switch (category)
    {
        case MESSAGES_PICKUP:
            if (!show_pickup_messages)
            {
                return;
            }
            break;

        case MESSAGES_TOGGLE:
            if (!show_toggle_messages)
            {
                return;
            }
            break;

        case MESSAGES_OBITUARY:
            if (!show_obituary_messages)
            {
                return;
            }
            break;

        default:
            break;
    }

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    FileMessage(CurrentTic(), text);
}

boolean HU_AnnounceSecret(struct player_s *target, int count, int total)
{
    char text[HU_MAXLINELENGTH];

    if (!hud_secret_message || target != &players[displayplayer])
    {
        return false;
    }

    if (hud_secret_message == SECRETMESSAGE_COUNT)
    {
        M_snprintf(text, sizeof(text), "Secret %d of %d revealed!", count,
                   total);
    }
    else
    {
        M_StringCopy(text, DEH_String(HUSTR_SECRETFOUND), sizeof(text));
    }

    FileAnnouncement(CurrentTic(), text);

    return true;
}

void HU_ChatChar(int tic, int player, char ch)
{
    hu_event_t *event;

    if (!ch || player < 0 || player >= MAXPLAYERS)
    {
        return;
    }

    if (CanPresent(tic))
    {
        HU_ChatCharEvent(player, ch);
        return;
    }

    event = EventFor(tic);

    if (event)
    {
        event->chat[player] = ch;
    }
}

void HU_BeginTic(int tic)
{
    hu_event_t *event = &events[tic % HU_EVENT_TICS];

    event->tic = tic;
    event->has_message = false;
    event->has_announce = false;
    memset(event->chat, 0, sizeof(event->chat));
}

void HU_DispatchEvents(void)
{
    const int curtic = CurrentTic();

    // A resimulation is not a new tic: its events are released afterwards,
    // when the tics it replayed are final.

    if (rollback_resim)
    {
        return;
    }

    if (present_tic < 0)
    {
        present_tic = curtic;
    }

    // A rollback moves the simulation back; the tics below the cursor have
    // already been shown and are not shown again.

    while (present_tic <= curtic && CanPresent(present_tic))
    {
        hu_event_t *event = EventFor(present_tic);

        if (event)
        {
            if (event->has_message)
            {
                PostMessage(event->message);
            }

            if (event->has_announce)
            {
                PostAnnouncement(event->announce);
            }

            for (int p = 0; p < MAXPLAYERS; ++p)
            {
                if (event->chat[p])
                {
                    HU_ChatCharEvent(p, event->chat[p]);
                }
            }

            event->tic = -1;
        }

        ++present_tic;
    }
}

void HU_ResetMessages(void)
{
    for (int i = 0; i < HU_EVENT_TICS; ++i)
    {
        events[i].tic = -1;
        events[i].has_message = false;
        events[i].has_announce = false;
        memset(events[i].chat, 0, sizeof(events[i].chat));
    }

    present_tic = -1;
    message_pending = false;
    posted_pending = false;
    announce_pending = false;
    pending_message[0] = '\0';
    posted_message[0] = '\0';
    pending_announce[0] = '\0';
}

boolean HU_PopMessage(char *buffer, size_t size)
{
    if (posted_pending)
    {
        M_StringCopy(buffer, posted_message, size);
        posted_pending = false;
        return true;
    }

    if (!message_pending)
    {
        return false;
    }

    M_StringCopy(buffer, pending_message, size);
    message_pending = false;

    return true;
}

boolean HU_PopAnnouncement(char *buffer, size_t size)
{
    if (!announce_pending)
    {
        return false;
    }

    M_StringCopy(buffer, pending_announce, size);
    announce_pending = false;

    return true;
}
