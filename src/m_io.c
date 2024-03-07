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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <direct.h>
#  include <io.h>
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "i_printf.h"
#include "i_system.h"
#include "m_array.h"
#include "m_misc.h"

#ifdef _WIN32
static wchar_t *ConvertMultiByteToWide(const char *str, UINT code_page)
{
    wchar_t *wstr = NULL;
    int wlen = 0;

    wlen = MultiByteToWideChar(code_page, 0, str, -1, NULL, 0);

    if (!wlen)
    {
        errno = EINVAL;
        I_Printf(VB_WARNING,
                 "Warning: Failed to convert path to wide encoding");
        return NULL;
    }

    wstr = malloc(sizeof(wchar_t) * wlen);

    if (!wstr)
    {
        I_Error("ConvertUtf8ToWide: Failed to allocate new string");
        return NULL;
    }

    if (MultiByteToWideChar(code_page, 0, str, -1, wstr, wlen) == 0)
    {
        errno = EINVAL;
        I_Printf(VB_WARNING,
                 "Warning: Failed to convert path to wide encoding");
        free(wstr);
        return NULL;
    }

    return wstr;
}

static char *ConvertWideToMultiByte(const wchar_t *wstr, UINT code_page)
{
    char *str = NULL;
    int len = 0;

    len = WideCharToMultiByte(code_page, 0, wstr, -1, NULL, 0, NULL, NULL);

    if (!len)
    {
        errno = EINVAL;
        I_Printf(VB_WARNING,
                 "Warning: Failed to convert path to multi byte encoding");
        return NULL;
    }

    str = malloc(sizeof(char) * len);

    if (!str)
    {
        I_Error("ConvertWideToMultiByte: Failed to allocate new string");
        return NULL;
    }

    if (WideCharToMultiByte(code_page, 0, wstr, -1, str, len, NULL, NULL) == 0)
    {
        errno = EINVAL;
        I_Printf(VB_WARNING,
                 "Warning: Failed to convert path to multi byte encoding");
        free(str);
        return NULL;
    }

    return str;
}

static wchar_t *ConvertUtf8ToWide(const char *str)
{
    return ConvertMultiByteToWide(str, CP_UTF8);
}

char *M_ConvertWideToUtf8(const wchar_t *wstr)
{
    return ConvertWideToMultiByte(wstr, CP_UTF8);
}

static wchar_t *ConvertSysNativeMBToWide(const char *str)
{
    return ConvertMultiByteToWide(str, CP_ACP);
}

static char *ConvertWideToSysNativeMB(const wchar_t *wstr)
{
    return ConvertWideToMultiByte(wstr, CP_ACP);
}
#endif

char *M_ConvertSysNativeMBToUtf8(const char *str)
{
#ifdef _WIN32
    char *ret = NULL;
    wchar_t *wstr = NULL;

    wstr = ConvertSysNativeMBToWide(str);

    if (!wstr)
    {
        return NULL;
    }

    ret = M_ConvertWideToUtf8(wstr);

    free(wstr);

    return ret;
#else
    return M_StringDuplicate(str);
#endif
}

char *M_ConvertUtf8ToSysNativeMB(const char *str)
{
#ifdef _WIN32
    char *ret = NULL;
    wchar_t *wstr = NULL;

    wstr = ConvertUtf8ToWide(str);

    if (!wstr)
    {
        return NULL;
    }

    ret = ConvertWideToSysNativeMB(wstr);

    free(wstr);

    return ret;
#else
    return M_StringDuplicate(str);
#endif
}

FILE *M_fopen(const char *filename, const char *mode)
{
#ifdef _WIN32
    FILE *file;
    wchar_t *wname = NULL;
    wchar_t *wmode = NULL;

    wname = ConvertUtf8ToWide(filename);

    if (!wname)
    {
        return NULL;
    }

    wmode = ConvertUtf8ToWide(mode);

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

    wpath = ConvertUtf8ToWide(path);

    if (!wpath)
    {
        return -1;
    }

    ret = _wremove(wpath);

    free(wpath);

    return ret;
#else
    return remove(path);
#endif
}

int M_rmdir(const char *dirname)
{
#ifdef _WIN32
    wchar_t *wdirname = NULL;
    int ret;

    wdirname = ConvertUtf8ToWide(dirname);

    if (!wdirname)
    {
        return 0;
    }

    ret = _wrmdir(wdirname);

    free(wdirname);

    return ret;
#else
    return rmdir(dirname);
#endif
}

int M_rename(const char *oldname, const char *newname)
{
#ifdef _WIN32
    wchar_t *wold = NULL;
    wchar_t *wnew = NULL;
    int ret;

    wold = ConvertUtf8ToWide(oldname);

    if (!wold)
    {
        return 0;
    }

    wnew = ConvertUtf8ToWide(newname);

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

    wpath = ConvertUtf8ToWide(path);

    if (!wpath)
    {
        return -1;
    }

    ret = _wstat(wpath, &wbuf);

    // The _wstat() function expects a struct _stat* parameter that is
    // incompatible with struct stat*. We copy only the required compatible
    // field.
    buf->st_mode = wbuf.st_mode;
    buf->st_mtime = wbuf.st_mtime;

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

    wname = ConvertUtf8ToWide(filename);

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

    wpath = ConvertUtf8ToWide(path);

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

    wdir = ConvertUtf8ToWide(path);

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

#ifdef _WIN32
typedef struct
{
    char *var;
    const char *name;
} env_var_t;

static env_var_t *env_vars = NULL;
#endif

char *M_getenv(const char *name)
{
#ifdef _WIN32
    int i;
    wchar_t *wenv = NULL, *wname = NULL;
    char *env;

    for (i = 0; i < array_size(env_vars); ++i)
    {
        if (!strcasecmp(name, env_vars[i].name))
        {
            return env_vars[i].var;
        }
    }

    wname = ConvertUtf8ToWide(name);

    if (!wname)
    {
        return NULL;
    }

    wenv = _wgetenv(wname);

    free(wname);

    if (wenv)
    {
        env = M_ConvertWideToUtf8(wenv);
    }
    else
    {
        env = NULL;
    }

    env_var_t env_var;
    env_var.var = env;
    env_var.name = M_StringDuplicate(name);
    array_push(env_vars, env_var);

    return env;
#else
    return getenv(name);
#endif
}
