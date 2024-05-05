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

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "SDL.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "am_map.h"
#include "config.h"
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_input.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_fixed.h"
#include "m_io.h"
#include "mn_menu.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_voxel.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "miniz.h"

// [FG] set the application icon

#include "icon.c"

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
fixed_t fractionaltic;

boolean dynamic_resolution;

int current_video_height;
static int default_current_video_height;
static int GetCurrentVideoHeight(void);

boolean uncapped;
static boolean default_uncapped;

int custom_fov;

int fps; // [FG] FPS counter widget
boolean resetneeded;
boolean setrefreshneeded;
boolean toggle_fullscreen;
boolean toggle_exclusive_fullscreen;

static boolean use_vsync; // killough 2/8/98: controls whether vsync is called
static boolean correct_aspect_ratio;
static int fpslimit; // when uncapped, limit framerate to this value
static boolean fullscreen;
static boolean exclusive_fullscreen;
static boolean change_display_resolution;
static int widescreen, default_widescreen;
static boolean vga_porch_flash; // emulate VGA "porch" behaviour
static boolean smooth_scaling;
static int video_display = 0; // display index
static boolean disk_icon; // killough 10/98

// [FG] rendering window, renderer, intermediate ARGB frame buffer and texture

static SDL_Window *screen;
static SDL_Renderer *renderer;
static SDL_Surface *screenbuffer;
static SDL_Surface *argbbuffer;
static SDL_Texture *texture;
static SDL_Texture *texture_upscaled;
static SDL_Rect blit_rect = {0};

static int window_x, window_y;
static int window_width, window_height;
static int default_window_width, default_window_height;
static int window_position_x, window_position_y;
static boolean window_resize;
static boolean window_focused = true;
static int scalefactor;

static int actualheight;
static int unscaled_actualheight;

static int max_video_width, max_video_height;
static int max_width, max_height;
static int max_height_adjusted;
static int display_refresh_rate;

static boolean use_limiter;
static int targetrefresh;

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse?
static boolean grabmouse = true, default_grabmouse;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible = true;

boolean drs_skip_frame;

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
    {
        return false;
    }

    // if we specify not to grab the mouse, never grab
    if (!grabmouse)
    {
        return false;
    }

    // when menu is active or game is paused, release the mouse
    if (menuactive || paused)
    {
        return false;
    }

    // only grab mouse when playing levels (but not demos)
    return (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION)
           && !demoplayback && !advancedemo;
}

// [FG] mouse grabbing from Chocolate Doom 3.0

static void SetShowCursor(boolean show)
{
    // When the cursor is hidden, grab the input.
    // Relative mode implicitly hides the cursor.
    SDL_SetRelativeMouseMode(!show);
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
        int w, h;

        SetShowCursor(true);

        SDL_GetWindowSize(screen, &w, &h);
        SDL_WarpMouseInWindow(screen, 3 * w / 4, 4 * h / 5);
    }

    currently_grabbed = grab;
}

void I_ShowMouseCursor(boolean toggle)
{
    SDL_ShowCursor(toggle);
}

void I_ResetRelativeMouseState(void)
{
    SDL_GetRelativeMouseState(NULL, NULL);
}

static void UpdatePriority(void)
{
    const boolean active = (screenvisible && window_focused);
#if defined(_WIN32)
    SetPriorityClass(GetCurrentProcess(), active ? ABOVE_NORMAL_PRIORITY_CLASS
                                                 : NORMAL_PRIORITY_CLASS);
#endif
    SDL_SetThreadPriority(active ? SDL_THREAD_PRIORITY_HIGH
                                 : SDL_THREAD_PRIORITY_NORMAL);
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
            UpdatePriority();
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            UpdatePriority();
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse is
        // removed if we lose focus (such as a popup window appearing), and
        // we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            UpdatePriority();
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            UpdatePriority();
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on
        // and update the video_display config variable.

        case SDL_WINDOWEVENT_RESIZED:
            if (!fullscreen)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);
                SDL_GetWindowPosition(screen, &window_x, &window_y);
            }
            window_resize = true;
            break;

        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                video_display = i;
            }
            break;

        default:
            break;
    }

    drs_skip_frame = true;
}

// [FG] fullscreen toggle from Chocolate Doom 3.0

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN
            || sym->scancode == SDL_SCANCODE_KP_ENTER)
           && (sym->mod & flags) != 0;
}

// Adjust window_width / window_height variables to be an an aspect
// ratio consistent with the aspect_ratio_correct variable.

static void AdjustWindowSize(void)
{
    if (!correct_aspect_ratio)
    {
        return;
    }

    static int old_w, old_h;

    // [FG] window size when returning from fullscreen mode
    if (scalefactor > 0)
    {
        window_width = scalefactor * video.unscaledw;
        window_height = scalefactor * ACTUALHEIGHT;
    }
    else if (old_w > 0 && old_h > 0)
    {
        int rendered_height;

        // rendered height does not necessarily match window height
        if (window_height * old_w > window_width * old_h)
        {
            rendered_height = (window_width * old_h + old_w - 1) / old_w;
        }
        else
        {
            rendered_height = window_height;
        }

        window_width = rendered_height * video.width / actualheight;
    }

    old_w = video.width;
    old_h = actualheight;
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
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowGrab(screen, SDL_TRUE);
    }

    SDL_SetWindowFullscreen(screen, flags);
#ifdef _WIN32
    I_InitWindowIcon();
#endif

    if (!fullscreen)
    {
        SDL_SetWindowGrab(screen, SDL_FALSE);
        AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
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

static void UpdateLimiter(void)
{
    if (uncapped)
    {
        if (fpslimit >= display_refresh_rate && display_refresh_rate > 0
            && use_vsync)
        {
            // SDL will limit framerate using vsync.
            use_limiter = false;
        }
        else if (fpslimit >= TICRATE && targetrefresh > 0)
        {
            use_limiter = true;
        }
        else
        {
            use_limiter = false;
        }
    }
    else
    {
        use_limiter = false;
    }
}

void I_ToggleVsync(void)
{
    SDL_RenderSetVSync(renderer, use_vsync);
    UpdateLimiter();
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
                if (I_UseController())
                {
                    I_HandleJoystickEvent(&sdlevent);
                }
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
void I_StartTic(void)
{
    I_GetEvent();

    if (menuactive)
    {
        if (I_UseController())
        {
            I_UpdateJoystickMenu();
        }

        static event_t ev;
        static int oldx, oldy;
        static SDL_Rect old_rect;
        int x, y, w, h;

        SDL_GetMouseState(&x, &y);

        SDL_GetWindowSize(screen, &w, &h);

        SDL_Rect rect;
        SDL_RenderGetViewport(renderer, &rect);
        if (SDL_RectEquals(&rect, &old_rect))
        {
            ev.data1 = 0;
        }
        else
        {
            old_rect = rect;
            ev.data1 = EV_RESIZE_VIEWPORT;
        }

        float scalex, scaley;
        SDL_RenderGetScale(renderer, &scalex, &scaley);

        int deltax = rect.x * scalex;
        int deltay = rect.y * scaley;

        x = (x - deltax) * video.unscaledw / (w - deltax * 2);
        y = (y - deltay) * SCREENHEIGHT / (h - deltay * 2);

        if (x != oldx || y != oldy)
        {
            oldx = x;
            oldy = y;
        }
        else
        {
            return;
        }

        ev.type = ev_mouse_state;
        ev.data2 = x;
        ev.data3 = y;

        D_PostEvent(&ev);
        return;
    }

    if (window_focused)
    {
        I_ReadMouse();
    }

    if (I_UseController())
    {
        I_UpdateJoystick(true);
    }
}

void I_StartDisplay(void)
{
    SDL_PumpEvents();

    if (window_focused)
    {
        I_ReadMouse();
    }

    if (I_UseController())
    {
        I_UpdateJoystick(false);
    }
}

//
// I_StartFrame
//
void I_StartFrame(void)
{
    ;
}

static void UpdateRender(void)
{
    SDL_LowerBlit(screenbuffer, &blit_rect, argbbuffer, &blit_rect);
    SDL_UpdateTexture(texture, &blit_rect, argbbuffer->pixels,
                      argbbuffer->pitch);
    SDL_RenderClear(renderer);

    if (texture_upscaled)
    {
        // Render this intermediate texture into the upscaled texture
        // using "nearest" integer scaling.

        SDL_SetRenderTarget(renderer, texture_upscaled);
        SDL_RenderCopy(renderer, texture, &blit_rect, NULL);

        // Finally, render this upscaled texture to screen using linear scaling.

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);
    }
    else
    {
        SDL_RenderCopy(renderer, texture, &blit_rect, NULL);
    }
}

static uint64_t frametime_start, frametime_withoutpresent;

static void ResetResolution(int height, boolean reset_pitch);
static void ResetLogicalSize(void);

void I_DynamicResolution(void)
{
    if (!dynamic_resolution || current_video_height <= DRS_MIN_HEIGHT
        || frametime_withoutpresent == 0 || targetrefresh <= 0
        || menuactive)
    {
        return;
    }

    if (drs_skip_frame)
    {
        frametime_start = frametime_withoutpresent = 0;
        drs_skip_frame = false;
        return;
    }

    static int frame_counter;
    static double averagepercent;

    // 1.25 milliseconds for SDL render present
    double target = (1.0 / targetrefresh) - 0.00125;
    double actual = frametime_withoutpresent / 1000000.0;

    double actualpercent = actual / target;

    #define DRS_DELTA   0.1
    #define DRS_GREATER (1 + DRS_DELTA)
    #define DRS_LESS    (1 - DRS_DELTA / 10.0)
    #define DRS_STEP    (SCREENHEIGHT / 2)

    int newheight = 0;
    int oldheight = video.height;

    // Decrease the resolution quickly, increase only when the average frame
    // time is stable for the `targetrefresh` number of frames.

    frame_counter++;
    averagepercent = (averagepercent + actualpercent) / frame_counter;

    if (actualpercent > DRS_GREATER)
    {
        double reduction = (actualpercent - DRS_GREATER) * 0.4;
        newheight = (int)MAX(DRS_MIN_HEIGHT, oldheight - oldheight * reduction);
    }
    else if (averagepercent < DRS_LESS && frame_counter > targetrefresh)
    {
        double addition = (DRS_LESS - averagepercent) * 0.25;
        newheight = (int)MIN(current_video_height, oldheight + oldheight * addition);
    }
    else
    {
        return;
    }

    frame_counter = 0;

    int mul = (newheight + (DRS_STEP - 1)) / DRS_STEP;  // integer round

    newheight = mul * DRS_STEP;

    if (newheight > current_video_height)
    {
        newheight -= DRS_STEP;
    }

    if (newheight == oldheight)
    {
        return;
    }

    if (newheight < oldheight)
    {
        VX_DecreaseMaxDist();
    }
    else
    {
        VX_IncreaseMaxDist();
    }

    ResetResolution(newheight, false);
    ResetLogicalSize();
}

static void I_DrawDiskIcon(), I_RestoreDiskBackground();
static unsigned int disk_to_draw, disk_to_restore;

static void CreateUpscaledTexture(boolean force);
static void I_ResetTargetRefresh(void);

void I_FinishUpdate(void)
{
    if (noblit)
    {
        return;
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

    UpdateGrab();

    // [FG] [AM] Real FPS counter
    if (frametime_start)
    {
        static uint64_t last_time;
        uint64_t time;
        static int frame_counter;

        frame_counter++;

        time = frametime_start - last_time;

        // Update FPS counter every second
        if (time >= 1000000)
        {
            fps = (frame_counter * 1000000) / time;
            frame_counter = 0;
            last_time = frametime_start;
        }
    }

    I_DrawDiskIcon();

    UpdateRender();

    if (frametime_start)
    {
        frametime_withoutpresent = I_GetTimeUS() - frametime_start;
    }

    SDL_RenderPresent(renderer);

    I_RestoreDiskBackground();

    if (window_resize)
    {
        if (smooth_scaling)
        {
            CreateUpscaledTexture(false);
        }
        window_resize = false;
    }

    if (use_limiter)
    {
        uint64_t target_time = 1000000ull / targetrefresh;

        while (true)
        {
            uint64_t current_time = I_GetTimeUS();
            uint64_t elapsed_time = current_time - frametime_start;
            uint64_t remaining_time = 0;

            if (elapsed_time >= target_time)
            {
                frametime_start = current_time;
                break;
            }

            remaining_time = target_time - elapsed_time;

            if (remaining_time > 1000)
            {
                I_Sleep((remaining_time - 1000) / 1000);
            }
        }
    }
    else
    {
        frametime_start = I_GetTimeUS();
    }

    if (setrefreshneeded)
    {
        setrefreshneeded = false;
        I_ResetTargetRefresh();
    }
}

//
// I_ReadScreen
//

void I_ReadScreen(byte *dst)
{
    V_GetBlock(0, 0, video.width, video.height, dst);
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
    {
        return;
    }

    if (disk_to_draw >= DISK_ICON_THRESHOLD)
    {
        V_GetBlock(video.width - disk.sw, video.height - disk.sh, disk.sw,
                   disk.sh, old_data);
        V_PutBlock(video.width - disk.sw, video.height - disk.sh, disk.sw,
                   disk.sh, diskflash);

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
    {
        return;
    }

    if (disk_to_restore)
    {
        V_PutBlock(video.width - disk.sw, video.height - disk.sh, disk.sw,
                   disk.sh, old_data);

        disk_to_restore = 0;
    }

    disk_to_draw = 0;
}

#include "i_gamma.h"

int gamma2;

void I_SetPalette(byte *palette)
{
    // haleyjd
    int i;
    const byte *const gamma = gammatable[gamma2];
    SDL_Color colors[256];

    if (noblit) // killough 8/11/98
    {
        return;
    }

    for (i = 0; i < 256; ++i)
    {
        colors[i].r = gamma[*palette++];
        colors[i].g = gamma[*palette++];
        colors[i].b = gamma[*palette++];
    }

    SDL_SetPaletteColors(screenbuffer->format->palette, colors, 0, 256);

    if (vga_porch_flash)
    {
        // "flash" the pillars/letterboxes with palette changes,
        // emulating VGA "porch" behaviour
        SDL_SetRenderDrawColor(renderer, colors[0].r, colors[0].g, colors[0].b,
                               SDL_ALPHA_OPAQUE);
    }
}

// Taken from Chocolate Doom chocolate-doom/src/i_video.c:L841-867

byte I_GetPaletteIndex(byte *palette, int r, int g, int b)
{
    byte best;
    int best_diff, diff;
    int i;

    best = 0;
    best_diff = INT_MAX;

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

    UpdateRender();

    // [FG] adjust cropping rectangle if necessary
    SDL_GetRendererOutputSize(renderer, &rect.w, &rect.h);
    if (rect.w * actualheight > rect.h * video.width)
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

        png = tdefl_write_image_to_png_file_in_memory(
            pixels, rect.w, rect.h, format->BytesPerPixel, &size);

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

    surface = SDL_CreateRGBSurfaceFrom((void *)icon_data, icon_w, icon_h, 32,
                                       icon_w * 4, 0xffu << 24, 0xffu << 16,
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
        I_Printf(VB_WARNING,
                 "CenterWindow: Failed to read display bounds "
                 "for display #%d!",
                 video_display);
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

static double CurrentAspectRatio(void)
{
    int w, h;

    switch (widescreen)
    {
        case RATIO_ORIG:
            w = SCREENWIDTH;
            h = unscaled_actualheight;
            break;
        case RATIO_AUTO:
            w = max_width;
            h = max_height;
            break;
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
            w = 16;
            h = 9;
            break;
    }

    double aspect_ratio = (double)w / (double)h;

    aspect_ratio = BETWEEN(ASPECT_RATIO_MIN, ASPECT_RATIO_MAX, aspect_ratio);

    return aspect_ratio;
}

static void ResetResolution(int height, boolean reset_pitch)
{
    double aspect_ratio = CurrentAspectRatio();

    actualheight = correct_aspect_ratio ? (int)(height * 1.2) : height;
    video.height = height;

    video.unscaledw = (int)(unscaled_actualheight * aspect_ratio);

    // Unscaled widescreen 16:9 resolution truncates to 426x240, which is not
    // quite 16:9. To avoid visual instability, we calculate the scaled width
    // without the actual aspect ratio. For example, at 1280x720 we get
    // 1278x720.

    double vertscale = (double)actualheight / (double)unscaled_actualheight;
    video.width = (int)ceil(video.unscaledw * vertscale);

    // [FG] For performance reasons, SDL2 insists that the screen pitch, i.e.
    // the *number of bytes* that one horizontal row of pixels occupy in
    // memory, must be a multiple of 4.

    if (reset_pitch)
    {
        video.pitch = (video.width + 3) & ~3;
    }

    video.deltaw = (video.unscaledw - NONWIDEWIDTH) / 2;

    Z_FreeTag(PU_VALLOC);

    V_Init();
    R_InitVisplanesRes();
    R_SetFuzzColumnMode();
    setsizeneeded = true; // run R_ExecuteSetViewSize

    if (automapactive)
    {
        AM_ResetScreenSize();
    }

    I_Printf(VB_DEBUG, "ResetResolution: %dx%d", video.width, video.height);

    drs_skip_frame = true;
}

static void DestroyUpscaledTexture(void)
{
    if (texture_upscaled)
    {
        SDL_DestroyTexture(texture_upscaled);
        texture_upscaled = NULL;
    }
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

    DestroyUpscaledTexture();

    if (w_upscale == 1)
    {
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        return;
    }
    else
    {
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    }

    // Set the scaling quality for rendering the upscaled texture
    // to "linear", which looks much softer and smoother than "nearest"
    // but does a better job at downscaling from the upscaled texture to
    // screen.

    texture_upscaled = SDL_CreateTexture(
        renderer, SDL_GetWindowPixelFormat(screen), SDL_TEXTUREACCESS_TARGET,
        w_upscale * screen_width, h_upscale * screen_height);

    SDL_SetTextureScaleMode(texture_upscaled, SDL_ScaleModeLinear);
}

static void ResetLogicalSize(void)
{
    blit_rect.w = video.width;
    blit_rect.h = video.height;

    if (SDL_RenderSetLogicalSize(renderer, video.width, actualheight))
    {
        I_Printf(VB_ERROR, "Failed to set logical size: %s", SDL_GetError());
    }

    if (smooth_scaling)
    {
        CreateUpscaledTexture(true);
    }
    else
    {
        DestroyUpscaledTexture();
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    }
}

static void I_ResetTargetRefresh(void)
{
    uncapped = default_uncapped;

    if (fpslimit < TICRATE)
    {
        fpslimit = 0;
    }

    if (uncapped)
    {
        // SDL may report native refresh rate as zero.
        targetrefresh = (fpslimit >= TICRATE) ? fpslimit : display_refresh_rate;
    }
    else
    {
        targetrefresh = TICRATE;
    }

    UpdateLimiter();
    MN_UpdateFpsLimitItem();
    drs_skip_frame = true;
}

//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitVideoParms(void)
{
    int p, tmp_scalefactor;
    SDL_DisplayMode mode;

    I_ResetInvalidDisplayIndex();
    if (SDL_GetCurrentDisplayMode(video_display, &mode))
    {
        I_Error("Error getting display mode: %s", SDL_GetError());
    }

    if (max_video_width && max_video_height)
    {
        if (correct_aspect_ratio && max_video_height < ACTUALHEIGHT)
        {
            I_Error("The vertical resolution is too low, turn off the aspect "
                    "ratio correction.");
        }
        double aspect_ratio =
            (double)max_video_width / (double)max_video_height;
        if (aspect_ratio < ASPECT_RATIO_MIN)
        {
            I_Error("Aspect ratio not supported, set other resolution");
        }
        max_width = max_video_width;
        max_height = max_video_height;
    }
    else
    {
        max_width = mode.w;
        max_height = mode.h;
    }

    if (correct_aspect_ratio)
    {
        max_height_adjusted = (int)(max_height / 1.2);
        unscaled_actualheight = ACTUALHEIGHT;
    }
    else
    {
        max_height_adjusted = max_height;
        unscaled_actualheight = SCREENHEIGHT;
    }

    // SDL may report native refresh rate as zero.
    display_refresh_rate = mode.refresh_rate;

    current_video_height = default_current_video_height;
    window_width = default_window_width;
    window_height = default_window_height;

    widescreen = default_widescreen;
    uncapped = default_uncapped;
    grabmouse = default_grabmouse;
    I_ResetTargetRefresh();

    //!
    // @category video
    //
    // Enables uncapped framerate.
    //

    if (M_CheckParm("-uncapped"))
    {
        uncapped = true;
    }

    //!
    // @category video
    //
    // Disables uncapped framerate.
    //

    else if (M_CheckParm("-nouncapped"))
    {
        uncapped = false;
    }

    if (M_CheckParm("-grabmouse"))
    {
        grabmouse = true;
    }

    //!
    // @category video
    //
    // Don't grab the mouse when running in windowed mode.
    //

    else if (M_CheckParm("-nograbmouse"))
    {
        grabmouse = false;
    }

    //!
    // @category video
    //
    // Don't scale up the screen. Implies -window.
    //

    if ((p = M_CheckParm("-1")))
    {
        tmp_scalefactor = 1;
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size. Implies -window.
    //

    else if ((p = M_CheckParm("-2")))
    {
        tmp_scalefactor = 2;
    }

    //!
    // @category video
    //
    // Triple up the screen to 3x its normal size. Implies -window.
    //

    else if ((p = M_CheckParm("-3")))
    {
        tmp_scalefactor = 3;
    }

    // -skipsec can take a negative number as a parameter
    if (p && strcasecmp("-skipsec", myargv[p - 1]))
    {
        scalefactor = tmp_scalefactor;
        GetCurrentVideoHeight();
        MN_DisableResolutionScaleItem();
    }

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

    MN_UpdateFpsLimitItem();
    MN_UpdateDynamicResolutionItem();
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
            if (change_display_resolution && max_video_width
                && max_video_height)
            {
                w = max_video_width;
                h = max_video_height;
            }
            else
            {
                SDL_DisplayMode mode;
                if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
                {
                    I_Error("Could not get display mode for video display #%d: %s",
                            video_display, SDL_GetError());
                }
                w = mode.w;
                h = mode.h;
            }
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
        I_Error("Error creating window for video startup: %s", SDL_GetError());
    }

    I_InitWindowIcon();

    if (fullscreen)
    {
        SDL_SetWindowGrab(screen, SDL_TRUE);
    }

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

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0)
    {
        I_Printf(VB_DEBUG, "SDL render driver: %s", info.name);
    }

    UpdateLimiter();
}

void I_GetResolutionScaling(resolution_scaling_t *rs)
{
    rs->max = max_height_adjusted;
    rs->step = 50;
}

static int GetCurrentVideoHeight(void)
{
    if (scalefactor > 0)
    {
        current_video_height = scalefactor * SCREENHEIGHT;
    }

    current_video_height =
        BETWEEN(SCREENHEIGHT, max_height_adjusted, current_video_height);

    return current_video_height;
}

static void CreateSurfaces(int w, int h)
{
    // [FG] create paletted frame buffer

    if (screenbuffer != NULL)
    {
        SDL_FreeSurface(screenbuffer);
    }

    screenbuffer = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
    SDL_FillRect(screenbuffer, NULL, 0);

    I_VideoBuffer = screenbuffer->pixels;
    V_RestoreBuffer();

    if (argbbuffer != NULL)
    {
        SDL_FreeSurface(argbbuffer);
    }

    // [FG] create intermediate ARGB frame buffer
    {
        uint32_t rmask, gmask, bmask, amask;
        int bpp;

        SDL_PixelFormatEnumToMasks(SDL_GetWindowPixelFormat(screen), &bpp,
                                   &rmask, &gmask, &bmask, &amask);
        argbbuffer =
            SDL_CreateRGBSurface(0, w, h, bpp, rmask, gmask, bmask, amask);
        SDL_FillRect(argbbuffer, NULL, 0);
    }

    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));

    // [FG] create texture

    if (texture != NULL)
    {
        SDL_DestroyTexture(texture);
    }

    texture = SDL_CreateTexture(renderer, SDL_GetWindowPixelFormat(screen),
                                SDL_TEXTUREACCESS_STREAMING, w, h);

    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    R_InitAnyRes();
    ST_InitRes();

    I_InitDiskFlash();

    int n = (scalefactor == 1 ? 1 : 2);
    SDL_SetWindowMinimumSize(screen, video.unscaledw * n,
                             correct_aspect_ratio ? ACTUALHEIGHT * n : SCREENHEIGHT * n);

    if (!fullscreen)
    {
        AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }
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
    ResetResolution(GetCurrentVideoHeight(), true);
    CreateSurfaces(video.pitch, video.height);
    ResetLogicalSize();
}

void I_ResetScreen(void)
{
    resetneeded = false;

    widescreen = default_widescreen;

    ResetResolution(GetCurrentVideoHeight(), true);
    CreateSurfaces(video.pitch, video.height);
    ResetLogicalSize();
}

void I_ShutdownGraphics(void)
{
    if (!(fullscreen && exclusive_fullscreen))
    {
        SDL_GetWindowPosition(screen, &window_position_x, &window_position_y);
    }

    if (scalefactor == 0)
    {
        default_window_width = window_width;
        default_window_height = window_height;
        default_current_video_height = current_video_height;
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

    I_InitVideoParms();
    I_InitGraphicsMode(); // killough 10/98
    ResetResolution(GetCurrentVideoHeight(), true);
    CreateSurfaces(video.pitch, video.height);
    ResetLogicalSize();

    // clear out events waiting at the start and center the mouse
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_MOUSEMOTION);
    I_ResetRelativeMouseState();
}

void I_BindVideoVariables(void)
{
    BIND2_NUM(current_video_height, 600, SCREENHEIGHT, UL,
        "Vertical resolution (600p by default)");
    BIND_BOOL_GEN(dynamic_resolution, true, "1 to enable dynamic resolution");
    BIND_BOOL(correct_aspect_ratio, true, "1 to perform aspect ratio correction");
    BIND_BOOL(fullscreen, true, "1 to enable fullscreen");
    BIND_BOOL(exclusive_fullscreen, false, "1 to enable exclusive fullscreen");
    BIND_BOOL_GEN(use_vsync, true,
        "1 to enable wait for vsync to avoid display tearing");
    BIND2_BOOL_GEN(uncapped, true, "1 to enable uncapped rendering frame rate");
    BIND_NUM_GEN(fpslimit, 0, 0, 500,
        "Framerate limit in frames per second (< 35 = disable)");
    BIND2_NUM_GEN(widescreen, RATIO_AUTO, 0, NUM_RATIOS - 1,
        "Widescreen (0 = Off, 1 = Auto, 2 = 16:10, 3 = 16:9, 4 = 21:9)");
    M_BindNum("fov", &custom_fov, NULL, FOV_DEFAULT, FOV_MIN, FOV_MAX,
        ss_gen, wad_no, "Field of view in degrees");
    BIND_NUM_GEN(gamma2, 9, 0, 17, "Custom gamma level (0 = -4, 9 = 0, 17 = 4)");
    BIND_BOOL_GEN(smooth_scaling, true, "1 to enable smooth pixel scaling");

    BIND_BOOL(vga_porch_flash, false, "1 to emulate VGA \"porch\" behaviour");
    BIND_BOOL(disk_icon, false, "1 to enable flashing icon during disk IO");
    BIND_NUM(video_display, 0, 0, UL, "Current video display index");
    BIND_NUM(max_video_width, 0, SCREENWIDTH, UL,
        "Maximum horizontal resolution (native by default)");
    BIND_NUM(max_video_height, 0, SCREENHEIGHT, UL,
        "Maximum vertical resolution (native by default)");
    BIND_BOOL(change_display_resolution, false,
        "1 to change display resolution with exclusive fullscreen (only useful for CRTs)");
    BIND_NUM(window_position_x, 0, UL, UL, "Window position X (0 = Center)");
    BIND_NUM(window_position_y, 0, UL, UL, "Window position Y (0 = Center)");
    BIND2_NUM(window_width, 1065, 0, UL, "Window width");
    BIND2_NUM(window_height, 600, 0, UL, "Window height");

    BIND2_BOOL(grabmouse, true, "1 to grab mouse during play");
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
