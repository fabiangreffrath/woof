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
//      SNDINFO
//

#ifndef __S_SNDINFO__
#define __S_SNDINFO__

typedef enum ambient_type_e
{
    AMB_TYPE_POINT,
    AMB_TYPE_WORLD,
} ambient_type_t;

typedef enum ambient_mode_e
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
void S_ParseSndInfo(int lumpnum);
void S_PostParseSndInfo(void);

#endif
