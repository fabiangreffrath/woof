//
// Copyright(C) 2018 Simon Howard
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
//
// File globbing API. This allows the contents of the filesystem
// to be interrogated.
//

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "i_glob.h"
#include "i_system.h"
#include "m_misc.h"

struct glob_s
{
    char **globs;
    int num_globs;
    int flags;
    char *directory;
    char **filenames;
    int filenames_len;
    int next_index;
};

static void FreeStringList(char **globs, int num_globs)
{
    int i;
    for (i = 0; i < num_globs; ++i)
    {
        free(globs[i]);
    }
    free(globs);
}

glob_t *I_StartMultiGlobInternal(const char *directory, int flags,
                                 const char *glob[], size_t num_globs)
{
    char **globs;
    glob_t *result;

    globs = malloc(num_globs * sizeof(*globs));
    if (globs == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < num_globs; ++i)
    {
        globs[i] = M_StringDuplicate(glob[i]);
    }

    result = malloc(sizeof(glob_t));
    if (result == NULL)
    {
        FreeStringList(globs, num_globs);
        return NULL;
    }

    if (!M_DirExists(directory))
    {
        FreeStringList(globs, num_globs);
        free(result);
        return NULL;
    }

    result->directory = M_StringDuplicate(directory);
    result->globs = globs;
    result->num_globs = num_globs;
    result->flags = flags;
    result->filenames = NULL;
    result->filenames_len = 0;
    result->next_index = -1;
    return result;
}

glob_t *I_StartGlob(const char *directory, const char *glob, int flags)
{
    return I_StartMultiGlob(directory, flags, glob);
}

void I_EndGlob(glob_t *glob)
{
    if (glob == NULL)
    {
        return;
    }

    FreeStringList(glob->globs, glob->num_globs);
    FreeStringList(glob->filenames, glob->filenames_len);

    free(glob->directory);
    free(glob);
}

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

static boolean MatchesAnyGlob(const char *name, glob_t *glob)
{
    int i;

    for (i = 0; i < glob->num_globs; ++i)
    {
        if (MatchesGlob(name, glob->globs[i], glob->flags))
        {
            return true;
        }
    }
    return false;
}

static SDL_EnumerationResult Callback(void *userdata, const char *dirname,
                                      const char *fname)
{
    glob_t *glob = userdata;
    char *path = M_StringJoin(dirname, fname);

    if (!M_DirExists(path) && MatchesAnyGlob(fname, glob))
    {
        glob->filenames = realloc(glob->filenames,
                                  (glob->filenames_len + 1) * sizeof(char *));
        glob->filenames[glob->filenames_len] = path;
        ++glob->filenames_len;
    }
    else
    {
        free(path);
    }

    return SDL_ENUM_CONTINUE;
}

static void ReadAllFilenames(glob_t *glob)
{
    glob->filenames = NULL;
    glob->filenames_len = 0;
    glob->next_index = 0;

    if (!SDL_EnumerateDirectory(glob->directory, Callback, glob))
    {
        I_Error("Failed to enumerate directory %s", glob->directory);
    }
}

static void SortFilenames(char **filenames, int len, int flags)
{
    char *pivot, *tmp;
    int i, left_len, cmp;

    if (len <= 1)
    {
        return;
    }
    pivot = filenames[len - 1];
    left_len = 0;
    for (i = 0; i < len - 1; ++i)
    {
        if ((flags & GLOB_FLAG_NOCASE) != 0)
        {
            cmp = strcasecmp(filenames[i], pivot);
        }
        else
        {
            cmp = strcmp(filenames[i], pivot);
        }

        if (cmp < 0)
        {
            tmp = filenames[i];
            filenames[i] = filenames[left_len];
            filenames[left_len] = tmp;
            ++left_len;
        }
    }
    filenames[len - 1] = filenames[left_len];
    filenames[left_len] = pivot;

    SortFilenames(filenames, left_len, flags);
    SortFilenames(&filenames[left_len + 1], len - left_len - 1, flags);
}

const char *I_NextGlob(glob_t *glob)
{
    const char *result;

    if (glob == NULL)
    {
        return NULL;
    }

    // We read the whole list of filenames into memory, sort them and return
    // them one at a time.
    if (glob->next_index < 0)
    {
        ReadAllFilenames(glob);
        if (glob->flags & GLOB_FLAG_SORTED)
        {
            SortFilenames(glob->filenames, glob->filenames_len, glob->flags);
        }
    }
    if (glob->next_index >= glob->filenames_len)
    {
        return NULL;
    }
    result = glob->filenames[glob->next_index];
    ++glob->next_index;
    return result;
}
