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

#include "SDL.h" // haleyjd

#include "z_zone.h"
#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"

// haleyjd: SDL init flags
#define BASE_INIT_FLAGS SDL_INIT_VIDEO

#ifdef _DEBUG
#define INIT_FLAGS (BASE_INIT_FLAGS | SDL_INIT_NOPARACHUTE)
#else
#define INIT_FLAGS BASE_INIT_FLAGS
#endif

int main(int argc, char **argv)
{
   myargc = argc;
   myargv = argv;

   // haleyjd: init SDL
   if(SDL_Init(INIT_FLAGS) == -1)
   {
      printf("Failed to initialize SDL library: %s\n", SDL_GetError());
      return -1;
   }
      
   /*
     killough 1/98:
   
     This fixes some problems with exit handling
     during abnormal situations.
    
     The old code called I_Quit() to end program,
     while now I_Quit() is installed as an exit
     handler and exit() is called to exit, either
     normally or abnormally. Seg faults are caught
     and the error handler is used, to prevent
     being left in graphics mode or having very
     loud SFX noise because the sound card is
     left in an unstable state.
   */
   
   Z_Init();                  // 1/18/98 killough: start up memory stuff first
   atexit(SDL_Quit);
   atexit(I_Quit);
   
   // 2/2/98 Stan
   // Must call this here.  It's required by both netgames and i_video.c.
   
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
