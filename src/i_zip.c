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

#include <stdarg.h>

#include "i_zip.h"

#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_array.h"
#include "m_misc.h"

#include "miniz.h"

struct glob_zip_s
{
    mz_zip_archive *zip;
    mz_zip_archive_file_stat *stat;
    int next_index;
    int flags;
    char *directory;
    char **globs;
};

static mz_zip_archive **handles = NULL;

void *I_ZIP_Open(const char *path)
{
    mz_zip_archive *handle = calloc(1, sizeof(*handle));

    if (!mz_zip_reader_init_file(handle, path, 0))
    {
        I_Printf(VB_DEBUG, "I_ZIP_Open: Failed to open %s", path);
        return NULL;
    }

    array_push(handles, handle);

    return handle;
}

void I_ZIP_CloseFiles(void)
{
    for (int i = 0; i < array_size(handles); ++i)
    {
        mz_zip_reader_end(handles[i]);
    }
}

glob_zip_t *I_ZIP_StartMultiGlob(void *handle, const char *directory,
                                 int flags, const char *glob, ...)
{
    glob_zip_t *result = calloc(1, sizeof(*result));

    result->zip = handle;
    result->stat = calloc(1, sizeof(mz_zip_archive_file_stat));
    if (directory)
    {
        result->directory = M_StringDuplicate(directory);
    }
    result->flags = flags;

    char **globs = NULL;

    array_push(globs, M_StringDuplicate(glob));

    va_list args;
    va_start(args, glob);
    while (true)
    {
        const char *arg = va_arg(args, const char *);

        if (arg == NULL)
        {
            break;
        }

        array_push(globs, M_StringDuplicate(arg));
    }
    va_end(args);

    result->globs = globs;

    return result;
}

void I_ZIP_EndGlob(glob_zip_t *glob)
{
    free(glob->stat);
    if (glob->directory)
    {
        free(glob->directory);
    }
    for (int i = 0; i < array_size(glob->globs); ++i)
    {
        free(glob->globs[i]);
    }
    array_free(glob->globs);
    free(glob);
}

// Taken from i_glob.c

static boolean MatchesGlob(const char *name, const char *glob, int flags)
{
    int n, g;

    while (*glob != '\0')
    {
        n = *name;
        g = *glob;

        if ((flags & GLOB_FLAG_NOCASE) != 0)
        {
            n = M_ToLower(n);
            g = M_ToLower(g);
        }

        if (g == '*')
        {
            // To handle *-matching we skip past the * and recurse
            // to check each subsequent character in turn. If none
            // match then the whole match is a failure.
            while (*name != '\0')
            {
                if (MatchesGlob(name, glob + 1, flags))
                {
                    return true;
                }
                ++name;
            }
            return glob[1] == '\0';
        }
        else if (g != '?' && n != g)
        {
            // For normal characters the name must match the glob,
            // but for ? we don't care what the character is.
            return false;
        }

        ++name;
        ++glob;
    }

    // Match successful when glob and name end at the same time.
    return *name == '\0';
}

const char *I_ZIP_NextGlob(glob_zip_t *glob)
{
    for (int i = glob->next_index; i < mz_zip_reader_get_num_files(glob->zip); ++i)
    {
        mz_zip_reader_file_stat(glob->zip, i, glob->stat);

        if (glob->stat->m_is_directory)
        {
            continue;
        }

        if (glob->directory)
        {
            char *dir = M_DirName(glob->stat->m_filename);
            boolean result = M_StringCaseEndsWith(dir, glob->directory);
            free(dir);
            if (!result)
            {
                continue;
            }
        }

        for (int j = 0; j < array_size(glob->globs); ++j)
        {
            if (MatchesGlob(glob->stat->m_filename, glob->globs[j], glob->flags))
            {
                glob->next_index = i + 1;
                return glob->stat->m_filename;
            }
        }
    }

    return NULL;
}

int I_ZIP_GetIndex(const glob_zip_t *glob)
{
    return glob->stat->m_file_index;
}

int I_ZIP_GetSize(const glob_zip_t *glob)
{
    return glob->stat->m_uncomp_size;
}

void *I_ZIP_GetHandle(const glob_zip_t *glob)
{
    return glob->zip;
}

void I_ZIP_ReadFile(void *handle, int index, void *dest, int size)
{
    if (!mz_zip_reader_extract_to_mem(handle, index, dest, size, 0))
    {
        I_Error("I_ZIP_ReadFile: mz_zip_reader_extract_to_mem failed");
    }
}
