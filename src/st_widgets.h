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

struct sbarelem_s;
struct player_s;

void UpdateMessage(struct sbarelem_s *elem, struct player_s *player);
void UpdateMonSec(struct sbarelem_s *elem);
void UpdateStTime(struct sbarelem_s *elem, struct player_s *player);

#endif