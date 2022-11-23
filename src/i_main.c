// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_main.c,v 1.8 1998/05/15 00:34:03 killough Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <signal.h>
#include "config.h"

#include "SDL.h" // haleyjd

#include "m_argv.h"
#include "i_system.h"
#include "m_misc2.h"

// Descriptions taken from MSDN
static const struct {
    int sig;
    const char *description;
} sigs[] = {
    { SIGABRT, "Abnormal termination" },
    { SIGFPE,  "Floating-point error" },
    { SIGILL,  "Illegal instruction" },
    { SIGINT,  "CTRL+C signal" },
    { SIGSEGV, "Illegal storage access" },
#ifdef SIGBUS
    { SIGBUS,  "Access to an invalid address" },
#endif
    { SIGTERM, "Termination request" },
};

static void I_SignalHandler(int sig)
{
    char buf[64];
    const char *str = NULL;

    // Ignore future instances of this signal.
    signal(sig, SIG_IGN);

#ifdef HAVE_STRSIGNAL
    str = strsignal(sig);

    if (str == NULL)
#endif
    {
        int i;
        for (i = 0; i < arrlen(sigs); ++i)
        {
            if (sigs[i].sig == sig)
            {
               str = sigs[i].description;
               break;
            }
        }
    }

    if (str)
        M_snprintf(buf, sizeof(buf), "%s (Signal %d)", str, sig);
    else
        M_snprintf(buf, sizeof(buf), "Signal %d", sig);

    I_Error("I_SignalHandler: Exit on %s", buf);
}

static void I_Signal(void)
{
    int i;

    for (i = 0; i < arrlen(sigs); i++)
    {
        signal(sigs[i].sig, I_SignalHandler);
    }
}

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
//

void D_DoomMain(void);

#if defined(WIN_LAUNCHER)
__declspec(dllexport) int Woof_Main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
   myargc = argc;
   myargv = argv;

   //!
   //
   // Print the program version and exit.
   //

   if (M_ParmExists("-version") || M_ParmExists("--version"))
   {
      puts(PROJECT_STRING);
      exit(0);
   }

   I_Signal();

   D_DoomMain();

   return 0;
}

//----------------------------------------------------------------------------
//
// $Log: i_main.c,v $
// Revision 1.8  1998/05/15  00:34:03  killough
// Remove unnecessary crash hack
//
// Revision 1.7  1998/05/13  22:58:04  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.6  1998/05/03  22:38:36  killough
// beautification
//
// Revision 1.5  1998/04/27  02:03:11  killough
// Improve signal handling, to use Z_DumpHistory()
//
// Revision 1.4  1998/03/09  07:10:47  killough
// Allow CTRL-BRK during game init
//
// Revision 1.3  1998/02/03  01:32:58  stan
// Moved __djgpp_nearptr_enable() call from I_video.c to i_main.c
//
// Revision 1.2  1998/01/26  19:23:24  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:57  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
