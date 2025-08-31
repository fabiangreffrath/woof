//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//
//-----------------------------------------------------------------------------

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "d_ticcmd.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"

ticcmd_t *I_BaseTiccmd(void)
{
  static ticcmd_t emptycmd; // killough
  return &emptycmd;
}

//
// I_Error
//

#ifdef WOOF_DEBUG
boolean I_IsDebuggerAttached(void)
{
#ifdef _WIN32
    return IsDebuggerPresent();
#else
    return false;
#endif
}
#endif

static char errmsg[2048];    // buffer of error message -- killough

void I_ErrorInternal(const char *prefix, const char *error, ...)
{
    size_t len = sizeof(errmsg) - strlen(errmsg) - 1; // [FG] for '\n'
    char *curmsg = errmsg + strlen(errmsg);
    char *msgptr = curmsg;

#ifdef WOOF_DEBUG
    if (prefix)
    {
        int offset = M_snprintf(msgptr, len, "%s: ", prefix);
        msgptr += offset;
        len -= offset;
    }
#endif

    va_list argptr;
    va_start(argptr, error);
    M_vsnprintf(msgptr, len, error, argptr);
    va_end(argptr);

    I_Printf(VB_ERROR, "%s", curmsg);
    strcat(curmsg, "\n");

    I_SafeExit(-1);
}

void I_ErrorMsg()
{
    //!
    // @category obscure
    //
    // Don't pop up a GUI dialog box to show the error message.
    //

    if (*errmsg && !M_CheckParm("-nogui") && !I_ConsoleStdout())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, PROJECT_STRING,
                                 errmsg, NULL);
    }
}

void I_MessageBox(const char *message, ...)
{
    char buffer[2048];
    va_list argptr;
    va_start(argptr, message);
    M_vsnprintf(buffer, sizeof(buffer), message, argptr);
    va_end(argptr);

    if (!M_CheckParm("-nogui"))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, PROJECT_STRING,
                                 buffer, NULL);
    }
    else
    {
        I_Printf(VB_INFO, "%s", buffer);
    }
}

// Schedule a function to be called when the program exits.
// If run_if_error is true, the function is called if the exit
// is due to an error (I_Error)

typedef struct atexit_listentry_s atexit_listentry_t;

struct atexit_listentry_s
{
    atexit_func_t func;
    boolean run_on_error;
    atexit_listentry_t *next;
    const char *name;
};

static atexit_listentry_t *exit_funcs[exit_priority_max];
static exit_priority_t exit_priority;

void I_AtExitPrio(atexit_func_t func, boolean run_on_error,
                  const char *name, exit_priority_t priority)
{
    atexit_listentry_t *entry;

    entry = malloc(sizeof(*entry));

    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = exit_funcs[priority];
    entry->name = name;
    exit_funcs[priority] = entry;
}

// I_SafeExit
// This function is called instead of exit() by functions that might be called
// during the exit process (i.e. after exit() has already been called)

void I_SafeExit(int rc)
{
    atexit_listentry_t *entry;

    // Run through all exit functions

    for (; exit_priority < exit_priority_max; ++exit_priority)
    {
        while ((entry = exit_funcs[exit_priority]))
        {
            exit_funcs[exit_priority] = exit_funcs[exit_priority]->next;

            if (rc == 0 || entry->run_on_error)
            {
                I_Printf(VB_DEBUG, "Exit Sequence[%d]: %s (%d)",
                         exit_priority, entry->name, rc);
                entry->func();
            }
        }
    }

#if defined(WIN_LAUNCHER)
    ExitProcess(rc);
#else
    exit(rc);
#endif
}

//
// I_Realloc
//

void *I_Realloc(void *ptr, size_t size)
{
    void *new_ptr;

    new_ptr = realloc(ptr, size);

    if (size != 0 && new_ptr == NULL)
    {
        I_Error("failed on reallocation");
    }

    return new_ptr;
}

//
// Read Access Violation emulation.
//
// From PrBoom+, by entryway.
//

// C:\>debug
// -d 0:0
//
// DOS 6.22:
// 0000:0000  (57 92 19 00) F4 06 70 00-(16 00)
// DOS 7.1:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// Win98:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// DOSBox under XP:
// 0000:0000  (00 00 00 F1) ?? ?? ?? 00-(07 00)

#define DOS_MEM_DUMP_SIZE 10

static const unsigned char mem_dump_dos622[DOS_MEM_DUMP_SIZE] = {
  0x57, 0x92, 0x19, 0x00, 0xF4, 0x06, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_win98[DOS_MEM_DUMP_SIZE] = {
  0x9E, 0x0F, 0xC9, 0x00, 0x65, 0x04, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_dosbox[DOS_MEM_DUMP_SIZE] = {
  0x00, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
static unsigned char mem_dump_custom[DOS_MEM_DUMP_SIZE];

static const unsigned char *dos_mem_dump = mem_dump_dos622;

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    static boolean firsttime = true;

    if (firsttime)
    {
        int p, i, val;

        firsttime = false;

        //!
        // @category compat
        // @arg <version>
        //
        // Specify DOS version to emulate for NULL pointer dereference
        // emulation.  Supported versions are: dos622, dos71, dosbox.
        // The default is to emulate DOS 7.1 (Windows 98).
        //

        p = M_CheckParmWithArgs("-setmem", 1);

        if (p > 0)
        {
            if (!strcasecmp(myargv[p + 1], "dos622"))
            {
                dos_mem_dump = mem_dump_dos622;
            }
            if (!strcasecmp(myargv[p + 1], "dos71"))
            {
                dos_mem_dump = mem_dump_win98;
            }
            else if (!strcasecmp(myargv[p + 1], "dosbox"))
            {
                dos_mem_dump = mem_dump_dosbox;
            }
            else
            {
                for (i = 0; i < DOS_MEM_DUMP_SIZE; ++i)
                {
                    ++p;

                    if (p >= myargc || myargv[p][0] == '-')
                    {
                        break;
                    }

                    if (!M_StrToInt(myargv[p], &val))
                    {
                        I_Error("Invalid parameter '%s' for -setmem, "
                                "valid values are dos622, dos71, dosbox "
                                "or memory dump (up to 10 bytes).", myargv[p]);
                    }

                    mem_dump_custom[i++] = (unsigned char) val;
                }

                dos_mem_dump = mem_dump_custom;
            }
        }
    }

    switch (size)
    {
    case 1:
        *((unsigned char *) value) = dos_mem_dump[offset];
        return true;
    case 2:
        *((unsigned short *) value) = dos_mem_dump[offset]
                                    | (dos_mem_dump[offset + 1] << 8);
        return true;
    case 4:
        *((unsigned int *) value) = dos_mem_dump[offset]
                                  | (dos_mem_dump[offset + 1] << 8)
                                  | (dos_mem_dump[offset + 2] << 16)
                                  | (dos_mem_dump[offset + 3] << 24);
        return true;
    }

    return false;
}

const char *I_GetPlatform(void)
{
    return SDL_GetPlatform();
}

//----------------------------------------------------------------------------
//
// $Log: i_system.c,v $
// Revision 1.14  1998/05/03  22:33:13  killough
// beautification
//
// Revision 1.13  1998/04/27  01:51:37  killough
// Increase errmsg size to 2048
//
// Revision 1.12  1998/04/14  08:13:39  killough
// Replace adaptive gametics with realtic_clock_rate
//
// Revision 1.11  1998/04/10  06:33:46  killough
// Add adaptive gametic timer
//
// Revision 1.10  1998/04/05  00:51:06  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.9  1998/04/02  05:02:31  jim
// Added ENDOOM, BOOM.TXT mods
//
// Revision 1.8  1998/03/23  03:16:13  killough
// Change to use interrupt-driver keyboard IO
//
// Revision 1.7  1998/03/18  16:17:32  jim
// Change to avoid Allegro key shift handling bug
//
// Revision 1.6  1998/03/09  07:12:21  killough
// Fix capslock bugs
//
// Revision 1.5  1998/03/03  00:21:41  jim
// Added predefined ENDBETA lump for beta test
//
// Revision 1.4  1998/03/02  11:31:14  killough
// Fix ENDOOM message handling
//
// Revision 1.3  1998/02/23  04:28:14  killough
// Add ENDOOM support, allow no sound FX at all
//
// Revision 1.2  1998/01/26  19:23:29  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
