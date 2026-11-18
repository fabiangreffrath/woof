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
// DESCRIPTION:
//    Exit text-mode ENDOOM screen.
//

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_video.h"
#include "i_system.h"

typedef struct
{
    const char *name;
    const uint8_t *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

#define TXT_FONT_W 8
#define TXT_FONT_H 18
#define TXT_SCREEN_W 80
#define TXT_SCREEN_H 25

#include "data/fonts/normal.h"

#define BLINK_PERIOD 250

static byte *screendata;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Palette *palette;
static int screen_image_w;
static int screen_image_h;

static const SDL_Color ega_colors[] = {
    {0x00, 0x00, 0x00, 0xff}, // 0: Black
    {0x00, 0x00, 0xa8, 0xff}, // 1: Blue
    {0x00, 0xa8, 0x00, 0xff}, // 2: Green
    {0x00, 0xa8, 0xa8, 0xff}, // 3: Cyan
    {0xa8, 0x00, 0x00, 0xff}, // 4: Red
    {0xa8, 0x00, 0xa8, 0xff}, // 5: Magenta
    {0xa8, 0x54, 0x00, 0xff}, // 6: Brown
    {0xa8, 0xa8, 0xa8, 0xff}, // 7: Grey
    {0x54, 0x54, 0x54, 0xff}, // 8: Dark grey
    {0x54, 0x54, 0xfe, 0xff}, // 9: Bright blue
    {0x54, 0xfe, 0x54, 0xff}, // 10: Bright green
    {0x54, 0xfe, 0xfe, 0xff}, // 11: Bright cyan
    {0xfe, 0x54, 0x54, 0xff}, // 12: Bright red
    {0xfe, 0x54, 0xfe, 0xff}, // 13: Bright magenta
    {0xfe, 0xfe, 0x54, 0xff}, // 14: Yellow
    {0xfe, 0xfe, 0xfe, 0xff}, // 15: Bright white
};

static void UpdateCharacter(byte *pixels, int pitch, int x, int y)
{
    byte character;
    const uint8_t *p;
    byte *s, *s1;
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

    p = &normal_font.data[(character * TXT_FONT_W * TXT_FONT_H) / 8];
    bit = 0;

    s = pixels + (y * TXT_FONT_H * pitch) + (x * TXT_FONT_W);

    for (y1 = 0; y1 < TXT_FONT_H; ++y1)
    {
        s1 = s;

        for (x1 = 0; x1 < TXT_FONT_W; ++x1)
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

static void UpdateLogicalPresentation(void)
{
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);

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
    SDL_SetRenderLogicalPresentation(renderer, screen_image_w,
                                     screen_image_h, mode);
}

static void UpdateScreen(void)
{
    void *pixels;
    int pitch;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    for (int y = 0; y < TXT_SCREEN_H; ++y)
    {
        for (int x = 0; x < TXT_SCREEN_W; ++x)
        {
            UpdateCharacter(pixels, pitch, x, y);
        }
    }

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void WaitForKey(void)
{
    SDL_Event ev;

    while (true)
    {
        UpdateScreen();

        while (SDL_PollEvent(&ev))
        {
            switch (ev.type)
            {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_QUIT:
                    return;

                case SDL_EVENT_WINDOW_RESIZED:
                    if (ev.window.windowID == SDL_GetWindowID(window))
                    {
                        UpdateLogicalPresentation();
                    }
                    break;

                default:
                    break;
            }
        }

        SDL_Delay(10);
    }
}

static void InitEndoomScreen(void)
{
    window = I_GetSDLWindow();
    renderer = I_GetSDLRenderer();

    if (!window || !renderer)
    {
        I_Error("Failed to get game window/renderer for ENDOOM");
    }

    screen_image_w = TXT_SCREEN_W * TXT_FONT_W;
    screen_image_h = TXT_SCREEN_H * TXT_FONT_H;

    UpdateLogicalPresentation();

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                SDL_TEXTUREACCESS_STREAMING, screen_image_w,
                                screen_image_h);
    if (!texture)
    {
        I_Error("Failed to create ENDOOM texture: %s", SDL_GetError());
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

    palette = SDL_CreatePalette(256);
    SDL_SetPaletteColors(palette, ega_colors, 0, 16);
    SDL_SetTexturePalette(texture, palette);

    screendata = malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);
}

static void ShutdownEndoomScreen(void)
{
    free(screendata);
    screendata = NULL;
    SDL_DestroyTexture(texture);
    texture = NULL;
    SDL_DestroyPalette(palette);
    palette = NULL;
    window = NULL;
    renderer = NULL;
}

//
// Displays the text mode ending screen after the game quits
//

void I_Endoom(byte *endoom_data)
{
    InitEndoomScreen();

    // Write the data to the screen memory

    for (int y = 0; y < TXT_SCREEN_H; ++y)
    {
        memcpy(screendata + y * TXT_SCREEN_W * 2,
               endoom_data + y * TXT_SCREEN_W * 2, TXT_SCREEN_W * 2);
    }

    // Wait for a keypress
    WaitForKey();

    // Shut down text mode screen
    ShutdownEndoomScreen();
}
