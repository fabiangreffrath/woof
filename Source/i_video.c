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

#include "SDL.h" // haleyjd
#include "SDL_image.h" // [FG] IMG_SavePNG()

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

// [FG] set the application icon

#include "icon.c"

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

// [FG] window size when returning from fullscreen mode
static int window_width, window_height;

/////////////////////////////////////////////////////////////////////////////
//
// JOYSTICK                                                  // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

extern int usejoystick;

// I_JoystickEvents() gathers joystick data and creates an event_t for
// later processing by G_Responder().

int joystickSens_x;
int joystickSens_y;

extern SDL_Joystick *sdlJoystick;

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

static int GetButtonsState(void)
{
    int i;
    int result;

    result = 0;

    for (i = 0; i < MAX_JSB; ++i)
    {
        if (SDL_JoystickGetButton(sdlJoystick, i))
        {
            result |= 1 << i;
        }
    }

    return result;
}

static int GetAxisState(int axis, int sens)
{
    int result;

    result = SDL_JoystickGetAxis(sdlJoystick, axis);

    if (result < -sens)
    {
        return -1;
    }
    else if (result > sens)
    {
        return 1;
    }

    return 0;
}

void I_UpdateJoystick(void)
{
    if (sdlJoystick != NULL)
    {
        event_t ev;

        ev.type = ev_joystick;
        ev.data1 = GetButtonsState();
        ev.data2 = GetAxisState(0, joystickSens_x);
        ev.data3 = GetAxisState(1, joystickSens_y);

        D_PostEvent(&ev);
    }
}

//
// I_StartFrame
//
void I_StartFrame(void)
{
    if (usejoystick)
    {
        I_UpdateJoystick();
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
static boolean window_focused;
boolean fullscreen;

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
   
   if(grab && !currently_grabbed)
   {
      SetShowCursor(false);
   }
   
   if(!grab && currently_grabbed)
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
            return KEYD_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEYD_RSHIFT;

        case SDL_SCANCODE_LALT:
            return KEYD_LALT;

        case SDL_SCANCODE_RALT:
            return KEYD_RALT;

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

static unsigned int mouse_button_state = 0;

static void UpdateMouseButtonState(unsigned int button, boolean on)
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
        mouse_button_state |= (1 << button);
    }
    else
    {
        mouse_button_state &= ~(1 << button);
    }

    // Post an event with the new button state.

    event.type = ev_mouse;
    event.data1 = mouse_button_state;
    event.data2 = event.data3 = 0;
    D_PostEvent(&event);
}

static void MapMouseWheelToButtons(SDL_MouseWheelEvent *wheel)
{
    // SDL2 distinguishes button events from mouse wheel events.
    // We want to treat the mouse wheel as two buttons, as per
    // SDL1
    static event_t up, down;
    int button;

    if (wheel->y <= 0)
    {   // scroll down
        button = 4;
    }
    else
    {   // scroll up
        button = 3;
    }

    // post a button down event
    mouse_button_state |= (1 << button);
    down.type = ev_mouse;
    down.data1 = mouse_button_state;
    down.data2 = down.data3 = 0;
    D_PostEvent(&down);

    // post a button up event
    mouse_button_state &= ~(1 << button);
    up.type = ev_mouse;
    up.data1 = mouse_button_state;
    up.data2 = up.data3 = 0;
    D_PostEvent(&up);
}

static void I_HandleMouseEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            UpdateMouseButtonState(sdlevent->button.button, true);
            break;

        case SDL_MOUSEBUTTONUP:
            UpdateMouseButtonState(sdlevent->button.button, false);
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
    // XXX: passing pointers to event for access after this function
    // has terminated is undefined behaviour
    event_t event;

    switch (sdlevent->type)
    {
        case SDL_KEYDOWN:
            event.type = ev_keydown;
            event.data1 = TranslateKey(&sdlevent->key.keysym);
            event.data2 = 0;
            event.data3 = 0;

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

            event.data2 = 0;
            event.data3 = 0;

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
    switch (event->event)
    {
        case SDL_WINDOWEVENT_RESIZED:
            if (!fullscreen)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);
            }
            break;

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

    fullscreen = !fullscreen;

    if (fullscreen)
    {
        SDL_GetWindowSize(screen, &window_width, &window_height);
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}

// [FG] the fullscreen variable gets toggled once by the menu code, so we
// toggle it back here, it is then toggled again in I_ToggleFullScreen()

void I_ToggleToggleFullScreen(void)
{
    fullscreen = !fullscreen;

    I_ToggleFullScreen();
}

// killough 3/22/98: rewritten to use interrupt-driven keyboard queue

extern int usemouse;

void I_GetEvent(void)
{
    SDL_Event sdlevent;

    SDL_PumpEvents();

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
                if (usemouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
/*
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
*/
                exit(0);
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

static const float mouse_acceleration = 1.0; // 2.0;
static const int mouse_threshold = 0; // 10;

static int AccelerateMouse(int val)
{
    if (val < 0)
        return -AccelerateMouse(-val);

    if (val > mouse_threshold)
    {
        return (int)((val - mouse_threshold) * mouse_acceleration + mouse_threshold);
    }
    else
    {
        return val;
    }
}

static void I_ReadMouse(void)
{
    int x, y;
    event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (x != 0 || y != 0)
    {
        ev.type = ev_mouse;
        ev.data1 = mouse_button_state;
        ev.data2 = AccelerateMouse(x);
        ev.data3 = -AccelerateMouse(y);

        // XXX: undefined behaviour since event is scoped to
        // this function
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

    if (usemouse && window_focused)
    {
        I_ReadMouse();
    }
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

static int in_graphics_mode;
static int in_page_flip, in_hires;

static void I_DrawDiskIcon(), I_RestoreDiskBackground();
static unsigned int disk_to_draw, disk_to_restore;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
fixed_t fractionaltic;

static int useaspect, actualheight; // [FG] aspect ratio correction
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
   if(devparm)
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
	fractionaltic = I_GetTimeMS() * TICRATE % 1000 * FRACUNIT / 1000;
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
      free(diskflash);
      free(old_data);
    }

  diskflash = malloc((16<<hires) * (16<<hires) * sizeof(*diskflash));
  old_data = malloc((16<<hires) * (16<<hires) * sizeof(*old_data));

  V_GetBlock(0, 0, 0, 16, 16, temp);
  V_DrawPatchDirect(0, 0, 0, W_CacheLumpName(M_CheckParm("-cdrom") ?
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
  if (!disk_icon || !in_graphics_mode)
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
  if (!disk_icon || !in_graphics_mode)
    return;

  if (disk_to_restore)
  {
    V_PutBlock(SCREENWIDTH-16, SCREENHEIGHT-16, 0, 16, 16, old_data);

    disk_to_restore = 0;
  }

  disk_to_draw = 0;
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
   
   SDL_SetPaletteColors(sdlscreen->format->palette, colors, 0, 256);
}

void I_ShutdownGraphics(void)
{
   if(in_graphics_mode)  // killough 10/98
   {
      UpdateGrab();      
      in_graphics_mode = false;
   }
}

// [FG] save screenshots in PNG format
boolean I_WritePNGfile(char *filename)
{
  SDL_Rect rect = {0};
  SDL_PixelFormat *format;
  SDL_Surface *png_surface;
  int pitch;
  byte *pixels;
  boolean ret;

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
  png_surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, rect.w, rect.h, format->BitsPerPixel, pitch, png_format);

  ret = (IMG_SavePNG(png_surface, filename) == 0);

  SDL_FreeSurface(png_surface);
  SDL_FreeFormat(format);
  free(pixels);

  return ret;
}

// Set the application icon

static void I_InitWindowIcon(void)
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

int cfg_scalefactor; // haleyjd 05/11/09: scale factor in config
int cfg_aspectratio; // haleyjd 05/11/09: aspect ratio correction

// haleyjd 05/11/09: true if called from I_ResetScreen
static boolean changeres = false;

// [crispy] re-calculate SCREENWIDTH, SCREENHEIGHT, NONWIDEWIDTH and WIDESCREENDELTA
void I_GetScreenDimensions(void)
{
   SDL_DisplayMode mode;
   int w = 16, h = 10;
   int ah;

   SCREENWIDTH = ORIGWIDTH;
   SCREENHEIGHT = ORIGHEIGHT;

   NONWIDEWIDTH = SCREENWIDTH;

   ah = useaspect ? (6 * SCREENHEIGHT / 5) : SCREENHEIGHT;

   if (SDL_GetCurrentDisplayMode(0/*video_display*/, &mode) == 0)
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
      // switch(crispy->widescreen)
      // {
      //     case RATIO_16_10:
      //         w = 16;
      //         h = 10;
      //         break;
      //     case RATIO_16_9:
      //         w = 16;
      //         h = 9;
      //         break;
      //     case RATIO_21_9:
      //         w = 21;
      //         h = 9;
      //         break;
      //     default:
      //         break;
      // }

      SCREENWIDTH = w * ah / h;
      // [crispy] make sure SCREENWIDTH is an integer multiple of 4 ...
      SCREENWIDTH = (SCREENWIDTH + (hires ? 0 : 3)) & (int)~3;
      // [crispy] ... but never exceeds MAXWIDTH (array size!)
      SCREENWIDTH = MIN(SCREENWIDTH, MAX_SCREENWIDTH);
   }

   WIDESCREENDELTA = (SCREENWIDTH - NONWIDEWIDTH) / 2;
}


//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitGraphicsMode(void)
{
   static boolean firsttime = true;
   
   // haleyjd
   int v_w = ORIGWIDTH;
   int v_h = ORIGHEIGHT;
   int flags = 0;
   int scalefactor = cfg_scalefactor;
   int usehires = hires;

   // [FG] SDL2
   uint32_t pixel_format;
   int video_display;
   SDL_DisplayMode mode;

   if(firsttime)
   {
      I_InitKeyboard();
      firsttime = false;

      if(M_CheckParm("-hires"))
         usehires = hires = true;
      else if(M_CheckParm("-nohires"))
         usehires = hires = false; // grrr...
   }

   useaspect = cfg_aspectratio;
   if(M_CheckParm("-aspect"))
      useaspect = true;

   I_GetScreenDimensions();

   if(usehires)
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

   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing   
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;

   // [FG] window flags
   flags |= SDL_WINDOW_RESIZABLE;
   flags |= SDL_WINDOW_ALLOW_HIGHDPI;

   // haleyjd: fullscreen support
   if(M_CheckParm("-window"))
   {
      fullscreen = false;
   }
   if(M_CheckParm("-fullscreen") || fullscreen)
   {
      fullscreen = true; // 5/11/09: forgotten O_O
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
   }

   if (scalefactor == 1 && usehires == false)
      scalefactor = 2;
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

   actualheight = useaspect ? (6 * v_h / 5) : v_h;

   // [FG] create rendering window

   if (screen == NULL)
   {
      screen = SDL_CreateWindow(NULL,
                                // centered window
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                v_w, v_h, flags);

      if (screen == NULL)
      {
         I_Error("Error creating window for video startup: %s",
                 SDL_GetError());
      }

      SDL_SetWindowTitle(screen, PROJECT_STRING);
      I_InitWindowIcon();
   }

   SDL_SetWindowMinimumSize(screen, v_w, actualheight);

   // [FG] window size when returning from fullscreen mode
   if (!window_width || !window_height)
   {
      window_width = scalefactor * v_w;
      window_height = scalefactor * actualheight;
   }

   if (!(flags & SDL_WINDOW_FULLSCREEN_DESKTOP))
   {
      SDL_SetWindowSize(screen, window_width, window_height);
   }

   pixel_format = SDL_GetWindowPixelFormat(screen);
   video_display = SDL_GetWindowDisplayIndex(screen);

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

   // Workaround for SDL 2.0.14 alt-tab bug (taken from Doom Retro)
#if defined(_WIN32)
   {
      SDL_version ver;
      SDL_GetVersion(&ver);
      if (ver.major == 2 && ver.minor == 0 && ver.patch == 14)
      {
         SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
      }
   }
#endif

   V_Init();

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
