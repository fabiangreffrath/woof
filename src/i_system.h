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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include "doomtype.h"

//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//

void I_StartFrame (void);

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.

void I_StartTic (void);

void I_StartDisplay(void);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.

struct ticcmd_s *I_BaseTiccmd(void);

// killough 3/20/98: add const
// killough 4/25/98: add gcc attributes
NORETURN void I_ErrorInternal(const char *prefix, const char *error, ...) PRINTF_ATTR(2, 3);

#ifdef WOOF_DEBUG
  #if defined(_MSC_VER)
    #include <intrin.h>
    #define DoDebugBreak() __debugbreak()
  #else
    #define DoDebugBreak()
  #endif
  boolean I_IsDebuggerAttached(void);
  #define I_Error(...)                          \
    do                                          \
    {                                           \
        if (I_IsDebuggerAttached())             \
        {                                       \
            DoDebugBreak();                     \
        }                                       \
        I_ErrorInternal(__func__, __VA_ARGS__); \
    } while (0)
#else // WOOF_DEBUG
  #define I_Error(...) I_ErrorInternal(__func__, __VA_ARGS__)
#endif

void I_MessageBox(const char *message, ...) PRINTF_ATTR(1, 2);

// Schedule a function to be called when the program exits.
// If run_if_error is true, the function is called if the exit
// is due to an error (I_Error)

typedef enum
{
  exit_priority_first,
  exit_priority_normal,
  exit_priority_last,
  exit_priority_verylast,
  exit_priority_max,
} exit_priority_t;

typedef void (*atexit_func_t)(void);
void I_AtExitPrio(atexit_func_t func, boolean run_if_error,
                  const char* name, exit_priority_t priority);
#define I_AtExit(a,b) I_AtExitPrio(a,b,#a,exit_priority_normal)
NORETURN void I_SafeExit(int rc);
void I_ErrorMsg(void);

void *I_Realloc(void *ptr, size_t size);

boolean I_GetMemoryValue(unsigned int offset, void *value, int size);

const char *I_GetPlatform(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: i_system.h,v $
// Revision 1.7  1998/05/03  22:33:43  killough
// beautification, remove unnecessary #includes
//
// Revision 1.6  1998/04/27  01:52:47  killough
// Add __attribute__ to I_Error for gcc checking
//
// Revision 1.5  1998/04/10  06:34:07  killough
// Add adaptive gametic timer
//
// Revision 1.4  1998/03/23  03:17:19  killough
// Add keyboard FIFO queue and make I_Error arg const
//
// Revision 1.3  1998/02/23  04:28:30  killough
// Add ENDOOM support
//
// Revision 1.2  1998/01/26  19:26:59  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
