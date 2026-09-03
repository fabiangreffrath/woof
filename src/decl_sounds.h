//
// Copyright(C) 2025 ceski
// Copyright(C) 2026, Roman Fomin
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

#ifndef DECL_SOUNDS_H
#define DECL_SOUNDS_H

#include "doomtype.h"

int S_RandomSound(int sfx_number);

typedef enum
{
    AMB_TYPE_POINT,
    AMB_TYPE_WORLD,
} ambient_type_t;

typedef enum
{
    AMB_MODE_CONTINUOUS,
    AMB_MODE_RANDOM,
    AMB_MODE_PERIODIC,
} ambient_mode_t;

typedef struct ambient_data_s
{
    ambient_type_t type;
    ambient_mode_t mode;
    int close_dist;
    int clipping_dist;
    int min_tics;
    int max_tics;
    int volume_scale;
    int sfx_id;
} ambient_data_t;

const ambient_data_t *S_GetAmbientData(int index);

// Query function used to decide whether a SNDINFO fallback should be
// loaded: skipped whenever DECLARE already defined ambient sounds.
boolean DECL_HasAmbientSounds(void);

// Feed-in functions used by the SNDINFO parser (decl_sndinfo.c) to
// populate the same internal sound/ambient tables that DECLARE uses.
void DECL_AddSndInfoSound(const char *name, const char *lump);
void DECL_AddSndInfoAmbient(int index, ambient_mode_t mode,
                             const char *sound_name, double attenuation,
                             double param1, double param2, double volume);

#endif
