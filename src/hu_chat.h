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

#ifndef HU_CHAT_H
#define HU_CHAT_H

#include "doomtype.h"

struct event_s;
struct sbe_widget_s;

extern boolean chat_on;

boolean HU_MessagesResponder(struct event_s *ev);

// The next character the local player typed, for its ticcmd.

char HU_DequeueChatChar(void);

// Show a character of a remote player. Chat is a one-shot event that the
// simulation never applies to a predicted tic, so the character is shown
// once, when the tic carrying it can no longer be rolled back.

void HU_ChatCharEvent(int player, char ch);

void HU_UpdateChatWidget(struct sbe_widget_s *widget);

void HU_ResetChat(void);

#endif
