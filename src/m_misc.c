//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "m_io.h"
#include "m_misc.h"
#include "z_zone.h"

// Check if a file exists

boolean M_FileExists(const char *filename)
{
    FILE *fstream;

    fstream = M_fopen(filename, "r");

    if (fstream != NULL)
    {
        fclose(fstream);
        return true;
    }
    else
    {
        // If we can't open because the file is a directory, the
        // "file" exists at least!

        return errno == EISDIR;
    }
}

// Returns the path to a temporary file of the given name, stored
// inside the system temporary directory.
//
// The returned value must be freed with Z_Free after use.

char *M_TempFile(const char *s)
{
    const char *tempdir;

#ifdef _WIN32

    // Check the TEMP environment variable to find the location.

    tempdir = M_getenv("TEMP");

    if (tempdir == NULL)
    {
        tempdir = ".";
    }
#else
    // In Unix, just use /tmp.

    tempdir = "/tmp";
#endif

    return M_StringJoin(tempdir, DIR_SEPARATOR_S, s, NULL);
}

// Check if a file exists by probing for common case variation of its filename.
// Returns a newly allocated string that the caller is responsible for freeing.

char *M_FileCaseExists(const char *path)
{
    char *path_dup, *filename, *ext;

    path_dup = M_StringDuplicate(path);

    // 0: actual path
    if (M_FileExists(path_dup))
    {
        return path_dup;
    }

    // cast result to (char *), because `path_dup` isn't (const char *) in the
    // first place
    filename = (char *)M_BaseName(path_dup);

    // 1: lowercase filename, e.g. doom2.wad
    M_StringToLower(filename);

    if (M_FileExists(path_dup))
    {
        return path_dup;
    }

    // 2: uppercase filename, e.g. DOOM2.WAD
    M_StringToUpper(filename);

    if (M_FileExists(path_dup))
    {
        return path_dup;
    }

    // 3. uppercase basename with lowercase extension, e.g. DOOM2.wad
    ext = strrchr(path_dup, '.');
    if (ext != NULL && ext > filename)
    {
        M_StringToLower(ext + 1);

        if (M_FileExists(path_dup))
        {
            return path_dup;
        }
    }

    // 4. lowercase filename with uppercase first letter, e.g. Doom2.wad
    if (strlen(filename) > 1)
    {
        M_StringToLower(filename + 1);

        if (M_FileExists(path_dup))
        {
            return path_dup;
        }
    }

    // 5. no luck
    free(path_dup);
    return NULL;
}

boolean M_StrToInt(const char *str, int *result)
{
    return sscanf(str, " 0x%x", (unsigned int *)result) == 1
           || sscanf(str, " 0X%x", (unsigned int *)result) == 1
           || sscanf(str, " 0%o", (unsigned int *)result) == 1
           || sscanf(str, " %d", result) == 1;
}

// Returns the directory portion of the given path, without the trailing
// slash separator character. If no directory is described in the path,
// the string "." is returned. In either case, the result is newly allocated
// and must be freed by the caller after use.

char *M_DirName(const char *path)
{
    char *result;
    const char *pf, *pb;

    pf = strrchr(path, '/');
#ifdef _WIN32
    pb = strrchr(path, '\\');
#else
    pb = NULL;
#endif
    if (pf == NULL && pb == NULL)
    {
        return M_StringDuplicate(".");
    }
    else
    {
        const char *p = MAX(pb, pf);
        result = M_StringDuplicate(path);
        result[p - path] = '\0';
        return result;
    }
}

// Returns the base filename described by the given path (without the
// directory name). The result points inside path and nothing new is
// allocated.

const char *M_BaseName(const char *path)
{
    const char *pf, *pb;

    pf = strrchr(path, '/');
#ifdef _WIN32
    pb = strrchr(path, '\\');
    // [FG] allow C:filename
    if (pf == NULL && pb == NULL)
    {
        pb = strrchr(path, ':');
    }
#else
    pb = NULL;
#endif
    if (pf == NULL && pb == NULL)
    {
        return path;
    }
    else
    {
        const char *p = MAX(pb, pf);
        return p + 1;
    }
}

// Change string to uppercase.

char M_ToUpper(const char c)
{
  if (c >= 'a' && c <= 'z')
    return c + 'A' - 'a';
  else
    return c;
}

void M_StringToUpper(char *text)
{
    char *p;

    for (p = text; *p != '\0'; ++p)
    {
        *p = M_ToUpper(*p);
    }
}

// Change string to lowercase.

char M_ToLower(const char c)
{
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 'a';
  else
    return c;
}

void M_StringToLower(char *text)
{
    char *p;

    for (p = text; *p != '\0'; ++p)
    {
        *p = M_ToLower(*p);
    }
}

// Safe version of strdup() that checks the string was successfully
// allocated.

char *M_StringDuplicate(const char *orig)
{
    char *result;

    result = strdup(orig);

    if (result == NULL)
    {
        I_Error("Failed to duplicate string (length %ld)\n",
                (long)strlen(orig));
    }

    return result;
}

// String replace function.

char *M_StringReplace(const char *haystack, const char *needle,
                      const char *replacement)
{
    char *result, *dst;
    const char *p;
    size_t needle_len = strlen(needle);
    size_t result_len, dst_len;

    // Iterate through occurrences of 'needle' and calculate the size of
    // the new string.
    result_len = strlen(haystack) + 1;
    p = haystack;

    for (;;)
    {
        p = strstr(p, needle);
        if (p == NULL)
        {
            break;
        }

        p += needle_len;
        result_len += strlen(replacement) - needle_len;
    }

    // Construct new string.

    result = malloc(result_len);
    if (result == NULL)
    {
        I_Error("M_StringReplace: Failed to allocate new string");
        return NULL;
    }

    dst = result;
    dst_len = result_len;
    p = haystack;

    while (*p != '\0')
    {
        if (!strncmp(p, needle, needle_len))
        {
            M_StringCopy(dst, replacement, dst_len);
            p += needle_len;
            dst += strlen(replacement);
            dst_len -= strlen(replacement);
        }
        else
        {
            *dst = *p;
            ++dst;
            --dst_len;
            ++p;
        }
    }

    *dst = '\0';

    return result;
}

// Safe string copy function that works like OpenBSD's strlcpy().
// Returns true if the string was not truncated.

boolean M_StringCopy(char *dest, const char *src, size_t dest_size)
{
    size_t len;

    if (dest_size >= 1)
    {
        dest[dest_size - 1] = '\0';
        strncpy(dest, src, dest_size - 1);
    }
    else
    {
        return false;
    }

    len = strlen(dest);
    return src[len] == '\0';
}

// Safe string concat function that works like OpenBSD's strlcat().
// Returns true if string not truncated.

boolean M_StringConcat(char *dest, const char *src, size_t dest_size)
{
    size_t offset;

    offset = strlen(dest);
    if (offset > dest_size)
    {
        offset = dest_size;
    }

    return M_StringCopy(dest + offset, src, dest_size - offset);
}

// Returns true if 's' ends with the specified suffix.

boolean M_StringEndsWith(const char *s, const char *suffix)
{
    return strlen(s) >= strlen(suffix)
           && strcmp(s + strlen(s) - strlen(suffix), suffix) == 0;
}

boolean M_StringCaseEndsWith(const char *s, const char *suffix)
{
    return strlen(s) >= strlen(suffix)
           && strcasecmp(s + strlen(s) - strlen(suffix), suffix) == 0;
}

// Return a newly-malloced string with all the strings given as arguments
// concatenated together.

char *M_StringJoin(const char *s, ...)
{
    char *result;
    const char *v;
    va_list args;
    size_t result_len;

    result_len = strlen(s) + 1;

    va_start(args, s);
    for (;;)
    {
        v = va_arg(args, const char *);
        if (v == NULL)
        {
            break;
        }

        result_len += strlen(v);
    }
    va_end(args);

    result = malloc(result_len);

    if (result == NULL)
    {
        I_Error("M_StringJoin: Failed to allocate new string.");
        return NULL;
    }

    M_StringCopy(result, s, result_len);

    va_start(args, s);
    for (;;)
    {
        v = va_arg(args, const char *);
        if (v == NULL)
        {
            break;
        }

        M_StringConcat(result, v, result_len);
    }
    va_end(args);

    return result;
}

// Safe, portable vsnprintf().
int PRINTF_ATTR(3, 0)
    M_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
{
    int result;

    if (buf_len < 1)
    {
        return 0;
    }

    // Windows (and other OSes?) has a vsnprintf() that doesn't always
    // append a trailing \0. So we must do it, and write into a buffer
    // that is one byte shorter; otherwise this function is unsafe.
    result = vsnprintf(buf, buf_len, s, args);

    // If truncated, change the final char in the buffer to a \0.
    // A negative result indicates a truncated buffer on Windows.
    if (result < 0 || result >= buf_len)
    {
        buf[buf_len - 1] = '\0';
        result = buf_len - 1;
    }

    return result;
}

// Safe, portable snprintf().
int M_snprintf(char *buf, size_t buf_len, const char *s, ...)
{
    va_list args;
    int result;
    va_start(args, s);
    result = M_vsnprintf(buf, buf_len, s, args);
    va_end(args);
    return result;
}

// Copy lump name (up to 8 chars) to dest buffer.

void M_CopyLumpName(char *dest, const char *src)
{
    size_t len;

    len = strnlen(src, 8);

    if (len < 8)
    {
        len++;
    }

    memcpy(dest, src, len);
}

//
// 1/18/98 killough: adds a default extension to a path
// Note: Backslashes are treated specially, for MS-DOS.
//

char *AddDefaultExtension(char *path, const char *ext)
{
    char *p = path;

    while (*p++)
        ;

    while (p-- > path && *p != '/' && *p != '\\')
    {
        if (*p == '.')
        {
            return path;
        }
    }

    if (*ext != '.')
    {
        strcat(path, ".");
    }

    return strcat(path, ext);
}

//
// M_WriteFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon

boolean M_WriteFile(char const *name, void *source, int length)
{
    FILE *fp;

    errno = 0;

    if (!(fp = M_fopen(name, "wb"))) // Try opening file
    {
        return 0; // Could not open file for writing
    }

    length = fwrite(source, 1, length, fp) == length; // Write data
    fclose(fp);

    if (!length) // Remove partially written file
    {
        M_remove(name);
    }

    return length;
}

//
// M_ReadFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon

int M_ReadFile(char const *name, byte **buffer)
{
    FILE *fp;

    errno = 0;

    if ((fp = M_fopen(name, "rb")))
    {
        size_t length;

        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        *buffer = Z_Malloc(length, PU_STATIC, 0);
        if (fread(*buffer, 1, length, fp) == length)
        {
            fclose(fp);
            return length;
        }
        fclose(fp);
    }

    I_Error("Couldn't read file %s: %s", name,
            errno ? strerror(errno) : "(Unknown Error)");

    return 0;
}
