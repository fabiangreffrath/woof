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

typedef struct w_module_s
{
    boolean (*AddDir)(w_handle_t handle, const char *path,
                      const char *start_marker, const char *end_marker);
    w_type_t (*Open)(const char *path, w_handle_t *handle);
    void (*Read)(w_handle_t handle, void *dest, int size);
    void (*Close)(void);
} w_module_t;

extern w_module_t w_zip_module;
extern w_module_t w_file_module;

void W_AddMarker(const char *name);
boolean W_SkipFile(const char *filename);

#endif
