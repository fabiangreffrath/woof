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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "i_system.h"

#ifdef _WIN32
static wchar_t* ConvertToUtf8(const char *str)
{
    wchar_t *wstr = NULL;
    int wlen = 0;

    wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

    if (!wlen)
    {
        errno = EINVAL;
        printf("Warning: Failed to convert path to UTF8\n");
        return NULL;
    }

    wstr = malloc(sizeof(wchar_t) * wlen);

    if (!wstr)
    {
        I_Error("ConvertToUtf8: Failed to allocate new string");
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wlen) == 0)
    {
        errno = EINVAL;
        printf("Warning: Failed to convert path to UTF8\n");
        free(wstr);
        return NULL;
    }

    return wstr;
}
#endif

FILE* M_fopen(const char *filename, const char *mode)
{
#ifdef _WIN32
    FILE *file;
    wchar_t *wname = NULL;
    wchar_t *wmode = NULL;

    wname = ConvertToUtf8(filename);

    if (!wname)
    {
        return NULL;
    }

    wmode = ConvertToUtf8(mode);

    if (!wmode)
    {
        free(wname);
        return NULL;
    }

    file = _wfopen(wname, wmode);

    free(wname);
    free(wmode);

    return file;
#else
    return fopen(filename, mode);
#endif
}

int M_remove(const char *path)
{
#ifdef _WIN32
    wchar_t *wpath = NULL;
    int ret;

    wpath = ConvertToUtf8(path);

    if (!wpath)
    {
        return 0;
    }

    ret = _wremove(wpath);

    free(wpath);

    return ret;
#else
    return remove(path);
#endif
}

int M_rename(const char *oldname, const char *newname)
{
#ifdef _WIN32
    wchar_t *wold = NULL;
    wchar_t *wnew = NULL;
    int ret;

    wold = ConvertToUtf8(oldname);

    if (!wold)
    {
        return 0;
    }

    wnew = ConvertToUtf8(newname);

    if (!wnew)
    {
        free(wold);
        return 0;
    }

    ret = _wrename(wold, wnew);

    free(wold);
    free(wnew);

    return ret;
#else
    return rename(oldname, newname);
#endif
}

int M_stat(const char *path, struct stat *buf)
{
#ifdef _WIN32
    wchar_t *wpath = NULL;
    struct _stat wbuf;
    int ret;

    wpath = ConvertToUtf8(path);

    if (!wpath)
    {
        return -1;
    }

    ret = _wstat(wpath, &wbuf);

    // The _wstat() function expects a struct _stat* parameter that is
    // incompatible with struct stat*. We copy only the required compatible
    // field.
    buf->st_mode = wbuf.st_mode;

    free(wpath);

    return ret;
#else
    return stat(path, buf);
#endif
}

int M_open(const char *filename, int oflag)
{
#ifdef _WIN32
    wchar_t *wname = NULL;
    int ret;

    wname = ConvertToUtf8(filename);

    if (!wname)
    {
        return 0;
    }

    ret = _wopen(wname, oflag);

    free(wname);

    return ret;
#else
    return open(filename, oflag);
#endif
}

int M_access(const char *path, int mode)
{
#ifdef _WIN32
    wchar_t *wpath = NULL;
    int ret;

    wpath = ConvertToUtf8(path);

    if (!wpath)
    {
        return 0;
    }

    ret = _waccess(wpath, mode);

    free(wpath);

    return ret;
#else
    return access(path, mode);
#endif
}

void M_MakeDirectory(const char *path)
{
#ifdef _WIN32
    wchar_t *wdir;

    wdir = ConvertToUtf8(path);

    if (!wdir)
    {
        return;
    }

    _wmkdir(wdir);

    free(wdir);
#else
    mkdir(path, 0755);
#endif
}
