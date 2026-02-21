//  Copyright (C) 2004 James Haley
//  Copyright (C) 2022 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//      Compatibility wrappers from Chocolate Doom
//

#ifndef M_IO_INCLUDED
#define M_IO_INCLUDED

#include <stdio.h>

#ifdef _MSC_VER
#  include <direct.h>
#  include <io.h>
#  define F_OK       0
#  define W_OK       2
#  define R_OK       4
#else
#  include <unistd.h>
#endif

FILE *M_fopen(const char *filename, const char *mode);
int M_remove(const char *path);
int M_rename(const char *oldname, const char *newname);
int M_access(const char *path, int mode);
void M_MakeDirectory(const char *dir);
const char *M_getenv(const char *name);

#ifdef _WIN32
char *M_ConvertWideToUtf8(const wchar_t *wstr);
#endif

#endif // M_IO_INCLUDED
