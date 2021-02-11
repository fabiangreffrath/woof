//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//      [FG] miscellaneous helper functions from Chocolate Doom.
//

#ifndef __M_MISC2__
#define __M_MISC2__

#include <string.h>

#include "doomtype.h"

void M_MakeDirectory(const char *dir);
boolean M_FileExists(const char *file);
char *M_TempFile(const char *s);
char *M_FileCaseExists(const char *file);
boolean M_StrToInt(const char *str, int *result);
char *M_DirName(const char *path);
const char *M_BaseName(const char *path);
void M_ForceUppercase(char *text);
void M_ForceLowercase(char *text);
char *M_StringDuplicate(const char *orig);
boolean M_StringCopy(char *dest, const char *src, size_t dest_size);
boolean M_StringConcat(char *dest, const char *src, size_t dest_size);
char *M_StringReplace(const char *haystack, const char *needle,
                      const char *replacement);
char *M_StringJoin(const char *s, ...);
boolean M_StringEndsWith(const char *s, const char *suffix);
int M_snprintf(char *buf, size_t buf_len, const char *s, ...) PRINTF_ATTR(3, 4);

#endif
