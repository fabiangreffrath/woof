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

#ifndef W_INTERNAL_H
#define W_INTERNAL_H

#include "w_wad.h"

typedef enum
{
    W_NONE,
    W_DIR,
    W_FILE
} w_type_t;

typedef struct
{
    const char *dir;          // reserved directory, e.g. "sprites"
    const char *start_marker; // marker lumps surrounding the directory
    const char *end_marker;   // contents, or NULL for ns_global directories
    namespace_t namespace;
    boolean is_map;           // files are single-level map WADs (maps/)
} w_dir_spec_t;

typedef struct w_module_s
{
    boolean (*AddDir)(w_handle_t handle, const char *path,
                      const w_dir_spec_t *spec);
    w_type_t (*Open)(const char *path, w_handle_t *handle);
    void (*Read)(w_handle_t handle, void *dest, int size);
    void (*Close)(void);
} w_module_t;

extern w_module_t w_zip_module;
extern w_module_t w_file_module;

void W_AddMarker(const char *name);

void W_ConvertSlashes(char *path);

// [ZDoom PK3] sprite frames containing the '\' character (e.g. VILE\1)
// are named with a caret in archives (e.g. VILE^1.png).
void W_ConvertSpriteName(char *name);

// Add the lumps of a WAD image held in memory. If map_name is non-NULL, only
// the lumps of the first level are added and the level marker lump is renamed
// to map_name (ZDoom PK3: WADs under maps/ hold a single level, and the
// file name determines the map name). data must remain valid: added lumps
// point into it.
void W_AddWadFromMemory(const char *name, const void *data, size_t data_size,
                        const char *map_name);

#endif
