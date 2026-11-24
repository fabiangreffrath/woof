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

#ifndef HU_MESSAGE_H
#define HU_MESSAGE_H

#include <stddef.h>

#include "doomtype.h"

#define HU_MAXLINELENGTH 120

struct player_s;

typedef enum
{
    MESSAGES_NONE,
    MESSAGES_TOGGLE,
    MESSAGES_PICKUP,
    MESSAGES_OBITUARY
} msg_category_t;

// A message is a one-shot presentation event. It is never stored in
// simulated state, so it cannot leak into a keyframe or be replayed by a
// rollback: the events of a tic are recorded for that tic and shown once,
// when the tic can no longer be rolled back.

void HU_Message(struct player_s *target, msg_category_t category,
                const char *fmt, ...) PRINTF_ATTR(3, 4);

// Immediate, unfiltered message: chat echo and feedback about the message
// settings themselves, which must be shown even while messages are off.

void HU_PostMessage(const char *text);

// Announce a revealed secret to the local view. Returns whether the
// announcement was made, so that the caller can keep the sound tied to it.

boolean HU_AnnounceSecret(struct player_s *target, int count, int total);

// Record the character a remote player typed on the given tic.

void HU_ChatChar(int tic, int player, char ch);

// Prepare the event slot of a tic that is about to be simulated. A tic can
// be resimulated, so its slot is replaced rather than appended to.

void HU_BeginTic(int tic);

// Release the events of the tics that can no longer change.

void HU_DispatchEvents(void);

// Drop every pending event. Called whenever a level is loaded.

void HU_ResetMessages(void);

boolean HU_PopMessage(char *buffer, size_t size);
boolean HU_PopAnnouncement(char *buffer, size_t size);

#define displaymsg(...)        HU_Message(NULL, MESSAGES_NONE, __VA_ARGS__)
#define pickupmsg(player, ...) HU_Message(player, MESSAGES_PICKUP, "%s", __VA_ARGS__)
#define togglemsg(...)         HU_Message(NULL, MESSAGES_TOGGLE, __VA_ARGS__)

#endif
