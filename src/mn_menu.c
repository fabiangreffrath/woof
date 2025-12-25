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
//  DOOM selection menu, options, episode etc. (aka Big Font menus)
//  Sliders and icons. Kinda widget stuff.
//  Setup Menus.
//  Extended HELP screens.
//  Dynamic HELP screen.
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "d_deh.h"
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dstrings.h"
#include "g_game.h"
#include "g_rewind.h"
#include "g_umapinfo.h"
#include "i_exit.h"
#include "i_input.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_input.h"
#include "m_io.h"
#include "m_misc.h"
#include "m_swap.h"
#include "mn_font.h"
#include "mn_menu.h"
#include "mn_internal.h"
#include "mn_snapshot.h"
#include "p_keyframe.h"
#include "p_saveg.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "z_zone.h"

// [crispy] remove DOS reference from the game quit confirmation dialogs
#ifndef _WIN32
#  include <unistd.h> // [FG] isatty()
#endif

//
// defaulted values
//

// Blocky mode, has default, 0 = high, 1 = normal
// int     detailLevel;    obsolete -- killough
int screenblocks, maxscreenblocks; // has default

static int quickSaveSlot; // -1 = no quicksave slot picked!

static int messageToPrint; // 1 = message to be printed

static char *messageString; // ...and here is the message string!

static int messageLastMenuActive;

static boolean messageNeedsInput; // timed message = no input from user

static void (*messageRoutine)(int response);

#define SAVESTRINGSIZE 24

// we are going to be entering a savegame string

static int saveStringEnter;
static int saveSlot;      // which slot to save in
static int saveCharIndex; // which char we're editing
// old save description before edit
static char saveOldString[SAVESTRINGSIZE];

boolean menuactive; // The menus are up
static boolean mouse_active_thermo;
static boolean options_active;
static boolean customskill_active;

boolean menu_pause_demos;
backdrop_t menu_backdrop;

int bigfont_priority = -1;

#define SKULLXOFF        -32
#define LINEHEIGHT       16

#define M_THRM_STEP      8

#define M_X_LOADSAVE     80
#define M_Y_LOADSAVE     34
#define M_Y_AUTOSAVE     26
#define M_LOADSAVE_WIDTH (24 * 8 + 8) // [FG] c.f. M_DrawSaveLoadBorder()

#define LOADGRAPHIC_Y 8
#define AUTOGRAPHIC_Y 2
static int loadsave_title_y = LOADGRAPHIC_Y;

static char savegamestrings[10][SAVESTRINGSIZE];

// [FG] support up to 8 pages of savegames
int savepage = 0;
static const int savepage_max = 7;

//
// MENU TYPEDEFS
//

typedef enum
{
    MF_HILITE   = 0x00000001,
    MF_THRM     = 0x00000002,
    MF_THRM_STR = 0x00000004,
    MF_PAGE     = 0x00000008,
    MF_OPTLUMP  = 0x00000010,
} mflags_t;

typedef enum
{
    CHOICE_LEFT  = -2,
    CHOICE_RIGHT = -1,
    CHOICE_VALUE = 0,
} mchoice_t;

typedef struct
{
    short status; // 0 = no cursor here, 1 = ok, 2 = arrows ok
    char name[10];

    // choice = menu item #.
    // if status = 2,
    //   choice = CHOICE_LEFT:leftarrow, CHOICE_RIGHT:rightarrow
    void (*routine)(int choice);
    char alphaKey; // hotkey in menu
    // [FG] alternative text for missing menu graphics lumps
    const char *alttext;
    mrect_t rect;
    mflags_t flags;
} menuitem_t;

typedef struct menu_s
{
    short numitems;          // # of menu items
    struct menu_s *prevMenu; // previous menu
    menuitem_t *menuitems;   // menu items
    void (*routine)();       // draw routine
    short x;
    short y;           // x,y of menu
    short lastOn;      // last item user was on in menu
    int lumps_missing; // [FG] indicate missing menu graphics lumps
} menu_t;

static int highlight_item;
static short itemOn;           // menu item skull is on (for Big Font menus)
static short skullAnimCounter; // skull animation counter
short whichSkull;              // which skull to draw (he blinks)

// graphic name of skulls

char skullName[2][/*8*/ 9] = {"M_SKULL1", "M_SKULL2"};

// Quick save and regular save menus. Auto save not shown.
static menu_t SaveDef;

// Regular load menu. Auto save not shown.
static menu_t LoadDef;

// Quick load menu. Shares same menuitems as LoadDef. Auto save not shown.
static menu_t QuickLoadDef;

// First page of regular load menu when auto saving is enabled.
static menu_t LoadAutoSaveDef;

static menu_t *currentMenu; // current menudef

// end of externs added for setup menus

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_ChangeMessages(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
/* void M_ChangeDetail(int choice);  unused -- killough */

static void M_FinishReadThis(int choice);
static void M_LoadAutoSaveSelect(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);
static void M_DrawSetup(void); // phares 3/21/98

static void M_DrawSaveLoadBorder(int x, int y, byte *cr);
static void M_DrawThermo(int x, int y, int thermWidth, int thermDot, byte *cr);
static void WriteText(int x, int y, const char *string);
static void M_StartMessage(char *string, void (*routine)(int), boolean input);

// phares 3/30/98
// prototypes added to support Setup Menus and Extended HELP screens

static void M_Setup(int choice);

static void M_InitExtendedHelp(void);
static void M_ExtHelpNextScreen(int choice);
static void M_ExtHelp(int choice);
static void M_DrawExtHelp(void);

static void M_CustomSkill(int choice);

static void M_PauseSound(void)
{
    if (!paused && gamestate == GS_LEVEL && !demoplayback && !netgame)
    {
        S_PauseSound();
    }
}

static void M_ResumeSound(void)
{
    if (!paused && gamestate == GS_LEVEL && !demoplayback && !netgame)
    {
        S_ResumeSound();
    }
}

//
// SetNextMenu
//

static void SetNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
    highlight_item = 0;
}

static menu_t NewDef; // phares 5/04/98

// end of prototypes added to support Setup Menus and Extended HELP screens

/////////////////////////////////////////////////////////////////////////////
//
// DOOM MENUS
//

/////////////////////////////
//
// MAIN MENU
//

// main_e provides numerical values for which Big Font screen you're on

enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

//
// MainMenu is the definition of what the main menu Screen should look
// like. Each entry shows that the cursor can land on each item (1), the
// built-in graphic lump (i.e. "M_NGAME") that should be displayed,
// the program which takes over when an item is selected, and the hotkey
// associated with the item.
//

#define MAIN_MENU_RECT \
    {0, 0, SCREENWIDTH, LINEHEIGHT}

static menuitem_t MainMenu[] = {
    {1, "M_NGAME",  M_NewGame,  'n', "New Game",   MAIN_MENU_RECT},
    // change M_Options to M_Setup
    {1, "M_OPTION", M_Setup,    'o', "Options",    MAIN_MENU_RECT},
    {1, "M_LOADG",  M_LoadGame, 'l', "Load Game",  MAIN_MENU_RECT},
    {1, "M_SAVEG",  M_SaveGame, 's', "Save Game",  MAIN_MENU_RECT},
    // Another hickup with Special edition.
    {1, "M_RDTHIS", M_ReadThis, 'r', "Read This!", MAIN_MENU_RECT},
    {1, "M_QUITG",  M_QuitDOOM, 'q', "Quit",       MAIN_MENU_RECT}
};

static menu_t MainDef = {
    main_end,       // number of menu items
    NULL,           // previous menu screen
    MainMenu,       // table that defines menu items
    M_DrawMainMenu, // drawing routine
    97,
    64, // initial cursor position
    0   // last menu item the user was on
};

//
// M_DrawMainMenu
//

static void M_DrawMainMenu(void)
{
    options_active = false;

    V_DrawPatch(94, 2, V_CachePatchName("M_DOOM", PU_CACHE));
}

/////////////////////////////
//
// Read This! MENU 1 & 2
//

// There are no menu items on the Read This! screens, so read_e just
// provides a placeholder to maintain structure.

enum
{
    rdthsempty1,
    read1_end
} read_e;

enum
{
    rdthsempty2,
    read2_end
} read_e2;

enum // killough 10/98
{
    helpempty,
    help_end
} help_e;

// The definitions of the Read This! screens

static menuitem_t ReadMenu1[] = {
    {1, "", M_ReadThis2, 0}
};

static menuitem_t ReadMenu2[] = {
    {1, "", M_FinishReadThis, 0}
};

static menu_t ReadDef1 = {
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    330, 175,
    // 280,185,              // killough 2/21/98: fix help screens
    0
};

static menu_t ReadDef2 = {
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330, 175,
    0
};

//
// M_ReadThis
//

static void M_ReadThis(int choice)
{
    SetNextMenu(&ReadDef1);
}

static void M_ReadThis2(int choice)
{
    SetNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int choice)
{
    SetNextMenu(&MainDef);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
// killough 10/98: updated with new screens

static void M_DrawReadThis1(void)
{
    V_DrawPatchFullScreen(
        V_CachePatchName(W_CheckWidescreenPatch("HELP2"), PU_CACHE));
}

//
// Read This Menus - optional second page.
//
// killough 10/98: updated with new screens

static void M_DrawReadThis2(void)
{
    // Display help screen from PWAD
    int helplump;
    if (gamemode == commercial)
    {
        helplump = W_CheckNumForName(W_CheckWidescreenPatch("HELP"));
    }
    else
    {
        helplump = W_CheckNumForName(W_CheckWidescreenPatch("HELP1"));
    }

    V_DrawPatchFullScreen(V_CachePatchNum(helplump, PU_CACHE));
}

static void M_DrawReadThisCommercial(void)
{
    V_DrawPatchFullScreen(
        V_CachePatchName(W_CheckWidescreenPatch("HELP"), PU_CACHE));
}

/////////////////////////////
//
// EPISODE SELECT
//

//
// episodes_e provides numbers for the episode menu items. The default is
// 4, to accomodate Ultimate Doom. If the user is running anything else,
// this is accounted for in the code.
//

enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

#define MAX_EPISODES 10 // [FG] UMAPINFO spec says 8, but oh well...

// The definitions of the Episodes menu

#define M_Y_EPISODES 63

#define EPISODES_RECT(n) \
    {0, M_Y_EPISODES + (n) * LINEHEIGHT, SCREENWIDTH, LINEHEIGHT}

static menuitem_t EpisodeMenu[MAX_EPISODES] = // added a few free entries for UMAPINFO
{
    {1, "M_EPI1", M_Episode, 'k', "Knee-Deep in the Dead", EPISODES_RECT(0)},
    {1, "M_EPI2", M_Episode, 't', "The Shores of Hell",    EPISODES_RECT(1)},
    {1, "M_EPI3", M_Episode, 'i', "Inferno",               EPISODES_RECT(2)},
    {1, "M_EPI4", M_Episode, 't', "Thy Flesh Consumed",    EPISODES_RECT(3)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(4)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(5)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(6)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(7)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(8)},
    {1, "",       M_Episode, '0', NULL, EPISODES_RECT(9)}
};

static menu_t EpiDef = {
    ep_end,        // # of menu items
    &MainDef,      // previous menu
    EpisodeMenu,   // menuitem_t ->
    M_DrawEpisode, // drawing routine ->
    48, M_Y_EPISODES, // x,y
    ep1           // lastOn
};

// This is for customized episode menus
boolean EpiCustom;
static short EpiMenuMap[MAX_EPISODES] = {1, 1, 1, 1, -1, -1, -1, -1, -1, -1};
static short EpiMenuEpi[MAX_EPISODES] = {1, 2, 3, 4, -1, -1, -1, -1, -1, -1};

//
//    M_Episode
//
static int epiChoice;

void MN_ClearEpisodes(void)
{
    EpiDef.numitems = 0;
    NewDef.prevMenu = &MainDef;
}

void MN_AddEpisode(const char *map, const char *gfx, const char *txt, char key)
{
    int epi, mapnum;

    if (!EpiCustom)
    {
        EpiCustom = true;
        NewDef.prevMenu = &EpiDef;

        if (gamemode == commercial)
        {
            EpiDef.numitems = 0;
        }
    }

    if (EpiDef.numitems == 8)
    {
        I_Printf(VB_WARNING,
                 "MN_AddEpisode: UMAPINFO spec limit of 8 episodes exceeded!");
    }
    else if (EpiDef.numitems >= MAX_EPISODES)
    {
        return;
    }

    G_ValidateMapName(map, &epi, &mapnum);
    EpiMenuEpi[EpiDef.numitems] = epi;
    EpiMenuMap[EpiDef.numitems] = mapnum;
    strncpy(EpisodeMenu[EpiDef.numitems].name, gfx, 8);
    EpisodeMenu[EpiDef.numitems].name[9] = 0;
    EpisodeMenu[EpiDef.numitems].alttext = txt ? strdup(txt) : NULL;
    EpisodeMenu[EpiDef.numitems].alphaKey = key;
    EpiDef.numitems++;

    if (EpiDef.numitems <= 4)
    {
        EpiDef.y = 63;
    }
    else
    {
        EpiDef.y = MAX(25, 63 - (EpiDef.numitems - 4) * (LINEHEIGHT / 2));
    }
}

static void M_DrawEpisode(void)
{
    MN_DrawTitle(54, EpiDef.y - 25, "M_EPISOD", "Which Episode?");
}

static void M_Episode(int choice)
{
    if (!EpiCustom)
    {
        if ((gamemode == shareware) && choice)
        {
            M_StartMessage(s_SWSTRING, NULL, false); // Ty 03/27/98 - externalized
            SetNextMenu(&ReadDef1);
            return;
        }

        // Yet another hack...
        if (gamemode == registered && choice > 2)
        {
            choice = 0; // killough 8/8/98
        }
    }
    epiChoice = choice;
    SetNextMenu(&NewDef);
}

/////////////////////////////
//
// NEW GAME
//

// numerical values for the New Game menu items

enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    customskill,
    newg_end
} newgame_e;

// The definitions of the New Game menu

#define M_Y_NEWGAME 63

#define NEW_GAME_RECT(n) \
    {0, M_Y_NEWGAME + (n) * LINEHEIGHT, SCREENWIDTH, LINEHEIGHT}

static menuitem_t NewGameMenu[] = {
    {1, "M_JKILL", M_ChooseSkill, 'i', "I'm too young to die.", NEW_GAME_RECT(0)},
    {1, "M_ROUGH", M_ChooseSkill, 'h', "Hey, not too rough.",   NEW_GAME_RECT(1)},
    {1, "M_HURT",  M_ChooseSkill, 'h', "Hurt me plenty.",       NEW_GAME_RECT(2)},
    {1, "M_ULTRA", M_ChooseSkill, 'u', "Ultra-Violence.",       NEW_GAME_RECT(3)},
    {1, "M_NMARE", M_ChooseSkill, 'n', "Nightmare!",            NEW_GAME_RECT(4)},
    {1, "M_CSTSKL", M_CustomSkill, 'c', "Custom Skill...",      NEW_GAME_RECT(5), MF_OPTLUMP}
};

static menu_t NewDef = {
    newg_end,      // # of menu items
    &EpiDef,       // previous menu
    NewGameMenu,   // menuitem_t ->
    M_DrawNewGame, // drawing routine ->
    48, M_Y_NEWGAME, // x,y
    hurtme       // lastOn
};

//
// M_NewGame
//

static void M_DrawNewGame(void)
{
    customskill_active = false;
    MN_DrawTitle(96, 14, "M_NEWG", "NEW GAME");
    MN_DrawTitle(54, 38, "M_SKILL", "Choose Skill Level:");
}

static void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
        M_StartMessage(s_NEWGAME, NULL, false); // Ty 03/27/98 - externalized
        return;
    }

    if (demorecording) // killough 5/26/98: exclude during demo recordings
    {
        M_StartMessage("you can't start a new game\n"
                       "while recording a demo!\n\n" PRESSKEY,
                       NULL, false); // killough 5/26/98: not externalized
        return;
    }

    if (((gamemode == commercial) && !EpiCustom) || EpiDef.numitems <= 1)
    {
        SetNextMenu(&NewDef);
    }
    else
    {
        epiChoice = 0;
        SetNextMenu(&EpiDef);
    }
}

static void M_VerifyNightmare(int ch)
{
    if (ch != 'y')
    {
        return;
    }

    //jff 3/24/98 remember last skill selected
    // killough 10/98 moved to here
    default_skill = nightmare + 1;

    if (!EpiCustom)
    {
        G_DeferedInitNew(nightmare, epiChoice + 1, 1);
    }
    else
    {
        G_DeferedInitNew(nightmare, EpiMenuEpi[epiChoice],
                         EpiMenuMap[epiChoice]);
    }

    MN_ClearMenus();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    { // Ty 03/27/98 - externalized
        M_StartMessage(s_NIGHTMARE, M_VerifyNightmare, true);
        return;
    }

    //jff 3/24/98 remember last skill selected
    // killough 10/98 moved to here
    default_skill = choice + 1;

    if (!EpiCustom)
    {
        G_DeferedInitNew(choice, epiChoice + 1, 1);
    }
    else
    {
        G_DeferedInitNew(choice, EpiMenuEpi[epiChoice], EpiMenuMap[epiChoice]);
    }

    MN_ClearMenus();
}

static void M_CustomSkill(int choice)
{
    customskill_active = true;
    MN_CustomSkill();
}

/////////////////////////////
//
// LOAD GAME MENU
//

// numerical values for the Load Game slots

enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load7, // jff 3/15/98 extend number of slots
    load8,
    load_page,
    load_end
} load_e;

#define SAVE_LOAD_RECT_Y(m_y, n) ((m_y) + (n) * LINEHEIGHT - 7)

#define SAVE_LOAD_RECT(n)                                               \
    {M_X_LOADSAVE, SAVE_LOAD_RECT_Y(M_Y_LOADSAVE, n), M_LOADSAVE_WIDTH, \
     LINEHEIGHT}

#define AUTO_SAVE_RECT(n)                                               \
    {M_X_LOADSAVE, SAVE_LOAD_RECT_Y(M_Y_AUTOSAVE, n), M_LOADSAVE_WIDTH, \
     LINEHEIGHT}

// The definitions of the Load Game screen

static menuitem_t LoadMenu[] = {
    {1, "", M_LoadSelect, '1', NULL, SAVE_LOAD_RECT(0)},
    {1, "", M_LoadSelect, '2', NULL, SAVE_LOAD_RECT(1)},
    {1, "", M_LoadSelect, '3', NULL, SAVE_LOAD_RECT(2)},
    {1, "", M_LoadSelect, '4', NULL, SAVE_LOAD_RECT(3)},
    {1, "", M_LoadSelect, '5', NULL, SAVE_LOAD_RECT(4)},
    {1, "", M_LoadSelect, '6', NULL, SAVE_LOAD_RECT(5)},
    //  jff 3/15/98 extend number of slots
    {1, "", M_LoadSelect, '7', NULL, SAVE_LOAD_RECT(6)},
    {1, "", M_LoadSelect, '8', NULL, SAVE_LOAD_RECT(7)},
    {-1, "", NULL, 0, NULL, SAVE_LOAD_RECT(8), MF_PAGE}
};

static menu_t LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    M_X_LOADSAVE,
    M_Y_LOADSAVE, // jff 3/15/98 move menu up
    0
};

static menu_t QuickLoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    M_X_LOADSAVE,
    M_Y_LOADSAVE,
};

enum
{
    autosave_page = load_end,
    autosave_end
} autosave_e;

static menuitem_t LoadAutoSaveMenu[] = {
    // Auto save slot.
    {1, "", M_LoadAutoSaveSelect, 'a', NULL, AUTO_SAVE_RECT(0)},
    // Regular save slots.
    {1, "", M_LoadSelect, '1', NULL, AUTO_SAVE_RECT(1)},
    {1, "", M_LoadSelect, '2', NULL, AUTO_SAVE_RECT(2)},
    {1, "", M_LoadSelect, '3', NULL, AUTO_SAVE_RECT(3)},
    {1, "", M_LoadSelect, '4', NULL, AUTO_SAVE_RECT(4)},
    {1, "", M_LoadSelect, '5', NULL, AUTO_SAVE_RECT(5)},
    {1, "", M_LoadSelect, '6', NULL, AUTO_SAVE_RECT(6)},
    {1, "", M_LoadSelect, '7', NULL, AUTO_SAVE_RECT(7)},
    {1, "", M_LoadSelect, '8', NULL, AUTO_SAVE_RECT(8)},
    // Save page navigation.
    {-1, "", NULL, 0, NULL, AUTO_SAVE_RECT(9), MF_PAGE}
};

static menu_t LoadAutoSaveDef =
{
    autosave_end,
    &MainDef,
    LoadAutoSaveMenu,
    M_DrawLoad,
    M_X_LOADSAVE,
    M_Y_AUTOSAVE,
};

// [FG] draw a snapshot of the n'th savegame into a separate window,
//      or fill window with solid color and "n/a" if snapshot not available

static int snapshot_width, snapshot_height;

static void M_DrawBorderedSnapshot(int slot)
{
    // [FG] a snapshot window smaller than 80*48 px is considered too small
    if (snapshot_width < SCREENWIDTH / 4)
    {
        return;
    }

    const int x =
        video.deltaw + currentMenu->x + SKULLXOFF - snapshot_width - 8;
    const int y = (currentMenu->numitems * LINEHEIGHT - snapshot_height) * slot
                  / currentMenu->numitems;

    const int snapshot_x = MAX(8, x);
    const int snapshot_y = currentMenu->y + MAX(0, y);

    const boolean draw_shot = MN_DrawSnapshot(slot, snapshot_x, snapshot_y,
                                              snapshot_width, snapshot_height);

    const boolean is_autosave = (currentMenu == &LoadAutoSaveDef && slot == 0);

    const char *txt;

    if (!draw_shot || is_autosave)
    {
        txt = (is_autosave ? "Auto Save" : "N/A");
        WriteText(snapshot_x + (snapshot_width - MN_StringWidth(txt)) / 2
                        - video.deltaw,
                    snapshot_y + snapshot_height / 2 - MN_StringHeight(txt) / 2,
                    txt);
    }

    txt = MN_GetSavegameTime(slot);
    MN_DrawString(snapshot_x + snapshot_width / 2 - MN_GetPixelWidth(txt) / 2
                      - video.deltaw,
                  snapshot_y + snapshot_height + MN_StringHeight(txt), CR_GOLD,
                  txt);

    // [FG] draw the view border around the snapshot
    R_DrawBorder(snapshot_x, snapshot_y, snapshot_width, snapshot_height);
}

// [FG] delete a savegame

static boolean delete_verify = false;

static void DeleteAutoSave(void)
{
    char *name = G_AutoSaveName();
    M_remove(name);
    free(name);
}

static void DeleteSaveGame(int slot)
{
    char *name = G_SaveGameName(slot);
    M_remove(name);
    free(name);

    if (slot == quickSaveSlot)
    {
        quickSaveSlot = -1;
    }

    if (slot == savegameslot)
    {
        savegameslot = -1;
    }
}

static void M_DeleteGame(int choice)
{
    int slot = choice;

    if (currentMenu == &LoadAutoSaveDef)
    {
        if (slot == 0)
        {
            DeleteAutoSave();
        }
        else
        {
            slot--;
            DeleteSaveGame(slot);
        }
    }
    else
    {
        DeleteSaveGame(slot);
    }
}

// [FG] support up to 8 pages of savegames
static void M_DrawSaveLoadBottomLine(void)
{
    char pagestr[16];
    const int x = currentMenu->x;
    const int y = currentMenu->y + LINEHEIGHT * (currentMenu->numitems - 1);

    int index = (menu_input == mouse_mode ? highlight_item : itemOn);

    int flags = currentMenu->menuitems[index].flags;
    byte *cr = (flags & MF_PAGE) ? cr_bright : NULL;

    M_DrawSaveLoadBorder(x, y, cr);

    if (savepage > 0)
    {
        MN_DrawString(x, y, CR_GOLD, "<-");
    }
    if (savepage < savepage_max)
    {
        MN_DrawString(x + (SAVESTRINGSIZE - 2) * 8, y, CR_GOLD, "->");
    }

    M_snprintf(pagestr, sizeof(pagestr), "page %d/%d", savepage + 1,
               savepage_max + 1);
    MN_DrawString(x + M_LOADSAVE_WIDTH / 2 - MN_StringWidth(pagestr) / 2, y,
                  CR_GOLD, pagestr);
}

static void M_DrawSaveLoadBorders(void)
{
    const int num_slots = currentMenu->numitems - 1;
    const int x = currentMenu->x;

    for (int i = 0; i < num_slots; i++)
    {
        const int y = currentMenu->y + LINEHEIGHT * i;

        const menuitem_t *item = &currentMenu->menuitems[i];
        byte *cr = (item->flags & MF_HILITE) ? cr_bright : NULL;

        M_DrawSaveLoadBorder(x, y, cr);
        WriteText(x, y, savegamestrings[i]);
    }
}

//
// M_LoadGame & Cie.
//

static void M_DrawLoad(void)
{
    // jff 3/15/98 use symbolic load position
    MN_DrawTitle(M_X_CENTER, loadsave_title_y, "M_LOADG", "Load Game");
    M_DrawSaveLoadBorders();

    int index = (menu_input == mouse_mode ? highlight_item : itemOn);

    if (index < currentMenu->numitems - 1)
    {
        M_DrawBorderedSnapshot(index);
    }

    M_DrawSaveLoadBottomLine();
}

//
// Draw border for the savegame description
//

static void M_DrawSaveLoadBorder(int x, int y, byte *cr)
{
    int i;

    V_DrawPatchTranslated(x - 8, y + 7, V_CachePatchName("M_LSLEFT", PU_CACHE), cr);

    for (i = 0; i < 24; i++)
    {
        V_DrawPatchTranslated(x, y + 7, V_CachePatchName("M_LSCNTR", PU_CACHE), cr);
        x += 8;
    }

    V_DrawPatchTranslated(x, y + 7, V_CachePatchName("M_LSRGHT", PU_CACHE), cr);
}

//
// User wants to load this game
//

static void M_LoadAutoSaveSelect(int choice)
{
    saveg_compat = saveg_woof510;
    char *name = G_AutoSaveName();
    G_LoadAutoSave(name, false);
    free(name);
    MN_ClearMenus();
    // Auto save slot doesn't exist for save menu, so don't change lastOn.
    //SaveDef.lastOn = choice;
}

static void M_LoadSelect(int choice)
{
    char *name = NULL; // killough 3/22/98
    int slot = choice;

    if (menuactive && currentMenu == &LoadAutoSaveDef)
    {
        slot--;
    }

    name = G_SaveGameName(slot);
    saveg_compat = saveg_woof510;

    if (M_access(name, F_OK) != 0)
    {
        if (name)
        {
            free(name);
        }
        name = G_MBFSaveGameName(slot);
        saveg_compat = saveg_mbf;
    }

    G_LoadGame(name, slot, false); // killough 3/16/98, 5/15/98: add slot, cmd

    MN_ClearMenus();
    if (name)
    {
        free(name);
    }

    // [crispy] save the last game you loaded
    SaveDef.lastOn = slot;
}

//
// killough 5/15/98: add forced loadgames
//

static void M_VerifyForcedLoadAutoSave(int ch)
{
    if (ch == 'y')
    {
        G_ForcedLoadAutoSave();
    }
    free(messageString);
    MN_ClearMenus();
}

static void M_VerifyForcedLoadGame(int ch)
{
    if (ch == 'y')
    {
        G_ForcedLoadGame();
    }
    free(messageString); // free the message strdup()'ed below
    MN_ClearMenus();
}

void MN_ForcedLoadAutoSave(const char *msg)
{
    M_StartMessage(strdup(msg), M_VerifyForcedLoadAutoSave, true);
}

void MN_ForcedLoadGame(const char *msg)
{
    M_StartMessage(strdup(msg), M_VerifyForcedLoadGame, true); // free()'d above
}

//
// Selected from DOOM menu
//

static void M_LoadGame(int choice)
{
    delete_verify = false;

    if (netgame && !demoplayback) // killough 5/26/98: add !demoplayback
    {
        M_StartMessage(s_LOADNET, NULL, false); // Ty 03/27/98 - externalized
        return;
    }

    if (demorecording) // killough 5/26/98: exclude during demo recordings
    {
        M_StartMessage("you can't load a game\n"
                       "while recording a demo!\n\n" PRESSKEY,
                       NULL, false); // killough 5/26/98: not externalized
        return;
    }

    if (G_AutoSaveEnabled() && savepage == 0)
    {
        SetNextMenu(&LoadAutoSaveDef);
    }
    else
    {
        SetNextMenu(&LoadDef);
    }

    M_ReadSaveStrings();
}

/////////////////////////////
//
// SAVE GAME MENU
//

// The definitions of the Save Game screen

static menuitem_t SaveMenu[] = {
    {1, "", M_SaveSelect, '1', NULL, SAVE_LOAD_RECT(0)},
    {1, "", M_SaveSelect, '2', NULL, SAVE_LOAD_RECT(1)},
    {1, "", M_SaveSelect, '3', NULL, SAVE_LOAD_RECT(2)},
    {1, "", M_SaveSelect, '4', NULL, SAVE_LOAD_RECT(3)},
    {1, "", M_SaveSelect, '5', NULL, SAVE_LOAD_RECT(4)},
    {1, "", M_SaveSelect, '6', NULL, SAVE_LOAD_RECT(5)},
    //  jff 3/15/98 extend number of slots
    {1, "", M_SaveSelect, '7', NULL, SAVE_LOAD_RECT(6)},
    {1, "", M_SaveSelect, '8', NULL, SAVE_LOAD_RECT(7)},
    {-1, "", NULL, 0, NULL, SAVE_LOAD_RECT(8), MF_PAGE}
};

static menu_t SaveDef =
{
    load_end, // same number of slots as the Load Game screen
    &MainDef,
    SaveMenu,
    M_DrawSave,
    M_X_LOADSAVE,
    M_Y_LOADSAVE, // jff 3/15/98 move menu up
    0
};

static void SetLoadSlotStatus(int slot, int status)
{
    if (currentMenu == &LoadAutoSaveDef)
    {
        if (slot > 0)
        {
            LoadDef.menuitems[slot - 1].status = status;
        }

        LoadAutoSaveDef.menuitems[slot].status = status;
    }
    else
    {
        LoadDef.menuitems[slot].status = status;
        LoadAutoSaveDef.menuitems[slot + 1].status = status;
    }
}

static void EmptySaveString(char *name, boolean is_autosave)
{
    if (is_autosave)
    {
        M_snprintf(name, SAVESTRINGSIZE, "%s (Auto)", s_EMPTYSTRING);
    }
    else
    {
        M_snprintf(name, SAVESTRINGSIZE, "%s", s_EMPTYSTRING);
    }
}

static void M_ReadSaveString(char *name, int menu_slot, int save_slot,
                             boolean is_autosave)
{
    FILE *fp = M_fopen(name, "rb");
    MN_ReadSavegameTime(menu_slot, name);
    free(name);

    MN_ResetSnapshot(menu_slot);

    if (!fp)
    {
        if (!is_autosave)
        {
            // Ty 03/27/98 - externalized:
            name = G_MBFSaveGameName(save_slot);
            fp = M_fopen(name, "rb");
            free(name);
        }

        if (!fp)
        {
            EmptySaveString(savegamestrings[menu_slot], is_autosave);
            SetLoadSlotStatus(menu_slot, 0);
            return;
        }
    }

    // [FG] check return value
    if (!fread(&savegamestrings[menu_slot], SAVESTRINGSIZE, 1, fp))
    {
        fclose(fp);
        EmptySaveString(savegamestrings[menu_slot], is_autosave);
        SetLoadSlotStatus(menu_slot, 0);
        return;
    }

    // Ensure that string is terminated
    savegamestrings[menu_slot][SAVESTRINGSIZE - 1] = '\0';

    if (!MN_ReadSnapshot(menu_slot, fp))
    {
        MN_ResetSnapshot(menu_slot);
    }

    fclose(fp);
    SetLoadSlotStatus(menu_slot, 1);
}

static void UpdateRectX(menu_t *menu, int x)
{
    for (int i = 0; i < menu->numitems; i++)
    {
        menu->menuitems[i].rect.x = x;
    }
}

//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
    // [FG] shift savegame descriptions a bit to the right
    //      to make room for the snapshots on the left
    const int x = M_X_LOADSAVE + MIN(M_LOADSAVE_WIDTH / 2, video.deltaw);
    SaveDef.x = LoadDef.x = QuickLoadDef.x = LoadAutoSaveDef.x = x;
    UpdateRectX(&SaveDef, x);
    UpdateRectX(&LoadDef, x);
    UpdateRectX(&LoadAutoSaveDef, x);

    // [FG] fit the snapshots into the resulting space
    snapshot_width = MIN((video.deltaw + SaveDef.x + 2 * SKULLXOFF) & ~7,
                         SCREENWIDTH / 2); // [FG] multiple of 8
    snapshot_height = MIN((snapshot_width * SCREENHEIGHT / SCREENWIDTH) & ~7,
                          SCREENHEIGHT / 2);

    int start_slot = 0;

    if (currentMenu == &LoadAutoSaveDef)
    {
        char *name = G_AutoSaveName();
        M_ReadSaveString(name, 0, 0, true);
        start_slot = 1;
    }

    const int num_slots = currentMenu->numitems - 1;

    for (int menu_slot = start_slot; menu_slot < num_slots; menu_slot++)
    {
        const int save_slot = menu_slot - start_slot;
        char *name = G_SaveGameName(save_slot);
        M_ReadSaveString(name, menu_slot, save_slot, false);
    }
}

//
//  M_SaveGame & Cie.
//
static void M_DrawSave(void)
{
    int i;

    // jff 3/15/98 use symbolic load position
    MN_DrawTitle(M_X_CENTER, loadsave_title_y, "M_SAVEG", "Save Game");
    M_DrawSaveLoadBorders();

    if (saveStringEnter)
    {
        i = MN_StringWidth(savegamestrings[saveSlot]);
        WriteText(currentMenu->x + i, currentMenu->y + LINEHEIGHT * saveSlot,
                  "_");
    }

    int index = (menu_input == mouse_mode ? highlight_item : itemOn);

    if (index < currentMenu->numitems - 1)
    {
        M_DrawBorderedSnapshot(index);
    }

    M_DrawSaveLoadBottomLine();
}

//
// M_Responder calls this when user is finished
//
static void M_DoSave(int slot)
{
    G_SaveGame(slot, savegamestrings[slot]);
    MN_ClearMenus();
}

void MN_SetQuickSaveSlot(int slot)
{
    if (quickSaveSlot == -2)
    {
        quickSaveSlot = slot;
    }
}

// [FG] generate a default save slot name when the user saves to an empty slot
static void SetDefaultSaveName(char *name, const char *append)
{
    char *maplump = MapName(gameepisode, gamemap);
    int maplumpnum = W_CheckNumForName(maplump);

    if (gamemapinfo && gamemapinfo->label
        && !(gamemapinfo->flags & MapInfo_LabelClear))
    {
        maplump = gamemapinfo->label;
    }

    if (!organize_savefiles)
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpnum));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        if (append)
        {
            M_snprintf(name, SAVESTRINGSIZE, "%s (%s) (%s)", maplump, wadname,
                       append);
        }
        else
        {
            M_snprintf(name, SAVESTRINGSIZE, "%s (%s)", maplump, wadname);
        }

        free(wadname);
    }
    else
    {
        if (append)
        {
            M_snprintf(name, SAVESTRINGSIZE, "%s (%s)", maplump, append);
        }
        else
        {
            M_snprintf(name, SAVESTRINGSIZE, "%s", maplump);
        }
    }

    M_StringToUpper(name);
}

// [FG] override savegame name if it already starts with a map identifier
boolean MN_StartsWithMapIdentifier(char *str)
{
    if (strnlen(str, 8) >= 4 && M_ToUpper(str[0]) == 'E' && isdigit(str[1])
        && M_ToUpper(str[2]) == 'M' && isdigit(str[3]))
    {
        return true;
    }

    if (strnlen(str, 8) >= 5 && M_ToUpper(str[0]) == 'M' && M_ToUpper(str[1]) == 'A'
        && M_ToUpper(str[2]) == 'P' && isdigit(str[3]) && isdigit(str[4]))
    {
        return true;
    }

    return false;
}

void M_SaveAutoSave(void)
{
    char autosave_string[SAVESTRINGSIZE];
    SetDefaultSaveName(autosave_string, "Auto");
    G_SaveAutoSave(autosave_string);
}

static boolean GamepadSave(int choice)
{
    if (menu_input == pad_mode)
    {
        // Immediately save game using a default name.
        SetDefaultSaveName(savegamestrings[choice], NULL);
        M_DoSave(choice);
        LoadDef.lastOn = choice;
        QuickLoadDef.lastOn = choice;
        LoadAutoSaveDef.lastOn = choice + 1;
        return true;
    }

    return false;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int choice)
{
    if (GamepadSave(choice))
    {
        return;
    }

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    I_StartTextInput();

    saveSlot = choice;
    strcpy(saveOldString, savegamestrings[choice]);
    // [FG] override savegame name if it already starts with a map identifier
    if (!strcmp(savegamestrings[choice], s_EMPTYSTRING) // Ty 03/27/98 - externalized
        || MN_StartsWithMapIdentifier(savegamestrings[choice]))
    {
        savegamestrings[choice][0] = 0;
        SetDefaultSaveName(savegamestrings[choice], NULL);
    }
    saveCharIndex = strlen(savegamestrings[choice]);

    // [crispy] load the last game you saved
    LoadDef.lastOn = choice;
    QuickLoadDef.lastOn = choice;
    LoadAutoSaveDef.lastOn = choice + 1;
}

//
// Selected from DOOM menu
//
static void M_SaveGame(int choice)
{
    delete_verify = false;

    // killough 10/6/98: allow savegames during single-player demo playback
    if (!usergame && (!demoplayback || netgame))
    {
        M_StartMessage(s_SAVEDEAD, NULL, false); // Ty 03/27/98 - externalized
        return;
    }

    if (gamestate != GS_LEVEL)
    {
        return;
    }

    SetNextMenu(&SaveDef);
    M_ReadSaveStrings();
}

/////////////////////////////
//
// M_QuitDOOM
//
static int quitsounds[8] = {sfx_pldeth, sfx_dmpain, sfx_popain, sfx_slop,
                            sfx_telept, sfx_posit1, sfx_posit3, sfx_sgtatk};

static int quitsounds2[8] = {sfx_vilact, sfx_getpow, sfx_boscub, sfx_slop,
                             sfx_skeswg, sfx_kntdth, sfx_bspact, sfx_sgtatk};

static void M_QuitResponse(int ch)
{
    if (ch != 'y')
    {
        return;
    }
    if (quit_sound &&
        (!netgame || demoplayback) && // killough 12/98
        !nosfxparm)                   // avoid delay if no sound card
    {
        if (gamemode == commercial)
        {
            M_StartSound(quitsounds2[(gametic >> 2) & 7]);
        }
        else
        {
            M_StartSound(quitsounds[(gametic >> 2) & 7]);
        }
        I_WaitVBL(105);
    }
    I_SafeExit(0); // killough
}

static void M_QuitDOOM(int choice)
{
    static char endstring[160];

    // We pick index 0 which is language sensitive,
    // or one at random, between 1 and maximum number.
    // Ty 03/27/98 - externalized DOSY as a string s_DOSY that's in the sprintf
    if (language != english)
    {
        sprintf(endstring, "%s\n\n%s", s_DOSY, *endmsg[0]);
    }
    else // killough 1/18/98: fix endgame message calculation:
    {
        sprintf(endstring, "%s\n\n%s",
                *endmsg[gametic % (NUM_QUITMESSAGES - 1) + 1], s_DOSY);
    }

    if (quit_prompt)
    {
        M_StartMessage(endstring, M_QuitResponse, true);
    }
    else
    {
        M_QuitResponse('y');
    }
}

/////////////////////////////
//
// SOUND VOLUME MENU
//

// numerical values for the Sound Volume menu items
// The 'empty' slots are where the sliding scales appear.

enum
{
    sfx_vol,
    sfx_vol_thermo,
    music_vol,
    music_vol_thermo,
    sound_end
} sound_e;

// The definitions of the Sound Volume menu

#define THERMO_VOLUME_RECT(n) \
    {80 /*SoundDef.x*/, 64 /*SoundDef.y*/ + LINEHEIGHT *(n), (16 + 2) * 8, 13}

static menuitem_t SoundMenu[] = {
    {2,  "M_SFXVOL", M_SfxVol,   's', "Sfx Volume", {0},                           MF_THRM_STR},
    {-1, "",         NULL,       0,   NULL, THERMO_VOLUME_RECT(sfx_vol_thermo),   MF_THRM    },
    {2,  "M_MUSVOL", M_MusicVol, 'm', "Music Volume", {0},                         MF_THRM_STR},
    {-1, "",         NULL,       0,   NULL, THERMO_VOLUME_RECT(music_vol_thermo), MF_THRM    },
};

static menu_t SoundDef = {sound_end, &MainDef, SoundMenu, M_DrawSound,
                          80,        64,       0};

//
// Change Sfx & Music volumes
//

static void M_DrawSound(void)
{
    MN_DrawTitle(M_X_CENTER, 38, "M_SVOL", "Sound Volume");

    int index = highlight_item + 1;
    menuitem_t *item = &currentMenu->menuitems[index];
    byte *cr;

    if (index == sfx_vol_thermo && (item->flags & MF_HILITE))
    {
        cr = cr_bright;
    }
    else
    {
        cr = NULL;
    }

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * sfx_vol_thermo, 16,
                 snd_SfxVolume, cr);

    if (index == music_vol_thermo && (item->flags & MF_HILITE))
    {
        cr = cr_bright;
    }
    else
    {
        cr = NULL;
    }

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * music_vol_thermo, 16,
                 snd_MusicVolume, cr);
}

void M_Sound(int choice)
{
    SetNextMenu(&SoundDef);
}

#define M_MAX_VOL 15

static void M_SfxVol(int choice)
{
    switch (choice)
    {
        case CHOICE_LEFT:
            if (snd_SfxVolume)
            {
                snd_SfxVolume--;
            }
            break;
        case CHOICE_RIGHT:
            if (snd_SfxVolume < M_MAX_VOL)
            {
                snd_SfxVolume++;
            }
            break;
        default:
            if (choice >= CHOICE_VALUE)
            {
                snd_SfxVolume = choice;
            }
            break;
    }

    S_SetSfxVolume(snd_SfxVolume /* *8 */);
}

static void M_MusicVol(int choice)
{
    switch (choice)
    {
        case CHOICE_LEFT:
            if (snd_MusicVolume)
            {
                snd_MusicVolume--;
            }
            break;
        case CHOICE_RIGHT:
            if (snd_MusicVolume < M_MAX_VOL)
            {
                snd_MusicVolume++;
            }
            break;
        default:
            if (choice >= CHOICE_VALUE)
            {
                snd_MusicVolume = choice;
            }
            break;
    }

    S_SetMusicVolume(snd_MusicVolume /* *8 */);
}

/////////////////////////////
//
//    M_QuickSave
//

static void M_QuickSaveResponse(int ch)
{
    if (ch == 'y')
    {
        if (MN_StartsWithMapIdentifier(savegamestrings[quickSaveSlot]))
        {
            SetDefaultSaveName(savegamestrings[quickSaveSlot], NULL);
        }
        M_DoSave(quickSaveSlot);
        M_StartSound(sfx_swtchx);
    }
}

static void M_QuickSave(void)
{
    if (!usergame && (!demoplayback || netgame)) // killough 10/98
    {
        M_StartSound(sfx_oof);
        return;
    }

    if (gamestate != GS_LEVEL)
    {
        return;
    }

    if (quickSaveSlot < 0)
    {
        MN_StartControlPanel();
        SetNextMenu(&SaveDef);
        M_ReadSaveStrings();
        quickSaveSlot = -2; // means to pick a slot now
        return;
    }
    M_QuickSaveResponse('y');
}

/////////////////////////////
//
// M_QuickLoad
//

static void M_QuickLoadResponse(int ch)
{
    if (ch == 'y')
    {
        M_LoadSelect(quickSaveSlot);
        M_StartSound(sfx_swtchx);
    }
}

static void M_QuickLoad(void)
{
    if (netgame && !demoplayback) // killough 5/26/98: add !demoplayback
    {
        M_StartSound(sfx_swtchn);
        M_StartMessage(s_QLOADNET, NULL, false); // Ty 03/27/98 - externalized
        return;
    }

    if (demorecording) // killough 5/26/98: exclude during demo recordings
    {
        M_StartSound(sfx_swtchn);
        M_StartMessage("you can't quickload\n"
                       "while recording a demo!\n\n" PRESSKEY,
                       NULL, false); // killough 5/26/98: not externalized
        return;
    }

    if (quickSaveSlot < 0)
    {
        // [crispy] allow quickload before quicksave
        MN_StartControlPanel();
        SetNextMenu(&QuickLoadDef);
        M_ReadSaveStrings();
        quickSaveSlot = -2; // means to pick a slot now
        return;
    }
    M_QuickLoadResponse('y');
}

/////////////////////////////
//
// M_EndGame
//

static void M_EndGameResponse(int ch)
{
    if (ch != 'y')
    {
        return;
    }

    // killough 5/26/98: make endgame quit if recording or playing back demo
    if (demorecording || singledemo)
    {
        G_CheckDemoStatus();
    }

    // [crispy] clear quicksave slot
    quickSaveSlot = -1;

    currentMenu->lastOn = itemOn;
    S_EvictChannels();
    MN_ClearMenus();
    D_StartTitle();
}

static void M_EndGame(int choice)
{
    if (netgame)
    {
        M_StartMessage(s_NETEND, NULL, false); // Ty 03/27/98 - externalized
        return;
    }
    M_StartMessage(s_ENDGAME, M_EndGameResponse,
                   true); // Ty 03/27/98 - externalized
}

/////////////////////////////
//
//    Toggle messages on/off
//

static void M_ChangeMessages(int choice)
{
    show_messages = 1 - show_messages;

    if (!show_messages)
    {
        displaymsg("%s", s_MSGOFF); // Ty 03/27/98 - externalized
    }
    else
    {
        displaymsg("%s", s_MSGON); // Ty 03/27/98 - externalized
    }
}

/////////////////////////////
//
// CHANGE DISPLAY SIZE
//
// jff 2/23/98 restored to pre-HUD state
// hud_active controlled soley by F5=key_detail (key_hud)
// hud_displayed is toggled by + or = in fullscreen
// hud_displayed is cleared by -

static void M_SizeDisplay(int choice)
{
    switch (choice)
    {
        case 0:
            screenblocks--;
            break;
        case 1:
            screenblocks++;
            break;
        default:
            break;
    }
    screenblocks = CLAMP(screenblocks, 3, maxscreenblocks);
    R_SetViewSize(screenblocks /*, detailLevel obsolete -- killough */);
}

/////////////////////////////////////////////////////////////////////////////
//
// Start of Extended HELP screens               // phares 3/30/98
//
// The wad designer can define a set of extended HELP screens for their own
// information display. These screens should be 320x200 graphic lumps
// defined in a separate wad. They should be named "HELP01" through "HELP99".
// "HELP01" is shown after the regular BOOM Dynamic HELP screen, and ENTER
// and BACKSPACE keys move the player through the HELP set.
//
// Rather than define a set of menu definitions for each of the possible
// HELP screens, one definition is used, and is altered on the fly
// depending on what HELPnn lumps the game finds.

// phares 3/30/98:
// Extended Help Screen variables

static int extended_help_count; // number of user-defined help screens found
static int extended_help_index; // index of current extended help screen

static menuitem_t ExtHelpMenu[] = {
    {1, "", M_ExtHelpNextScreen, 0}
};

static menu_t ExtHelpDef = {
    1,           // # of menu items
    &ReadDef1,   // previous menu
    ExtHelpMenu, // menuitem_t ->
    M_DrawExtHelp, // drawing routine ->
    330, 181, // x,y
    0    // lastOn
};

// M_ExtHelpNextScreen establishes the number of the next HELP screen in
// the series.

static void M_ExtHelpNextScreen(int choice)
{
    if (++extended_help_index > extended_help_count)
    {

        // when finished with extended help screens, return to Main Menu

        extended_help_index = 1;
        SetNextMenu(&MainDef);
    }
}

// phares 3/30/98:
// Routine to look for HELPnn screens and create a menu
// definition structure that defines extended help screens.

static void M_InitExtendedHelp(void)
{
    int index, i;
    char namebfr[] = "HELPnn"; // haleyjd: can't write into constant

    extended_help_count = 0;
    for (index = 1; index < 100; index++)
    {
        namebfr[4] = index / 10 + 0x30;
        namebfr[5] = index % 10 + 0x30;
        i = W_CheckNumForName(namebfr);
        if (i == -1)
        {
            if (extended_help_count)
            {
                // Restore extended help functionality
                // for all game versions
                ExtHelpDef.prevMenu = &ReadDef2; // previous menu
                ReadMenu2[0].routine = M_ExtHelp;
            }
            return;
        }
        extended_help_count++;
    }
}

// Initialization for the extended HELP screens.

static void M_ExtHelp(int choice)
{
    extended_help_index = 1; // Start with first extended help screen
    SetNextMenu(&ExtHelpDef);
}

// Initialize the drawing part of the extended HELP screens.

static void M_DrawExtHelp(void)
{
    char namebfr[] = "HELPnn"; // [FG] char array!
    namebfr[4] = extended_help_index / 10 + 0x30;
    namebfr[5] = extended_help_index % 10 + 0x30;
    V_DrawPatchFullScreen(V_CachePatchName(namebfr, PU_CACHE));
}

//
// End of Extended HELP screens               // phares 3/30/98
//
////////////////////////////////////////////////////////////////////////////

/////////////////////////////
//
// SetupMenu is the definition of what the main Setup Screen should look
// like. Each entry shows that the cursor can land on each item (1), the
// built-in graphic lump (i.e. "M_KEYBND") that should be displayed,
// the program which takes over when an item is selected, and the hotkey
// associated with the item.

#define M_Y_SETUP 37

#define SETUP_MENU_RECT(n) \
    {0, M_Y_SETUP + (n) * LINEHEIGHT, SCREENWIDTH, LINEHEIGHT}

static menuitem_t SetupMenu[] = {
  // [FG] alternative text for missing menu graphics lumps
    {1, "M_GENERL", MN_General,     'g', "General",            SETUP_MENU_RECT(0)},
    {1, "M_KEYBND", MN_KeyBindings, 'k', "Key Bindings",       SETUP_MENU_RECT(1)},
    {1, "M_COMPAT", MN_Compat,      'c', "Compatibility",      SETUP_MENU_RECT(2)},
    {1, "M_STAT",   MN_StatusBar,   's', "Status Bar/HUD",     SETUP_MENU_RECT(3)},
    {1, "M_AUTO",   MN_Automap,     'a', "Automap",            SETUP_MENU_RECT(4)},
    {1, "M_WEAP",   MN_Weapons,     'w', "Weapons",            SETUP_MENU_RECT(5)},
    {1, "M_ENEM",   MN_Enemy,       'e', "Enemies",            SETUP_MENU_RECT(6)},
};

/////////////////////////////
//
// M_DoNothing does just that: nothing. Just a placeholder.

static void M_DoNothing(int choice)
{
}

/////////////////////////////
//
// Items needed to satisfy the 'Big Font' menu structures:
//
// the generic_setup_e enum mimics the 'Big Font' menu structures, but
// means nothing to the Setup Menus.

enum
{
    generic_setupempty1,
    generic_setup_end
} generic_setup_e;

// Generic_Setup is a do-nothing definition that the mainstream Menu code
// can understand, while the Setup Menu code is working. Another placeholder.

static menuitem_t Generic_Setup[] = {
    {1, "", M_DoNothing, 0}
};

/////////////////////////////
//
// SetupDef is the menu definition that the mainstream Menu code understands.
// This is used by M_Setup (below) to define what is drawn and what is done
// with the main Setup screen.

static menu_t SetupDef = {
    ss_comp + 1,   // number of Setup Menu items (Key Bindings, etc.)
    &MainDef,      // menu to return to when BACKSPACE is hit on this menu
    SetupMenu,     // definition of items to show on the Setup Screen
    M_DrawSetup,   // program that draws the Setup Screen
    60, M_Y_SETUP, // x,y position of the skull (modified when the skull is
                   // drawn). The skull is parked on the upper-left corner
                   // of the Setup screens, since it isn't needed as a cursor
    0              // last item the user was on for this menu
};

/////////////////////////////
//
// Here are the definitions of the individual Setup Menu screens. They
// follow the format of the 'Big Font' menu structures. See the comments
// for SetupDef (above) to help understand what each of these says.

static menu_t KeybndDef =
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawKeybnd,
    34, 5, // skull drawn here
    0
};

static menu_t WeaponDef =
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawWeapons,
    34, 5, // skull drawn here
    0
};

static menu_t StatusHUDDef =
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawStatusHUD,
    34, 5, // skull drawn here
    0
};

static menu_t AutoMapDef =
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawAutoMap,
    34, 5, // skull drawn here
    0
};

static menu_t EnemyDef = // phares 4/08/98
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawEnemy,
    34, 5, // skull drawn here
    0
};

static menu_t GeneralDef = // killough 10/98
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawGeneral,
    34, 5, // skull drawn here
    0
};

static menu_t CompatDef = // killough 10/98
{
    generic_setup_end,
    &SetupDef,
    Generic_Setup,
    MN_DrawCompat,
    34, 5, // skull drawn here
    0
};

static menu_t SfxDef = {
    generic_setup_end,  // numitems
    &SetupDef,          // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawSfx,         // routine
    34, 5,              // x, y (skull drawn here)
};

static menu_t MidiDef = {
    generic_setup_end,  // numitems
    &SetupDef,          // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawMidi,        // routine
    34, 5,              // x, y (skull drawn here)
};

static menu_t EqualizerDef = {
    generic_setup_end,  // numitems
    &SetupDef,          // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawEqualizer,   // routine
    34, 5,              // x, y (skull drawn here)
};

static menu_t PadAdvDef = {
    generic_setup_end,  // numitems
    &SetupDef,          // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawPadAdv,      // routine
    34, 5,              // x, y (skull drawn here)
};

static menu_t GyroDef = {
    generic_setup_end,  // numitems
    &SetupDef,          // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawGyro,        // routine
    34, 5,              // x, y (skull drawn here)
};

static menu_t CustomSkillDef = {
    generic_setup_end,  // numitems
    &NewDef,            // prevMenu
    Generic_Setup,      // menuitems
    MN_DrawCustomSkill, // routine
    34, 5,              // x, y (skull drawn here)
};

void MN_SetNextMenuAlt(ss_types type)
{
    static menu_t *setup_defs[] = {
        &KeybndDef, &WeaponDef,    &StatusHUDDef, &AutoMapDef,
        &EnemyDef,  &GeneralDef,   &CompatDef,    &SfxDef,
        &MidiDef,   &EqualizerDef, &PadAdvDef,    &GyroDef,
        &CustomSkillDef
    };

    SetNextMenu(setup_defs[type]);
}

/////////////////////////////
//
// Draws the Title for the main Setup screen

static void M_DrawSetup(void)
{
    MN_DrawTitle(M_X_CENTER, 15, "M_OPTTTL", "OPTIONS");
}

/////////////////////////////
//
// Uses the SetupDef structure to draw the menu items for the main
// Setup screen

static void M_Setup(int choice)
{
    options_active = true;

    SetNextMenu(&SetupDef);
}

//
// M_ClearMenus
//
// Called when leaving the menu screens for the real world

void MN_ClearMenus(void)
{
    menuactive = 0;
    mouse_active_thermo = false;
    options_active = false;
    print_warning_about_changes = 0; // killough 8/15/98
    default_verify = 0;              // killough 10/98

    // if (!netgame && usergame && paused)
    //     sendpause = true;

    I_SetSensorEventState(false);
    G_ClearInput();
    M_ResumeSound();
}

static boolean MenuBack(void)
{
    if (!currentMenu->prevMenu)
    {
        return false;
    }

    currentMenu = currentMenu->prevMenu;
    itemOn = currentMenu->lastOn;
    highlight_item = 0;
    M_StartSound(sfx_swtchn);
    return true;
}

void MN_Back(void)
{
    MenuBack();
}

void MN_BackSecondary(void)
{
    if (MenuBack())
    {
        if (currentMenu->menuitems && currentMenu->numitems > itemOn
            && currentMenu->menuitems[itemOn].routine)
        {
            currentMenu->menuitems[itemOn].routine(0);
        }
    }
}

static void UpdateRectY(menu_t *menu, int m_y)
{
    for (int i = 0; i < menu->numitems; i++)
    {
        menu->menuitems[i].rect.y = SAVE_LOAD_RECT_Y(m_y, i);
    }
}

void M_ResetAutoSave(void)
{
    int m_y;

    if (G_AutoSaveEnabled())
    {
        loadsave_title_y = AUTOGRAPHIC_Y;
        m_y = M_Y_AUTOSAVE;
    }
    else
    {
        loadsave_title_y = LOADGRAPHIC_Y;
        m_y = M_Y_LOADSAVE;
    }

    LoadDef.y = m_y;
    UpdateRectY(&LoadDef, m_y);

    SaveDef.y = m_y;
    UpdateRectY(&SaveDef, m_y);
}

//
// M_Init
//

#define MAX_STRLEN 42

static void AddLineBreaks(char *string)
{
    char *start = string;
    char *p = start;

    while (strlen(p) > MAX_STRLEN)
    {
        start = p;
        p += MAX_STRLEN;

        do
        {
            if (*p == ' ')
            {
                *p++ = '\n';
                break;
            }
        } while (--p > start);

        if (p == start)
        {
            break;
        }
    }
}

#undef MAX_STRLEN

void M_Init(void)
{
    MN_InitDefaults(); // killough 11/98
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    highlight_item = 0;
    whichSkull = 0;
    skullAnimCounter = 10;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;
    M_ResetAutoSave();

    int lumpnum = W_CheckNumForName("DBIGFONT");
    if (lumpnum >= 0)
    {
        bigfont_priority = lumpinfo[lumpnum].handle.priority;
        MN_LoadFon2(W_CacheLumpNum(lumpnum, PU_CACHE), W_LumpLength(lumpnum));
    }

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion == exe_final)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
        // [crispy] rearrange Skull in Final Doom HELP screen
        ReadDef2.y -= 10;
    }

    if (gamemode == commercial)
    {
        // This is used because DOOM 2 had only one HELP
        //  page. I use CREDIT as second page now, but
        //  kept this hack for educational purposes.
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        if (!EpiCustom || EpiDef.numitems <= 1)
        {
            NewDef.prevMenu = &MainDef;
        }
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[0].routine = M_FinishReadThis;
    }

    if (pwad_help2)
    {
        MainMenu[readthis].routine = M_ReadThis;
        ReadDef2.prevMenu = &ReadDef1;
    }
    else if (gamemode == retail)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate && !EpiCustom)
    {
        EpiDef.numitems--;
    }

    MN_SetupResetMenu();
    M_InitExtendedHelp(); // init extended help screens // phares 3/30/98

    // [crispy] remove DOS reference from the game quit confirmation dialogs
    {
        const char *platform = I_GetPlatform();
        char *string;
        char *replace;

        string = *endmsg[3];
        replace = M_StringReplace(string, "dos", platform);
        *endmsg[3] = replace;

        string = *endmsg[4];
        replace = M_StringReplace(string, "dos", platform);
        *endmsg[4] = replace;

        string = *endmsg[9];
        replace = M_StringReplace(string, "dos", platform);

#if defined(_WIN32)
        string = M_StringReplace(replace, "prompt", "desktop");
#else
        if (isatty(STDOUT_FILENO))
        {
            string = M_StringReplace(replace, "prompt", "shell");
        }
        else
        {
            string = M_StringReplace(replace, "prompt", "desktop");
        }
#endif
        free(replace);
        *endmsg[9] = string;
    }

    for (int i = 0; i < NUM_QUITMESSAGES; i++)
    {
        char *const msg = *endmsg[i];

        if (strchr(msg, '\n') == NULL)
        {
            AddLineBreaks(*endmsg[i]);
        }
    }
}

/////////////////////////////
//
// M_Ticker
//

void M_Ticker(void)
{
    if (--skullAnimCounter <= 0)
    {
        whichSkull ^= 1;
        skullAnimCounter = 8;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// M_Responder
//
// Examines incoming keystrokes and button pushes and determines some
// action based on the state of the system.
//

boolean M_ShortcutResponder(const event_t *ev)
{
    // If there is no active menu displayed...

    if (menuactive || chat_on)
    {
        return false;
    }

    if (M_InputActivated(input_netgame_stats))
    {
        if (gamestate == GS_LEVEL && (netgame || deathmatch))
        {
            wi_overlay = !wi_overlay;
            return true;
        }
    }

    if (M_InputActivated(input_autorun)) // Autorun
    {
        autorun = !autorun;
        togglemsg("Always Run %s", autorun ? "On" : "Off");
        // return true; // [FG] don't let toggles eat keys
    }

    if (M_InputActivated(input_novert))
    {
        novert = !novert;
        togglemsg("Vertical Mouse %s", !novert ? "On" : "Off");
        // return true; // [FG] don't let toggles eat keys
    }

    if (M_InputActivated(input_freelook))
    {
        freelook = !freelook;
        togglemsg("Free Look %s", freelook ? "On" : "Off");
        MN_UpdateFreeLook();
        // return true; // [FG] don't let toggles eat keys
    }

    if (M_InputActivated(input_speed_up) && !D_CheckNetConnect() && !strictmode)
    {
        realtic_clock_rate += 10;
        realtic_clock_rate = CLAMP(realtic_clock_rate, 10, 1000);
        displaymsg("Game Speed: %d", realtic_clock_rate);
        G_SetTimeScale();
        setrefreshneeded = true;
    }

    if (M_InputActivated(input_speed_down) && !D_CheckNetConnect()
        && !strictmode)
    {
        realtic_clock_rate -= 10;
        realtic_clock_rate = CLAMP(realtic_clock_rate, 10, 1000);
        displaymsg("Game Speed: %d", realtic_clock_rate);
        G_SetTimeScale();
        setrefreshneeded = true;
    }

    if (M_InputActivated(input_speed_default) && !D_CheckNetConnect()
        && !strictmode)
    {
        realtic_clock_rate = 100;
        displaymsg("Game Speed: %d", realtic_clock_rate);
        G_SetTimeScale();
        setrefreshneeded = true;
    }

    if (M_InputActivated(input_help)) // Help key
    {
        boolean help2 = (gamemode < commercial || pwad_help2);
        MN_StartControlPanel();
        currentMenu = help2 ? &ReadDef1 : &ReadDef2; // killough 10/98: new help screen
        currentMenu->prevMenu = NULL;
        itemOn = 0;
        return true;
    }

    if (M_InputActivated(input_savegame)) // Save Game
    {
        MN_StartControlPanel();
        M_SaveGame(0);
        return true;
    }

    if (M_InputActivated(input_loadgame)) // Load Game
    {
        MN_StartControlPanel();
        M_LoadGame(0);
        return true;
    }

    if (M_InputActivated(input_soundvolume)) // Sound Volume
    {
        MN_StartControlPanel();
        currentMenu = &SoundDef;
        itemOn = currentMenu->lastOn;
        return true;
    }

    if (M_InputActivated(input_quicksave)) // Quicksave
    {
        M_QuickSave();
        return true;
    }

    if (M_InputActivated(input_endgame)) // End game
    {
        M_PauseSound();
        M_StartSound(sfx_swtchn);
        M_EndGame(0);
        return true;
    }

    if (M_InputActivated(input_messages)) // Toggle messages
    {
        M_ChangeMessages(0);
        M_StartSound(sfx_swtchn);
        return true;
    }

    if (M_InputActivated(input_quickload)) // Quickload
    {
        M_QuickLoad();
        return true;
    }

    if (M_InputActivated(input_quit)) // Quit DOOM
    {
        if (quit_prompt)
        {
            M_PauseSound();
            M_StartSound(sfx_swtchn);
        }
        M_QuitDOOM(0);
        return true;
    }

    if (M_InputActivated(input_gamma)) // gamma toggle
    {
        gamma2++;
        if (gamma2 > 17)
        {
            gamma2 = 0;
        }
        togglemsg("Gamma correction level %s", gamma_strings[gamma2]);
        MN_ResetGamma();
        return true;
    }

    if (M_InputActivated(input_zoomout)) // zoom out
    {
        if (automapactive)
        {
            return false;
        }
        M_SizeDisplay(0);
        M_StartSound(sfx_stnmov);
        return true;
    }

    if (M_InputActivated(input_zoomin)) // zoom in
    {                                   // jff 2/23/98
        if (automapactive)              // allow
        {                               // key_hud==key_zoomin
            return false;
        }
        M_SizeDisplay(1);
        M_StartSound(sfx_stnmov);
        return true;
    }

    if (M_InputActivated(input_hud)) // heads-up mode
    {
        if (automap_on) // jff 2/22/98
        {
            return false; // HUD mode control
        }

        if (screenblocks < 10)
        {
            screenblocks = 10;
        }
        else
        {
            ++screenblocks;
            if (screenblocks > maxscreenblocks)
            {
                screenblocks = 10;
            }
        }

        R_SetViewSize(screenblocks);
        return true;
    }

    if (M_InputActivated(input_hud_timestats))
    {
        if (automapactive)
        {
            if ((hud_level_stats | hud_level_time) & HUD_WIDGET_AUTOMAP)
            {
                hud_level_stats &= ~HUD_WIDGET_AUTOMAP;
                hud_level_time &= ~HUD_WIDGET_AUTOMAP;
            }
            else
            {
                hud_level_stats |= HUD_WIDGET_AUTOMAP;
                hud_level_time |= HUD_WIDGET_AUTOMAP;
            }
        }
        else
        {
            if ((hud_level_stats | hud_level_time) & HUD_WIDGET_HUD)
            {
                hud_level_stats &= ~HUD_WIDGET_HUD;
                hud_level_time &= ~HUD_WIDGET_HUD;
            }
            else
            {
                hud_level_stats |= HUD_WIDGET_HUD;
                hud_level_time |= HUD_WIDGET_HUD;
            }
        }
        return true;
    }

    static boolean fastdemo_timer = false;

    // [FG] reload current level / go to next level
    if (M_InputActivated(input_menu_nextlevel))
    {
        if (demoplayback && singledemo && !PLAYBACK_SKIP)
        {
            fastdemo_timer = false;
            playback_nextlevel = true;
            G_EnableWarp(true);
            return true;
        }
        else if (G_GotoNextLevel(NULL, NULL))
        {
            return true;
        }
    }

    if (M_InputActivated(input_menu_prevlevel))
    {
        if (demoplayback && singledemo && !PLAYBACK_SKIP)
        {
            return false;
        }
        else if (G_GotoPrevLevel())
        {
            return true;
        }
    }

    if (M_InputActivated(input_demo_fforward))
    {
        if (demoplayback && !PLAYBACK_SKIP && !fastdemo && !D_CheckNetConnect())
        {
            fastdemo_timer = !fastdemo_timer;
            I_SetFastdemoTimer(fastdemo_timer);
            return true;
        }
    }

    if (M_InputActivated(input_rewind))
    {
        G_Rewind();
        return true;
    }

    return false;
}

menu_input_mode_t help_input, old_help_input;
menu_input_mode_t menu_input, old_menu_input;

static int mouse_state_x, mouse_state_y;

boolean MN_PointInsideRect(mrect_t *rect, int x, int y)
{
    return x > rect->x + video.deltaw && x < rect->x + rect->w + video.deltaw
           && y > rect->y && y < rect->y + rect->h;
}

static void CursorPosition(void)
{
    if (!menuactive || messageToPrint || delete_verify)
    {
        return;
    }

    if (MN_SetupCursorPostion(mouse_state_x, mouse_state_y))
    {
        return;
    }

    for (int i = 0; i < currentMenu->numitems; i++)
    {
        menuitem_t *item = &currentMenu->menuitems[i];
        mrect_t *rect = &item->rect;

        item->flags &= ~MF_HILITE;

        if (MN_PointInsideRect(rect, mouse_state_x, mouse_state_y))
        {
            if (item->flags & MF_THRM_STR)
            {
                continue;
            }

            item->flags |= MF_HILITE;

            int cursor = i;

            if (item->flags & MF_THRM)
            {
                cursor--;
            }

            if (highlight_item != cursor)
            {
                highlight_item = cursor;
                M_StartSound(sfx_itemup);
            }
        }
    }
}

static void ClearHighlightedItems(void)
{
    for (int i = 0; i < currentMenu->numitems; ++i)
    {
        currentMenu->menuitems[i].flags &= ~MF_HILITE;
    }
}

static void M_UpdateLoadMenu(void)
{
    if (G_AutoSaveEnabled()
        && (currentMenu == &LoadDef || currentMenu == &LoadAutoSaveDef))
    {
        if (savepage == 0 && currentMenu == &LoadDef)
        {
            LoadDef.lastOn = itemOn;
            SetNextMenu(&LoadAutoSaveDef);
        }
        else if (savepage != 0 && currentMenu == &LoadAutoSaveDef)
        {
            LoadAutoSaveDef.lastOn = itemOn;
            SetNextMenu(&LoadDef);
        }
    }
}

static boolean AnyLoadSaveMenu(void)
{
    return (currentMenu == &LoadDef || currentMenu == &QuickLoadDef
            || currentMenu == &SaveDef || currentMenu == &LoadAutoSaveDef);
}

static boolean SaveLoadResponder(menu_action_t action, int ch)
{
    if (!AnyLoadSaveMenu())
    {
        return false;
    }

    // [FG] delete a savegame

    if (delete_verify)
    {
        if (M_ToUpper(ch) == 'Y' || action == MENU_ENTER)
        {
            M_DeleteGame(old_menu_input == mouse_mode ? highlight_item : itemOn);
            M_ReadSaveStrings();
            M_StartSound(sfx_itemup);
            delete_verify = false;
        }
        else if (M_ToUpper(ch) == 'N' || action == MENU_BACKSPACE
                 || action == MENU_ESCAPE)
        {
            M_StartSound(sfx_itemup);
            delete_verify = false;
        }
        return true;
    }

    // [FG] support up to 8 pages of savegames

    if (action == MENU_LEFT)
    {
        if (savepage > 0)
        {
            savepage--;
            quickSaveSlot = -1;
            M_UpdateLoadMenu();
            M_ReadSaveStrings();
            M_StartSound(sfx_pstop);
        }
        return true;
    }
    else if (action == MENU_RIGHT)
    {
        if (savepage < savepage_max)
        {
            savepage++;
            quickSaveSlot = -1;
            M_UpdateLoadMenu();
            M_ReadSaveStrings();
            M_StartSound(sfx_pstop);
        }
        return true;
    }

    return false;
}

void MN_ResetMouseCursor(void)
{
    static boolean state;

    if (menu_input == mouse_mode && !state)
    {
        state = true;
        I_ShowMouseCursor(true);
    }
    else if (menu_input != mouse_mode && state)
    {
        state = false;
        ClearHighlightedItems();
        I_ShowMouseCursor(false);
    }
}

static boolean MouseResponder(void)
{
    if (!menuactive || messageToPrint || delete_verify)
    {
        return false;
    }

    MN_ResetMouseCursor();

    if (setup_active)
    {
        return MN_SetupMouseResponder(mouse_state_x, mouse_state_y);
    }

    menuitem_t *current_item = &currentMenu->menuitems[highlight_item];

    mrect_t *rect = &current_item->rect;

    if (current_item->flags & MF_PAGE)
    {
        if (M_InputActivated(input_menu_enter))
        {
            int dot = mouse_state_x - video.deltaw * 2 - rect->x;
            if (dot >= rect->w / 2)
            {
                SaveLoadResponder(MENU_RIGHT, 0);
            }
            else
            {
                SaveLoadResponder(MENU_LEFT, 0);
            }
            return true;
        }
        return false;
    }

    if (current_item->flags & MF_THRM_STR)
    {
        current_item++;
    }

    rect = &current_item->rect;

    if (M_InputActivated(input_menu_enter))
    {
        if (!MN_PointInsideRect(rect, mouse_state_x, mouse_state_y))
        {
            return true; // eat event
        }

        itemOn = highlight_item;
    }

    if (!(current_item->flags & MF_THRM))
    {
        return false;
    }

    if (M_InputActivated(input_menu_enter))
    {
        mouse_active_thermo = true;
    }
    else if (M_InputDeactivated(input_menu_enter))
    {
        mouse_active_thermo = false;
    }

    if (mouse_active_thermo)
    {
        int dot = mouse_state_x - (rect->x + M_THRM_STEP + video.deltaw);
        int step = M_MAX_VOL * FRACUNIT / (rect->w - M_THRM_STEP * 3);
        int value = dot * step / FRACUNIT;
        value = CLAMP(value, 0, M_MAX_VOL);

        current_item--;
        if (current_item->routine)
        {
            current_item->routine(value);
            M_StartSound(sfx_stnmov);
        }

        return true;
    }

    return false;
}

static boolean AllowDeleteSaveGame(void)
{
    int index = (old_menu_input == mouse_mode ? highlight_item : itemOn);
    return (((currentMenu == &LoadDef || currentMenu == &QuickLoadDef
              || currentMenu == &SaveDef)
             && LoadDef.menuitems[index].status)
            || (currentMenu == &LoadAutoSaveDef
                && LoadAutoSaveDef.menuitems[index].status));
}

boolean M_Responder(event_t *ev)
{
    int ch;
    static int joywait = 0;

    static menu_action_t repeat = MENU_NULL;
    menu_action_t action = MENU_NULL;

    old_help_input = help_input;
    old_menu_input = menu_input;

    ch = 0; // will be changed to a legit char if we're going to use it here

    switch (ev->type)
    {
        case ev_joystick_state:
            if (menu_input == pad_mode && repeat != MENU_NULL
                && joywait < I_GetTime())
            {
                action = repeat;
                joywait = I_GetTime() + 2;
            }
            else
            {
                return true;
            }
            break;

        case ev_joyb_down:
            help_input = pad_mode;
            menu_input = pad_mode;
            break;

        case ev_joyb_up:
            if (M_InputDeactivated(input_menu_up)
                || M_InputDeactivated(input_menu_down)
                || M_InputDeactivated(input_menu_right)
                || M_InputDeactivated(input_menu_left))
            {
                repeat = MENU_NULL;
            }
            return false;

        case ev_keydown:
            help_input = key_mode;
            menu_input = key_mode;
            ch = ev->data1.i;
            break;

        case ev_text:
            break;

        case ev_keyup:
            return false;

        case ev_mouseb_down:
            menu_input = mouse_mode;
            if (MouseResponder())
            {
                return true;
            }
            break;

        case ev_mouseb_up:
            if (menu_input == mouse_mode)
            {
                return MouseResponder();
            }
            return false;

        case ev_mouse_state:
            if (ev->data1.i == EV_RESIZE_VIEWPORT && menu_input != mouse_mode)
            {
                return true;
            }
            menu_input = mouse_mode;
            mouse_state_x = (int)ev->data2.f;
            mouse_state_y = (int)ev->data3.f;
            CursorPosition();
            MouseResponder();
            return true;

        default:
            return false;
    }

    if (M_InputActivated(input_menu_up))
    {
        action = MENU_UP;
    }
    else if (M_InputActivated(input_menu_down))
    {
        action = MENU_DOWN;
    }
    else if (M_InputActivated(input_menu_left))
    {
        action = MENU_LEFT;
    }
    else if (M_InputActivated(input_menu_right))
    {
        action = MENU_RIGHT;
    }
    else if (M_InputActivated(input_menu_backspace))
    {
        action = MENU_BACKSPACE;
    }
    else if (M_InputActivated(input_menu_enter))
    {
        action = MENU_ENTER;
    }
    else if (M_InputActivated(input_menu_escape))
    {
        action = MENU_ESCAPE;
    }
    else if (M_InputActivated(input_menu_clear))
    {
        action = MENU_CLEAR;
    }

    if (ev->type == ev_joyb_down && action >= MENU_UP && action <= MENU_RIGHT)
    {
        repeat = action;
        joywait = I_GetTime() + 15;
    }

    // Save Game string input

    if (saveStringEnter)
    {
        if (ch == KEY_BACKSPACE) // phares 3/7/98
        {
            if (saveCharIndex > 0)
            {
                if (MN_StartsWithMapIdentifier(savegamestrings[saveSlot])
                    && saveStringEnter == 1)
                {
                    saveCharIndex = 0;
                }
                else
                {
                    saveCharIndex--;
                }
                savegamestrings[saveSlot][saveCharIndex] = 0;
            }
        }
        else if (action == MENU_ESCAPE) // phares 3/7/98
        {
            I_StopTextInput();
            saveStringEnter = 0;
            strcpy(&savegamestrings[saveSlot][0], saveOldString);
        }
        else if (action == MENU_ENTER) // phares 3/7/98
        {
            I_StopTextInput();
            saveStringEnter = 0;
            if (savegamestrings[saveSlot][0])
            {
                M_DoSave(saveSlot);
            }
        }
        else if (ev->type == ev_text)
        {
            int txt = ev->data1.i;

            txt = M_ToUpper(txt);

            if (txt >= ' ' && txt <= '_' && saveCharIndex < SAVESTRINGSIZE - 1
                && MN_StringWidth(savegamestrings[saveSlot])
                       < (SAVESTRINGSIZE - 2) * 8)
            {
                savegamestrings[saveSlot][saveCharIndex++] = txt;
                savegamestrings[saveSlot][saveCharIndex] = 0;
            }
            saveStringEnter = 2; // [FG] save string modified
        }
        return true;
    }

    // Take care of any messages that need input

    if (messageToPrint)
    {
        if (action == MENU_ENTER)
        {
            ch = 'y';
        }

        if (messageNeedsInput == true
            && !(ch == ' ' || ch == 'n' || ch == 'y' || ch == key_escape
                 || action == MENU_BACKSPACE)) // phares
        {
            return false;
        }

        menuactive = messageLastMenuActive;
        messageToPrint = 0;
        if (messageRoutine)
        {
            messageRoutine(ch);
        }

        I_SetSensorEventState(false);
        G_ClearInput();
        menuactive = false;
        M_ResumeSound();
        M_StartSound(sfx_swtchx);
        return true;
    }

    // killough 2/22/98: add support for screenshot key:

    if ((devparm && ch == key_help) || M_InputActivated(input_screenshot))
    {
        G_ScreenShot();
        // return true; // [FG] don't let toggles eat keys
    }

    if (M_InputActivated(input_clean_screenshot))
    {
        clean_screenshot = true;
        G_ScreenShot();
    }

    // Pop-up Main menu?

    if (!menuactive)
    {
        if (!chat_on
            && ((demoplayback
                 && (action == MENU_ENTER || action == MENU_BACKSPACE))
                || action == MENU_ESCAPE)) // phares
        {
            I_ShowMouseCursor(menu_input != pad_mode);
            MN_StartControlPanel();
            return true;
        }
        return false;
    }

    if (SaveLoadResponder(action, ch))
    {
        return true;
    }

    if (MN_SetupResponder(action, ch))
    {
        return true;
    }

    if (menuactive)
    {
        MN_ResetMouseCursor();
    }

    // From here on, these navigation keys are used on the BIG FONT menus
    // like the Main Menu.

    if (action == MENU_DOWN) // phares 3/7/98
    {
        do
        {
            if (itemOn + 1 > currentMenu->numitems - 1)
            {
                itemOn = 0;
            }
            else
            {
                itemOn++;
            }
            M_StartSound(sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }

    if (action == MENU_UP) // phares 3/7/98
    {
        do
        {
            if (!itemOn)
            {
                itemOn = currentMenu->numitems - 1;
            }
            else
            {
                itemOn--;
            }
            M_StartSound(sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }

    if (action == MENU_LEFT) // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine
            && currentMenu->menuitems[itemOn].status == 2)
        {
            M_StartSound(sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(CHOICE_LEFT);
        }
        return true;
    }

    if (action == MENU_RIGHT) // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine
            && currentMenu->menuitems[itemOn].status == 2)
        {
            M_StartSound(sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(CHOICE_RIGHT);
        }
        return true;
    }

    if (action == MENU_ENTER) // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine
            && currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;
            currentMenu->menuitems[itemOn].flags &= ~MF_HILITE;
            if (currentMenu->menuitems[itemOn].status == 2)
            {
                currentMenu->menuitems[itemOn].routine(CHOICE_RIGHT);
                M_StartSound(sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                M_StartSound(sfx_pistol);
            }
        }
        else
        {
            M_StartSound(sfx_oof); // [FG] disabled menu item
        }
        // jff 3/24/98 remember last skill selected
        //  killough 10/98 moved to skill-specific functions
        return true;
    }

    if (action == MENU_ESCAPE) // phares 3/7/98
    {
        if (!(currentMenu->menuitems[itemOn].flags & MF_PAGE))
        {
            currentMenu->lastOn = itemOn;
        }
        MN_ClearMenus();
        M_StartSound(sfx_swtchx);
        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();
        return true;
    }

    if (action == MENU_BACKSPACE) // phares 3/7/98
    {
        menuitem_t *current_item = &currentMenu->menuitems[itemOn];
        if (!(current_item->flags & MF_PAGE))
        {
            currentMenu->lastOn = itemOn;
        }
        current_item->flags &= ~MF_HILITE;

        // phares 3/30/98:
        // add checks to see if you're in the extended help screens
        // if so, stay with the same menu definition, but bump the
        // index back one. if the index bumps back far enough ( == 0)
        // then you can return to the Read_Thisn menu definitions

        if (currentMenu->prevMenu)
        {
            if (currentMenu == &ExtHelpDef)
            {
                if (--extended_help_index == 0)
                {
                    currentMenu = currentMenu->prevMenu;
                    extended_help_index = 1; // reset
                }
            }
            else
            {
                currentMenu = currentMenu->prevMenu;
            }
            itemOn = currentMenu->lastOn;
            highlight_item = 0;
            M_StartSound(sfx_swtchn);
        }
        else
        {
            MN_ClearMenus();
            M_StartSound(sfx_swtchx);
        }
        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();
        return true;
    }

    // [FG] delete a savegame

    if (action == MENU_CLEAR)
    {
        if (AnyLoadSaveMenu())
        {
            if (AllowDeleteSaveGame())
            {
                M_StartSound(sfx_itemup);
                currentMenu->lastOn = itemOn;
                help_input = old_help_input;
                menu_input = old_menu_input;
                delete_verify = true;
                return true;
            }
            else
            {
                M_StartSound(sfx_oof);
            }
        }
    }

    if (ch) // fix items with alphaKey == 0
    {
        int i;

        for (i = itemOn + 1; i < currentMenu->numitems; i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                M_StartSound(sfx_pstop);
                return true;
            }
        }

        for (i = 0; i <= itemOn; i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                M_StartSound(sfx_pstop);
                return true;
            }
        }
    }

    return false;
}

//
// End of M_Responder
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
// General Routines
//
// This displays the Main menu and gets the menu screens rolling.
// Plus a variety of routines that control the Big Font menu display.
// Plus some initialization for game-dependant situations.

void MN_StartControlPanel(void)
{
    // intro might call this repeatedly

    if (menuactive)
    {
        return;
    }

    // jff 3/24/98 make default skill menu choice follow -skill or defaultskill
    // from command line or config file
    //
    //  killough 10/98:
    //  Fix to make "always floating" with menu selections, and to always follow
    //  defaultskill, instead of -skill.

    NewDef.lastOn = default_skill - 1;

    default_verify = 0; // killough 10/98
    menuactive = 1;
    currentMenu = &MainDef;              // JDC
    itemOn = currentMenu->lastOn;        // JDC
    highlight_item = 0;
    print_warning_about_changes = false; // killough 11/98

    I_SetSensorEventState(true);
    G_ClearInput();

    M_PauseSound();
    M_StartSound(sfx_swtchn);
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
// killough 9/29/98: Significantly reformatted source
//

boolean MN_MenuIsShaded(void)
{
    return (options_active || customskill_active)
           && menu_backdrop == MENU_BG_DARK;
}

void M_Drawer(void)
{
    // Horiz. & Vertically center string and print it.
    // killough 9/29/98: simplified code, removed 40-character width limit
    if (messageToPrint)
    {
        // haleyjd 11/11/04: must strdup message, cannot write into
        // string constants!
        char *d = strdup(messageString);
        char *p;
        int y = 100 - MN_StringHeight(messageString) / 2;

        p = d;

        while (*p)
        {
            char *string = p, c;

            while ((c = *p) && *p != '\n')
            {
                p++;
            }
            *p = 0;

            WriteText(160 - MN_StringWidth(string) / 2, y, string);
            y += SHORT(hu_font[0]->height);

            if ((*p = c))
            {
                p++;
            }
        }

        // haleyjd 11/11/04: free duplicate string
        free(d);
        return;
    }

    if (!menuactive)
    {
        return;
    }

    if (MN_MenuIsShaded())
    {
        V_ShadeScreen();
    }

    if (currentMenu->routine)
    {
        currentMenu->routine(); // call Draw routine
    }

    // DRAW MENU

    int x, y, max;

    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    // [FG] check current menu for missing menu graphics lumps - only once
    if (currentMenu->lumps_missing == 0)
    {
        for (int i = 0; i < max; i++)
        {
            char *name = currentMenu->menuitems[i].name;
            int patch_lump = -1;
            int patch_priority = -1;

            if (name[0])
            {
                patch_lump = W_CheckNumForName(name);
                if (patch_lump >= 0)
                {
                    patch_priority = lumpinfo[patch_lump].handle.priority;
                }
                else
                {
                    name[0] = '\0';
                }
            }

            if ((patch_lump < 0 || patch_priority < bigfont_priority)
                && currentMenu->menuitems[i].alttext
                && !(currentMenu->menuitems[i].flags & MF_OPTLUMP))
            {
                currentMenu->lumps_missing++;
                break;
            }
        }

        // [FG] no lump missing, no need to check again
        if (currentMenu->lumps_missing == 0)
        {
            currentMenu->lumps_missing = -1;
        }
    }

    for (int i = 0; i < max; ++i)
    {
        menuitem_t *item = &currentMenu->menuitems[i];

        const char *alttext = item->alttext;
        const char *name = item->name;

        byte *cr;
        if (item->status == 0)
        {
            cr = cr_dark;
        }
        else if (item->flags & MF_HILITE)
        {
            cr = cr_bright;
        }
        else
        {
            cr = NULL;
        }

        mrect_t *rect = &item->rect;
        // due to the MainMenu[] hacks, we have to set `y` here
        rect->y = y;

        // [FG] at least one menu graphics lump is missing, draw alternative
        // text
        if (currentMenu->lumps_missing > 0
            || (!name[0] && item->flags & MF_OPTLUMP))
        {
            if (alttext)
            {
                if (!MN_DrawFon2String(x, y, cr, alttext))
                {
                    MN_DrawStringCR(x, y + 8 - MN_StringHeight(alttext) / 2, cr,
                                    NULL, alttext);
                }
            }
        }
        else if (name[0])
        {
            patch_t *patch = V_CachePatchName(name, PU_CACHE);
            rect->y -= SHORT(patch->topoffset);
            V_DrawPatchTranslated(x, y, patch, cr);
        }

        y += LINEHEIGHT;
    }

    // DRAW SKULL

    y = setup_active ? SCREENHEIGHT - 19 : currentMenu->y;

    V_DrawPatch(x + SKULLXOFF, y - 5 + itemOn * LINEHEIGHT,
                V_CachePatchName(skullName[whichSkull], PU_CACHE));

    if (delete_verify)
    {
        MN_DrawDelVerify();
    }
}

/////////////////////////////
//
// Message Routines
//

static void M_StartMessage(char *string, void (*routine)(int), boolean input)
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}

/////////////////////////////
//
// Thermometer Routines
//

//
// M_DrawThermo draws the thermometer graphic for Mouse Sensitivity,
// Sound Volume, etc.
//
static void M_DrawThermo(int x, int y, int thermWidth, int thermDot, byte *cr)
{
    int xx;
    int i;
    char num[4];

    xx = x;
    V_DrawPatchTranslated(xx, y, V_CachePatchName("M_THERML", PU_CACHE), cr);
    xx += 8;
    for (i = 0; i < thermWidth; i++)
    {
        V_DrawPatchTranslated(xx, y, V_CachePatchName("M_THERMM", PU_CACHE), cr);
        xx += 8;
    }
    V_DrawPatchTranslated(xx, y, V_CachePatchName("M_THERMR", PU_CACHE), cr);

    // [FG] write numerical values next to thermometer
    M_snprintf(num, 4, "%3d", thermDot);
    WriteText(xx + 8, y + 3, num);

    // [FG] do not crash anymore if value exceeds thermometer range
    if (thermDot >= thermWidth)
    {
        thermDot = thermWidth - 1;
    }

    V_DrawPatchTranslated((x + 8) + thermDot * 8, y,
                          V_CachePatchName("M_THERMO", PU_CACHE), cr);
}

//
//    Write a string using the hu_font
//

static void WriteText(int x, int y, const char *string)
{
    int w;
    const char *ch;
    int c;
    int cx;
    int cy;

    ch = string;
    cx = x;
    cy = y;

    while (1)
    {
        c = *ch++;
        if (!c)
        {
            break;
        }
        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            cx += 4;
            continue;
        }

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
        {
            break;
        }
        V_DrawPatch(cx, cy, hu_font[c]);
        cx += w;
    }
}

void M_StartSound(int sound_id)
{
    S_StartSoundPitch(NULL, sound_id, PITCH_NONE);
}

//----------------------------------------------------------------------------
//
// $Log: m_menu.c,v $
// Revision 1.54  1998/05/28  05:27:13  killough
// Fix some load / save / end game handling r.w.t. demos
//
// Revision 1.53  1998/05/16  09:17:09  killough
// Make loadgame checksum friendlier
//
// Revision 1.52  1998/05/05  15:34:55  phares
// Documentation and Reformatting changes
//
// Revision 1.51  1998/05/03  21:55:58  killough
// Provide minimal required headers and decls
//
// Revision 1.50  1998/05/01  21:35:06  killough
// Fix status bar update after leaving help screens
//
// Revision 1.49  1998/04/24  23:51:51  thldrmn
// Reinstated gamma correction deh variables
//
// Revision 1.48  1998/04/23  13:07:05  jim
// Add exit line to automap
//
// Revision 1.47  1998/04/22  13:46:02  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.46  1998/04/19  01:19:42  killough
// Tidy up last fix's code
//
// Revision 1.45  1998/04/17  14:46:33  killough
// fix help showstopper
//
// Revision 1.44  1998/04/17  10:28:46  killough
// Add traditional_menu
//
// Revision 1.43  1998/04/14  11:29:50  phares
// Added demorecording as a condition for delaying config change
//
// Revision 1.42  1998/04/14  10:55:24  phares
// Recoil, Bobbing, Monsters Remember changes in Setup now take effect
// immediately
//
// Revision 1.41  1998/04/13  21:36:24  phares
// Cemented ESC and F1 in place
//
// Revision 1.40  1998/04/12  22:55:23  phares
// Remaining 3 Setup screens
//
// Revision 1.39  1998/04/06  05:01:04  killough
// set inhelpscreens=true for status bar update, rearrange menu yet again
//
// Revision 1.38  1998/04/05  00:50:59  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.37  1998/04/03  19:18:31  phares
// Automap Palette work, slot 0 = disable, 247 = BLACK
//
// Revision 1.36  1998/04/03  14:45:28  jim
// Fixed automap disables at 0, mouse sens unbounded
//
// Revision 1.35  1998/04/01  15:34:09  phares
// Added Automap Setup Screen, fixed Seg Viol in Setup Menus
//
// Revision 1.34  1998/03/31  23:07:40  phares
// Fixed bug in key binding screen causing seg viol
//
// Revision 1.33  1998/03/31  10:40:06  killough
// Fix incorrect order of quit message
//
// Revision 1.32  1998/03/31  01:07:59  phares
// Initial Setup screens and Extended HELP screens
//
// Revision 1.31  1998/03/28  05:32:25  jim
// Text enabling changes for DEH
//
// Revision 1.30  1998/03/24  15:59:36  jim
// Added default_skill parameter to config file
//
// Revision 1.29  1998/03/23  15:21:24  phares
// Start of setup menus
//
// Revision 1.28  1998/03/23  03:22:00  killough
// Use G_SaveGameName for consistent savegame naming
//
// Revision 1.27  1998/03/16  12:31:11  killough
// Remember savegame slot when loading
//
// Revision 1.26  1998/03/15  14:41:15  jim
// added two more save/load slots
//
// Revision 1.25  1998/03/11  17:48:10  phares
// New cheats, clean help code, friction fix
//
// Revision 1.24  1998/03/10  07:07:25  jim
// Fixed display glitch in HUD cycle
//
// Revision 1.23  1998/03/09  18:29:06  phares
// Created separately bound automap and menu keys
//
// Revision 1.22  1998/03/09  07:36:45  killough
// Some #ifdef'ed help screen fixes, saved autorun status
//
// Revision 1.21  1998/03/05  11:29:26  jim
// Fixed mis-merge in m_menu.c
//
// Revision 1.20  1998/03/05  01:12:34  jim
// Added distributed hud to key_hud function
//
// Revision 1.19  1998/03/04  22:15:51  phares
// Included missing externs
//
// Revision 1.18  1998/03/04  21:02:20  phares
// Dynamic HELP screen
//
// Revision 1.17  1998/03/04  11:54:56  jim
// Fix fullscreen bug in F5 key
//
// Revision 1.16  1998/03/02  15:34:06  jim
// Added Rand's HELP screen as lump and loaded and displayed it
//
// Revision 1.15  1998/02/24  10:52:13  jim
// Fixed missing changes in m_menu.c
//
// Revision 1.14  1998/02/24  09:13:01  phares
// Corrected key_detail->key_hud oversight
//
// Revision 1.13  1998/02/24  08:46:00  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.12  1998/02/24  04:14:08  jim
// Added double keys to status
//
// Revision 1.11  1998/02/23  14:21:04  jim
// Merged HUD stuff, fixed p_plats.c to support elevators again
//
// Revision 1.10  1998/02/23  04:35:44  killough
// Fix help screens and broken HUD control
//
// Revision 1.8  1998/02/19  16:54:40  jim
// Optimized HUD and made more configurable
//
// Revision 1.7  1998/02/18  11:56:03  jim
// Fixed issues with HUD and reduced screen size
//
// Revision 1.5  1998/02/17  06:11:59  killough
// Support basesavegame path to savegames
//
// Revision 1.4  1998/02/15  02:47:50  phares
// User-defined keys
//
// Revision 1.3  1998/02/02  13:38:15  killough
// Add mouse sensitivity menu bar lumps
//
// Revision 1.2  1998/01/26  19:23:47  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
