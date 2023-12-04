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
// DESCRIPTION:
//      DOOM graphics stuff
//
//-----------------------------------------------------------------------------

#include <math.h>

#include "SDL.h" // haleyjd

#include "../miniz/miniz.h"

#include "doomstat.h"
#include "i_printf.h"
#include "v_video.h"
#include "d_main.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "w_wad.h"
#include "r_draw.h"
#include "r_main.h"
#include "am_map.h"
#include "m_menu.h"
#include "wi_stuff.h"
#include "i_input.h"
#include "i_video.h"
#include "m_io.h"

// [FG] set the application icon

#include "icon.c"

video_t video;

boolean use_vsync;  // killough 2/8/98: controls whether vsync is called
int hires, default_hires;      // killough 11/98
boolean use_aspect;
boolean uncapped, default_uncapped; // [FG] uncapped rendering frame rate
int fpslimit; // when uncapped, limit framerate to this value
boolean fullscreen;
boolean exclusive_fullscreen;
int widescreen; // widescreen mode
boolean integer_scaling; // [FG] force integer scales
boolean vga_porch_flash; // emulate VGA "porch" behaviour
boolean smooth_scaling;

boolean need_reset;
boolean toggle_fullscreen;
boolean toggle_exclusive_fullscreen;

int video_display = 0; // display index
int window_width, window_height;
int window_position_x, window_position_y;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
fixed_t fractionaltic;

boolean disk_icon;  // killough 10/98
int fps; // [FG] FPS counter widget

// [FG] rendering window, renderer, intermediate ARGB frame buffer and texture

static SDL_Window *screen;
static SDL_Renderer *renderer;
static SDL_Surface *sdlscreen;
static SDL_Surface *argbbuffer;
static SDL_Texture *texture;
static SDL_Texture *texture_upscaled;
static SDL_Rect blit_rect = {0};

static int window_x, window_y;
static int actualheight;

static boolean need_resize;

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse?
boolean grabmouse = true, default_grabmouse;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible = true;

static boolean window_focused = true;

void *I_GetSDLWindow(void)
{
    return screen;
}

void *I_GetSDLRenderer(void)
{
    return renderer;
}

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
            need_resize = true;
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

static void I_ReinitGraphicsMode(void);

static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    if (exclusive_fullscreen)
    {
        I_ReinitGraphicsMode();
        return;
    }

    if (fullscreen)
    {
        SDL_GetWindowSize(screen, &window_width, &window_height);
        SDL_GetWindowPosition(screen, &window_x, &window_y);
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);
#ifdef _WIN32
    I_InitWindowIcon();
#endif

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
        SDL_SetWindowPosition(screen, window_x, window_y);
    }
}

static void I_ToggleExclusiveFullScreen(void)
{
    if (!fullscreen)
    {
        return;
    }

    I_ReinitGraphicsMode();
}

void I_ToggleVsync(void)
{
    SDL_RenderSetVSync(renderer, use_vsync);
}

// killough 3/22/98: rewritten to use interrupt-driven keyboard queue

static void I_GetEvent(void)
{
    SDL_Event sdlevent;

    I_DelayEvent();

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    fullscreen = !fullscreen;
                    toggle_fullscreen = true;
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
                    static event_t event;
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
// I_StartTic
//
void I_StartTic (void)
{
    I_GetEvent();

    if (window_focused)
    {
        I_ReadMouse();
    }

    I_UpdateJoystick();
}

//
// I_StartFrame
//
void I_StartFrame(void)
{

}

static inline void I_UpdateRender (void)
{
    SDL_LowerBlit(sdlscreen, &blit_rect, argbbuffer, &blit_rect);
    SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);
    SDL_RenderClear(renderer);

    if (smooth_scaling)
    {
        // Render this intermediate texture into the upscaled texture
        // using "nearest" integer scaling.

        SDL_SetRenderTarget(renderer, texture_upscaled);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        // Finally, render this upscaled texture to screen using linear scaling.

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);
    }
    else
    {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
    }
}

static void I_DrawDiskIcon(), I_RestoreDiskBackground();
static unsigned int disk_to_draw, disk_to_restore;

static void CreateUpscaledTexture(boolean force);

void I_FinishUpdate(void)
{
    if (noblit)
        return;

    UpdateGrab();

    if (need_reset)
    {
        I_ResetScreen();
        need_reset = false;
    }

    if (need_resize)
    {
        CreateUpscaledTexture(false);
        need_resize = false;
    }

    if (toggle_fullscreen)
    {
        I_ToggleFullScreen();
        toggle_fullscreen = false;
    }

    if (toggle_exclusive_fullscreen)
    {
        I_ToggleExclusiveFullScreen();
        toggle_exclusive_fullscreen = false;
    }

// TODO
#if 0
  // draws little dots on the bottom of the screen
  if (devparm)
    {
      static int lasttic;
      byte *s = I_VideoBuffer;

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
#endif

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

    I_UpdateRender();

    SDL_RenderPresent(renderer);

    if (uncapped)
    {
        if (fpslimit >= TICRATE)
        {
            uint64_t target_time = 1000000ull / fpslimit;
            static uint64_t start_time;

            while (1)
            {
                uint64_t current_time = I_GetTimeUS();
                uint64_t elapsed_time = current_time - start_time;
                uint64_t remaining_time = 0;

                if (elapsed_time >= target_time)
                {
                    start_time = current_time;
                    break;
                }

                remaining_time = target_time - elapsed_time;

                if (remaining_time > 1000)
                    I_Sleep((remaining_time - 1000) / 1000);
            }
        }

        // [AM] Figure out how far into the current tic we're in as a fixed_t.
        fractionaltic = I_GetFracTime();
    }

    I_RestoreDiskBackground();
}

//
// I_ReadScreen
//

void I_ReadScreen(byte *scr)
{
   int size = video.width * video.height;

   // haleyjd
   memcpy(scr, I_VideoBuffer, size);
}

//
// killough 10/98: init disk icon
//

static pixel_t *diskflash, *old_data;
static vrect_t disk;

static void I_InitDiskFlash(void)
{
  pixel_t *temp;

  disk.x = 0;
  disk.y = 0;
  disk.w = 16;
  disk.h = 16;

  V_ScaleRect(&disk);

  temp = Z_Malloc(disk.sw * disk.sh * sizeof(*temp), PU_STATIC, 0);

  if (diskflash)
  {
    Z_Free(diskflash);
    Z_Free(old_data);
  }

  diskflash = Z_Malloc(disk.sw * disk.sh * sizeof(*diskflash), PU_STATIC, 0);
  old_data = Z_Malloc(disk.sw * disk.sh * sizeof(*old_data), PU_STATIC, 0);

  V_GetBlock(0, 0, disk.sw, disk.sh, temp);
  V_DrawPatch(-video.deltaw, 0, W_CacheLumpName("STDISK", PU_CACHE));
  V_GetBlock(0, 0, disk.sw, disk.sh, diskflash);
  V_PutBlock(0, 0, disk.sw, disk.sh, temp);

  Z_Free(temp);
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
  if (!disk_icon || PLAYBACK_SKIP)
    return;

  if (disk_to_draw >= DISK_ICON_THRESHOLD)
  {
    V_GetBlock(video.width - disk.sw, video.height - disk.sh, disk.sw, disk.sh, old_data);
    V_PutBlock(video.width - disk.sw, video.height - disk.sh, disk.sw, disk.sh, diskflash);

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
  if (!disk_icon || PLAYBACK_SKIP)
    return;

  if (disk_to_restore)
  {
    V_PutBlock(video.width - disk.sw, video.height - disk.sh, disk.sw, disk.sh, old_data);

    disk_to_restore = 0;
  }

  disk_to_draw = 0;
}

static const float gammalevels[9] =
{
    // Darker
    0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f,
};

static byte gamma2table[18][256];

static void I_InitGamma2Table(void)
{
  int i, j, k;

  for (i = 0; i < 9; ++i)
    for (j = 0; j < 256; ++j)
    {
      gamma2table[i][j] = (byte)(pow(j / 255.0, 1.0 / gammalevels[i]) * 255.0 + 0.5);
    }

  // [crispy] 5 original gamma levels
  for (i = 9, k = 0; i < 18 && k < 5; i += 2, k++)
    memcpy(gamma2table[i], gammatable[k], 256);

  // [crispy] 4 intermediate gamma levels
  for (i = 10, k = 0; i < 18 && k < 4; i += 2, k++)
    for (j = 0; j < 256; ++j)
    {
      gamma2table[i][j] = (gammatable[k][j] + gammatable[k + 1][j]) / 2;
    }
}

int gamma2;

void I_SetPalette(byte *palette)
{
  // haleyjd
  int i;
  byte *const gamma = gamma2table[gamma2];
  SDL_Color colors[256];

  if (noblit)             // killough 8/11/98
    return;

  for(i = 0; i < 256; ++i)
  {
    colors[i].r = gamma[*palette++];
    colors[i].g = gamma[*palette++];
    colors[i].b = gamma[*palette++];
  }

  SDL_SetPaletteColors(sdlscreen->format->palette, colors, 0, 256);

  if (vga_porch_flash)
  {
    // "flash" the pillars/letterboxes with palette changes,
    // emulating VGA "porch" behaviour
    SDL_SetRenderDrawColor(renderer,
                           colors[0].r, colors[0].g, colors[0].b,
                           SDL_ALPHA_OPAQUE);
  }
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

  I_UpdateRender();

  // [FG] adjust cropping rectangle if necessary
  SDL_GetRendererOutputSize(renderer, &rect.w, &rect.h);
  if (integer_scaling)
  {
    int temp1, temp2, scale;
    temp1 = rect.w;
    temp2 = rect.h;
    scale = MIN(rect.w / video.width, rect.h / actualheight);

    rect.w = video.width * scale;
    rect.h = actualheight * scale;

    rect.x = (temp1 - rect.w) / 2;
    rect.y = (temp2 - rect.h) / 2;
  }
  else if (rect.w * actualheight > rect.h * video.width)
  {
    int temp = rect.w;
    rect.w = rect.h * video.width / actualheight;
    rect.x = (temp - rect.w) / 2;
  }
  else if (rect.h * video.width > rect.w * actualheight)
  {
    int temp = rect.h;
    rect.h = rect.w * actualheight / video.width;
    rect.y = (temp - rect.h) / 2;
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
          I_Printf(VB_INFO, "I_WritePNGfile: %s", filename);
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
                                       0xffu << 24, 0xffu << 16,
                                       0xffu << 8, 0xffu << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        I_Printf(VB_WARNING, "CenterWindow: Failed to read display bounds "
                             "for display #%d!", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);

    // Fix exclusive fullscreen mode.
    if (*x == 0 && *y == 0)
    {
        *x = SDL_WINDOWPOS_CENTERED;
        *y = SDL_WINDOWPOS_CENTERED;
    }
}

static void I_ResetInvalidDisplayIndex(void)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        I_Printf(VB_WARNING,
                "I_ResetInvalidDisplayIndex: We were configured to run on "
                "display #%d, but it no longer exists (max %d). "
                "Moving to display 0.",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }
}

static void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.

    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }

    // center
    if (window_position_x == 0 && window_position_y == 0)
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTERED, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else
    {
        *x = window_position_x;
        *y = window_position_y;
    }
}

static void I_GetScreenDimensions(void)
{
    SDL_DisplayMode mode;
    int w = 16, h = 9;

    int unscaled_ah = use_aspect ? (6 * SCREENHEIGHT / 5) : SCREENHEIGHT;

    if (SDL_GetCurrentDisplayMode(video_display, &mode) == 0)
    {
        // [crispy] sanity check: really widescreen display?
        if (mode.w * unscaled_ah >= mode.h * SCREENWIDTH)
        {
            w = mode.w;
            h = mode.h;
        }
    }

    if (hires)
    {
        video.height = use_aspect ? (5 * mode.h / 6) : mode.h;
        actualheight = mode.h;
    }
    else
    {
        video.height = SCREENHEIGHT;
        actualheight = unscaled_ah;
    }

    video.unscaledh = SCREENHEIGHT;

    if (widescreen)
    {
        switch(widescreen)
        {
            case RATIO_16_10:
                w = 16;
                h = 10;
                break;
            case RATIO_16_9:
                w = 16;
                h = 9;
                break;
            case RATIO_21_9:
                w = 21;
                h = 9;
                break;
            default:
                break;
        }

        video.unscaledw = w * unscaled_ah / h;
        video.width = w * actualheight / h;
    }
    else
    {
        video.unscaledw = SCREENWIDTH;
        video.width = 4 * actualheight / 3;
    }

    // [FG] For performance reasons, SDL2 insists that the screen pitch, i.e.
    // the *number of bytes* that one horizontal row of pixels occupy in
    // memory, must be a multiple of 4.
    video.width = (video.width + 3) & ~3;
    video.height = (video.height + 3) & ~3;

    video.deltaw = (video.unscaledw - NONWIDEWIDTH) / 2;

    video.fov = 2 * atan(video.unscaledw / (1.2 * video.unscaledh) * 3 / 4) / M_PI * ANG180;

    video.xscale = (video.width << FRACBITS) / video.unscaledw;
    video.yscale = (video.height << FRACBITS) / video.unscaledh;
    video.xstep  = ((video.unscaledw << FRACBITS) / video.width) + 1;
    video.ystep  = ((video.unscaledh << FRACBITS) / video.height) + 1;

    R_InitAnyRes();

    printf("render resolution: %dx%d\n", video.width, video.height);
}

static void CreateUpscaledTexture(boolean force)
{
    SDL_RendererInfo info;
    int w, h, w_upscale, h_upscale;
    static int h_upscale_old, w_upscale_old;

    const int screen_width = video.width;
    const int screen_height = video.height;

    SDL_GetRendererInfo(renderer, &info);

    if (info.flags & SDL_RENDERER_SOFTWARE)
    {
        return;
    }

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).

    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0)
    {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.

    if (w * actualheight < h * screen_width)
    {
        // Tall window.

        h = w * actualheight / screen_width;
    }
    else
    {
        // Wide window.

        w = h * screen_width / actualheight;
    }

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.

    w_upscale = (w + screen_width - 1) / screen_width;
    h_upscale = (h + screen_height - 1) / screen_height;

    while (w_upscale * screen_width > info.max_texture_width)
    {
        --w_upscale;
    }
    while (h_upscale * screen_height > info.max_texture_height)
    {
        --h_upscale;
    }

    if (w_upscale < 1)
    {
        w_upscale = 1;
    }
    if (h_upscale < 1)
    {
        h_upscale = 1;
    }

    // Create a new texture only if the upscale factors have actually changed.

    if (h_upscale == h_upscale_old && w_upscale == w_upscale_old && !force)
    {
        return;
    }

    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    if (texture_upscaled != NULL)
    {
        SDL_DestroyTexture(texture_upscaled);
    }

    // Set the scaling quality for rendering the upscaled texture
    // to "linear", which looks much softer and smoother than "nearest"
    // but does a better job at downscaling from the upscaled texture to
    // screen.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    texture_upscaled = SDL_CreateTexture(renderer,
                                        SDL_GetWindowPixelFormat(screen),
                                        SDL_TEXTUREACCESS_TARGET,
                                        w_upscale * screen_width,
                                        h_upscale * screen_height);
}

static int scalefactor;

static void I_ResetGraphicsMode(void)
{
    int w, h;
    int window_w, window_h;
    //static int old_w, old_h;

    uint32_t pixel_format;

    I_GetScreenDimensions();

    w = video.width;
    h = video.height;
    window_w = SCREENWIDTH * 2;
    window_h = SCREENHEIGHT * 2;

    blit_rect.w = w;
    blit_rect.h = h;

    I_Printf(VB_DEBUG, "I_ResetGraphicsMode: Rendering resolution %dx%d",
             w, actualheight);

    SDL_SetWindowMinimumSize(screen, window_w, window_h);

// TODO
#if 0
    if (!fullscreen)
    {
        SDL_GetWindowSize(screen, &window_width, &window_height);
    }

    // [FG] window size when returning from fullscreen mode
    if (scalefactor > 0)
    {
        window_width = scalefactor * w;
        window_height = scalefactor * actualheight;
    }
    else if (old_w > 0 && old_h > 0)
    {
        int rendered_height;

        // rendered height does not necessarily match window height
        if (window_height * old_w > window_width * old_h)
            rendered_height = (window_width * old_h + old_w - 1) / old_w;
        else
            rendered_height = window_height;

        window_width = rendered_height * w / actualheight;
    }

    old_w = w;
    old_h = actualheight;
#endif

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
    }

    SDL_RenderSetLogicalSize(renderer, w, actualheight);

    // [FG] force integer scales
    SDL_RenderSetIntegerScale(renderer, integer_scaling ? SDL_TRUE : SDL_FALSE);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    V_Init();
    ST_InitRes();

    // [FG] create paletted frame buffer

    if (sdlscreen != NULL)
    {
        SDL_FreeSurface(sdlscreen);
    }

    sdlscreen = SDL_CreateRGBSurface(0,
                                     w, h, 8,
                                     0, 0, 0, 0);
    SDL_FillRect(sdlscreen, NULL, 0);

    // [FG] screen buffer

    I_VideoBuffer = sdlscreen->pixels;
    V_RestoreBuffer();

    pixel_format = SDL_GetWindowPixelFormat(screen);

    // [FG] create intermediate ARGB frame buffer

    if (argbbuffer != NULL)
    {
        SDL_FreeSurface(argbbuffer);
    }

    {
        uint32_t rmask, gmask, bmask, amask;
        int bpp;

        SDL_PixelFormatEnumToMasks(pixel_format, &bpp,
                                   &rmask, &gmask, &bmask, &amask);
        argbbuffer = SDL_CreateRGBSurface(0,
                                          w, h, bpp,
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
                                w, h);

    if (smooth_scaling)
    {
        CreateUpscaledTexture(true);
    }

    setsizeneeded = true;

    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));

    I_InitDiskFlash();        // Initialize disk icon
}

//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitVideoParms(void)
{
    int p, tmp_scalefactor;

    I_ResetInvalidDisplayIndex();
    hires = default_hires;
    uncapped = default_uncapped;
    grabmouse = default_grabmouse;

    //!
    // @category video
    //
    // Enables uncapped framerate.
    //

    if (M_CheckParm("-uncapped"))
        uncapped = true;

    //!
    // @category video
    //
    // Disables uncapped framerate.
    //

    else if (M_CheckParm("-nouncapped"))
        uncapped = false;

    if (M_CheckParm("-grabmouse"))
        grabmouse = true;

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    else if (M_CheckParm("-nograbmouse"))
        grabmouse = false;

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

    else if (M_CheckParm("-fullscreen"))
    {
        fullscreen = true;
    }
}

static void I_InitGraphicsMode(void)
{
    int w, h;
    uint32_t flags = 0;

    // [FG] window flags
    flags |= SDL_WINDOW_RESIZABLE;
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    w = window_width;
    h = window_height;

    if (fullscreen)
    {
        if (exclusive_fullscreen)
        {
            SDL_DisplayMode mode;
            if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
            {
                I_Error("Could not get display mode for video display #%d: %s",
                        video_display, SDL_GetError());
            }
            w = mode.w;
            h = mode.h;
            // [FG] exclusive fullscreen
            flags |= SDL_WINDOW_FULLSCREEN;
        }
        else
        {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
    }

    if (M_CheckParm("-borderless"))
    {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    I_GetWindowPosition(&window_x, &window_y, w, h);

    // [FG] create rendering window

    screen = SDL_CreateWindow(PROJECT_STRING, window_x, window_y, w, h, flags);

    if (screen == NULL)
    {
        I_Error("Error creating window for video startup: %s",
                SDL_GetError());
    }

    I_InitWindowIcon();

    flags = 0;

    if (use_vsync && !timingdemo)
    {
        flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    // [FG] create renderer
    renderer = SDL_CreateRenderer(screen, -1, flags);

    // [FG] try again without hardware acceleration
    if (renderer == NULL)
    {
        flags |= SDL_RENDERER_SOFTWARE;
        flags &= ~SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(screen, -1, flags);

        if (renderer != NULL)
        {
            // remove any special flags
            use_vsync = false;
        }
    }

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }

    I_ResetGraphicsMode();
}

static void I_ReinitGraphicsMode(void)
{
    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if (screen != NULL)
    {
        const int i = SDL_GetWindowDisplayIndex(screen);
        video_display = i < 0 ? 0 : i;
        SDL_DestroyWindow(screen);
        screen = NULL;
    }

    window_position_x = 0;
    window_position_y = 0;

    I_InitGraphicsMode();
}

void I_ResetScreen(void)
{
    hires = default_hires;

    I_ResetGraphicsMode();     // Switch to new graphics mode

    if (automapactive)
        AM_Start();        // Reset automap dimensions

    ST_Start();            // Reset palette

    if (gamestate == GS_INTERMISSION)
    {
        WI_slamBackground();
    }

    M_ResetSetupMenuVideo();
}

void I_ShutdownGraphics(void)
{
    if (!(fullscreen && exclusive_fullscreen))
    {
        SDL_GetWindowPosition(screen, &window_position_x, &window_position_y);
    }

    UpdateGrab();
}

void I_InitGraphics(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    I_AtExit(I_ShutdownGraphics, true);

    // Initialize and generate gamma-correction levels.
    I_InitGamma2Table();

    I_InitVideoParms();
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
