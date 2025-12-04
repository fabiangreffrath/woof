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

#include <SDL3/SDL.h>

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "am_map.h"
#include "config.h"
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_exit.h"
#include "i_input.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_fixed.h"
#include "m_io.h"
#include "m_misc.h"
#include "mn_menu.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_voxel.h"
#include "st_stuff.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "spng.h"

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

static boolean use_vsync; // killough 2/8/98: controls whether vsync is called
boolean correct_aspect_ratio;
static int fpslimit; // when uncapped, limit framerate to this value
static boolean fullscreen;
static boolean vga_porch_flash; // emulate VGA "porch" behaviour
static boolean smooth_scaling;
static int video_display = 0; // display index
static SDL_DisplayID video_display_id; // display instance id
static boolean disk_icon; // killough 10/98

typedef enum
{
    RATIO_ORIG,
    RATIO_AUTO,
    RATIO_16_10,
    RATIO_16_9,
    RATIO_21_9,
    RATIO_32_9,
    NUM_RATIOS
} aspect_ratio_mode_t;

static aspect_ratio_mode_t widescreen, default_widescreen;

// [FG] rendering window, renderer, intermediate ARGB frame buffer and texture

static SDL_Window *screen;
static SDL_Renderer *renderer;
static SDL_Palette *palette;
static SDL_Texture *texture;
static SDL_Rect rect = {0};
static SDL_FRect frect = {0.0f};

static int window_x, window_y;
static int window_width, window_height;
static int default_window_width, default_window_height;
static int window_position_x, window_position_y;
static boolean window_focused = true;
static int scalefactor;

static int actualheight;
static int unscaled_actualheight;

static int max_video_width, max_video_height;
static int max_width, max_height;
static int max_height_adjusted;
static float display_refresh_rate;

static boolean use_limiter;
static float targetrefresh;

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse?
static boolean grabmouse = true, default_grabmouse;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible = true;

static boolean drs_skip_frame;

void *I_GetSDLWindow(void)
{
    return screen;
}

void *I_GetSDLRenderer(void)
{
    return renderer;
}

static int GetDisplayIndexFromID(SDL_DisplayID display_id)
{
    int num_displays = 0;
    SDL_DisplayID *display_ids = SDL_GetDisplays(&num_displays);
    int display_index = 0;

    if (!display_ids)
    {
        I_Error("Failed to get display list: %s", SDL_GetError());
    }

    for (int i = 0; i < num_displays; i++)
    {
        if (display_ids[i] == display_id)
        {
            display_index = i;
            break;
        }
    }

    SDL_free(display_ids);
    return display_index;
}

static SDL_DisplayID GetDisplayIDFromIndex(int display_index)
{
    int num_displays = 0;
    SDL_DisplayID *display_ids = SDL_GetDisplays(&num_displays);
    SDL_DisplayID display_id = 0;

    if (!display_ids)
    {
        I_Error("Failed to get display list: %s", SDL_GetError());
    }

    if (display_index >= 0 && display_index < num_displays)
    {
        display_id = display_ids[display_index];
    }

    SDL_free(display_ids);
    return display_id;
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
    SDL_SetWindowRelativeMouseMode(screen, !show);
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
    if (toggle)
    {
        SDL_ShowCursor();
    }
    else
    {
        SDL_HideCursor();
    }
}

void I_ResetRelativeMouseState(void)
{
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_EVENT_MOUSE_MOTION);
    SDL_GetRelativeMouseState(NULL, NULL);
}

void I_UpdatePriority(boolean active)
{
#if defined(_WIN32)
    SetPriorityClass(GetCurrentProcess(), active ? ABOVE_NORMAL_PRIORITY_CLASS
                                                 : NORMAL_PRIORITY_CLASS);
#endif
    SDL_SetCurrentThreadPriority(active ? SDL_THREAD_PRIORITY_HIGH
                                 : SDL_THREAD_PRIORITY_NORMAL);
}

static void UpdateDisplayIndex(void)
{
    SDL_DisplayID current_id = SDL_GetDisplayForWindow(screen);

    if (current_id == 0)
    {
        I_Error("Failed to get display: %s", SDL_GetError());
    }

    if (video_display_id != current_id)
    {
        video_display_id = current_id;
        video_display = GetDisplayIndexFromID(video_display_id);
    }
}

// [FG] window event handling from Chocolate Doom 3.0

static void HandleWindowEvent(SDL_WindowEvent *event)
{
    switch (event->type)
    {
        // Don't render the screen when the window is minimized:

        case SDL_EVENT_WINDOW_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse is
        // removed if we lose focus (such as a popup window appearing), and
        // we dont move the mouse around if we aren't focused either.

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            window_focused = true;
            I_UpdatePriority(true);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            window_focused = false;
            I_UpdatePriority(false);
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on
        // and update the video_display config variable.

        case SDL_EVENT_WINDOW_RESIZED:
            if (!fullscreen)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);
            }
            break;

        case SDL_EVENT_WINDOW_MOVED:
            UpdateDisplayIndex();
            break;

        default:
            break;
    }

    I_ResetDRS();
}

// [FG] fullscreen toggle from Chocolate Doom 3.0

static boolean ToggleFullScreenKeyShortcut(SDL_Scancode scancode)
{
    Uint16 flags = (SDL_KMOD_LALT | SDL_KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (SDL_KMOD_LGUI | SDL_KMOD_RGUI);
#endif
    return (scancode == SDL_SCANCODE_RETURN
            || scancode == SDL_SCANCODE_KP_ENTER)
           && (SDL_GetModState() & flags) != 0;
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

static void I_ToggleFullScreen(void)
{
    if (fullscreen)
    {
        SDL_SetWindowMouseGrab(screen, true);
        SDL_SetWindowResizable(screen, false);
        SDL_SetWindowFullscreenMode(screen, NULL);
    }
    else
    {
        SDL_SetWindowMouseGrab(screen, false);
        SDL_SetWindowResizable(screen, true);
        SDL_SetWindowBordered(screen, true);
        AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }

    SDL_SetWindowFullscreen(screen, fullscreen);
    SDL_SyncWindow(screen);
}

static void UpdateLimiter(void)
{
    if (uncapped)
    {
        if (fpslimit >= display_refresh_rate && display_refresh_rate > 0.0f
            && use_vsync)
        {
            // SDL will limit framerate using vsync.
            use_limiter = false;
        }
        else if (fpslimit >= TICRATE && targetrefresh > 0.0f)
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
    SDL_SetRenderVSync(renderer, use_vsync);
    UpdateLimiter();
}

static void ProcessEvent(SDL_Event *ev)
{
    switch (ev->type)
    {
        case SDL_EVENT_KEY_DOWN:
            if (ToggleFullScreenKeyShortcut(ev->key.scancode))
            {
                fullscreen = !fullscreen;
                toggle_fullscreen = true;
                break;
            }
            // deliberate fall-though

        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
            I_HandleKeyboardEvent(ev);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            if (window_focused)
            {
                I_HandleMouseEvent(ev);
            }
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            I_OpenGamepad(ev->gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            I_CloseGamepad(ev->gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            if (I_UseGamepad())
            {
                I_HandleGamepadEvent(ev, menuactive);
            }
            break;

        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            if (I_UseGamepad())
            {
                I_HandleSensorEvent(ev);
            }
            break;

        case SDL_EVENT_QUIT:
            fast_exit = true;
            I_SafeExit(0);
            break;

        default:
            if (ev->type >= SDL_EVENT_WINDOW_FIRST
                && ev->type <= SDL_EVENT_WINDOW_LAST
                && ev->window.windowID == SDL_GetWindowID(screen))
            {
                HandleWindowEvent(&ev->window);
            }
            break;
    }
}

static void I_GetEvent(void)
{
    #define NUM_PEEP 32
    static SDL_Event sdlevents[NUM_PEEP];

    I_DelayEvent();

    SDL_PumpEvents();

    while (true)
    {
        const int num_events = SDL_PeepEvents(sdlevents, NUM_PEEP, SDL_GETEVENT,
                                              SDL_EVENT_FIRST, SDL_EVENT_LAST);

        if (num_events < 1)
        {
            break;
        }

        for (int i = 0; i < num_events; i++)
        {
            ProcessEvent(&sdlevents[i]);
        }
    }
}

static void UpdateMouseMenu(void)
{
    static event_t ev;
    static int oldx, oldy;
    static SDL_Rect old_rect;
    int x, y, w, h;

    float outx, outy;
    SDL_GetMouseState(&outx, &outy);
    x = (int)outx;
    y = (int)outy;

    SDL_GetWindowSize(screen, &w, &h);

    SDL_Rect rect;
    SDL_GetRenderViewport(renderer, &rect);
    if (SDL_RectsEqual(&rect, &old_rect))
    {
        ev.data1.i = 0;
    }
    else
    {
        old_rect = rect;
        ev.data1.i = EV_RESIZE_VIEWPORT;
    }

    float scalex, scaley;
    SDL_GetRenderScale(renderer, &scalex, &scaley);

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
    ev.data2.i = x;
    ev.data3.i = y;

    D_PostEvent(&ev);
}

//
// I_StartTic
//
void I_StartTic(void)
{
    I_GetEvent();

    if (menuactive)
    {
        UpdateMouseMenu();

        if (I_UseGamepad())
        {
            I_UpdateGamepad(ev_joystick_state, true);
        }
    }
    else
    {
        if (window_focused)
        {
            I_ReadMouse();
        }

        if (I_UseGamepad())
        {
            I_ReadGyro();
            I_UpdateGamepad(ev_joystick, true);
        }
    }
}

void I_StartDisplay(void)
{
    SDL_PumpEvents();

    if (window_focused)
    {
        I_ReadMouse();
    }

    if (I_UseGamepad())
    {
        I_ReadGyro();
        I_UpdateGamepad(ev_joystick, false);
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
    // When using SDL_LockTexture, the pixels made available for editing may not
    // contain the original texture data. We have to maintain a copy of the
    // video buffer in order to emulate HOM effects.
    void *pixels;
    int dst_pitch;
    SDL_LockTexture(texture, &rect, &pixels, &dst_pitch);
    int h = rect.h;
    int src_pitch = video.width;
    pixel_t *dst = pixels;
    pixel_t *src = I_VideoBuffer;
    while (h--)
    {
        memcpy(dst, src, src_pitch);
        dst += dst_pitch;        
        src += src_pitch;
    }
    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, &frect, NULL);
}

static uint64_t frametime_start, frametime_withoutpresent;

static void ResetResolution(int height);
static void ResetLogicalSize(void);

#define DRS_FRAME_HISTORY 60

typedef struct
{
    double history[DRS_FRAME_HISTORY];
    int history_index;
    int history_frames;
    int cooldown_counter;
    int cooldown_frames;
} drs_t;

static drs_t drs;

void I_ResetDRS(void)
{
    memset(drs.history, 0, sizeof(drs.history));
    drs.history_index = 0;
    drs.history_frames = MIN(DRS_FRAME_HISTORY, (int)(targetrefresh / 2.0f));
    drs.cooldown_counter = 0;
    drs.cooldown_frames = drs.history_frames;
    drs_skip_frame = true;
}

void I_DynamicResolution(void)
{
    if (!dynamic_resolution || current_video_height <= DRS_MIN_HEIGHT
        || frametime_withoutpresent == 0 || menuactive)
    {
        return;
    }

    if (drs_skip_frame)
    {
        frametime_start = frametime_withoutpresent = 0;
        drs_skip_frame = false;
        return;
    }

    if (drs.cooldown_counter > 0)
    {
        --drs.cooldown_counter;
        return;
    }

    // 1.25 milliseconds for SDL render present
    double target = (1.0 / targetrefresh) - 0.00125;
    double actual = frametime_withoutpresent / 1000000.0;

    drs.history[drs.history_index] = actual;
    drs.history_index = (drs.history_index + 1) % drs.history_frames;
    double total = 0;
    for (int i = 0; i < drs.history_frames; ++i)
    {
        total += drs.history[i];
    }
    const double avg_frame_time = total / drs.history_frames;
    const double performance_ratio = avg_frame_time / target;

    static boolean needs_upscale;

    #define DRS_STEP         (SCREENHEIGHT / 2)
    #define DRS_DOWNSCALE_T1 1.08
    #define DRS_DOWNSCALE_T2 1.2   // 20% over target -> 2x step
    #define DRS_DOWNSCALE_T3 1.5   // 50% over target -> 3x step
    #define DRS_UPSCALE_T1   0.6
    #define DRS_UPSCALE_T2   0.4

    int oldheight = video.height;
    int newheight = 0;
    int step_multipler = 1;

    if (performance_ratio > DRS_DOWNSCALE_T1)
    {
        if (performance_ratio > DRS_DOWNSCALE_T3)
        {
            step_multipler = 3;
        }
        else if (performance_ratio > DRS_DOWNSCALE_T2)
        {
            step_multipler = 2;
        }

        newheight = MAX(DRS_MIN_HEIGHT, oldheight - DRS_STEP * step_multipler);
    }
    else if (performance_ratio < DRS_UPSCALE_T1 && needs_upscale)
    {
        if (performance_ratio < DRS_UPSCALE_T2)
        {
            step_multipler = 2;
        }

        newheight =
            MIN(current_video_height, oldheight + DRS_STEP * step_multipler);
    }
    else
    {
        return;
    }

    if (newheight < current_video_height)
    {
        int mul = (newheight + (DRS_STEP - 1)) / DRS_STEP;
        newheight = mul * DRS_STEP;
    }

    if (newheight == oldheight)
    {
        return;
    }

    needs_upscale = newheight < current_video_height;

    drs.cooldown_counter = drs.cooldown_frames;

    if (newheight < oldheight)
    {
        VX_DecreaseMaxDist(step_multipler);
    }
    else
    {
        VX_IncreaseMaxDist(step_multipler);
    }

    ResetResolution(newheight);
    ResetLogicalSize();
}

static void I_DrawDiskIcon(), I_RestoreDiskBackground();
static unsigned int disk_to_draw, disk_to_restore;

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
            fps = ((uint64_t)frame_counter * 1000000) / time;
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

    if (use_limiter)
    {
        uint64_t target_time = (uint64_t)(1000000.0f / targetrefresh);

        while (true)
        {
            uint64_t current_time = I_GetTimeUS();
            int64_t elapsed_time = current_time - frametime_start;

            if (elapsed_time >= target_time)
            {
                frametime_start = current_time;
                break;
            }

            int64_t remaining_time = target_time - elapsed_time;

            if (remaining_time > 1000ull)
            {
                I_SleepUS(500ull);
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

void I_ReadScreen(pixel_t *dst)
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
    V_DrawPatch(-video.deltaw, 0, V_CachePatchName("STDISK", PU_CACHE));
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

void I_SetPalette(byte *playpal)
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
        colors[i].r = gamma[*playpal++];
        colors[i].g = gamma[*playpal++];
        colors[i].b = gamma[*playpal++];
        colors[i].a = 0xffu;
    }

    SDL_SetPaletteColors(palette, colors, 0, 256);

    if (vga_porch_flash)
    {
        // "flash" the pillars/letterboxes with palette changes,
        // emulating VGA "porch" behaviour
        SDL_SetRenderDrawColor(renderer, colors[0].r, colors[0].g, colors[0].b,
                               SDL_ALPHA_OPAQUE);
    }
}

// Taken from Chocolate Doom chocolate-doom/src/i_video.c:L841-867

byte I_GetNearestColor(byte *palette, int r, int g, int b)
{
    byte best;
    int best_diff, diff;
    int i, dr, dg, db;

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        dr = r - *palette++;
        dg = g - *palette++;
        db = b - *palette++;

        diff = dr * dr + dg * dg + db * db;

        if (diff < best_diff)
        {
            if (!diff)
            {
                return i;
            }

            best = i;
            best_diff = diff;
        }
    }

    return best;
}

// [FG] save screenshots in PNG format
boolean I_WritePNGfile(char *filename)
{
    UpdateRender();

    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    if (surface == NULL)
    {
        I_Printf(VB_ERROR, "I_WritePNGfile: SDL_RenderReadPixels failed: %s",
                 SDL_GetError());
        return false;
    }
    int pitch = surface->w * 3;
    int size = surface->h * pitch;
    void *pixels = malloc(size);
    if (!SDL_ConvertPixels(surface->w, surface->h,
                           surface->format, surface->pixels,
                           surface->pitch, SDL_PIXELFORMAT_RGB24, pixels,
                           pitch))
    {
        I_Printf(VB_ERROR, "I_WritePNGfile: SDL_ConvertPixels failed: %s",
                 SDL_GetError());
        free(pixels);
        SDL_DestroySurface(surface);
        return false;
    }

    FILE *file = M_fopen(filename, "wb");
    if (!file)
    {
        free(pixels);
        SDL_DestroySurface(surface);
        return false;
    }

    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    spng_set_png_file(ctx, file);
    spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, 1);

    struct spng_ihdr ihdr = {0};
    ihdr.width = surface->w;
    ihdr.height = surface->h;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR;
    ihdr.bit_depth = 8;
    spng_set_ihdr(ctx, &ihdr);

    int ret = spng_encode_image(ctx, pixels, size, SPNG_FMT_PNG,
                                SPNG_ENCODE_FINALIZE);
    if (ret)
    {
        I_Printf(VB_ERROR, "I_WritePNGfile: spng_encode_image failed: %s\n",
                 spng_strerror(ret));
    }
    else
    {
        I_Printf(VB_INFO, "I_WritePNGfile: %s", filename);
    }

    fclose(file);

    spng_ctx_free(ctx);
    free(pixels);
    SDL_DestroySurface(surface);

    I_ResetDRS();

    return !ret;
}

// Set the application icon

void I_InitWindowIcon(void)
{
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        icon_w, icon_h,
        SDL_GetPixelFormatForMasks(32, 0xffu << 24, 0xffu << 16, 0xffu << 8,
                                   0xffu << 0),
        (void *)icon_data, icon_w * 4);

    SDL_SetWindowIcon(screen, surface);
    SDL_DestroySurface(surface);
}

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (!SDL_GetDisplayBounds(video_display_id, &bounds))
    {
        I_Printf(VB_WARNING,
                 "CenterWindow: Failed to read display bounds "
                 "for display #%d!",
                 video_display);
        return;
    }

    *x = bounds.x + MAX((bounds.w - w) / 2, 0);
    *y = bounds.y + MAX((bounds.h - h) / 2, 0);
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
        case RATIO_32_9:
            w = 32;
            h = 9;
            break;
        default:
            w = 16;
            h = 9;
            break;
    }

    double aspect_ratio = (double)w / (double)h;

    aspect_ratio = CLAMP(aspect_ratio, ASPECT_RATIO_MIN, ASPECT_RATIO_MAX);

    return aspect_ratio;
}

static void ResetResolution(int height)
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

    I_Printf(VB_DEBUG, "ResetResolution: %dx%d (%s)", video.width, video.height,
             widescreen_strings[widescreen]);

    drs_skip_frame = true;
}

static void ResetLogicalSize(void)
{
    rect.w = video.width;
    rect.h = video.height;
    SDL_RectToFRect(&rect, &frect);

    if (!SDL_SetRenderLogicalPresentation(renderer, video.width, actualheight,
        SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        I_Printf(VB_ERROR, "Failed to set logical size: %s", SDL_GetError());
    }
}

static void UpdateUncapped(void)
{
    uncapped = default_uncapped;

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
}

static void I_ResetTargetRefresh(void)
{
    UpdateUncapped();

    if (fpslimit < TICRATE)
    {
        fpslimit = 0;
    }

    if (uncapped)
    {
        if (fpslimit >= TICRATE)
        {
            targetrefresh = fpslimit;
        }
        else if (display_refresh_rate)
        {
            targetrefresh = display_refresh_rate;
        }
        else
        {
            targetrefresh = 60.0f;
        }
    }
    else
    {
        targetrefresh = (float)TICRATE * realtic_clock_rate / 100.0f;
    }

    UpdateLimiter();
    MN_UpdateFpsLimitItem();
    I_ResetDRS();
}

static void I_ResetInvalidDisplayIndex(void)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    video_display_id = GetDisplayIDFromIndex(video_display);

    if (video_display_id == 0)
    {
        video_display = 0;
        video_display_id = GetDisplayIDFromIndex(video_display);
    }
}

//
// killough 11/98: New routine, for setting hires and page flipping
//

static void I_InitVideoParms(void)
{
    int p, tmp_scalefactor;

    I_ResetInvalidDisplayIndex();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(video_display_id);
    if (!mode)
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
            I_Printf(VB_ERROR, "Aspect ratio not supported, set other resolution");
            max_video_width = mode->w;
            max_video_height = mode->h;
        }
        max_width = max_video_width;
        max_height = max_video_height;
    }
    else
    {
        max_width = mode->w;
        max_height = mode->h;
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
    display_refresh_rate = mode->refresh_rate;

    current_video_height = default_current_video_height;
    window_width = default_window_width;
    window_height = default_window_height;

    widescreen = default_widescreen;
    grabmouse = default_grabmouse;
    I_ResetTargetRefresh();

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
    SDL_WindowFlags flags = 0;

    // [FG] window flags
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_INPUT_FOCUS;

    if (fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MOUSE_GRABBED;
    }
    else
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    AdjustWindowSize();
    int w = window_width;
    int h = window_height;

    if (M_CheckParm("-borderless"))
    {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    I_GetWindowPosition(&window_x, &window_y, w, h);

    // [FG] create rendering window

    char *title = M_StringJoin(gamedescription, " - ", PROJECT_STRING);
    screen = SDL_CreateWindow(title, w, h, flags);
    free(title);

    if (screen == NULL)
    {
        I_Error("Error creating window for video startup: %s", SDL_GetError());
    }

    SDL_SetWindowPosition(screen, window_x, window_y);

    I_InitWindowIcon();

    // [FG] create renderer
    renderer = SDL_CreateRenderer(screen, NULL);

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }

    if (use_vsync && !timingdemo)
    {
        SDL_SetRenderVSync(renderer, 1);
    }

    palette = SDL_CreatePalette(256);

    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));

    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    I_Printf(VB_DEBUG, "SDL %d.%d.%d (%s) render driver: %s (%s)",
             SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
             SDL_GetPlatform(),
             SDL_GetRendererName(renderer),
             SDL_GetCurrentVideoDriver());

    UpdateLimiter();
}

resolution_scaling_t I_GetResolutionScaling(void)
{
    resolution_scaling_t rs = {.max = max_height_adjusted, .step = 50};
    return rs;
}

static int GetCurrentVideoHeight(void)
{
    if (scalefactor > 0)
    {
        current_video_height = scalefactor * SCREENHEIGHT;
    }

    current_video_height =
        CLAMP(current_video_height, SCREENHEIGHT, max_height_adjusted);

    return current_video_height;
}

static void CreateVideoBuffer(void)
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                SDL_TEXTUREACCESS_STREAMING,
                                video.width, video.height);
    if (!texture)
    {
        I_Error("Failed to create texture: %s", SDL_GetError());
    }

    if (!SDL_SetTexturePalette(texture, palette))
    {
        I_Error("Failed to set palette: %s", SDL_GetError());
    }

    SDL_SetTextureScaleMode(texture,
        smooth_scaling ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST);

    if (I_VideoBuffer)
    {
        free(I_VideoBuffer);
    }
    I_VideoBuffer = malloc(video.width * video.height);
    V_RestoreBuffer();

    Z_FreeTag(PU_RENDERER);
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

void I_ResetScreen(void)
{
    resetneeded = false;

    widescreen = default_widescreen;

    ResetResolution(GetCurrentVideoHeight());
    CreateVideoBuffer();
    ResetLogicalSize();

    SDL_SetTextureScaleMode(texture, smooth_scaling ? SDL_SCALEMODE_PIXELART
                                                    : SDL_SCALEMODE_NEAREST);

    static aspect_ratio_mode_t oldwidescreen;
    if (oldwidescreen != widescreen)
    {
        MN_UpdateWideShiftItem(true);
        oldwidescreen = widescreen;
    }
}

void I_ShutdownGraphics(void)
{
    if (!fullscreen)
    {
        SDL_GetWindowPosition(screen, &window_position_x, &window_position_y);
    }

    if (scalefactor == 0)
    {
        default_window_width = window_width;
        default_window_height = window_height;
        default_current_video_height = current_video_height;
    }

    SetShowCursor(true);

    SDL_DestroyTexture(texture);

    if (!D_AllowEndDoom())
    {
        // ENDOOM will be skipped, so destroy the renderer and window now.
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(screen);
    }
}

void I_InitGraphics(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    I_AtExit(I_ShutdownGraphics, true);

    I_InitVideoParms();
    I_InitGraphicsMode(); // killough 10/98
    ResetResolution(GetCurrentVideoHeight());
    CreateVideoBuffer();
    ResetLogicalSize();

    MN_UpdateWideShiftItem(false);

    // Mouse motion is based on SDL_GetRelativeMouseState() values only.
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, false);

    // clear out events waiting at the start and center the mouse
    I_ResetRelativeMouseState();
}

void I_QuitVideo(void)
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    SDL_Quit();
}

void I_BindVideoVariables(void)
{
    M_BindNum("current_video_height", &default_current_video_height,
              &current_video_height, 600, 200, UL, ss_none, wad_no,
              "Vertical resolution");
    BIND_BOOL_GENERAL(dynamic_resolution, true, "Dynamic resolution");
    BIND_BOOL(correct_aspect_ratio, true, "Aspect ratio correction");
    BIND_BOOL(fullscreen, true, "Fullscreen");
    BIND_BOOL_GENERAL(use_vsync, true,
        "Vertical sync to prevent display tearing");
    M_BindBool("uncapped", &default_uncapped, &uncapped, true, ss_gen, wad_no,
        "Uncapped rendering frame rate");
    BIND_NUM_GENERAL(fpslimit, 0, 0, 500,
        "Framerate limit in frames per second (< 35 = Disable)");
    M_BindNum("widescreen", &default_widescreen, &widescreen, RATIO_AUTO, 0,
              NUM_RATIOS - 1, ss_gen, wad_no,
              "Widescreen (0 = Off; 1 = Auto; 2 = 16:10; 3 = 16:9; 4 = 21:9; 5 = 32:9)");
    M_BindNum("fov", &custom_fov, NULL, FOV_DEFAULT, FOV_MIN, FOV_MAX, ss_gen,
              wad_no, "Field of view in degrees");
    BIND_NUM_GENERAL(gamma2, 9, 0, 17, "Custom gamma level (0 = -4; 9 = 0; 17 = 4)");
    BIND_BOOL_GENERAL(smooth_scaling, true, "Smooth pixel scaling");

    BIND_BOOL(vga_porch_flash, false, "Emulate VGA \"porch\" behaviour");
    BIND_BOOL(disk_icon, false, "Flashing icon during disk I/O");
    BIND_NUM(video_display, 0, 0, UL, "Current video display index");
    BIND_NUM(max_video_width, 0, SCREENWIDTH, UL,
        "Maximum horizontal resolution (0 = Native)");
    BIND_NUM(max_video_height, 0, SCREENHEIGHT, UL,
        "Maximum vertical resolution (0 = Native)");
    BIND_NUM(window_position_x, 0, UL, UL, "Window position X (0 = Center)");
    BIND_NUM(window_position_y, 0, UL, UL, "Window position Y (0 = Center)");
    M_BindNum("window_width", &default_window_width, &window_width, 1065, 0, UL,
        ss_none, wad_no, "Window width");
    M_BindNum("window_height", &default_window_height, &window_height, 600, 0, UL,
        ss_none, wad_no, "Window height");

    M_BindBool("grabmouse", &default_grabmouse, &grabmouse, true, ss_none,
               wad_no, "Grab mouse during play");
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
