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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "SDL.h"

#include "textscreen.h"

#include "execute.h"

#include "m_argv.h"
#include "m_misc2.h"
#include "z_zone.h"

//#include "setup_icon.c"

#include "multiplayer.h"

static void DoQuit(void *widget, void *dosave)
{
    TXT_Shutdown();

    exit(0);
}

static void QuitConfirm(void *unused1, void *unused2)
{
    txt_window_t *window;
    txt_label_t *label;
    txt_button_t *yes_button;
    txt_button_t *no_button;

    window = TXT_NewWindow(NULL);

    TXT_AddWidgets(window, 
                   label = TXT_NewLabel("Exiting setup.\nSave settings?"),
                   TXT_NewStrut(24, 0),
                   yes_button = TXT_NewButton2("  Yes  ", DoQuit, DoQuit),
                   no_button = TXT_NewButton2("  No   ", DoQuit, NULL),
                   NULL);

    TXT_SetWidgetAlign(label, TXT_HORIZ_CENTER);
    TXT_SetWidgetAlign(yes_button, TXT_HORIZ_CENTER);
    TXT_SetWidgetAlign(no_button, TXT_HORIZ_CENTER);

    // Only an "abort" button in the middle.
    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, NULL);
    TXT_SetWindowAction(window, TXT_HORIZ_CENTER, 
                        TXT_NewWindowAbortAction(window));
    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, NULL);
}

void MainMenu(void)
{
    txt_window_t *window;
    txt_window_action_t *quit_action;

    window = TXT_NewWindow("Main Menu");

    TXT_AddWidgets(window,
        TXT_NewButton2("Start a Network Game",
                       (TxtWidgetSignalFunc) StartMultiGame, NULL),
        TXT_NewButton2("Join a Network Game",
                       (TxtWidgetSignalFunc) JoinMultiGame, NULL),
        NULL);

    quit_action = TXT_NewWindowAction(KEY_ESCAPE, "Quit");
    TXT_SignalConnect(quit_action, "pressed", QuitConfirm, NULL);
    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, quit_action);
}

//
// Application icon
//
#if 0
static void SetIcon(void)
{
    extern SDL_Window *TXT_SDLWindow;
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) setup_icon_data, setup_icon_w,
                                       setup_icon_h, 32, setup_icon_w * 4,
                                       0xff << 24, 0xff << 16,
                                       0xff << 8, 0xff << 0);

    SDL_SetWindowIcon(TXT_SDLWindow, surface);
    SDL_FreeSurface(surface);
}
#endif

static void SetWindowTitle(void)
{
    char *title;

    title = M_StringDuplicate("Multiplayer");

    TXT_SetDesktopTitle(title);

    free(title);
}

// Initialize the textscreen library.

static void InitTextscreen(void)
{
    if (!TXT_Init())
    {
        fprintf(stderr, "Failed to initialize GUI\n");
        exit(-1);
    }

    // Set Romero's "funky blue" color:
    // <https://doomwiki.org/wiki/Romero_Blue>
    TXT_SetColor(TXT_COLOR_BLUE, 0x04, 0x14, 0x40);

    // [crispy] Crispy colors for Crispy Setup
    TXT_SetColor(TXT_COLOR_BRIGHT_GREEN, 249, 227, 0);  // 0xF9, 0xE3, 0x00
    TXT_SetColor(TXT_COLOR_CYAN, 220, 153, 0);          // 0xDC, 0x99, 0x00
    TXT_SetColor(TXT_COLOR_BRIGHT_CYAN, 76, 160, 223);  // 0x4C, 0xA0, 0xDF

    //SetIcon();
    SetWindowTitle();
}

// Restart the textscreen library.  Used when the video_driver variable
// is changed.

void RestartTextscreen(void)
{
    TXT_Shutdown();
    InitTextscreen();
}

// 
// Initialize and run the textscreen GUI.
//

static void RunGUI(void)
{
    InitTextscreen();

    TXT_GUIMainLoop();
}

static void MissionSet(void)
{
    SetWindowTitle();
    MainMenu();
}

int main(int argc, char **argv)
{
   myargc = argc;
   myargv = argv;

   Z_Init();

   //SetupMission(MissionSet);
   MissionSet();

   RunGUI();

   return 0;
}
