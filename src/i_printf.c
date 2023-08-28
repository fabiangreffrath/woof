//
//  Copyright (C) 2023 Fabian Greffrath
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
// DESCRIPTION: clean interface for logging to console
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <io.h>
#else
 #include <unistd.h> // [FG] isatty()
#endif

#include "i_printf.h"
#include "m_argv.h"

// [FG] returns true if stdout is a real console, false if it is a file

int I_ConsoleStdout(void)
{
    static int ret = -1;

    if (ret == -1)
    {
#ifdef _WIN32
        if (_isatty(_fileno(stdout)))
#else
        if (isatty(fileno(stdout)) && isatty(fileno(stderr)))
#endif
            ret = 1;
        else
            ret = 0;
    }

    return ret;
}

static verbosity_t verbosity = VB_INFO;
verbosity_t cfg_verbosity;

void I_InitPrintf(void)
{
    verbosity = cfg_verbosity;

    if (M_ParmExists("-verbose") || M_ParmExists("--verbose"))
        verbosity = VB_MAX;
}

void I_Printf(verbosity_t prio, const char *msg, ...)
{
    FILE *stream = stdout;
    const char *color_prefix = NULL, *color_suffix = NULL;
    va_list args;

    if (prio > verbosity)
        return;

    switch (prio)
    {
        case VB_WARNING:
        case VB_ERROR:
        case VB_DEBUG:
            stream = stderr;
            break;
        default:
            break;
    }

    if (I_ConsoleStdout())
    {
        switch (prio)
        {
            case VB_WARNING:
                color_prefix = "\033[36m";
                break;
            case VB_ERROR:
                color_prefix = "\033[31m";
                break;
            case VB_DEBUG:
                color_prefix = "\033[35m";
                break;
            default:
                break;
        }

        if (color_prefix)
            color_suffix = "\033[0m";
    }

    if (color_prefix)
        fprintf(stream, "%s", color_prefix);

    va_start(args, msg);
    vfprintf(stream, msg, args);
    va_end(args);

    if (color_suffix)
        fprintf(stream, "%s", color_suffix);

    fprintf(stream, "%s", "\n");
}

