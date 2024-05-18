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

typedef struct glob_zip_s glob_zip_t;

#define GLOB_FLAG_NOCASE 0x01
#define GLOB_FLAG_SORTED 0x02

glob_zip_t *I_ZIP_StartMultiGlob(void *handle, const char *directory,
                                 int flags, const char *glob, ...);
void I_ZIP_EndGlob(glob_zip_t *glob);
const char *I_ZIP_NextGlob(glob_zip_t *glob);

int I_ZIP_GetIndex(const glob_zip_t *glob);
int I_ZIP_GetSize(const glob_zip_t *glob);
void *I_ZIP_GetHandle(const glob_zip_t *glob);

void *I_ZIP_Open(const char *path);
void I_ZIP_CloseFiles(void);
void I_ZIP_ReadFile(void *handle, int index, void *dest, int size);
