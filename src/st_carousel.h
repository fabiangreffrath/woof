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

#ifndef ST_CAROUSEL_H
#define ST_CAROUSEL_H

struct player_s;
struct sbarelem_s;

void ST_ResetCarousel(void);

void ST_UpdateCarousel(struct player_s *player);

void ST_DrawCarousel(int x, int y, struct sbarelem_s *elem);

#endif
