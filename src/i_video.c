// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_video.c,v 1.12 1998/05/03 22:40:35 killough Exp $
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
//      DOOM graphics stuff
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_video.c,v 1.12 1998/05/03 22:40:35 killough Exp $";

#include "SDL.h" // haleyjd

#include "z_zone.h"  /* memory allocation wrappers -- killough */
#include "doomstat.h"
#include "v_video.h"
#include "d_main.h"
#include "m_bbox.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "w_wad.h"
#include "r_draw.h"
#include "am_map.h"
#include "m_menu.h"
#include "wi_stuff.h"
#include "i_video.h"

SDL_Surface *sdlscreen;

/////////////////////////////////////////////////////////////////////////////
//
// JOYSTICK                                                  // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

extern int usejoystick;
extern int joystickpresent;
extern int joy_x,joy_y;
extern int joy_b1,joy_b2,joy_b3,joy_b4;

// I_JoystickEvents() gathers joystick data and creates an event_t for
// later processing by G_Responder().

int joystickSens_x;
int joystickSens_y;

extern SDL_Joystick *sdlJoystick;

void I_JoystickEvents(void)
{
   // haleyjd 04/15/02: SDL joystick support

   event_t event;
   int joy_b1, joy_b2, joy_b3, joy_b4;
   Sint16 joy_x, joy_y;
   static int old_joy_b1, old_joy_b2, old_joy_b3, old_joy_b4;
   
   if(!joystickpresent || !usejoystick || !sdlJoystick)
      return;
   
   SDL_JoystickUpdate(); // read the current joystick settings
   event.type = ev_joystick;
   event.data1 = 0;
   
   // read the button settings
   if((joy_b1 = SDL_JoystickGetButton(sdlJoystick, 0)))
      event.data1 |= 1;
   if((joy_b2 = SDL_JoystickGetButton(sdlJoystick, 1)))
      event.data1 |= 2;
   if((joy_b3 = SDL_JoystickGetButton(sdlJoystick, 2)))
      event.data1 |= 4;
   if((joy_b4 = SDL_JoystickGetButton(sdlJoystick, 3)))
      event.data1 |= 8;
   
   // Read the x,y settings. Convert to -1 or 0 or +1.
   joy_x = SDL_JoystickGetAxis(sdlJoystick, 0);
   joy_y = SDL_JoystickGetAxis(sdlJoystick, 1);
   
   if(joy_x < -joystickSens_x)
      event.data2 = -1;
   else if(joy_x > joystickSens_x)
      event.data2 = 1;
   else
      event.data2 = 0;

   if(joy_y < -joystickSens_y)
      event.data3 = -1;
   else if(joy_y > joystickSens_y)
      event.data3 = 1;
   else
      event.data3 = 0;
   
   // post what you found
   
   D_PostEvent(&event);
}


//
// I_StartFrame
//
void I_StartFrame(void)
{
   static boolean firstframe = true;
   
   // haleyjd 02/23/04: turn mouse event processing on
   if(firstframe)
   {
      SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
      firstframe = false;
   }
   
   I_JoystickEvents(); // Obtain joystick data                 phares 4/3/98
}

/////////////////////////////////////////////////////////////////////////////
//
// END JOYSTICK                                              // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse? (int type for config code)
int grabmouse = 1;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible;
static boolean window_focused;
static boolean fullscreen;

//
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
static boolean MouseShouldBeGrabbed(void)
{
   // if the window doesnt have focus, never grab it
   if(!window_focused)
      return false;
   
   // always grab the mouse when full screen (dont want to 
   // see the mouse pointer)
   if(fullscreen)
      return true;
   
   // if we specify not to grab the mouse, never grab
   if(!grabmouse)
      return false;
   
   // when menu is active or game is paused, release the mouse 
   if(menuactive || paused)
      return false;
   
   // only grab mouse when playing levels (but not demos)
   return (gamestate == GS_LEVEL) && !demoplayback;
}

//
// UpdateFocus
// 
// haleyjd 10/08/05: From Chocolate Doom
// Update the value of window_focused when we get a focus event
//
// We try to make ourselves be well-behaved: the grab on the mouse
// is removed if we lose focus (such as a popup window appearing),
// and we dont move the mouse around if we aren't focused either.
//
static void UpdateFocus(void)
{
   Uint8 state = SDL_GetAppState();
   
   // We should have input (keyboard) focus and be visible 
   // (not minimised)   
   window_focused = (state & SDL_APPINPUTFOCUS) && (state & SDL_APPACTIVE);
   
   // Should the screen be grabbed?   
   screenvisible = (state & SDL_APPACTIVE) != 0;
}

// 
// UpdateGrab
//
// haleyjd 10/08/05: from Chocolate DOOM
//
static void UpdateGrab(void)
{
   static boolean currently_grabbed = false;
   boolean grab;
   
   grab = MouseShouldBeGrabbed();
   
   if(grab && !currently_grabbed)
   {
      SDL_ShowCursor(SDL_DISABLE);
      SDL_WM_GrabInput(SDL_GRAB_ON);
   }
   
   if(!grab && currently_grabbed)
   {
      SDL_ShowCursor(SDL_ENABLE);
      SDL_WM_GrabInput(SDL_GRAB_OFF);
   }
   
   currently_grabbed = grab;   
}

//
// Keyboard routines
// By Lee Killough
// Based only a little bit on Chi's v0.2 code
//

extern void I_InitKeyboard();      // i_system.c

//
// I_TranslateKey
//
// haleyjd
// For SDL, translates from SDL keysyms to DOOM key values.
//
static int I_TranslateKey(int sym)
{
   int rc = 0;
   switch (sym) 
   {  
   case SDLK_LEFT:     rc = KEYD_LEFTARROW;  break;
   case SDLK_RIGHT:    rc = KEYD_RIGHTARROW; break;
   case SDLK_DOWN:     rc = KEYD_DOWNARROW;  break;
   case SDLK_UP:       rc = KEYD_UPARROW;    break;
   case SDLK_ESCAPE:   rc = KEYD_ESCAPE;     break;
   case SDLK_RETURN:   rc = KEYD_ENTER;      break;
   case SDLK_TAB:      rc = KEYD_TAB;        break;
   case SDLK_F1:       rc = KEYD_F1;         break;
   case SDLK_F2:       rc = KEYD_F2;         break;
   case SDLK_F3:       rc = KEYD_F3;         break;
   case SDLK_F4:       rc = KEYD_F4;         break;
   case SDLK_F5:       rc = KEYD_F5;         break;
   case SDLK_F6:       rc = KEYD_F6;         break;
   case SDLK_F7:       rc = KEYD_F7;         break;
   case SDLK_F8:       rc = KEYD_F8;         break;
   case SDLK_F9:       rc = KEYD_F9;         break;
   case SDLK_F10:      rc = KEYD_F10;        break;
   case SDLK_F11:      rc = KEYD_F11;        break;
   case SDLK_F12:      rc = KEYD_F12;        break;
   case SDLK_BACKSPACE:
   case SDLK_DELETE:   rc = KEYD_BACKSPACE;  break;
   case SDLK_PAUSE:    rc = KEYD_PAUSE;      break;
   case SDLK_EQUALS:   rc = KEYD_EQUALS;     break;
   case SDLK_MINUS:    rc = KEYD_MINUS;      break;
      
   case SDLK_NUMLOCK:    rc = KEYD_NUMLOCK;  break;
   case SDLK_SCROLLOCK:  rc = KEYD_SCROLLLOCK; break;
   case SDLK_CAPSLOCK:   rc = KEYD_CAPSLOCK; break;
   case SDLK_LSHIFT:
   case SDLK_RSHIFT:     rc = KEYD_RSHIFT;   break;
   case SDLK_LCTRL:
   case SDLK_RCTRL:      rc = KEYD_RCTRL;    break;
      
   case SDLK_LALT:
   case SDLK_RALT:
   case SDLK_LMETA:
   case SDLK_RMETA:      rc = KEYD_RALT;     break;
   case SDLK_PAGEUP:     rc = KEYD_PAGEUP;   break;
   case SDLK_PAGEDOWN:   rc = KEYD_PAGEDOWN; break;
   case SDLK_HOME:       rc = KEYD_HOME;     break;
   case SDLK_END:        rc = KEYD_END;      break;
   case SDLK_INSERT:     rc = KEYD_INSERT;   break;
   default:
      rc = sym;
      break;
   }
   return rc;
}

int I_ScanCode2DoomCode (int a)
{
   // haleyjd
   return a;
}

// Automatic caching inverter, so you don't need to maintain two tables.
// By Lee Killough

int I_DoomCode2ScanCode (int a)
{
   // haleyjd
   return a;
}

// killough 3/22/98: rewritten to use interrupt-driven keyboard queue

extern int usemouse;

void I_GetEvent()
{
   SDL_Event event;
   event_t   d_event;   
   
   event_t mouseevent = { ev_mouse, 0, 0, 0 };
   static int buttons = 0;
   
   int sendmouseevent = 0;
   
   while(SDL_PollEvent(&event))
   {
      // haleyjd 10/08/05: from Chocolate DOOM
      if(!window_focused && 
         (event.type == SDL_MOUSEMOTION || 
          event.type == SDL_MOUSEBUTTONDOWN || 
          event.type == SDL_MOUSEBUTTONUP))
      {
         continue;
      }

      switch(event.type)
      {
      case SDL_KEYDOWN:
         d_event.type = ev_keydown;
         d_event.data1 = I_TranslateKey(event.key.keysym.sym);
         // haleyjd 08/29/03: don't post out-of-range keys
         if(d_event.data1 > 0 && d_event.data1 < 256)
            D_PostEvent(&d_event);
         break;
      case SDL_KEYUP:
         d_event.type = ev_keyup;
         d_event.data1 = I_TranslateKey(event.key.keysym.sym);
         // haleyjd 08/29/03: don't post out-of-range keys
         if(d_event.data1 > 0 && d_event.data1 < 256)
            D_PostEvent(&d_event);
         break;
      case SDL_MOUSEMOTION:       
         if(!usemouse)
            continue;

         // SoM 1-20-04 Ok, use xrel/yrel for mouse movement because most people like it the most.
         mouseevent.data3 -= event.motion.yrel;
         mouseevent.data2 += event.motion.xrel;
         sendmouseevent = 1;
         break;
      case SDL_MOUSEBUTTONUP:
         if(!usemouse)
            continue;
         sendmouseevent = 1;
         d_event.type = ev_keyup;
         if(event.button.button == SDL_BUTTON_LEFT)
         {
            buttons &= ~1;
            d_event.data1 = buttons;
         }
         else if(event.button.button == SDL_BUTTON_MIDDLE)
         {
            buttons &= ~4;
            d_event.data1 = buttons;
         }
         else
         {
            buttons &= ~2;
            d_event.data1 = buttons;
         }
         D_PostEvent(&d_event);
         break;
      case SDL_MOUSEBUTTONDOWN:
         if(!usemouse)
            continue;
         sendmouseevent = 1;
         d_event.type = ev_keydown;
         if(event.button.button == SDL_BUTTON_LEFT)
         {
            buttons |= 1;
            d_event.data1 = buttons;
         }
         else if(event.button.button == SDL_BUTTON_MIDDLE)
         {
            buttons |= 4;
            d_event.data1 = buttons;
         }
         else
         {
            buttons |= 2;
            d_event.data1 = buttons;
         }
         D_PostEvent(&d_event);
         break;

      case SDL_QUIT:
         exit(0);
         break;

      case SDL_ACTIVEEVENT:
         // haleyjd 10/08/05: from Chocolate DOOM:
         // need to update our focus state
         UpdateFocus();
         break;

      default:
         break;
      }
   }

   if(sendmouseevent)
   {
      mouseevent.data1 = buttons;
      D_PostEvent(&mouseevent);
   }

   if(paused || !window_focused)
      SDL_Delay(1);
}

//
// I_StartTic
//

void I_StartTic()
{
  I_GetEvent();
}

//
// I_UpdateNoBlit
//

void I_UpdateNoBlit (void)
{
}


int use_vsync;     // killough 2/8/98: controls whether vsync is called
int page_flip;     // killough 8/15/98: enables page flipping
int hires;
boolean noblit;

// haleyjd 05/11/09: low-level video scaling
static int real_width;
static int real_height;
static void (*i_scalefunc)(Uint8 *, SDL_Surface *);

static int in_graphics_mode;
static int in_page_flip, in_hires, linear;
static int scroll_offset;
static unsigned long screen_base_addr;
static unsigned destscreen;

void I_FinishUpdate(void)
{
   // haleyjd
   int pitch1;
   int pitch2;
   boolean locked = false;

   static int v_width;
   static int v_height;

   if (noblit || !in_graphics_mode)
      return;

   // haleyjd 10/08/05: from Chocolate DOOM:

   UpdateGrab();

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetAppState() & SDL_APPACTIVE))
      return;

   v_width  = in_hires ? 640 : 320;
   v_height = in_hires ? 400 : 200;
   
   // draws little dots on the bottom of the screen
   if(devparm && !i_scalefunc)
   {
      static int lasttic;
      byte *s = screens[0];
      
      int i = I_GetTime();
      int tics = i - lasttic;
      lasttic = i;
      if (tics > 20)
         tics = 20;
      if (in_hires)    // killough 11/98: hires support
      {
         for (i=0 ; i<tics*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+1] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+SCREENWIDTH*2] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+SCREENWIDTH*2+1] =
            0xff;
         for ( ; i<20*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+1] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+SCREENWIDTH*2] =
            s[(SCREENHEIGHT-1)*SCREENWIDTH*4+i+SCREENWIDTH*2+1] =
            0x0;
      }
      else
      {
         for (i=0 ; i<tics*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
         for ( ; i<20*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
      }
   }

   // haleyjd 04/11/03: make sure the surface is locked
   // 01/05/04: patched by schepe to stop crashes w/ alt+tab
   if(SDL_MUSTLOCK(sdlscreen))
   {
      // Must drop out if lock fails!
      // SDL docs are very vague about this.
      if(SDL_LockSurface(sdlscreen) == -1)
         return;
      
      locked = true;      
   }
   
   if(i_scalefunc)
   {
      i_scalefunc(screens[0], sdlscreen);
   }
   else
   {
      pitch1 = sdlscreen->pitch;
      pitch2 = sdlscreen->w * sdlscreen->format->BytesPerPixel;
      
      // SoM 3/14/2002: Only one way to go about this without asm code
      if(pitch1 == pitch2)
         memcpy(sdlscreen->pixels, screens[0], v_width * v_height);
      else
      {
         int y = v_height;
         
         // SoM: optimized a bit
         while(--y >= 0)
            memcpy((char *)sdlscreen->pixels + (y * pitch1), screens[0] + (y * pitch2), pitch2);
      }
   }

   
   if(locked)
      SDL_UnlockSurface(sdlscreen);
   
   SDL_Flip(sdlscreen);
}

//
// I_ReadScreen
//

void I_ReadScreen(byte *scr)
{
   int size = hires ? SCREENWIDTH*SCREENHEIGHT*4 : SCREENWIDTH*SCREENHEIGHT;

   // haleyjd
   memcpy(scr, *screens, size);
}

//
// killough 10/98: init disk icon
//

int disk_icon;

#if 0
static BITMAP *diskflash, *old_data;
#endif

static void I_InitDiskFlash(void)
{
#if 0
  byte temp[32*32];

  if (diskflash)
    {
      destroy_bitmap(diskflash);
      destroy_bitmap(old_data);
    }

  diskflash = create_bitmap_ex(8, 16<<hires, 16<<hires);
  old_data = create_bitmap_ex(8, 16<<hires, 16<<hires);

  V_GetBlock(0, 0, 0, 16, 16, temp);
  V_DrawPatchDirect(0, 0, 0, W_CacheLumpName(M_CheckParm("-cdrom") ?
                                             "STCDROM" : "STDISK", PU_CACHE));
  V_GetBlock(0, 0, 0, 16, 16, diskflash->line[0]);
  V_DrawBlock(0, 0, 0, 16, 16, temp);
#endif
}

//
// killough 10/98: draw disk icon
//

void I_BeginRead(void)
{
#if 0
  if (!disk_icon || !in_graphics_mode)
    return;

  blit(screen, old_data,
       (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-16)<<hires),
       0, 0, 16 << hires, 16 << hires);

  blit(diskflash, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-16)<<hires), 16 << hires, 16 << hires);
#endif
}

//
// killough 10/98: erase disk icon
//

void I_EndRead(void)
{
#if 0
  if (!disk_icon || !in_graphics_mode)
    return;

  blit(old_data, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-16)<<hires), 16 << hires, 16 << hires);
#endif
}

void I_SetPalette(byte *palette)
{
   // haleyjd
   int i;
   SDL_Color colors[256];
   
   if(!in_graphics_mode)             // killough 8/11/98
      return;
   
   for(i = 0; i < 256; ++i)
   {
      colors[i].r = gammatable[usegamma][*palette++];
      colors[i].g = gammatable[usegamma][*palette++];
      colors[i].b = gammatable[usegamma][*palette++];
   }
   
   SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
}

void I_ShutdownGraphics(void)
{
   if(in_graphics_mode)  // killough 10/98
   {
      UpdateGrab();      
      in_graphics_mode = false;
      sdlscreen = NULL;
   }
}

extern boolean setsizeneeded;

extern void I_InitKeyboard();

int cfg_scalefactor; // haleyjd 05/11/09: scale factor in config
int cfg_aspectratio; // haleyjd 05/11/09: aspect ratio correction

// haleyjd 05/11/09: scaler funcs
extern void I_InitStretchTables(byte *);
extern void I_GetScaler(int, int, int, int, int *, int *, 
                        void (**f)(Uint8 *, SDL_Surface *));

// haleyjd 05/11/09: true if called from I_ResetScreen
static boolean changeres = false;

//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitGraphicsMode(void)
{
   static boolean firsttime = true;
   
   // haleyjd
   int v_w = SCREENWIDTH;
   int v_h = SCREENHEIGHT;
   int flags = SDL_SWSURFACE;
   int scalefactor = cfg_scalefactor;
   int usehires = hires;
   int useaspect = cfg_aspectratio;

   if(firsttime)
   {
      I_InitKeyboard();
      firsttime = false;

      if(M_CheckParm("-hires"))
         usehires = hires = true;
      else if(M_CheckParm("-nohires"))
         usehires = hires = false; // grrr...
   }

   if(usehires)
   {
      v_w = SCREENWIDTH*2;
      v_h = SCREENHEIGHT*2;
   }

   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing   
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync || page_flip)
      flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

   // haleyjd: fullscreen support
   if(M_CheckParm("-fullscreen"))
   {
      fullscreen = true; // 5/11/09: forgotten O_O
      flags |= SDL_FULLSCREEN;
   }

   if(M_CheckParm("-1"))
      scalefactor = 1;
   else if(M_CheckParm("-2"))
      scalefactor = 2;
   else if(M_CheckParm("-3"))
      scalefactor = 3;
   else if(M_CheckParm("-4"))
      scalefactor = 4;
   else if(M_CheckParm("-5"))
      scalefactor = 5;

   if(M_CheckParm("-aspect"))
      useaspect = true;
   
   // haleyjd 05/11/09: try to get a scaler
   if(scalefactor > 1 || useaspect)
   {
      static boolean initstretch = true;
      
      // haleyjd 05/11/09: initialize scaling
      if(initstretch)
      {
         // only do this once
         byte *palette = W_CacheLumpName("PLAYPAL", PU_STATIC);
         I_InitStretchTables(palette);
         Z_ChangeTag(palette, PU_CACHE);
         initstretch = false;
      }

      I_GetScaler(v_w, v_h, useaspect, scalefactor,
                  &v_w, &v_h, &i_scalefunc);
   }

   if(!(sdlscreen = SDL_SetVideoMode(v_w, v_h, 8, flags)))
   {
      // try 320x200 if initial set fails
      if(v_w != 320 || v_h != 200)
      {
         printf("Failed to set video mode %dx%dx8 %s\n"
                "Attempting to set 320x200x8 windowed mode\n",
                v_w, v_h, fullscreen ? "fullscreen" : "windowed");

         v_w = 320;
         v_h = 200;
         i_scalefunc = NULL;         

         // remove any special flags
         flags = SDL_SWSURFACE;
         fullscreen = false;
         use_vsync = page_flip = usehires = hires = false;

         sdlscreen = SDL_SetVideoMode(v_w, v_h, 8, flags);
      }

      // if still bad, error time.
      if(!sdlscreen)
         I_Error("Failed to set video mode %dx%dx8\n"
                 "Please check your scaling and aspect ratio settings.\n",
                 v_w, v_h);
   }

   if(!changeres)
   {
      printf("I_InitGraphicsMode: set video mode %dx%dx8\n", v_w, v_h);

      if(i_scalefunc)
         printf("   scale factor = %d, aspect ratio correction %s.\n", 
                scalefactor, useaspect ? "on" : "off");
   }
   
   V_Init();

   SDL_WM_SetCaption("WinMBF v2.03 Build 3", NULL);

   UpdateFocus();
   UpdateGrab();

   in_graphics_mode = 1;
   in_page_flip = false;
   in_hires = usehires;
   
   setsizeneeded = true;
   
   I_InitDiskFlash();        // Initialize disk icon   
   I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

void I_ResetScreen(void)
{
   if(!in_graphics_mode)
   {
      setsizeneeded = true;
      V_Init();
      return;
   }

   I_ShutdownGraphics();     // Switch out of old graphics mode
   
   changeres = true; // haleyjd 05/11/09

   I_InitGraphicsMode();     // Switch to new graphics mode
   
   changeres = false;
   
   if(automapactive)
      AM_Start();             // Reset automap dimensions
   
   ST_Start();               // Reset palette
   
   if(gamestate == GS_INTERMISSION)
   {
      WI_DrawBackground();
      V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
   }
   
   Z_CheckHeap();
}

void I_InitGraphics(void)
{
  static int firsttime = 1;

  if(!firsttime)
    return;

  firsttime = 0;

#if 0
  if (nodrawers) // killough 3/2/98: possibly avoid gfx mode
    return;
#endif

  //
  // enter graphics mode
  //

  atexit(I_ShutdownGraphics);

  in_page_flip = page_flip;

  I_InitGraphicsMode();    // killough 10/98

  Z_CheckHeap();
}

//----------------------------------------------------------------------------
//
// $Log: i_video.c,v $
// Revision 1.12  1998/05/03  22:40:35  killough
// beautification
//
// Revision 1.11  1998/04/05  00:50:53  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.10  1998/03/23  03:16:10  killough
// Change to use interrupt-driver keyboard IO
//
// Revision 1.9  1998/03/09  07:13:35  killough
// Allow CTRL-BRK during game init
//
// Revision 1.8  1998/03/02  11:32:22  killough
// Add pentium blit case, make -nodraw work totally
//
// Revision 1.7  1998/02/23  04:29:09  killough
// BLIT tuning
//
// Revision 1.6  1998/02/09  03:01:20  killough
// Add vsync for flicker-free blits
//
// Revision 1.5  1998/02/03  01:33:01  stan
// Moved __djgpp_nearptr_enable() call from I_video.c to i_main.c
//
// Revision 1.4  1998/02/02  13:33:30  killough
// Add support for -noblit
//
// Revision 1.3  1998/01/26  19:23:31  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:59:14  killough
// New PPro blit routine
//
// Revision 1.1.1.1  1998/01/19  14:02:50  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
