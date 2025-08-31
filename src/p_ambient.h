//
// Copyright(C) 2025 ceski
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
//
// DESCRIPTION:
//      Ambient sound
//

#ifndef __P_AMBIENT__
#define __P_AMBIENT__

#include "p_mobj.h"
#include "s_sndinfo.h"

#define AMB_UPDATE_NOW -1 // Try to start ambient sound immediately.

struct sfxparams_s;

typedef struct ambient_s
{
    thinker_t thinker;   // Additional thinker attached to map thing mobj.
    mobj_t *source;      // Causes sound (map thing mobj).
    mobj_t *origin;      // Emits sound (NULL for world sounds).
    ambient_data_t data; // Local instance of the data from SNDINFO.
    int update_tics;     // Wait interval between attempts to start sound.
    int wait_tics;       // Wait interval for random or periodic sounds.
    boolean active;      // Should the ambient sound be playing?
    boolean playing;     // Is the ambient sound playing?
    float offset;        // Effective offset.
    float last_offset;   // Last saved offset in sound buffer (seconds).
    int last_leveltime;  // Last saved leveltime (tics).
} ambient_t;

void P_GetAmbientSoundParams(ambient_t *ambient, struct sfxparams_s *params);
float P_GetAmbientSoundOffset(ambient_t *ambient);
boolean P_PlayingAmbientSound(ambient_t *ambient);
void P_StopAmbientSound(ambient_t *ambient);
void P_MarkAmbientSound(ambient_t *ambient, int handle);
void P_EvictAmbientSound(ambient_t *ambient, int handle);
void T_AmbientSoundAdapter(mobj_t *mobj);
void P_AddAmbientSoundThinker(mobj_t *mobj, int index);
void P_InitAmbientSoundMobjInfo(void);

#endif
