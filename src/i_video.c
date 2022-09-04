// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_video.c,v 1.12 1998/05/03 22:40:35 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2021 Fabian Greffrath
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

#include "SDL.h" // haleyjd

#include "../miniz/miniz.h"

#include "z_zone.h"  /* memory allocation wrappers -- killough */
#include "doomstat.h"
#include "doomkeys.h"
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
#include "m_misc2.h"
#include "m_io.h"

// [FG] set the application icon

#include "icon.c"

#ifdef _WIN32
#include "../win32/win_version.h"
#endif

int SCREENWIDTH, SCREENHEIGHT;
int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
int WIDESCREENDELTA; // [crispy] horizontal widescreen offset

static SDL_Surface *sdlscreen;

// [FG] rendering window, renderer, intermediate ARGB frame buffer and texture

static SDL_Window *screen;
static SDL_Renderer *renderer;
static SDL_Surface *argbbuffer;
static SDL_Texture *texture;
static SDL_Rect blit_rect = {0};

int window_width, window_height;
static int window_x, window_y;
char *window_position;
int video_display = 0;
int fullscreen_width = 0, fullscreen_height = 0; // [FG] exclusive fullscreen

void *I_GetSDLWindow(void)
{
    return screen;
}

void *I_GetSDLRenderer(void)
{
    return renderer;
}

/////////////////////////////////////////////////////////////////////////////
//
// JOYSTICK                                                  // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

// I_JoystickEvents() gathers joystick data and creates an event_t for
// later processing by G_Responder().

extern SDL_GameController *controller;

// When an axis is within the dead zone, it is set to zero.
#define DEAD_ZONE (32768 / 3)

#define TRIGGER_THRESHOLD 30 // from xinput.h

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

static int GetAxisState(int axis)
{
    int result;

    result = SDL_GameControllerGetAxis(controller, axis);

    if (result < DEAD_ZONE && result > -DEAD_ZONE)
    {
        result = 0;
    }

    return result;
}

static void AxisToButton(int value, int* state, int direction)
{
  int button = -1;

  if (value < 0)
    button = direction;
  else if (value > 0)
    button = direction + 1;

  if (button != *state)
  {
    if (*state != -1)
    {
        static event_t up;
        up.data1 = *state;
        up.type = ev_joyb_up;
        D_PostEvent(&up);
    }

    if (button != -1)
    {
        static event_t down;
        down.data1 = button;
        down.type = ev_joyb_down;
        D_PostEvent(&down);
    }

    *state = button;
  }
}

int axisbuttons[] = { -1, -1, -1, -1 };

void I_UpdateJoystick(void)
{
    if (controller != NULL)
    {
        static event_t ev;

        ev.type = ev_joystick;
        ev.data1 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
        ev.data2 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
        ev.data3 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
        ev.data4 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

        AxisToButton(ev.data1, &axisbuttons[0], CONTROLLER_LEFT_STICK_LEFT);
        AxisToButton(ev.data2, &axisbuttons[1], CONTROLLER_LEFT_STICK_UP);
        AxisToButton(ev.data3, &axisbuttons[2], CONTROLLER_RIGHT_STICK_LEFT);
        AxisToButton(ev.data4, &axisbuttons[3], CONTROLLER_RIGHT_STICK_UP);

        D_PostEvent(&ev);
    }
}

//
// I_StartFrame
//
void I_StartFrame(void)
{

}

static void UpdateJoystickButtonState(unsigned int button, boolean on)
{
    static event_t event;
    if (on)
    {
        event.type = ev_joyb_down;
    }
    else
    {
        event.type = ev_joyb_up;
    }

    event.data1 = button;
    D_PostEvent(&event);
}

static void UpdateControllerAxisState(unsigned int value, boolean left_trigger)
{
    int button;
    static event_t event;
    static boolean left_trigger_on;
    static boolean right_trigger_on;

    if (left_trigger)
    {
        if (value > TRIGGER_THRESHOLD && !left_trigger_on)
        {
            left_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= TRIGGER_THRESHOLD && left_trigger_on)
        {
            left_trigger_on = false;
            event.type = ev_joyb_up;
        }
        else
        {
            return;
        }

        button = CONTROLLER_LEFT_TRIGGER;
    }
    else
    {
        if (value > TRIGGER_THRESHOLD && !right_trigger_on)
        {
            right_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= TRIGGER_THRESHOLD && right_trigger_on)
        {
            right_trigger_on = false;
            event.type = ev_joyb_up;
        }
        else
        {
            return;
        }

        button = CONTROLLER_RIGHT_TRIGGER;
    }

    event.data1 = button;
    D_PostEvent(&event);
}

static void I_HandleJoystickEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_CONTROLLERBUTTONDOWN:
            UpdateJoystickButtonState(sdlevent->cbutton.button, true);
            break;

        case SDL_CONTROLLERBUTTONUP:
            UpdateJoystickButtonState(sdlevent->cbutton.button, false);
            break;

        case SDL_CONTROLLERAXISMOTION:
            if (sdlevent->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
            {
                UpdateControllerAxisState(sdlevent->caxis.value, true);
            }
            else if (sdlevent->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            {
                UpdateControllerAxisState(sdlevent->caxis.value, false);
            }
            break;

        default:
            break;
    }
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
static boolean window_focused = true;
boolean fullscreen;

//
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
static boolean MouseShouldBeGrabbed(void)
{
   // if the window doesnt have focus, never grab it
   if (!window_focused)
      return false;
   
   // always grab the mouse when full screen (dont want to 
   // see the mouse pointer)
   if (fullscreen)
      return true;
   
   // if we specify not to grab the mouse, never grab
   if (!grabmouse)
      return false;
   
   // when menu is active or game is paused, release the mouse 
   if (menuactive || paused)
      return false;
   
   // only grab mouse when playing levels (but not demos)
   return (gamestate == GS_LEVEL) && !demoplayback;
}

// [FG] mouse grabbing from Chocolate Doom 3.0

static void SetShowCursor(boolean show)
{
   // When the cursor is hidden, grab the input.
   // Relative mode implicitly hides the cursor.
   SDL_SetRelativeMouseMode(!show);
   SDL_GetRelativeMouseState(NULL, NULL);
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
   
   if (grab && !currently_grabbed)
   {
      SetShowCursor(false);
   }
   
   if (!grab && currently_grabbed)
   {
      int screen_w, screen_h;

      SetShowCursor(true);

      // When releasing the mouse from grab, warp the mouse cursor to
      // the bottom-right of the screen. This is a minimally distracting
      // place for it to appear - we may only have released the grab
      // because we're at an end of level intermission screen, for
      // example.

      SDL_GetWindowSize(screen, &screen_w, &screen_h);
      SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
      SDL_GetRelativeMouseState(NULL, NULL);
   }
   
   currently_grabbed = grab;   
}

//
// I_TranslateKey
//
// haleyjd
// For SDL, translates from SDL keysyms to DOOM key values.
//

// [FG] updated to scancode-based approach from Chocolate Doom 3.0

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

// Translates the SDL key to a value of the type found in doomkeys.h
static int TranslateKey(SDL_Keysym *sym)
{
    int scancode = sym->scancode;

    switch (scancode)
    {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return KEY_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEY_RSHIFT;

        case SDL_SCANCODE_LALT:
            return KEY_LALT;

        case SDL_SCANCODE_RALT:
            return KEY_RALT;

        default:
            if (scancode >= 0 && scancode < arrlen(scancode_translate_table))
            {
                return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }
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

// [FG] mouse button and movement handling from Chocolate Doom 3.0

static void UpdateMouseButtonState(unsigned int button, boolean on, unsigned int dclick)
{
    static event_t event;

    if (button < SDL_BUTTON_LEFT || button > MAX_MB)
    {
        return;
    }

    // Note: button "0" is left, button "1" is right,
    // button "2" is middle for Doom.  This is different
    // to how SDL sees things.

    switch (button)
    {
        case SDL_BUTTON_LEFT:
            button = 0;
            break;

        case SDL_BUTTON_RIGHT:
            button = 1;
            break;

        case SDL_BUTTON_MIDDLE:
            button = 2;
            break;

        default:
            // SDL buttons are indexed from 1.
            --button;
            break;
    }

    // Turn bit representing this button on or off.

    if (on)
    {
        event.type = ev_mouseb_down;
    }
    else
    {
        event.type = ev_mouseb_up;
    }

    // Post an event with the new button state.

    event.data1 = button;
    event.data2 = dclick;
    D_PostEvent(&event);
}

static event_t delay_event;

static void DelayEvent(void)
{
    if (delay_event.data1)
    {
        D_PostEvent(&delay_event);
        delay_event.data1 = 0;
    }
}

static void MapMouseWheelToButtons(SDL_MouseWheelEvent *wheel)
{
    // SDL2 distinguishes button events from mouse wheel events.
    // We want to treat the mouse wheel as two buttons, as per
    // SDL1
    static event_t down;
    int button;

    if (wheel->y <= 0)
    {   // scroll down
        button = MOUSE_BUTTON_WHEELDOWN;
    }
    else
    {   // scroll up
        button = MOUSE_BUTTON_WHEELUP;
    }

    // post a button down event
    down.type = ev_mouseb_down;
    down.data1 = button;
    D_PostEvent(&down);

    // hold button for one tic, required for checks in G_BuildTiccmd
    delay_event.type = ev_mouseb_up;
    delay_event.data1 = button;
}

static void I_HandleMouseEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            UpdateMouseButtonState(sdlevent->button.button, true, sdlevent->button.clicks);
            break;

        case SDL_MOUSEBUTTONUP:
            UpdateMouseButtonState(sdlevent->button.button, false, 0);
            break;

        case SDL_MOUSEWHEEL:
            MapMouseWheelToButtons(&(sdlevent->wheel));
            break;

        default:
            break;
    }
}

// [FG] keyboard event handling from Chocolate Doom 3.0

static void I_HandleKeyboardEvent(SDL_Event *sdlevent)
{
    static event_t event;

    switch (sdlevent->type)
    {
        case SDL_KEYDOWN:
            event.type = ev_keydown;
            event.data1 = TranslateKey(&sdlevent->key.keysym);

            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        case SDL_KEYUP:
            event.type = ev_keyup;
            event.data1 = TranslateKey(&sdlevent->key.keysym);

            // data2/data3 are initialized to zero for ev_keyup.
            // For ev_keydown it's the shifted Unicode character
            // that was typed, but if something wants to detect
            // key releases it should do so based on data1
            // (key ID), not the printable char.

            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        default:
            break;
    }
}

// [FG] window event handling from Chocolate Doom 3.0

static void HandleWindowEvent(SDL_WindowEvent *event)
{
    int i;

    switch (event->event)
    {
        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on and
        // update the video_display config variable.

        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                video_display = i;
            }
            if (!fullscreen)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);
                SDL_GetWindowPosition(screen, &window_x, &window_y);
            }
            break;

        default:
            break;
    }
}

// [FG] fullscreen toggle from Chocolate Doom 3.0

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN ||
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    // [FG] exclusive fullscreen
    if (fullscreen_width != 0 || fullscreen_height != 0)
    {
        return;
    }

    fullscreen = !fullscreen;

    if (fullscreen)
    {
        SDL_GetWindowSize(screen, &window_width, &window_height);
        SDL_GetWindowPosition(screen, &window_x, &window_y);
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
        SDL_SetWindowPosition(screen, window_x, window_y);
    }
}

// [FG] the fullscreen variable gets toggled once by the menu code, so we
// toggle it back here, it is then toggled again in I_ToggleFullScreen()

void I_ToggleToggleFullScreen(void)
{
    // [FG] exclusive fullscreen
    if (fullscreen_width != 0 || fullscreen_height != 0)
    {
        return;
    }

    fullscreen = !fullscreen;

    I_ToggleFullScreen();
}

// killough 3/22/98: rewritten to use interrupt-driven keyboard queue

void I_GetEvent(void)
{
    SDL_Event sdlevent;

    DelayEvent();

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
		I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_CONTROLLERDEVICEADDED:
                I_OpenController(sdlevent.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                I_CloseController(sdlevent.cdevice.which);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERAXISMOTION:
                I_HandleJoystickEvent(&sdlevent);
                break;

            case SDL_QUIT:
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
    }
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.

int mouse_acceleration;
int mouse_threshold; // 10;

static int AccelerateMouse(int val)
{
    if (val < 0)
        return -AccelerateMouse(-val);

    if (val > mouse_threshold)
    {
        return (val - mouse_threshold) * mouse_acceleration / 100 + mouse_threshold;
    }
    else
    {
        return val;
    }
}

// [crispy] Distribute the mouse movement between the current tic and the next
// based on how far we are into the current tic. Compensates for mouse sampling
// jitter.

static void SmoothMouse(int* x, int* y)
{
    static int x_remainder_old = 0;
    static int y_remainder_old = 0;
    int x_remainder, y_remainder;
    fixed_t correction_factor;
    fixed_t fractic;

    *x += x_remainder_old;
    *y += y_remainder_old;

    fractic = I_GetFracTime();
    correction_factor = FixedDiv(fractic, FRACUNIT + fractic);

    x_remainder = FixedMul(*x, correction_factor);
    *x -= x_remainder;
    x_remainder_old = x_remainder;

    y_remainder = FixedMul(*y, correction_factor);
    *y -= y_remainder;
    y_remainder_old = y_remainder;
}

static void I_ReadMouse(void)
{
    int x, y;
    static event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (uncapped)
    {
        SmoothMouse(&x, &y);
    }

    if (x != 0 || y != 0)
    {
        ev.type = ev_mouse;
        ev.data1 = 0;
        ev.data2 = AccelerateMouse(x);
        ev.data3 = -AccelerateMouse(y);

        D_PostEvent(&ev);
    }
}

//
// I_StartTic
//

void I_StartTic (void)
{
/*
    if (!initialized)
    {
        return;
    }
*/
    I_GetEvent();

    if (window_focused)
    {
        I_ReadMouse();
    }

    I_UpdateJoystick();
}

int use_vsync;     // killough 2/8/98: controls whether vsync is called
int page_flip;     // killough 8/15/98: enables page flipping
int hires;

static int in_graphics_mode;

static void I_DrawDiskIcon(), I_RestoreDiskBackground();
static unsigned int disk_to_draw, disk_to_restore;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
fixed_t fractionaltic;

// [FG] aspect ratio correction
int useaspect;
static int actualheight;

int uncapped; // [FG] uncapped rendering frame rate
int integer_scaling; // [FG] force integer scales
int fps; // [FG] FPS counter widget
int widescreen; // widescreen mode

void I_FinishUpdate(void)
{
   if (noblit || !in_graphics_mode)
      return;

   // haleyjd 10/08/05: from Chocolate DOOM:

   UpdateGrab();

   // draws little dots on the bottom of the screen
   if (devparm)
   {
      static int lasttic;
      byte *s = screens[0];
      
      int i = I_GetTime();
      int tics = i - lasttic;
      lasttic = i;
      if (tics > 20)
         tics = 20;
      if (hires)    // killough 11/98: hires support
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

   // [FG] [AM] Real FPS counter
   {
      static int lastmili;
      static int fpscount;
      int i, mili;

      fpscount++;

      i = SDL_GetTicks();
      mili = i - lastmili;

      // Update FPS counter every second
      if (mili >= 1000)
      {
         fps = (fpscount * 1000) / mili;
         fpscount = 0;
         lastmili = i;
      }
   }

   I_DrawDiskIcon();

   SDL_LowerBlit(sdlscreen, &blit_rect, argbbuffer, &blit_rect);

   SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, texture, NULL, NULL);
   SDL_RenderPresent(renderer);

   // [AM] Figure out how far into the current tic we're in as a fixed_t.
   if (uncapped)
   {
        fractionaltic = I_GetFracTime();
   }

   I_RestoreDiskBackground();
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

static byte *diskflash, *old_data;

static void I_InitDiskFlash(void)
{
  byte temp[32*32];

  if (diskflash)
    {
      Z_Free(diskflash);
      Z_Free(old_data);
    }

  diskflash = Z_Malloc((16<<hires) * (16<<hires) * sizeof(*diskflash), PU_STATIC, 0);
  old_data = Z_Malloc((16<<hires) * (16<<hires) * sizeof(*old_data), PU_STATIC, 0);

  V_GetBlock(0, 0, 0, 16, 16, temp);
  V_DrawPatchDirect(0-WIDESCREENDELTA, 0, 0, W_CacheLumpName(M_CheckParm("-cdrom") ?
                                             "STCDROM" : "STDISK", PU_CACHE));
  V_GetBlock(0, 0, 0, 16, 16, diskflash);
  V_DrawBlock(0, 0, 0, 16, 16, temp);
}

//
// killough 10/98: draw disk icon
//

void I_BeginRead(unsigned int bytes)
{
  disk_to_draw += bytes;
}

static void I_DrawDiskIcon(void)
{
  if (!disk_icon || !in_graphics_mode || PLAYBACK_SKIP)
    return;

  if (disk_to_draw >= DISK_ICON_THRESHOLD)
  {
    V_GetBlock(SCREENWIDTH-16, SCREENHEIGHT-16, 0, 16, 16, old_data);
    V_PutBlock(SCREENWIDTH-16, SCREENHEIGHT-16, 0, 16, 16, diskflash);

    disk_to_restore = 1;
  }
}

//
// killough 10/98: erase disk icon
//

void I_EndRead(void)
{
  // [FG] posponed to next tic
}

static void I_RestoreDiskBackground(void)
{
  if (!disk_icon || !in_graphics_mode || PLAYBACK_SKIP)
    return;

  if (disk_to_restore)
  {
    V_PutBlock(SCREENWIDTH-16, SCREENHEIGHT-16, 0, 16, 16, old_data);

    disk_to_restore = 0;
  }

  disk_to_draw = 0;
}

static const float gammalevels[18] =
{
    // Darker
    0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f,

    // No gamma correction
    1.0f,

    // Lighter
    1.125f, 1.25f, 1.375f, 1.5f, 1.625f, 1.75f, 1.875f, 2.0f,
};

static byte gamma2table[18][256];

static void I_InitGamma2Table(void)
{
  int i, j;

  for (i = 0; i < 18; ++i)
    for (j = 0; j < 256; ++j)
    {
      gamma2table[i][j] = (byte)(pow(j / 255.0, 1.0 / gammalevels[i]) * 255.0 + 0.5);
    }
}

int gamma2;

void I_SetPalette(byte *palette)
{
   // haleyjd
   int i;
   byte *gamma;
   SDL_Color colors[256];
   
   if (!in_graphics_mode)             // killough 8/11/98
      return;
   
   if (gamma2 != 9) // 1.0f
   {
      gamma = gamma2table[gamma2];
   }
   else
   {
      gamma = gammatable[usegamma];
   }

   for(i = 0; i < 256; ++i)
   {
      colors[i].r = gamma[*palette++];
      colors[i].g = gamma[*palette++];
      colors[i].b = gamma[*palette++];
   }
   
   SDL_SetPaletteColors(sdlscreen->format->palette, colors, 0, 256);
}

// Taken from Chocolate Doom chocolate-doom/src/i_video.c:L841-867

byte I_GetPaletteIndex(byte *palette, int r, int g, int b)
{
  byte best;
  int best_diff, diff;
  int i;

  best = 0; best_diff = INT_MAX;

  for (i = 0; i < 256; ++i)
  {
    diff = (r - palette[3 * i + 0]) * (r - palette[3 * i + 0])
          + (g - palette[3 * i + 1]) * (g - palette[3 * i + 1])
          + (b - palette[3 * i + 2]) * (b - palette[3 * i + 2]);

    if (diff < best_diff)
    {
      best = i;
      best_diff = diff;
    }

    if (diff == 0)
    {
      break;
    }
  }

  return best;
}

void I_ShutdownGraphics(void)
{
   if (in_graphics_mode)  // killough 10/98
   {
      char buf[16];
      int buflen;

      // Store the (x, y) coordinates of the window
      // in the "window_position" config parameter
      SDL_GetWindowPosition(screen, &window_x, &window_y);
      M_snprintf(buf, sizeof(buf), "%i,%i", window_x, window_y);
      buflen = strlen(buf) + 1;
      if (strlen(window_position) < buflen)
      {
          window_position = I_Realloc(window_position, buflen);
      }
      M_StringCopy(window_position, buf, buflen);

      UpdateGrab();
      in_graphics_mode = false;
   }
}

// [FG] save screenshots in PNG format
boolean I_WritePNGfile(char *filename)
{
  SDL_Rect rect = {0};
  SDL_PixelFormat *format;
  int pitch;
  byte *pixels;
  boolean ret = false;

  // [FG] native PNG pixel format
  const uint32_t png_format = SDL_PIXELFORMAT_RGB24;
  format = SDL_AllocFormat(png_format);

  // [FG] adjust cropping rectangle if necessary
  SDL_GetRendererOutputSize(renderer, &rect.w, &rect.h);
  if (useaspect || integer_scaling)
  {
    int temp;
    if (integer_scaling)
    {
      int temp1, temp2, scale;
      temp1 = rect.w;
      temp2 = rect.h;
      scale = MIN(rect.w / (SCREENWIDTH<<hires), rect.h / actualheight);

      rect.w = (SCREENWIDTH<<hires) * scale;
      rect.h = actualheight * scale;

      rect.x = (temp1 - rect.w) / 2;
      rect.y = (temp2 - rect.h) / 2;
    }
    else
    if (rect.w * actualheight > rect.h * (SCREENWIDTH<<hires))
    {
      temp = rect.w;
      rect.w = rect.h * (SCREENWIDTH<<hires) / actualheight;
      rect.x = (temp - rect.w) / 2;
    }
    else
    if (rect.h * (SCREENWIDTH<<hires) > rect.w * actualheight)
    {
      temp = rect.h;
      rect.h = rect.w * actualheight / (SCREENWIDTH<<hires);
      rect.y = (temp - rect.h) / 2;
    }
  }

  // [FG] allocate memory for screenshot image
  pitch = rect.w * format->BytesPerPixel;
  pixels = malloc(rect.h * pitch);
  SDL_RenderReadPixels(renderer, &rect, format->format, pixels, pitch);

  {
    size_t size = 0;
    void *png = NULL;
    FILE *file;

    png = tdefl_write_image_to_png_file_in_memory(pixels,
                                                  rect.w, rect.h,
                                                  format->BytesPerPixel,
                                                  &size);

    if (png)
    {
      if ((file = M_fopen(filename, "wb")))
      {
        if (fwrite(png, 1, size, file) == size)
        {
          ret = true;
        }
        fclose(file);
      }
      free(png);
    }
  }

  SDL_FreeFormat(format);
  free(pixels);

  return ret;
}

// Set the application icon

void I_InitWindowIcon(void)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) icon_data, icon_w, icon_h,
                                       32, icon_w * 4,
                                       0xff << 24, 0xff << 16,
                                       0xff << 8, 0xff << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

extern boolean setsizeneeded;

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}

static void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        fprintf(stderr,
                "I_GetWindowPosition: We were configured to run on display #%d, "
                "but it no longer exists (max %d). Moving to display 0.\n",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }

    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.

    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }

    // in windowed mode, the desired window position can be specified
    // in the configuration file.

    if (window_position == NULL || !strcmp(window_position, ""))
    {
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
    else if (!strcmp(window_position, "center"))
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTER, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else if (sscanf(window_position, "%i,%i", x, y) != 2)
    {
        // invalid format: revert to default
        fprintf(stderr, "I_GetWindowPosition: invalid window_position setting\n");
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
}

// [crispy] re-calculate SCREENWIDTH, SCREENHEIGHT, NONWIDEWIDTH and WIDESCREENDELTA
void I_GetScreenDimensions(void)
{
   SDL_DisplayMode mode;
   int w = 16, h = 9;
   int ah;

   SCREENWIDTH = ORIGWIDTH;
   SCREENHEIGHT = ORIGHEIGHT;

   NONWIDEWIDTH = SCREENWIDTH;

   ah = useaspect ? (6 * SCREENHEIGHT / 5) : SCREENHEIGHT;

   if (SDL_GetCurrentDisplayMode(video_display, &mode) == 0)
   {
      // [crispy] sanity check: really widescreen display?
      if (mode.w * ah >= mode.h * SCREENWIDTH)
      {
         w = mode.w;
         h = mode.h;
      }
   }

   // [crispy] widescreen rendering makes no sense without aspect ratio correction
   if (widescreen && useaspect)
   {
      SCREENWIDTH = w * ah / h;
      // [crispy] make sure SCREENWIDTH is an integer multiple of 4 ...
      if (hires)
      {
        SCREENWIDTH = ((2 * SCREENWIDTH) & (int)~3) / 2;
      }
      else
      {
        SCREENWIDTH = (SCREENWIDTH + 3) & (int)~3;
      }
      // [crispy] ... but never exceeds MAX_SCREENWIDTH (array size!)
      SCREENWIDTH = MIN(SCREENWIDTH, MAX_SCREENWIDTH / 2);
   }

   WIDESCREENDELTA = (SCREENWIDTH - NONWIDEWIDTH) / 2;
}

//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitGraphicsMode(void)
{
   static boolean firsttime = true;

   static int old_v_w, old_v_h;
   int v_w, v_h;

   int flags = 0;
   int scalefactor = 0;

   // [FG] SDL2
   uint32_t pixel_format;
   SDL_DisplayMode mode;

   v_w = window_width;
   v_h = window_height;

   if (firsttime)
   {
      int p, tmp_scalefactor;
      firsttime = false;

      //!
      // @category video
      //
      // Enables 640x400 resolution for internal video buffer.
      //

      if (M_CheckParm("-hires"))
         hires = true;

      //!
      // @category video
      //
      // Enables original 320x200 resolution for internal video buffer.
      //

      else if (M_CheckParm("-nohires"))
         hires = false;

      if (M_CheckParm("-grabmouse"))
         grabmouse = 1;

      //!
      // @category video 
      //
      // Don't grab the mouse when running in windowed mode.
      //

      else if (M_CheckParm("-nograbmouse"))
         grabmouse = 0;

      //!
      // @category video
      //
      // Don't scale up the screen. Implies -window.
      //

      if ((p = M_CheckParm("-1")))
         tmp_scalefactor = 1;

      //!
      // @category video
      //
      // Double up the screen to 2x its normal size. Implies -window.
      //

      else if ((p = M_CheckParm("-2")))
         tmp_scalefactor = 2;

      //!
      // @category video
      //
      // Triple up the screen to 3x its normal size. Implies -window.
      //

      else if ((p = M_CheckParm("-3")))
         tmp_scalefactor = 3;
      else if ((p = M_CheckParm("-4")))
         tmp_scalefactor = 4;
      else if ((p = M_CheckParm("-5")))
         tmp_scalefactor = 5;

      // -skipsec can take a negative number as a parameter
      if (p && strcasecmp("-skipsec", myargv[p - 1]))
        scalefactor = tmp_scalefactor;

      //!
      // @category video
      //
      // Run in a window.
      //

      if (M_CheckParm("-window") || scalefactor > 0)
      {
         fullscreen = false;
      }

      //!
      // @category video
      //
      // Run in fullscreen mode.
      //

      else if (M_CheckParm("-fullscreen") || fullscreen ||
               fullscreen_width != 0 || fullscreen_height != 0)
      {
         fullscreen = true;
      }
   }

   // [FG] window flags
   flags |= SDL_WINDOW_RESIZABLE;
   flags |= SDL_WINDOW_ALLOW_HIGHDPI;

   if (fullscreen)
   {
       if (fullscreen_width == 0 && fullscreen_height == 0)
       {
           flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
       }
       else
       {
           v_w = fullscreen_width;
           v_h = fullscreen_height;
           // [FG] exclusive fullscreen
           flags |= SDL_WINDOW_FULLSCREEN;
       }
   }

   if (M_CheckParm("-borderless"))
   {
       flags |= SDL_WINDOW_BORDERLESS;
   }

   I_GetWindowPosition(&window_x, &window_y, v_w, v_h);

#ifdef _WIN32
   if (I_CheckWindows11())
   {
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d12");
   }
#endif

   // [FG] create rendering window
   if (screen == NULL)
   {
      screen = SDL_CreateWindow(NULL,
                                window_x, window_y,
                                v_w, v_h, flags);

      if (screen == NULL)
      {
         I_Error("Error creating window for video startup: %s",
                 SDL_GetError());
      }

      SDL_SetWindowTitle(screen, PROJECT_STRING);
      I_InitWindowIcon();
   }

   // end of rendering window / fullscreen creation (reset v_w, v_h and flags)

   video_display = SDL_GetWindowDisplayIndex(screen);

   I_GetScreenDimensions();

   if (hires)
   {
      v_w = SCREENWIDTH*2;
      v_h = SCREENHEIGHT*2;
   }
   else
   {
      v_w = SCREENWIDTH;
      v_h = SCREENHEIGHT;
   }

   blit_rect.w = v_w;
   blit_rect.h = v_h;

   actualheight = useaspect ? (6 * v_h / 5) : v_h;

   SDL_SetWindowMinimumSize(screen, v_w, actualheight);
   if (!fullscreen)
   {
      SDL_GetWindowSize(screen, &window_width, &window_height);
   }

   // [FG] window size when returning from fullscreen mode
   if (scalefactor > 0)
   {
      window_width = scalefactor * v_w;
      window_height = scalefactor * actualheight;
   }
   else if (old_v_w > 0 && old_v_h > 0)
   {
      int rendered_height;

      // rendered height does not necessarily match window height
      if (window_height * old_v_w > window_width * old_v_h)
          rendered_height = (window_width * old_v_h + old_v_w - 1) / old_v_w;
      else
          rendered_height = window_height;

      window_width = rendered_height * v_w / actualheight;
   }

   old_v_w = v_w;
   old_v_h = actualheight;

   if (!fullscreen)
   {
      SDL_SetWindowSize(screen, window_width, window_height);
   }

   pixel_format = SDL_GetWindowPixelFormat(screen);

   // [FG] renderer flags
   flags = 0;

   if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
   {
      I_Error("Could not get display mode for video display #%d: %s",
              video_display, SDL_GetError());
   }

   if (page_flip && use_vsync && !timingdemo && mode.refresh_rate > 0)
   {
      flags |= SDL_RENDERER_PRESENTVSYNC;
   }

   // [FG] page_flip = !force_software_renderer
   if (!page_flip)
   {
      flags |= SDL_RENDERER_SOFTWARE;
      flags &= ~SDL_RENDERER_PRESENTVSYNC;
      use_vsync = false;
   }

   // [FG] create renderer

   if (renderer != NULL)
   {
      SDL_DestroyRenderer(renderer);
      texture = NULL;
   }

   renderer = SDL_CreateRenderer(screen, -1, flags);

   // [FG] try again without hardware acceleration
   if (renderer == NULL && page_flip)
   {
      flags |= SDL_RENDERER_SOFTWARE;
      flags &= ~SDL_RENDERER_PRESENTVSYNC;

      renderer = SDL_CreateRenderer(screen, -1, flags);

      if (renderer != NULL)
      {
         // remove any special flags
         use_vsync = page_flip = false;
      }
   }

   if (renderer == NULL)
   {
      I_Error("Error creating renderer for screen window: %s",
              SDL_GetError());
   }

   SDL_RenderSetLogicalSize(renderer, v_w, actualheight);

   // [FG] force integer scales
   SDL_RenderSetIntegerScale(renderer, integer_scaling);

   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderClear(renderer);
   SDL_RenderPresent(renderer);

   // [FG] create paletted frame buffer

   if (sdlscreen != NULL)
   {
      SDL_FreeSurface(sdlscreen);
      sdlscreen = NULL;
   }

   if (sdlscreen == NULL)
   {
      sdlscreen = SDL_CreateRGBSurface(0,
                                       v_w, v_h, 8,
                                       0, 0, 0, 0);
      SDL_FillRect(sdlscreen, NULL, 0);

      // [FG] screen buffer
      screens[0] = sdlscreen->pixels;
      memset(screens[0], 0, v_w * v_h * sizeof(*screens[0]));
   }

   // [FG] create intermediate ARGB frame buffer

   if (argbbuffer != NULL)
   {
      SDL_FreeSurface(argbbuffer);
      argbbuffer = NULL;
   }

   if (argbbuffer == NULL)
   {
      unsigned int rmask, gmask, bmask, amask;
      int bpp;

      SDL_PixelFormatEnumToMasks(pixel_format, &bpp,
                                 &rmask, &gmask, &bmask, &amask);
      argbbuffer = SDL_CreateRGBSurface(0,
                                        v_w, v_h, bpp,
                                        rmask, gmask, bmask, amask);
      SDL_FillRect(argbbuffer, NULL, 0);
   }

   // [FG] create texture

   if (texture != NULL)
   {
      SDL_DestroyTexture(texture);
   }

   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

   texture = SDL_CreateTexture(renderer,
                               pixel_format,
                               SDL_TEXTUREACCESS_STREAMING,
                               v_w, v_h);

   // Workaround for SDL 2.0.14 (and 2.0.16) alt-tab bug (taken from Doom Retro)
#if defined(_WIN32)
   {
      SDL_version ver;
      SDL_GetVersion(&ver);
      if (ver.major == 2 && ver.minor == 0 && (ver.patch == 14 || ver.patch == 16))
      {
         SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
      }
   }
#endif

   V_Init();

   UpdateGrab();

   in_graphics_mode = 1;
   setsizeneeded = true;

   I_InitDiskFlash();        // Initialize disk icon   
   I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

void I_ResetScreen(void)
{
   if (!in_graphics_mode)
   {
      setsizeneeded = true;
      V_Init();
      return;
   }

   I_ShutdownGraphics();     // Switch out of old graphics mode

   I_InitGraphicsMode();     // Switch to new graphics mode
   
   if (automapactive)
      AM_Start();             // Reset automap dimensions
   
   ST_Start();               // Reset palette
   
   if (gamestate == GS_INTERMISSION)
   {
      WI_DrawBackground();
      V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
   }

   M_ResetSetupMenuVideo();
}

void I_InitGraphics(void)
{
  static int firsttime = 1;

  if (!firsttime)
    return;

  firsttime = 0;

#if 0
  if (nodrawers) // killough 3/2/98: possibly avoid gfx mode
    return;
#endif

  //
  // enter graphics mode
  //

  if (SDL_Init(SDL_INIT_VIDEO) < 0) 
  {
    I_Error("Failed to initialize video: %s", SDL_GetError());
  }

  I_AtExit(I_ShutdownGraphics, true);

  // Initialize and generate gamma-correction levels.
  I_InitGamma2Table();

  I_InitGraphicsMode();    // killough 10/98

  M_ResetSetupMenuVideo();
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
