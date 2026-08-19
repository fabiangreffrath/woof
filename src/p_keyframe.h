//
// Copyright(C) 2025 Roman Fomin
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

#ifndef P_KEYFRAME_H
#define P_KEYFRAME_H

#include "m_json.h"

typedef struct
{
    struct keyframe_data_s *data;
    int tic;
    int episode;
    int map;
} keyframe_t;

keyframe_t *P_SaveKeyframe(int tic);
void P_LoadKeyframe(const keyframe_t *keyframe);
void P_FreeKeyframe(keyframe_t *keyframe);

void P_ArchiveKeyframe(json_mut_doc_t *doc, json_mut_t *root_mut);
void P_UnArchiveKeyframe(void);

#endif
