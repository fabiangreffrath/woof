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

#ifndef ST_WIDGETS_H
#define ST_WIDGETS_H

#include "doomtype.h"

struct sbe_widget_s;
struct player_s;
struct event_s;

void UpdateMessage(struct sbe_widget_s *widget, struct player_s *player);
void UpdateSecretMessage(struct sbe_widget_s *widget, struct player_s *player);
void UpdateMonSec(struct sbe_widget_s *widget);
void UpdateStTime(struct sbe_widget_s *widget, struct player_s *player);

void UpdateChatMessage(void);
void UpdateChat(struct sbe_widget_s *widget);
boolean MessagesResponder(struct event_s *ev);

char ST_DequeueChatChar(void);

#endif