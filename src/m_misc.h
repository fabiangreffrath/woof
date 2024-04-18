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

#ifndef __M_MISC__
#define __M_MISC__

#include <stdarg.h>

#include "doomtype.h"

boolean M_FileExists(const char *file);
char *M_TempFile(const char *s);
char *M_FileCaseExists(const char *file);
boolean M_StrToInt(const char *str, int *result);
char *M_DirName(const char *path);
const char *M_BaseName(const char *path);
char M_ToUpper(const char c);
void M_StringToUpper(char *text);
char M_ToLower(const char c);
void M_StringToLower(char *text);
char *M_StringDuplicate(const char *orig);
boolean M_StringCopy(char *dest, const char *src, size_t dest_size);
boolean M_StringConcat(char *dest, const char *src, size_t dest_size);
char *M_StringReplace(const char *haystack, const char *needle,
                      const char *replacement);
char *M_StringJoin(const char *s, ...);
boolean M_StringEndsWith(const char *s, const char *suffix);
boolean M_StringCaseEndsWith(const char *s, const char *suffix);
int M_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
    PRINTF_ATTR(3, 0);
int M_snprintf(char *buf, size_t buf_len, const char *s, ...) PRINTF_ATTR(3, 4);

void M_CopyLumpName(char *dest, const char *src);
char *AddDefaultExtension(char *path, const char *ext);
boolean M_WriteFile(const char *name, void *source, int length);
int M_ReadFile(const char *name, byte **buffer);

#endif
