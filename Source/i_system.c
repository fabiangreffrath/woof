// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_system.c,v 1.14 1998/05/03 22:33:13 killough Exp $
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
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_system.c,v 1.14 1998/05/03 22:33:13 killough Exp $";

// haleyjd
#ifdef _MSC_VER
#include <conio.h>
#include <stdarg.h>
#endif

#include "SDL.h"

#include "z_zone.h"
#include "i_system.h"
#include "i_sound.h"
#include "doomstat.h"
#include "m_misc.h"
#include "g_game.h"
#include "w_wad.h"
#include "v_video.h"
#include "m_argv.h"

ticcmd_t *I_BaseTiccmd(void)
{
  static ticcmd_t emptycmd; // killough
  return &emptycmd;
}

void I_WaitVBL(int count)
{
   // haleyjd
   SDL_Delay((count*500)/TICRATE);
}

// Most of the following has been rewritten by Lee Killough
//
// I_GetTime
//

static Uint32 basetime=0;

int I_GetTime_RealTime(void)
{
   // haleyjd
   Uint32        ticks;
   
   // milliseconds since SDL initialization
   ticks = SDL_GetTicks();
   
   return ((ticks - basetime)*TICRATE)/1000;
}

// killough 4/13/98: Make clock rate adjustable by scale factor
int realtic_clock_rate = 100;
static Long64 I_GetTime_Scale = 1<<24;
int I_GetTime_Scaled(void)
{
   // haleyjd:
   return (int)((Long64) I_GetTime_RealTime() * I_GetTime_Scale >> 24);
}

static int  I_GetTime_FastDemo(void)
{
  static int fasttic;
  return fasttic++;
}

static int I_GetTime_Error()
{
  I_Error("Error: GetTime() used before initialization");
  return 0;
}

int (*I_GetTime)() = I_GetTime_Error;                           // killough

int mousepresent;
int joystickpresent;                                         // phares 4/3/98

static int orig_key_shifts;  // killough 3/6/98: original keyboard shift state
extern int autorun;          // Autorun state
int leds_always_off;         // Tells it not to update LEDs

// haleyjd: SDL joystick support

// current device number -- saved in config file
int i_SDLJoystickNum = -1;
 
// pointer to current joystick device information
SDL_Joystick *sdlJoystick = NULL;

static SDLMod oldmod; // haleyjd: save old modifier key state

void I_Shutdown(void)
{
   SDL_SetModState(oldmod);
   
   // haleyjd 04/15/02: shutdown joystick
   if(joystickpresent && sdlJoystick && i_SDLJoystickNum >= 0)
   {
      if(SDL_JoystickOpened(i_SDLJoystickNum))
         SDL_JoystickClose(sdlJoystick);
      
      joystickpresent = false;
   }

   SDL_Quit();
}

//
// I_SetJoystickDevice
//
// haleyjd
//
boolean I_SetJoystickDevice(int deviceNum)
{
   if(deviceNum >= SDL_NumJoysticks())
      return false;
   else
   {
      sdlJoystick = SDL_JoystickOpen(deviceNum);

      if(!sdlJoystick)
	 return false;

      // check that the device has at least 2 axes and
      // 4 buttons
      if(SDL_JoystickNumAxes(sdlJoystick) < 2 ||
	 SDL_JoystickNumButtons(sdlJoystick) < 4)
	 return false;

      return true;
   }
}

// haleyjd
void I_InitKeyboard(void)
{
   SDLMod   mod;
      
   oldmod = SDL_GetModState();
   switch(key_autorun)
   {
   case KEYD_CAPSLOCK:
      mod = KMOD_CAPS;
      break;
   case KEYD_NUMLOCK:
      mod = KMOD_NUM;
      break;
   case KEYD_SCROLLLOCK:
      mod = KMOD_MODE;
      break;
   default:
      mod = KMOD_NONE;
   }
   
   if(autorun)
      SDL_SetModState(mod);
   else
      SDL_SetModState(KMOD_NONE);
}


void I_Init(void)
{
   extern int key_autorun;
   int clock_rate = realtic_clock_rate, p;
   
   if((p = M_CheckParm("-speed")) && p < myargc-1 &&
      (p = atoi(myargv[p+1])) >= 10 && p <= 1000)
      clock_rate = p;
   
   // init timer
   basetime = SDL_GetTicks();

   // killough 4/14/98: Adjustable speedup based on realtic_clock_rate
   if(fastdemo)
      I_GetTime = I_GetTime_FastDemo;
   else
      if(clock_rate != 100)
      {
         I_GetTime_Scale = ((Long64) clock_rate << 24) / 100;
         I_GetTime = I_GetTime_Scaled;
      }
      else
         I_GetTime = I_GetTime_RealTime;

   // haleyjd
   if(i_SDLJoystickNum != -1)
   {
      joystickpresent = I_SetJoystickDevice(i_SDLJoystickNum);
   }
   else
   {
      joystickpresent = false;
   }

  // killough 3/6/98: save keyboard state, initialize shift state and LEDs:

  // killough 3/6/98: end of keyboard / autorun state changes

   atexit(I_Shutdown);
   
   // killough 2/21/98: avoid sound initialization if no sound & no music
   { 
      extern boolean nomusicparm, nosfxparm;
      if(!(nomusicparm && nosfxparm))
	 I_InitSound();
   }
}

int waitAtExit;

//
// I_Quit
//

static char errmsg[2048];    // buffer of error message -- killough

static int has_exited;

void I_Quit (void)
{
   has_exited=1;   /* Prevent infinitely recursive exits -- killough */
   
   if (*errmsg)
      puts(errmsg);   // killough 8/8/98
   else
      I_EndDoom();
   
   if (demorecording)
      G_CheckDemoStatus();
   M_SaveDefaults();

#if defined(MY_SDL_VER)
   // Under Visual C++, the console window likes to rudely slam
   // shut -- this can stop it
   if(*errmsg || waitAtExit)
   {
      puts("Press any key to continue");
      getch();
   }
#endif
}

//
// I_Error
//

void I_Error(const char *error, ...) // killough 3/20/98: add const
{
   if(!*errmsg)   // ignore all but the first message -- killough
   {
      va_list argptr;
      va_start(argptr,error);
      vsprintf(errmsg,error,argptr);
      va_end(argptr);
   }
   
   if(!has_exited)    // If it hasn't exited yet, exit now -- killough
   {
      has_exited=1;   // Prevent infinitely recursive exits -- killough
      exit(-1);
   }
}

// killough 2/22/98: Add support for ENDBOOM, which is PC-specific
// killough 8/1/98: change back to ENDOOM

void I_EndDoom(void)
{
   // haleyjd
   puts("\nWinMBF exiting.\n");
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
