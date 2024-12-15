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

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "doomkeys.h"
#include "i_timer.h"
#include "m_misc.h"
#include "multiplayer.h"
#include "setup_icon.c"

#include "textscreen.h"

static void DoQuit(void *widget, void *dosave)
{
    TXT_Shutdown();

    exit(0);
}

#if 0
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
#endif

#if 0
static void LaunchDoom(void *unused1, void *unused2)
{
    execute_context_t *exec;

    // Shut down textscreen GUI

    TXT_Shutdown();

    // Launch Doom

    exec = NewExecuteContext();
    PassThroughArguments(exec);
    ExecuteDoom(exec);

    exit(0);
}
#endif

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
    TXT_SignalConnect(quit_action, "pressed", DoQuit, NULL);
    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, quit_action);
}

//
// Application icon
//

static void SetIcon(void)
{
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        setup_icon_w, setup_icon_h,
        SDL_GetPixelFormatForMasks(32, 0xffu << 24, 0xffu << 16, 0xffu << 8,
                                   0xffu << 0),
        (void *)setup_icon_data, setup_icon_w * 4);

    SDL_SetWindowIcon(TXT_SDLWindow, surface);
    SDL_DestroySurface(surface);
}

static void SetWindowTitle(void)
{
    char *title;

    title = M_StringDuplicate(PROJECT_NAME " Setup ver " PROJECT_VERSION);

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

    SetIcon();
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
    I_InitTimer();

    InitTextscreen();

    TXT_GUIMainLoop();
}

static void MissionSet(void)
{
    SetWindowTitle();
    MainMenu();
}

void D_DoomMain(void)
{
   //SetupMission(MissionSet);
   MissionSet();

   RunGUI();
}
