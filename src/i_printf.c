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
#include <string.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <io.h>
#else
 #include <unistd.h> // [FG] isatty()
#endif

#include "i_printf.h"
#include "i_system.h"
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

#ifdef _WIN32
static HANDLE hConsole;
static DWORD OldMode;
static boolean vt_mode_enabled = false;

static void EnableVTMode(void)
{
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE)
    {
        return;
    }

    if (!GetConsoleMode(hConsole, &OldMode))
    {
        return;
    }

    if (!SetConsoleMode(hConsole, ENABLE_PROCESSED_OUTPUT |
                                  ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    {
        return;
    }

    vt_mode_enabled = true;
}

static void RestoreOldMode(void)
{
    if (!vt_mode_enabled)
    {
        return;
    }

    SetConsoleMode(hConsole, OldMode);
}
#endif

static void I_ShutdownPrintf(void)
{
#ifdef _WIN32
    RestoreOldMode();
#endif
}

void I_InitPrintf(void)
{
#ifdef _WIN32
    EnableVTMode();
#endif

    verbosity = cfg_verbosity;

    //!
    //
    // Print debugging info with maximum verbosity.
    //

    if (M_ParmExists("-verbose") || M_ParmExists("--verbose"))
        verbosity = VB_MAX;

    I_AtExit(I_ShutdownPrintf, true);
}

static boolean whole_line = true;

void I_Printf(verbosity_t prio, const char *msg, ...)
{
    FILE *stream = stdout;
    const int msglen = strlen(msg);
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

    if (I_ConsoleStdout()
#ifdef _WIN32
        && vt_mode_enabled
#endif
        )
    {
        switch (prio)
        {
            case VB_WARNING:
                color_prefix = "\033[33m"; // [FG] yellow
                break;
            case VB_ERROR:
                color_prefix = "\033[31m"; // [FG] red
                break;
            case VB_DEBUG:
                color_prefix = "\033[36m"; // [FG] cyan
                break;
            default:
                break;
        }

        if (color_prefix)
            color_suffix = "\033[0m"; // [FG] reset
    }

    // [FG] warnings always get their own new line
    if (!whole_line && prio != VB_INFO)
        fprintf(stream, "%s", "\n");

    if (color_prefix)
        fprintf(stream, "%s", color_prefix);

    va_start(args, msg);
    vfprintf(stream, msg, args);
    va_end(args);

    if (color_suffix)
        fprintf(stream, "%s", color_suffix);

    // [FG] no newline if format string has trailing space
    if (msglen && msg[msglen - 1] != ' ')
    {
        fprintf(stream, "%s", "\n");
        whole_line = true;
    }
    else
    {
        whole_line = false;
    }
}

void I_PutChar(verbosity_t prio, int c)
{
    if (prio <= verbosity)
    {
        putchar(c);

        whole_line = (c == '\n');
    }
}
