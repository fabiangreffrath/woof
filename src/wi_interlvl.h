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

#ifndef WI_INTERLVL
#define WI_INTERLVL

typedef enum
{
    AnimCondition_None,

    AnimCondition_MapNumGreater, // Checks: Current/next map number.
                                 // Parameter: map number

    AnimCondition_MapNumEqual, // Checks: Current/next map number.
                               // Parameter: map number

    AnimCondition_MapVisited, // Checks: Visited flag for map number.
                              // Parameter: map number

    AnimCondition_MapNotSecret, // Checks: Current/next map.
                                // Parameter: none

    AnimCondition_SecretVisited, // Checks: Any secret map visited.
                                 // Parameter: none

    AnimCondition_Tally, // Checks: Victory screen type
                         // Parameter: none

    AnimCondition_IsEntering, // Checks: Victory screen type
                              // Parameter: none
} animcondition_t;

typedef enum
{
    Frame_None                      = 0x0000,
    Frame_Infinite                  = 0x0001,
    Frame_FixedDuration             = 0x0002,
    Frame_RandomDuration            = 0x0004,
    Frame_RandomStart               = 0x1000,
} frametype_t;

typedef struct interlevelcond_s
{
    animcondition_t condition;
    int param;
} interlevelcond_t;

typedef struct interlevelframe_s
{
    char *image_lump;
    frametype_t type;
    int duration;
    int maxduration;
} interlevelframe_t;

typedef struct interlevelanim_s
{
    interlevelframe_t *frames;
    interlevelcond_t *conditions;
    int x_pos;
    int y_pos;
} interlevelanim_t;

typedef struct interlevellayer_s
{
    interlevelanim_t *anims;
    interlevelcond_t *conditions;
} interlevellayer_t;

typedef struct interlevel_s
{
    char *music_lump;
    char *background_lump;
    interlevellayer_t *layers;
} interlevel_t;

interlevel_t *WI_ParseInterlevel(const char *lumpname);

#endif
