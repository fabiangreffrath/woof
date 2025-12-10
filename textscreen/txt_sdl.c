//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Text mode emulation in SDL
//

#include <SDL3/SDL.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomkeys.h"

#include "txt_main.h"
#include "txt_sdl.h"
#include "txt_utf8.h"

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

typedef struct
{
    const char *name;
    const uint8_t *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

// Fonts:

#include "fonts/normal.h"

#include "fonts/codepage.h"

// Time between character blinks in ms

#define BLINK_PERIOD 250

SDL_Window *TXT_SDLWindow = NULL;
static SDL_Palette *palette;
static unsigned char *screendata;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

// Current input mode.
static txt_input_mode_t input_mode = TXT_INPUT_NORMAL;

// Dimensions of the screen image in screen coordinates (not pixels); this
// is the value that was passed to SDL_CreateWindow().
static int screen_image_w, screen_image_h;

static TxtSDLEventCallbackFunc event_callback;
static void *event_callback_data;

// Font we are using:
static const txt_font_t *font;

// Mapping from SDL keyboard scancode to internal key code.
static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

// String names of keys. This is a fallback; we usually use the SDL API.
static const struct {
    int key;
    const char *name;
} key_names[] = KEY_NAMES_ARRAY;

// Unicode key mapping; see codepage.h.
static const short code_page_to_unicode[] = CODE_PAGE_TO_UNICODE;

static const SDL_Color ega_colors[] =
{
    {0x00, 0x00, 0x00, 0xff},          // 0: Black
    {0x00, 0x00, 0xa8, 0xff},          // 1: Blue
    {0x00, 0xa8, 0x00, 0xff},          // 2: Green
    {0x00, 0xa8, 0xa8, 0xff},          // 3: Cyan
    {0xa8, 0x00, 0x00, 0xff},          // 4: Red
    {0xa8, 0x00, 0xa8, 0xff},          // 5: Magenta
    {0xa8, 0x54, 0x00, 0xff},          // 6: Brown
    {0xa8, 0xa8, 0xa8, 0xff},          // 7: Grey
    {0x54, 0x54, 0x54, 0xff},          // 8: Dark grey
    {0x54, 0x54, 0xfe, 0xff},          // 9: Bright blue
    {0x54, 0xfe, 0x54, 0xff},          // 10: Bright green
    {0x54, 0xfe, 0xfe, 0xff},          // 11: Bright cyan
    {0xfe, 0x54, 0x54, 0xff},          // 12: Bright red
    {0xfe, 0x54, 0xfe, 0xff},          // 13: Bright magenta
    {0xfe, 0xfe, 0x54, 0xff},          // 14: Yellow
    {0xfe, 0xfe, 0xfe, 0xff},          // 15: Bright white
};

//
// Initialize text mode screen
//
// Returns 1 if successful, 0 if an error occurred
//

void TXT_PreInit(SDL_Window *preset_window, SDL_Renderer *preset_renderer)
{
    if (preset_window != NULL)
    {
        TXT_SDLWindow = preset_window;
    }

    if (preset_renderer != NULL)
    {
        renderer = preset_renderer;
    }
}

static void UpdateLogicalPresentation(void)
{
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    int w, h;

    SDL_GetWindowSizeInPixels(TXT_SDLWindow, &w, &h);

    // Window aspect ratio less than txt aspect ratio?
    if (w * screen_image_h < h * screen_image_w)
    {
        // Window width is the constraint.
        const int border_w = w % screen_image_w;

        if (border_w >= 0 && border_w <= w / 6)
        {
            // Borders are small enough, so use integer scaling.
            mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
        }
    }
    else
    {
        // Window height is the constraint.
        const int border_h = h % screen_image_h;

        if (border_h >= 0 && border_h <= h / 6)
        {
            // Borders are small enough, so use integer scaling.
            mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
        }
    }

    // Set width and height of the logical viewport for automatic scaling.
    SDL_SetRenderLogicalPresentation(renderer, screen_image_w, screen_image_h,
                                     mode);
}

int TXT_Init(void)
{
    SDL_WindowFlags flags = 0;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return 0;
    }

    font = &normal_font;

    screen_image_w = TXT_SCREEN_W * font->w;
    screen_image_h = TXT_SCREEN_H * font->h;

    // try to initialize high dpi rendering.
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (TXT_SDLWindow == NULL)
    {
        flags |= SDL_WINDOW_RESIZABLE;

        TXT_SDLWindow =
            SDL_CreateWindow(NULL, screen_image_w, screen_image_h, flags);
        SDL_SetWindowMinimumSize(TXT_SDLWindow, screen_image_w, screen_image_h);
    }

    if (TXT_SDLWindow == NULL)
    {
        return 0;
    }

    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(TXT_SDLWindow, NULL);
        SDL_SetRenderVSync(renderer, 1);
    }

    if (renderer == NULL)
    {
        return 0;
    }

    UpdateLogicalPresentation();

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                SDL_TEXTUREACCESS_STREAMING, screen_image_w,
                                screen_image_h);

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

    palette = SDL_CreatePalette(256);
    SDL_SetPaletteColors(palette, ega_colors, 0, 16);
    SDL_SetTexturePalette(texture, palette);

    screendata = malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);

    return 1;
}

void TXT_Shutdown(void)
{
    free(screendata);
    screendata = NULL;
    SDL_DestroyTexture(texture);
    texture = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void TXT_SetColor(txt_color_t color, int r, int g, int b)
{
    SDL_Color c = {r, g, b, 0xff};
    SDL_SetPaletteColors(palette, &c, color, 1);
}

unsigned char *TXT_GetScreenData(void)
{
    return screendata;
}

static inline void UpdateCharacter(unsigned char *pixels, int pitch, int x, int y)
{
    unsigned char character;
    const uint8_t *p;
    unsigned char *s, *s1;
    unsigned int bit;
    int bg, fg;
    unsigned int x1, y1;

    p = &screendata[(y * TXT_SCREEN_W + x) * 2];
    character = p[0];

    fg = p[1] & 0xf;
    bg = (p[1] >> 4) & 0xf;

    if (bg & 0x8)
    {
        // blinking

        bg &= ~0x8;

        if (((SDL_GetTicks() / BLINK_PERIOD) % 2) == 0)
        {
            fg = bg;
        }
    }

    // How many bytes per line?
    p = &font->data[(character * font->w * font->h) / 8];
    bit = 0;

    s = pixels + (y * font->h * pitch) + (x * font->w);

    for (y1=0; y1<font->h; ++y1)
    {
        s1 = s;

        for (x1=0; x1<font->w; ++x1)
        {
            if (*p & (1 << bit))
            {
                *s1++ = fg;
            }
            else
            {
                *s1++ = bg;
            }

            ++bit;
            if (bit == 8)
            {
                ++p;
                bit = 0;
            }
        }

        s += pitch;
    }
}

static int LimitToRange(int val, int min, int max)
{
    if (val < min)
    {
        return min;
    }
    else if (val > max)
    {
        return max;
    }
    else
    {
        return val;
    }
}

void TXT_UpdateScreenArea(int x, int y, int w, int h)
{
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    int x_end = LimitToRange(x + w, 0, TXT_SCREEN_W);
    int y_end = LimitToRange(y + h, 0, TXT_SCREEN_H);
    x = LimitToRange(x, 0, TXT_SCREEN_W);
    y = LimitToRange(y, 0, TXT_SCREEN_H);

    for (int y1 = y; y1 < y_end; ++y1)
    {
        for (int x1 = x; x1 < x_end; ++x1)
        {
            UpdateCharacter(pixels, pitch, x1, y1);
        }
    }

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void TXT_UpdateScreen(void)
{
    TXT_UpdateScreenArea(0, 0, TXT_SCREEN_W, TXT_SCREEN_H);
}

void TXT_GetMousePosition(int *x, int *y)
{
    int window_w, window_h;
    float outx, outy;

    SDL_GetMouseState(&outx, &outy);

    *x = (int)outx;
    *y = (int)outy;

    // Translate mouse position from 'pixel' position into character position.
    // We are working here in screen coordinates and not pixels, since this is
    // what SDL_GetWindowSize() returns.
    SDL_GetWindowSize(TXT_SDLWindow, &window_w, &window_h);
    *x = ((*x) * TXT_SCREEN_W) / window_w;
    *y = ((*y) * TXT_SCREEN_H) / window_h;

    if (*x < 0)
    {
        *x = 0;
    }
    else if (*x >= TXT_SCREEN_W)
    {
        *x = TXT_SCREEN_W - 1;
    }
    if (*y < 0)
    {
        *y = 0;
    }
    else if (*y >= TXT_SCREEN_H)
    {
        *y = TXT_SCREEN_H - 1;
    }
}

//
// Translates the SDL key
//

// XXX: duplicate from doomtype.h
#define arrlen(array) (sizeof(array) / sizeof(*array))

static int TranslateScancode(SDL_Scancode scancode)
{
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
            if (scancode < arrlen(scancode_translate_table))
            {
                return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }
}

static int TranslateKeysym(SDL_KeyboardEvent *key)
{
    int translated;

    // We cheat here and make use of TranslateScancode. The range of keys
    // associated with printable characters is pretty contiguous, so if it's
    // inside that range we want the localized version of the key instead.
    translated = TranslateScancode(key->scancode);

    if (translated >= 0x20 && translated < 0x7f)
    {
        return key->key;
    }
    else
    {
        return translated;
    }
}

// Convert an SDL button index to textscreen button index.
//
// Note special cases because 2 == mid in SDL, 3 == mid in textscreen/setup

static int SDLButtonToTXTButton(int button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:
            return TXT_MOUSE_LEFT;
        case SDL_BUTTON_RIGHT:
            return TXT_MOUSE_RIGHT;
        case SDL_BUTTON_MIDDLE:
            return TXT_MOUSE_MIDDLE;
        default:
            return TXT_MOUSE_BASE + button - 1;
    }
}

// Convert an SDL wheel motion to a textscreen button index.

static int SDLWheelToTXTButton(const SDL_MouseWheelEvent *wheel)
{
    if (wheel->y <= 0)
    {
        return TXT_MOUSE_SCROLLDOWN;
    }
    else
    {
        return TXT_MOUSE_SCROLLUP;
    }
}

static int MouseHasMoved(void)
{
    static int last_x = 0, last_y = 0;
    int x, y;

    TXT_GetMousePosition(&x, &y);

    if (x != last_x || y != last_y)
    {
        last_x = x; last_y = y;
        return 1;
    }
    else
    {
        return 0;
    }
}

signed int TXT_GetChar(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
    {
        // If there is an event callback, allow it to intercept this
        // event.

        if (event_callback != NULL)
        {
            if (event_callback(&ev, event_callback_data))
            {
                continue;
            }
        }

        // Process the event.

        switch (ev.type)
        {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button < TXT_MAX_MOUSE_BUTTONS)
                {
                    return SDLButtonToTXTButton(ev.button.button);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                return SDLWheelToTXTButton(&ev.wheel);

            case SDL_EVENT_KEY_DOWN:
                switch (input_mode)
                {
                    case TXT_INPUT_RAW:
                        return TranslateScancode(ev.key.scancode);
                    case TXT_INPUT_NORMAL:
                        return TranslateKeysym(&ev.key);
                    case TXT_INPUT_TEXT:
                        // We ignore key inputs in this mode, except for a
                        // few special cases needed during text input:
                        if (ev.key.key == SDLK_ESCAPE
                         || ev.key.key == SDLK_BACKSPACE
                         || ev.key.key == SDLK_RETURN)
                        {
                            return TranslateKeysym(&ev.key);
                        }
                        break;
                }
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (input_mode == TXT_INPUT_TEXT)
                {
                    // TODO: Support input of more than just the first char?
                    const char *p = ev.text.text;
                    int result = TXT_DecodeUTF8(&p);
                    // 0-127 is ASCII, but we map non-ASCII Unicode chars into
                    // a higher range to avoid conflicts with special keys.
                    return TXT_UNICODE_TO_KEY(result);
                }
                break;

            case SDL_EVENT_QUIT:
                // Quit = escape
                return 27;

            case SDL_EVENT_MOUSE_MOTION:
                if (MouseHasMoved())
                {
                    return 0;
                }

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                SDL_PushEvent(&ev);
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                if (ev.window.windowID == SDL_GetWindowID(TXT_SDLWindow))
                {
                    UpdateLogicalPresentation();
                }
                break;

            default:
                break;
        }
    }

    return -1;
}

int TXT_GetModifierState(txt_modifier_t mod)
{
    SDL_Keymod state;

    state = SDL_GetModState();

    switch (mod)
    {
        case TXT_MOD_SHIFT:
            return (state & SDL_KMOD_SHIFT) != 0;
        case TXT_MOD_CTRL:
            return (state & SDL_KMOD_CTRL) != 0;
        case TXT_MOD_ALT:
            return (state & SDL_KMOD_ALT) != 0;
        default:
            return 0;
    }
}

int TXT_UnicodeCharacter(unsigned int c)
{
    unsigned int i;

    // Check the code page mapping to see if this character maps
    // to anything.

    for (i = 0; i < arrlen(code_page_to_unicode); ++i)
    {
        if (code_page_to_unicode[i] == c)
        {
            return i;
        }
    }

    return -1;
}

// Returns true if the given UTF8 key name is printable to the screen.
static int PrintableName(const char *s)
{
    const char *p;
    unsigned int c;

    p = s;
    while (*p != '\0')
    {
        c = TXT_DecodeUTF8(&p);
        if (TXT_UnicodeCharacter(c) < 0)
        {
            return 0;
        }
    }

    return 1;
}

static const char *NameForKey(int key)
{
    const char *result;
    int i;

    // Overrides purely for aesthetical reasons, so that default
    // window accelerator keys match those of setup.exe.
    switch (key)
    {
        case KEY_ESCAPE: return "ESC";
        case KEY_ENTER:  return "ENTER";
        default:
            break;
    }

    // This key presumably maps to a scan code that is listed in the
    // translation table. Find which mapping and once we have a scancode,
    // we can convert it into a virtual key, then a string via SDL.
    for (i = 0; i < arrlen(scancode_translate_table); ++i)
    {
        if (scancode_translate_table[i] == key)
        {
            result = SDL_GetKeyName(SDL_GetKeyFromScancode(i, SDL_GetModState(), false));
            if (TXT_UTF8_Strlen(result) > 6 || !PrintableName(result))
            {
                break;
            }
            return result;
        }
    }

    // Use US English fallback names, if the localized name is too long,
    // not found in the scancode table, or contains unprintable chars
    // (non-extended ASCII character set):
    for (i = 0; i < arrlen(key_names); ++i)
    {
        if (key_names[i].key == key)
        {
            return key_names[i].name;
        }
    }

    return NULL;
}

void TXT_GetKeyDescription(int key, char *buf, size_t buf_len)
{
    const char *keyname;
    int i;

    keyname = NameForKey(key);

    if (keyname != NULL)
    {
        TXT_StringCopy(buf, keyname, buf_len);

        // Key description should be all-uppercase to match setup.exe.
        for (i = 0; buf[i] != '\0'; ++i)
        {
            buf[i] = toupper(buf[i]);
        }
    }
    else
    {
        TXT_snprintf(buf, buf_len, "??%i", key);
    }
}

// Searches the desktop screen buffer to determine whether there are any
// blinking characters.

int TXT_ScreenHasBlinkingChars(void)
{
    int x, y;
    unsigned char *p;

    // Check all characters in screen buffer

    for (y=0; y<TXT_SCREEN_H; ++y)
    {
        for (x=0; x<TXT_SCREEN_W; ++x) 
        {
            p = &screendata[(y * TXT_SCREEN_W + x) * 2];

            if (p[1] & 0x80)
            {
                // This character is blinking

                return 1;
            }
        }
    }

    // None found

    return 0;
}

// Sleeps until an event is received, the screen needs to be redrawn, 
// or until timeout expires (if timeout != 0)

void TXT_Sleep(int timeout)
{
    unsigned int start_time;

    if (TXT_ScreenHasBlinkingChars())
    {
        int time_to_next_blink;

        time_to_next_blink = BLINK_PERIOD - (SDL_GetTicks() % BLINK_PERIOD);

        // There are blinking characters on the screen, so we 
        // must time out after a while
       
        if (timeout == 0 || timeout > time_to_next_blink)
        {
            // Add one so it is always positive

            timeout = time_to_next_blink + 1;
        }
    }

    if (timeout == 0)
    {
        // We can just wait forever until an event occurs

        SDL_WaitEvent(NULL);
    }
    else
    {
        // Sit in a busy loop until the timeout expires or we have to
        // redraw the blinking screen

        start_time = SDL_GetTicks();

        while (SDL_GetTicks() < start_time + timeout)
        {
            if (SDL_PollEvent(NULL) != 0)
            {
                // Received an event, so stop waiting

                break;
            }

            // Don't hog the CPU

            SDL_Delay(1);
        }
    }
}

void TXT_SetInputMode(txt_input_mode_t mode)
{
    if (mode == TXT_INPUT_TEXT && !SDL_TextInputActive(TXT_SDLWindow))
    {
        SDL_StartTextInput(TXT_SDLWindow);
    }
    else if (SDL_TextInputActive(TXT_SDLWindow) && mode != TXT_INPUT_TEXT)
    {
        SDL_StopTextInput(TXT_SDLWindow);
    }

    input_mode = mode;
}

void TXT_SetWindowTitle(const char *title)
{
    SDL_SetWindowTitle(TXT_SDLWindow, title);
}

void TXT_SDL_SetEventCallback(TxtSDLEventCallbackFunc callback, void *user_data)
{
    event_callback = callback;
    event_callback_data = user_data;
}

// Safe string functions.

void TXT_StringCopy(char *dest, const char *src, size_t dest_len)
{
    if (dest_len < 1)
    {
        return;
    }

    dest[dest_len - 1] = '\0';
    strncpy(dest, src, dest_len - 1);
}

void TXT_StringConcat(char *dest, const char *src, size_t dest_len)
{
    size_t offset;

    offset = strlen(dest);
    if (offset > dest_len)
    {
        offset = dest_len;
    }

    TXT_StringCopy(dest + offset, src, dest_len - offset);
}

// Safe, portable vsnprintf().
int TXT_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
{
    int result;

    if (buf_len < 1)
    {
        return 0;
    }

    // Windows (and other OSes?) has a vsnprintf() that doesn't always
    // append a trailing \0. So we must do it, and write into a buffer
    // that is one byte shorter; otherwise this function is unsafe.
    result = vsnprintf(buf, buf_len, s, args);

    // If truncated, change the final char in the buffer to a \0.
    // A negative result indicates a truncated buffer on Windows.
    if (result < 0 || result >= buf_len)
    {
        buf[buf_len - 1] = '\0';
        result = buf_len - 1;
    }

    return result;
}

// Safe, portable snprintf().
int TXT_snprintf(char *buf, size_t buf_len, const char *s, ...)
{
    va_list args;
    int result;
    va_start(args, s);
    result = TXT_vsnprintf(buf, buf_len, s, args);
    va_end(args);
    return result;
}
