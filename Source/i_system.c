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

// haleyjd
#ifdef _MSC_VER
#include <conio.h>
#include <stdarg.h>
#endif

#ifndef _WIN32
#include <unistd.h> // [FG] isatty()
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
#include "i_endoom.h"

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

// [FG] let the CPU sleep if there is no tic to proceed

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

// Same as I_GetTime, but returns time in milliseconds

static Uint32 basetime = 0;

int I_GetTimeMS(void)
{
  return SDL_GetTicks() - basetime;
}

// Most of the following has been rewritten by Lee Killough
//
// I_GetTime
//

int I_GetTime_RealTime(void)
{
  return (int64_t)I_GetTimeMS() * TICRATE / 1000;
}

// killough 4/13/98: Make clock rate adjustable by scale factor
int realtic_clock_rate = 100;
int clock_rate;

static int I_GetTime_Scaled(void)
{
  return (int64_t)I_GetTimeMS() * clock_rate * TICRATE / 100000;
}

static int I_GetTime_FastDemo(void)
{
  static int fasttic;
  return fasttic++;
}

static int I_GetTime_Error(void)
{
  I_Error("Error: GetTime() used before initialization");
  return 0;
}

int (*I_GetTime)() = I_GetTime_Error;                           // killough

// During a fast demo, no time elapses in between ticks
static int I_GetFracTimeFastDemo(void)
{
  return 0;
}

static int I_GetFracRealTime(void)
{
  return (int64_t)I_GetTimeMS() * TICRATE % 1000 * FRACUNIT / 1000;
}

static int I_GetFracScaledTime(void)
{
  return (int64_t)I_GetTimeMS() * clock_rate * TICRATE / 100 % 1000 * FRACUNIT / 1000;
}

int (*I_GetFracTime)(void) = I_GetFracRealTime;

int controllerpresent;                                         // phares 4/3/98

int leds_always_off;         // Tells it not to update LEDs

// pointer to current joystick device information
SDL_GameController *controller = NULL;

static SDL_Keymod oldmod; // haleyjd: save old modifier key state

static void I_ShutdownJoystick(void)
{
    if (controller != NULL)
    {
        SDL_GameControllerClose(controller);
        controller = NULL;
    }

    if (controllerpresent)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        controllerpresent = false;
    }
}

void I_Shutdown(void)
{
   SDL_SetModState(oldmod);

   I_ShutdownJoystick();
}

extern int usejoystick;

void I_InitJoystick(void)
{
    int i;

    if (!usejoystick)
    {
        I_ShutdownJoystick();
        return;
    }

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
    {
        printf("I_InitJoystick: Failed to initialize game controller: %s\n",
                SDL_GetError());
        return;
    }

    controllerpresent = true;

    // Open the joystick

    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            controller = SDL_GameControllerOpen(i);
            if (controller)
            {
                printf("I_InitJoystick: Found a valid game controller, named: %s\n",
                        SDL_GameControllerName(controller));
                break;
            }
            else
            {
                printf("I_InitJoystick: Could not open game controller %i: %s\n",
                        i, SDL_GetError());
            }
        }
    }

    if (controller == NULL)
    {
        printf("I_InitJoystick: Failed to open game controller.\n");
        I_ShutdownJoystick();
        return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
}

// haleyjd
void I_InitKeyboard(void)
{
   SDL_Keymod   mod;
      
   oldmod = SDL_GetModState();
   if (M_InputMatchKey(input_autorun, KEYD_CAPSLOCK))
      mod = KMOD_CAPS;
   else if (M_InputMatchKey(input_autorun, KEYD_NUMLOCK))
      mod = KMOD_NUM;
   else if (M_InputMatchKey(input_autorun, KEYD_SCROLLLOCK))
      mod = KMOD_MODE;
   else
      mod = KMOD_NONE;
   
   if(autorun)
      SDL_SetModState(mod);
   else
      SDL_SetModState(KMOD_NONE);
}

extern boolean nomusicparm, nosfxparm;
static void I_ErrorMsg();

void I_Init(void)
{
   int p;

   clock_rate = realtic_clock_rate;
   
   if((p = M_CheckParm("-speed")) && p < myargc-1 &&
      (p = atoi(myargv[p+1])) >= 10 && p <= 1000)
      clock_rate = p;
   
   // init timer
   basetime = SDL_GetTicks();

   // killough 4/14/98: Adjustable speedup based on realtic_clock_rate
   if(fastdemo)
   {
      I_GetTime = I_GetTime_FastDemo;
      I_GetFracTime = I_GetFracTimeFastDemo;
   }
   else
      if(clock_rate != 100)
      {
         I_GetTime = I_GetTime_Scaled;
         I_GetFracTime = I_GetFracScaledTime;
      }
      else
      {
         I_GetTime = I_GetTime_RealTime;
         I_GetFracTime = I_GetFracRealTime;
      }

   I_InitJoystick();

  // killough 3/6/98: save keyboard state, initialize shift state and LEDs:

  // killough 3/6/98: end of keyboard / autorun state changes

   I_AtExit(I_Shutdown, true);
   I_AtExitPrio(I_ErrorMsg, true, "I_ErrorMsg", exit_priority_verylast);
   
   // killough 2/21/98: avoid sound initialization if no sound & no music
   { 
      if(!(nomusicparm && nosfxparm))
	 I_InitSound();
   }
}

// [FG] toggle demo warp mode
void I_EnableWarp (boolean warp)
{
	static int (*I_GetTime_old)() = I_GetTime_Error;
	static boolean nodrawers_old, noblit_old;
	static boolean nomusicparm_old, nosfxparm_old;

	if (warp)
	{
		I_GetTime_old = I_GetTime;
		nodrawers_old = nodrawers;
		noblit_old = noblit;
		nomusicparm_old = nomusicparm;
		nosfxparm_old = nosfxparm;

		I_GetTime = I_GetTime_FastDemo;
		nodrawers = true;
		noblit = true;
		nomusicparm = true;
		nosfxparm = true;
	}
	else
	{
		I_GetTime = I_GetTime_old;
		D_StartGameLoop();
		nodrawers = nodrawers_old;
		noblit = noblit_old;
		nomusicparm = nomusicparm_old;
		nosfxparm = nosfxparm_old;
	}
}

//
// I_Quit
//

static char errmsg[2048];    // buffer of error message -- killough
static boolean was_demorecording = false;

void I_Quit (void)
{
   if (!*errmsg && !was_demorecording)
      I_EndDoom();

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   SDL_Quit();
}

void I_QuitFirst (void)
{
   if (demorecording)
   {
      was_demorecording = true;
      G_CheckDemoStatus();
   }
}

void I_QuitLast (void)
{
   M_SaveDefaults();
}

// [FG] returns true if stdout is a real console, false if it is a file

static boolean I_ConsoleStdout(void)
{
#ifdef _WIN32
    // SDL "helpfully" always redirects stdout to a file.
    return false;
#else
    return isatty(fileno(stdout));
#endif
}

//
// I_Error
//

void I_Error(const char *error, ...) // killough 3/20/98: add const
{
    size_t len = sizeof(errmsg) - strlen(errmsg) - 1; // [FG] for '\n'
    char *dest = errmsg + strlen(errmsg);

    va_list argptr;
    va_start(argptr,error);
    vsnprintf(dest,len,error,argptr);
    strcat(dest,"\n");
    va_end(argptr);

    fputs(dest, stderr);

    I_SafeExit(-1);
}

static void I_ErrorMsg()
{
    // Pop up a GUI dialog box to show the error message, if the
    // game was not run from the console (and the user will
    // therefore be unable to otherwise see the message).
    if (*errmsg && !M_CheckParm("-nogui") && !I_ConsoleStdout())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 PROJECT_STRING, errmsg, NULL);
    }
}

// killough 2/22/98: Add support for ENDBOOM, which is PC-specific
// killough 8/1/98: change back to ENDOOM

int show_endoom;

void I_EndDoom(void)
{
    int lumpnum;
    byte *endoom;
    extern boolean main_loop_started;

    // Don't show ENDOOM if we have it disabled.

    if (!show_endoom || !main_loop_started)
    {
        return;
    }

    lumpnum = W_CheckNumForName("ENDOOM");
    if (show_endoom == 2 && W_IsIWADLump(lumpnum))
    {
        return;
    }

    endoom = W_CacheLumpNum(lumpnum, PU_STATIC);

    I_Endoom(endoom);
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
