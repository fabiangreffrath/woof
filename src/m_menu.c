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

#include "doomdef.h"
#include "doomstat.h"
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_main.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "s_sound.h"
#include "sounds.h"
#include "m_menu.h"
#include "d_deh.h"
#include "m_misc.h"
#include "m_misc2.h" // [FG] M_StringDuplicate()
#include "m_io.h"
#include "m_swap.h"
#include "w_wad.h" // [FG] W_IsIWADLump() / W_WadNameForLump()
#include "p_saveg.h" // saveg_compat
#include "m_input.h"
#include "i_gamepad.h"
#include "r_draw.h" // [FG] R_SetFuzzColumnMode
#include "r_sky.h" // [FG] R_InitSkyMap()
#include "r_plane.h" // [FG] R_InitPlanes()
#include "r_voxel.h"
#include "m_argv.h"
#include "m_snapshot.h"
#include "i_sound.h"
#include "r_bmaps.h"
#include "m_array.h"
#include "am_map.h"
#include "r_voxel.h"

// [crispy] remove DOS reference from the game quit confirmation dialogs
#ifndef _WIN32
  #include <unistd.h> // [FG] isatty()
#endif

extern boolean  message_dontfuckwithme;

extern boolean chat_on;          // in heads-up code

//
// defaulted values
//

int mouseSensitivity_horiz; // has default   //  killough
int mouseSensitivity_vert;  // has default
int mouseSensitivity_horiz_strafe; // [FG] strafe
int mouseSensitivity_vert_look; // [FG] look

int showMessages;    // Show messages has default, 0 = off, 1 = on
int show_toggle_messages;
int show_pickup_messages;

int traditional_menu;

// Blocky mode, has default, 0 = high, 1 = normal
//int     detailLevel;    obsolete -- killough
int screenblocks;    // has default

static int saved_screenblocks;

static int screenSize;      // temp for screenblocks (0-9)

static int quickSaveSlot;   // -1 = no quicksave slot picked!

static int messageToPrint;  // 1 = message to be printed

static char *messageString; // ...and here is the message string!

static int     messageLastMenuActive;

static boolean messageNeedsInput; // timed message = no input from user

static void (*messageRoutine)(int response);

static boolean default_reset;

#define SAVESTRINGSIZE  24

static int warning_about_changes, print_warning_about_changes;

// killough 8/15/98: warn about changes not being committed until next game
#define warn_about_changes(x) (warning_about_changes = (x), \
                               print_warning_about_changes = 2)

// we are going to be entering a savegame string

static int saveStringEnter;
static int saveSlot;        // which slot to save in
static int saveCharIndex;   // which char we're editing
// old save description before edit
static char saveOldString[SAVESTRINGSIZE]; 

boolean inhelpscreens; // indicates we are in or just left a help screen

boolean menuactive;    // The menus are up

static boolean options_active;

backdrop_t menu_backdrop;

#define SKULLXOFF  -32
#define LINEHEIGHT  16

#define M_SPC        9
#define M_X          240
#define M_Y          (29 + M_SPC)
#define M_Y_WARN     (SCREENHEIGHT - 15) //(29 + 17 * M_SPC)

#define M_THRM_STEP   8
#define M_THRM_HEIGHT 13
#define M_THRM_SIZE4  4
#define M_THRM_SIZE8  8
#define M_THRM_SIZE11 11
#define M_X_THRM4     (M_X - (M_THRM_SIZE4 + 3) * M_THRM_STEP)
#define M_X_THRM8     (M_X - (M_THRM_SIZE8 + 3) * M_THRM_STEP)
#define M_X_THRM11    (M_X - (M_THRM_SIZE11 + 3) * M_THRM_STEP)
#define M_THRM_TXT_OFFSET 3
#define M_THRM_SPC    (M_THRM_HEIGHT + 1)
#define M_THRM_UL_VAL 20

#define M_X_LOADSAVE 80
#define M_LOADSAVE_WIDTH (24 * 8 + 8) // [FG] c.f. M_DrawSaveLoadBorder()

#define DISABLE_ITEM(condition, item) \
        ((condition) ? (item.m_flags |= S_DISABLE) : (item.m_flags &= ~S_DISABLE))

// Final entry
#define MI_END \
  {0, S_SKIP|S_END, m_null}

// Button for resetting to defaults
#define MI_RESET \
  {0, S_RESET, m_null, X_BUTTON, Y_BUTTON}

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
  char  name[10];

  // choice = menu item #.
  // if status = 2,
  //   choice = CHOICE_LEFT:leftarrow, CHOICE_RIGHT:rightarrow
  void  (*routine)(int choice);
  char  alphaKey; // hotkey in menu     
  const char *alttext; // [FG] alternative text for missing menu graphics lumps
  mflags_t flags;
  mrect_t rect;
} menuitem_t;

typedef struct menu_s
{
  short           numitems;     // # of menu items
  struct menu_s*  prevMenu;     // previous menu
  menuitem_t*     menuitems;    // menu items
  void            (*routine)(); // draw routine
  short           x;
  short           y;            // x,y of menu
  short           lastOn;       // last item user was on in menu
  int             lumps_missing; // [FG] indicate missing menu graphics lumps
} menu_t;

static short itemOn;           // menu item skull is on (for Big Font menus)
static short skullAnimCounter; // skull animation counter
static short whichSkull;       // which skull to draw (he blinks)

// graphic name of skulls

char skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

static menu_t SaveDef, LoadDef;
static menu_t* currentMenu; // current menudef

// phares 3/30/98
// externs added for setup menus
// [FG] double click acts as "use"
extern int dclick_use;
extern int health_red;    // health amount less than which status is red
extern int health_yellow; // health amount less than which status is yellow
extern int health_green;  // health amount above is blue, below is green
extern int armor_red;     // armor amount less than which status is red
extern int armor_yellow;  // armor amount less than which status is yellow
extern int armor_green;   // armor amount above is blue, below is green
extern int ammo_red;      // ammo percent less than which status is red
extern int ammo_yellow;   // ammo percent less is yellow more green
extern int sts_colored_numbers;// status numbers do not change colors
extern int sts_pct_always_gray;// status percents do not change colors
extern int sts_traditional_keys;  // display keys the traditional way

extern const char shiftxform[];
extern default_t defaults[];

// end of externs added for setup menus

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
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
static void M_SizeDisplay(int choice);

static void M_FinishReadThis(int choice);
static void M_FinishHelp(int choice);            // killough 10/98
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
static void M_DrawSetup(void);                                     // phares 3/21/98
static void M_DrawHelp (void);                                     // phares 5/04/98

static void M_DrawSaveLoadBorder(int x, int y, byte *cr);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x, int y, int thermWidth, int thermDot, byte *cr);
static void M_WriteText(int x, int y, const char *string);
static int  M_StringWidth(const char *string);
static int  M_StringHeight(const char *string);
static void M_StartMessage(char *string,void (*routine)(int),boolean input);
static void M_ClearMenus (void);

// phares 3/30/98
// prototypes added to support Setup Menus and Extended HELP screens

static int M_GetKeyString(int, int);
static void M_Setup(int choice);
static void M_KeyBindings(int choice);
static void M_Weapons(int);
static void M_StatusBar(int);
static void M_Automap(int);
static void M_Enemy(int);
static void M_InitExtendedHelp(void);
static void M_ExtHelpNextScreen(int);
static void M_ExtHelp(int);
int  M_GetPixelWidth(const char*);
static void M_DrawKeybnd(void);
static void M_DrawWeapons(void);
static void M_DrawMenuString(int,int,int);
static void M_DrawStatusHUD(void);
static void M_DrawExtHelp(void);
static void M_DrawAutoMap(void);
static void M_DrawEnemy(void);
static void M_Compat(int);       // killough 10/98
static void M_General(int);      // killough 10/98
static void M_DrawCompat(void);  // killough 10/98
static void M_DrawGeneral(void); // killough 10/98
static void M_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch);
// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it is not tied to menu_buffer
void M_DrawString(int,int,int,const char*);

// [FG] alternative text for missing menu graphics lumps
static void M_DrawTitle(int x, int y, const char *patch, const char *alttext);

static menu_t NewDef;                                              // phares 5/04/98

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
  loadgame,
  savegame,
  options,
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

static menuitem_t MainMenu[]=
{
  {1,"M_NGAME", M_NewGame, 'n'},
  {1,"M_LOADG", M_LoadGame,'l'},
  {1,"M_SAVEG", M_SaveGame,'s'},
  {1,"M_OPTION", M_Setup, 'o'}, // change M_Options to M_Setup
  // Another hickup with Special edition.
  {1,"M_RDTHIS",M_ReadThis,'r'},
  {1,"M_QUITG", M_QuitDOOM,'q'}
};

static menu_t MainDef =
{
  main_end,       // number of menu items
  NULL,           // previous menu screen
  MainMenu,       // table that defines menu items
  M_DrawMainMenu, // drawing routine
  97,64,          // initial cursor position
  0               // last menu item the user was on
};

//
// M_DrawMainMenu
//

static void M_DrawMainMenu(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  options_active = false;

  V_DrawPatchDirect (94,2,W_CacheLumpName("M_DOOM",PU_CACHE));
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

enum               // killough 10/98
{
  helpempty,
  help_end
} help_e;


// The definitions of the Read This! screens

static menuitem_t ReadMenu1[] =
{
  {1,"",M_ReadThis2,0}
};

static menuitem_t ReadMenu2[]=
{
  {1,"",M_FinishReadThis,0}
};

static menuitem_t HelpMenu[]=    // killough 10/98
{
  {1,"",M_FinishHelp,0}
};

static menu_t ReadDef1 =
{
  read1_end,
  &MainDef,
  ReadMenu1,
  M_DrawReadThis1,
  330,175,
  //280,185,              // killough 2/21/98: fix help screens
  0
};

static menu_t ReadDef2 =
{
  read2_end,
  &ReadDef1,
  ReadMenu2,
  M_DrawReadThis2,
  330,175,
  0
};

static menu_t HelpDef =           // killough 10/98
{
  help_end,
  &HelpDef,
  HelpMenu,
  M_DrawHelp,
  330,175,
  0
};

//
// M_ReadThis
//

static void M_ReadThis(int choice)
{
  M_SetupNextMenu(&ReadDef1);
}

static void M_ReadThis2(int choice)
{
  M_SetupNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int choice)
{
  M_SetupNextMenu(&MainDef);
}

static void M_FinishHelp(int choice)        // killough 10/98
{
  M_SetupNextMenu(&MainDef);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
// killough 10/98: updated with new screens

static void M_DrawReadThis1(void)
{
    inhelpscreens = true;
  
    V_DrawPatchFullScreen(W_CacheLumpName("HELP2", PU_CACHE));
}

//
// Read This Menus - optional second page.
//
// killough 10/98: updated with new screens

static void M_DrawReadThis2(void)
{
    inhelpscreens = true;
    
    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered
    
    V_DrawPatchFullScreen(W_CacheLumpName("HELP1", PU_CACHE));
}

static void M_DrawReadThisCommercial(void)
{
    inhelpscreens = true;

    V_DrawPatchFullScreen(W_CacheLumpName("HELP", PU_CACHE));
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

// The definitions of the Episodes menu

static menuitem_t EpisodeMenu[]=   // added a few free entries for UMAPINFO
{
  {1,"M_EPI1", M_Episode,'k'},
  {1,"M_EPI2", M_Episode,'t'},
  {1,"M_EPI3", M_Episode,'i'},
  {1,"M_EPI4", M_Episode,'t'},
  {1,"", M_Episode,'0'},
  {1,"", M_Episode,'0'},
  {1,"", M_Episode,'0'},
  {1,"", M_Episode,'0'}
};

static menu_t EpiDef =
{
  ep_end,        // # of menu items
  &MainDef,      // previous menu
  EpisodeMenu,   // menuitem_t ->
  M_DrawEpisode, // drawing routine ->
  48,63,         // x,y
  ep1            // lastOn
};

// This is for customized episode menus
boolean EpiCustom;
static short EpiMenuMap[] = { 1, 1, 1, 1, -1, -1, -1, -1 };
static short EpiMenuEpi[] = { 1, 2, 3, 4, -1, -1, -1, -1 };

//
//    M_Episode
//
static int epiChoice;

void M_ClearEpisodes(void)
{
    EpiDef.numitems = 0;
    NewDef.prevMenu = &MainDef;
}

void M_AddEpisode(const char *map, const char *gfx, const char *txt, const char *alpha)
{
    int epi, mapnum;

    if (!EpiCustom)
    {
        EpiCustom = true;
        NewDef.prevMenu = &EpiDef;

        if (gamemode == commercial)
            EpiDef.numitems = 0;
    }

    if (EpiDef.numitems >= 8)
        return;

    G_ValidateMapName(map, &epi, &mapnum);
    EpiMenuEpi[EpiDef.numitems] = epi;
    EpiMenuMap[EpiDef.numitems] = mapnum;
    strncpy(EpisodeMenu[EpiDef.numitems].name, gfx, 8);
    EpisodeMenu[EpiDef.numitems].name[9] = 0;
    EpisodeMenu[EpiDef.numitems].alttext = txt ? strdup(txt) : NULL;
    EpisodeMenu[EpiDef.numitems].alphaKey = alpha ? *alpha : 0;
    EpiDef.numitems++;

    if (EpiDef.numitems <= 4)
    {
        EpiDef.y = 63;
    }
    else
    {
        EpiDef.y = 63 - (EpiDef.numitems - 4) * (LINEHEIGHT / 2);
    }
}

static void M_DrawEpisode(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  M_DrawTitle(54, EpiDef.y - 25, "M_EPISOD", "WHICH EPISODE?");
}

static void M_Episode(int choice)
{
  if (!EpiCustom)
  {
  if ( (gamemode == shareware) && choice)
    {
      M_StartMessage(s_SWSTRING,NULL,false); // Ty 03/27/98 - externalized
      M_SetupNextMenu(&ReadDef1);
      return;
    }

  // Yet another hack...
  if (gamemode == registered && choice > 2)
    choice = 0;         // killough 8/8/98
   
  }
  epiChoice = choice;
  M_SetupNextMenu(&NewDef);
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
  newg_end
} newgame_e;

// The definitions of the New Game menu

static menuitem_t NewGameMenu[]=
{
  {1,"M_JKILL", M_ChooseSkill, 'i', "I'm too young to die."},
  {1,"M_ROUGH", M_ChooseSkill, 'h', "Hey, not too rough."},
  {1,"M_HURT",  M_ChooseSkill, 'h', "Hurt me plenty."},
  {1,"M_ULTRA", M_ChooseSkill, 'u', "Ultra-Violence."},
  {1,"M_NMARE", M_ChooseSkill, 'n', "Nightmare!"}
};

static menu_t NewDef =
{
  newg_end,       // # of menu items
  &EpiDef,        // previous menu
  NewGameMenu,    // menuitem_t ->
  M_DrawNewGame,  // drawing routine ->
  48,63,          // x,y
  hurtme          // lastOn
};

//
// M_NewGame
//

static void M_DrawNewGame(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  V_DrawPatchDirect (96,14,W_CacheLumpName("M_NEWG",PU_CACHE));
  V_DrawPatchDirect (54,38,W_CacheLumpName("M_SKILL",PU_CACHE));
}

static void M_NewGame(int choice)
{
  if (netgame && !demoplayback)
    {
      M_StartMessage(s_NEWGAME,NULL,false); // Ty 03/27/98 - externalized
      return;
    }
  
  if (demorecording)   // killough 5/26/98: exclude during demo recordings
    {
      M_StartMessage("you can't start a new game\n"
		     "while recording a demo!\n\n"PRESSKEY,
		     NULL, false); // killough 5/26/98: not externalized
      return;
    }
  
  if ( ((gamemode == commercial) && !EpiCustom) || EpiDef.numitems <= 1)
    M_SetupNextMenu(&NewDef);
  else
    {
      epiChoice = 0;
      M_SetupNextMenu(&EpiDef);
    }
}

static void M_VerifyNightmare(int ch)
{
  if (ch != 'y')
    return;

  if (!EpiCustom)
    G_DeferedInitNew(nightmare, epiChoice + 1, 1);
  else
    G_DeferedInitNew(nightmare, EpiMenuEpi[epiChoice], EpiMenuMap[epiChoice]);

  M_ClearMenus ();
}

static void M_ChooseSkill(int choice)
{
  if (choice == nightmare)
    {   // Ty 03/27/98 - externalized
      M_StartMessage(s_NIGHTMARE,M_VerifyNightmare,true);
      return;
    }

  if (!EpiCustom)
    G_DeferedInitNew(choice, epiChoice + 1, 1);
  else
    G_DeferedInitNew(choice, EpiMenuEpi[epiChoice], EpiMenuMap[epiChoice]);

  M_ClearMenus ();
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
  load7, //jff 3/15/98 extend number of slots
  load8,
  load_page,
  load_end
} load_e;

#define SAVE_LOAD_RECT(n) \
    {M_X_LOADSAVE, LINEHEIGHT * (n + 1), M_LOADSAVE_WIDTH, LINEHEIGHT}

// The definitions of the Load Game screen

static menuitem_t LoadMenu[]=
{
  {1, "", M_LoadSelect,'1', NULL, 0, SAVE_LOAD_RECT(1)},
  {1, "", M_LoadSelect,'2', NULL, 0, SAVE_LOAD_RECT(2)},
  {1, "", M_LoadSelect,'3', NULL, 0, SAVE_LOAD_RECT(3)},
  {1, "", M_LoadSelect,'4', NULL, 0, SAVE_LOAD_RECT(4)},
  {1, "", M_LoadSelect,'5', NULL, 0, SAVE_LOAD_RECT(5)},
  {1, "", M_LoadSelect,'6', NULL, 0, SAVE_LOAD_RECT(6)}, //jff 3/15/98 extend number of slots
  {1, "", M_LoadSelect,'7', NULL, 0, SAVE_LOAD_RECT(7)},
  {1, "", M_LoadSelect,'8', NULL, 0, SAVE_LOAD_RECT(8)},
  {-1, "", NULL, 0, NULL, MF_PAGE, SAVE_LOAD_RECT(9)}
};

static menu_t LoadDef =
{
  load_end,
  &MainDef,
  LoadMenu,
  M_DrawLoad,
  M_X_LOADSAVE,34, //jff 3/15/98 move menu up
  0
};

#define LOADGRAPHIC_Y 8

// [FG] draw a snapshot of the n'th savegame into a separate window,
//      or fill window with solid color and "n/a" if snapshot not available

static int snapshot_width, snapshot_height;

static void M_DrawBorderedSnapshot (int n)
{
  const char *txt = "n/a";

  const int snapshot_x = MAX((video.deltaw + SaveDef.x + SKULLXOFF - snapshot_width) / 2, 8);
  const int snapshot_y = LoadDef.y + MAX((load_end * LINEHEIGHT - snapshot_height) * n / load_end, 0);

  // [FG] a snapshot window smaller than 80*48 px is considered too small
  if (snapshot_width < SCREENWIDTH/4)
    return;

  if (!M_DrawSnapshot(n, snapshot_x, snapshot_y, snapshot_width, snapshot_height))
  {
    M_WriteText(snapshot_x + snapshot_width/2 - M_StringWidth(txt)/2 - video.deltaw,
                snapshot_y + snapshot_height/2 - M_StringHeight(txt)/2,
                txt);
  }

  txt = M_GetSavegameTime(n);
  M_DrawString(snapshot_x + snapshot_width/2 - M_GetPixelWidth(txt)/2 - video.deltaw,
               snapshot_y + snapshot_height + M_StringHeight(txt),
               CR_GOLD, txt);

  // [FG] draw the view border around the snapshot
  R_DrawBorder(snapshot_x, snapshot_y, snapshot_width, snapshot_height);
}

// [FG] delete a savegame

static boolean delete_verify = false;

static void M_DrawDelVerify(void);

static void M_DeleteGame(int i)
{
  char *name = G_SaveGameName(i);
  M_remove(name);

  if (i == quickSaveSlot)
    quickSaveSlot = -1;

  M_ReadSaveStrings();

  if (name) free(name);
}

// [FG] support up to 8 pages of savegames
static void M_DrawSaveLoadBottomLine(void)
{
  char pagestr[16];
  const int y = LoadDef.y + LINEHEIGHT * load_page;

  // [crispy] force status bar refresh
  inhelpscreens = true;

  int flags = currentMenu->menuitems[itemOn].flags;
  byte *cr = (flags & MF_PAGE) ? cr_bright : NULL;

  M_DrawSaveLoadBorder(LoadDef.x, y, cr);

  if (savepage > 0)
    M_DrawString(LoadDef.x, y, CR_GOLD, "<-");
  if (savepage < savepage_max)
    M_DrawString(LoadDef.x+(SAVESTRINGSIZE-2)*8, y, CR_GOLD, "->");

  M_snprintf(pagestr, sizeof(pagestr), "page %d/%d", savepage + 1, savepage_max + 1);
  M_DrawString(LoadDef.x+M_LOADSAVE_WIDTH/2-M_StringWidth(pagestr)/2, y, CR_GOLD, pagestr);
}

//
// M_LoadGame & Cie.
//

static void M_DrawLoad(void)
{
    int i;

    //jff 3/15/98 use symbolic load position
    V_DrawPatch(72, LOADGRAPHIC_Y, W_CacheLumpName("M_LOADG",PU_CACHE));
    for (i = 0 ; i < load_page ; i++)
    {
        menuitem_t *item = &currentMenu->menuitems[i];
        byte *cr = (item->flags & MF_HILITE) ? cr_bright : NULL;

        M_DrawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i, cr);
        M_WriteText(LoadDef.x, LoadDef.y + LINEHEIGHT * i, savegamestrings[i]);
    }

    M_DrawBorderedSnapshot(itemOn);

    M_DrawSaveLoadBottomLine();
}

//
// Draw border for the savegame description
//

static void M_DrawSaveLoadBorder(int x, int y, byte *cr)
{
    int i;

    V_DrawPatchTranslated(x - 8, y + 7, W_CacheLumpName("M_LSLEFT",PU_CACHE), cr);

    for (i = 0 ; i < 24 ; i++)
    {
        V_DrawPatchTranslated(x, y + 7, W_CacheLumpName("M_LSCNTR",PU_CACHE), cr);
        x += 8;
    }

    V_DrawPatchTranslated(x, y + 7, W_CacheLumpName("M_LSRGHT",PU_CACHE), cr);
}

//
// User wants to load this game
//

static void M_LoadSelect(int choice)
{
  char *name = NULL;     // killough 3/22/98

  name = G_SaveGameName(choice);

  saveg_compat = saveg_woof510;

  if (M_access(name, F_OK) != 0)
  {
    if (name) free(name);
    name = G_MBFSaveGameName(choice);
    saveg_compat = saveg_mbf;
  }

  G_LoadGame(name, choice, false); // killough 3/16/98, 5/15/98: add slot, cmd

  M_ClearMenus ();
  if (name) free(name);

  // [crispy] save the last game you loaded
  SaveDef.lastOn = choice;
}

//
// killough 5/15/98: add forced loadgames
//

static void M_VerifyForcedLoadGame(int ch)
{
  if (ch=='y')
    G_ForcedLoadGame();
  free(messageString);       // free the message strdup()'ed below
  M_ClearMenus();
}

void M_ForcedLoadGame(const char *msg)
{
  M_StartMessage(strdup(msg), M_VerifyForcedLoadGame, true); // free()'d above
}

//
// Selected from DOOM menu
//

static void M_LoadGame (int choice)
{
  delete_verify = false;

  if (netgame && !demoplayback)     // killough 5/26/98: add !demoplayback
    {
      M_StartMessage(s_LOADNET,NULL,false); // Ty 03/27/98 - externalized
      return;
    }

  if (demorecording)   // killough 5/26/98: exclude during demo recordings
    {
      M_StartMessage("you can't load a game\n"
		     "while recording a demo!\n\n"PRESSKEY,
		     NULL, false); // killough 5/26/98: not externalized
      return;
    }
  
  M_SetupNextMenu(&LoadDef);
  M_ReadSaveStrings();
}

/////////////////////////////
//
// SAVE GAME MENU
//

// The definitions of the Save Game screen

static menuitem_t SaveMenu[]=
{
  {1,"", M_SaveSelect,'1', NULL, 0, SAVE_LOAD_RECT(1)},
  {1,"", M_SaveSelect,'2', NULL, 0, SAVE_LOAD_RECT(2)},
  {1,"", M_SaveSelect,'3', NULL, 0, SAVE_LOAD_RECT(3)},
  {1,"", M_SaveSelect,'4', NULL, 0, SAVE_LOAD_RECT(4)},
  {1,"", M_SaveSelect,'5', NULL, 0, SAVE_LOAD_RECT(5)},
  {1,"", M_SaveSelect,'6', NULL, 0, SAVE_LOAD_RECT(6)},
  {1,"", M_SaveSelect,'7', NULL, 0, SAVE_LOAD_RECT(7)}, //jff 3/15/98 extend number of slots
  {1,"", M_SaveSelect,'8', NULL, 0, SAVE_LOAD_RECT(8)},
  {-1, "", NULL, 0, NULL, MF_PAGE, SAVE_LOAD_RECT(9)}
};

static menu_t SaveDef =
{
  load_end, // same number of slots as the Load Game screen
  &MainDef,
  SaveMenu,
  M_DrawSave,
  M_X_LOADSAVE,34, //jff 3/15/98 move menu up
  0
};

//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
  int i;

  // [FG] shift savegame descriptions a bit to the right
  //      to make room for the snapshots on the left
  SaveDef.x = LoadDef.x = M_X_LOADSAVE + MIN(M_LOADSAVE_WIDTH/2, video.deltaw);

  // [FG] fit the snapshots into the resulting space
  snapshot_width = MIN((video.deltaw + SaveDef.x + 2 * SKULLXOFF) & ~7, SCREENWIDTH/2); // [FG] multiple of 8
  snapshot_height = MIN((snapshot_width * SCREENHEIGHT / SCREENWIDTH) & ~7, SCREENHEIGHT/2);

  for (i = 0 ; i < load_page ; i++)
    {
      FILE *fp;  // killough 11/98: change to use stdio

      char *name = G_SaveGameName(i);    // killough 3/22/98
      fp = M_fopen(name,"rb");
      M_ReadSavegameTime(i, name);
      if (name) free(name);

      M_ResetSnapshot(i);

      if (!fp)
	{   // Ty 03/27/98 - externalized:
	  name = G_MBFSaveGameName(i);
	  fp = M_fopen(name,"rb");
	  if (name) free(name);
	  if (!fp)
	  {
	  strcpy(&savegamestrings[i][0],s_EMPTYSTRING);
	  LoadMenu[i].status = 0;
	  continue;
	  }
	}
      // [FG] check return value
      if (!fread(&savegamestrings[i], SAVESTRINGSIZE, 1, fp))
	{
	  strcpy(&savegamestrings[i][0],s_EMPTYSTRING);
	  LoadMenu[i].status = 0;
	  fclose(fp);
	  continue;
	}

      if (!M_ReadSnapshot(i, fp))
        M_ResetSnapshot(i);

      fclose(fp);
      LoadMenu[i].status = 1;
    }
}

//
//  M_SaveGame & Cie.
//
static void M_DrawSave(void)
{
    int i;

    //jff 3/15/98 use symbolic load position
    V_DrawPatchDirect (72,LOADGRAPHIC_Y,W_CacheLumpName("M_SAVEG",PU_CACHE));
    for (i = 0 ; i < load_page ; i++)
    {
      menuitem_t *item = &currentMenu->menuitems[i];
      byte *cr = (item->flags & MF_HILITE) ? cr_bright : NULL;

      M_DrawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i, cr);
      M_WriteText(LoadDef.x, LoadDef.y + LINEHEIGHT * i, savegamestrings[i]);
    }

    if (saveStringEnter)
    {
        i = M_StringWidth(savegamestrings[saveSlot]);
        M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }

    M_DrawBorderedSnapshot(itemOn);

    M_DrawSaveLoadBottomLine();
}

//
// M_Responder calls this when user is finished
//
static void M_DoSave(int slot)
{
  G_SaveGame (slot,savegamestrings[slot]);
  M_ClearMenus ();
}

void M_SetQuickSaveSlot(int slot)
{
  if (quickSaveSlot == -2)
    quickSaveSlot = slot;
}

// [FG] generate a default save slot name when the user saves to an empty slot
static void SetDefaultSaveName (int slot)
{
    char *maplump = MAPNAME(gameepisode, gamemap);
    int maplumpnum = W_CheckNumForName(maplump);

    if (!organize_savefiles)
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpnum));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        M_snprintf(savegamestrings[slot], SAVESTRINGSIZE,
                  "%s (%s)", maplump, wadname);
        free(wadname);
    }
    else
    {
        M_snprintf(savegamestrings[slot], SAVESTRINGSIZE, "%s", maplump);
    }

    M_ForceUppercase(savegamestrings[slot]);
}

// [FG] override savegame name if it already starts with a map identifier
boolean StartsWithMapIdentifier (char *str)
{
    if (strnlen(str, 8) >= 4 &&
        toupper(str[0]) == 'E' &&
        isdigit(str[1]) &&
        toupper(str[2]) == 'M' &&
        isdigit(str[3]))
    {
        return true;
    }

    if (strnlen(str, 8) >= 5 &&
        toupper(str[0]) == 'M' &&
        toupper(str[1]) == 'A' &&
        toupper(str[2]) == 'P' &&
        isdigit(str[3]) &&
        isdigit(str[4]))
    {
        return true;
    }

    return false;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int choice)
{
  // we are going to be intercepting all chars
  saveStringEnter = 1;
  
  saveSlot = choice;
  strcpy(saveOldString,savegamestrings[choice]);
  // [FG] override savegame name if it already starts with a map identifier
  if (!strcmp(savegamestrings[choice],s_EMPTYSTRING) || // Ty 03/27/98 - externalized
      StartsWithMapIdentifier(savegamestrings[choice]))
  {
    savegamestrings[choice][0] = 0;
    SetDefaultSaveName(choice);
  }
  saveCharIndex = strlen(savegamestrings[choice]);

  // [crispy] load the last game you saved
  LoadDef.lastOn = choice;
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
      M_StartMessage(s_SAVEDEAD,NULL,false); // Ty 03/27/98 - externalized
      return;
    }
  
  if (gamestate != GS_LEVEL)
    return;
  
  M_SetupNextMenu(&SaveDef);
  M_ReadSaveStrings();
}

/////////////////////////////
//
// M_QuitDOOM
//
static int quitsounds[8] =
{
  sfx_pldeth,
  sfx_dmpain,
  sfx_popain,
  sfx_slop,
  sfx_telept,
  sfx_posit1,
  sfx_posit3,
  sfx_sgtatk
};

static int quitsounds2[8] =
{
  sfx_vilact,
  sfx_getpow,
  sfx_boscub,
  sfx_slop,
  sfx_skeswg,
  sfx_kntdth,
  sfx_bspact,
  sfx_sgtatk
};

static void M_QuitResponse(int ch)
{
  if (ch != 'y')
    return;
  if (D_CheckEndDoom() && // play quit sound only if showing ENDOOM
      (!netgame || demoplayback) && // killough 12/98
      !nosfxparm) // avoid delay if no sound card
    {
      if (gamemode == commercial)
	S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
      else
	S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
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
    sprintf(endstring,"%s\n\n%s",s_DOSY, *endmsg[0] );
  else         // killough 1/18/98: fix endgame message calculation:
    sprintf(endstring,"%s\n\n%s", *endmsg[gametic%(NUM_QUITMESSAGES-1)+1], s_DOSY);
  
  M_StartMessage(endstring,M_QuitResponse,true);
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
    {80/*SoundDef.x*/, 64/*SoundDef.y*/ + LINEHEIGHT * (n), (16 + 2) * 8, 13}

static menuitem_t SoundMenu[]=
{
  { 2, "M_SFXVOL", M_SfxVol,'s', NULL, MF_THRM_STR},
  {-1, "", NULL, 0, NULL, MF_THRM, THERMO_VOLUME_RECT(sfx_vol_thermo)},
  { 2, "M_MUSVOL", M_MusicVol, 'm', NULL, MF_THRM_STR},
  {-1, "", NULL, 0, NULL, MF_THRM, THERMO_VOLUME_RECT(music_vol_thermo)},
};

static menu_t SoundDef =
{
  sound_end,
  &MainDef,
  SoundMenu,
  M_DrawSound,
  80,64,
  0
};

//
// Change Sfx & Music volumes
//

static void M_DrawSound(void)
{
  V_DrawPatchDirect (60, 38, W_CacheLumpName("M_SVOL", PU_CACHE));

  int index = itemOn + 1;
  menuitem_t *item = &currentMenu->menuitems[index];
  byte *cr;

  if (index == sfx_vol_thermo && (item->flags & MF_HILITE))
    cr = cr_bright;
  else
    cr = NULL;

  M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * sfx_vol_thermo, 16,
               snd_SfxVolume, cr);

  if (index == music_vol_thermo && (item->flags & MF_HILITE))
    cr = cr_bright;
  else
    cr = NULL;

  M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * music_vol_thermo, 16,
               snd_MusicVolume, cr);
}

void M_Sound(int choice)
{
  M_SetupNextMenu(&SoundDef);
}

#define M_MAX_VOL 15

static void M_SfxVol(int choice)
{
  switch(choice)
    {
    case CHOICE_LEFT:
      if (snd_SfxVolume)
        snd_SfxVolume--;
      break;
    case CHOICE_RIGHT:
      if (snd_SfxVolume < M_MAX_VOL)
        snd_SfxVolume++;
      break;
    default:
      if (choice >= CHOICE_VALUE)
        snd_SfxVolume = choice;
      break;
    }
  
  S_SetSfxVolume(snd_SfxVolume /* *8 */);
}

static void M_MusicVol(int choice)
{
  switch(choice)
    {
    case CHOICE_LEFT:
      if (snd_MusicVolume)
        snd_MusicVolume--;
      break;
    case CHOICE_RIGHT:
      if (snd_MusicVolume < M_MAX_VOL)
        snd_MusicVolume++;
      break;
    default:
      if (choice >= CHOICE_VALUE)
        snd_MusicVolume = choice;
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
      if (StartsWithMapIdentifier(savegamestrings[quickSaveSlot]))
          SetDefaultSaveName(quickSaveSlot);
      M_DoSave(quickSaveSlot);
      S_StartSound(NULL,sfx_swtchx);
    }
}

static void M_QuickSave(void)
{
  if (!usergame && (!demoplayback || netgame))  // killough 10/98
    {
      S_StartSound(NULL,sfx_oof);
      return;
    }

  if (gamestate != GS_LEVEL)
    return;
  
  if (quickSaveSlot < 0)
    {
      M_StartControlPanel();
      M_ReadSaveStrings();
      M_SetupNextMenu(&SaveDef);
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
      S_StartSound(NULL,sfx_swtchx);
    }
}

static void M_QuickLoad(void)
{
  if (netgame && !demoplayback)    // killough 5/26/98: add !demoplayback
    {
      M_StartMessage(s_QLOADNET,NULL,false); // Ty 03/27/98 - externalized
      return;
    }
  
  if (demorecording)   // killough 5/26/98: exclude during demo recordings
    {
      M_StartMessage("you can't quickload\n"
		     "while recording a demo!\n\n"PRESSKEY,
		     NULL, false); // killough 5/26/98: not externalized
      return;
    }
  
  if (quickSaveSlot < 0)
    {
      // [crispy] allow quickload before quicksave
      M_StartControlPanel();
      M_ReadSaveStrings();
      M_SetupNextMenu(&LoadDef);
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
    return;

  // killough 5/26/98: make endgame quit if recording or playing back demo
  if (demorecording || singledemo)
    G_CheckDemoStatus();

  // [crispy] clear quicksave slot
  quickSaveSlot = -1;

  currentMenu->lastOn = itemOn;
  M_ClearMenus ();
  D_StartTitle ();
}

static void M_EndGame(int choice)
{
  if (netgame)
    {
      M_StartMessage(s_NETEND,NULL,false); // Ty 03/27/98 - externalized
      return;
    }
  M_StartMessage(s_ENDGAME,M_EndGameResponse,true); // Ty 03/27/98 - externalized
}

/////////////////////////////
//
//    Toggle messages on/off
//

static void M_ChangeMessages(int choice)
{
  // warning: unused parameter `int choice'
  choice = 0;
  showMessages = 1 - showMessages;
  
  if (!showMessages)
    displaymsg("%s", s_MSGOFF); // Ty 03/27/98 - externalized
  else
    displaymsg("%s", s_MSGON); // Ty 03/27/98 - externalized

  message_dontfuckwithme = true;
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
  switch(choice)
    {
    case 0:
      if (screenSize > 0)
	{
	  screenblocks--;
	  screenSize--;
	  hud_displayed = 0;
	}
      break;
    case 1:
      if (screenSize < 8)
	{
	  screenblocks++;
	  screenSize++;
	}
      else
	{
	  hud_displayed = !hud_displayed;
	  HU_disable_all_widgets();
	}
      break;
    }
  R_SetViewSize (screenblocks /*, detailLevel obsolete -- killough */);
  saved_screenblocks = screenblocks;
}

//
// End of Original Menus
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
// SETUP MENU (phares)
//
// We've added a set of Setup Screens from which you can configure a number
// of variables w/o having to restart the game. There are 7 screens:
//
//    Key Bindings
//    Weapons
//    Status Bar / HUD
//    Automap
//    Enemies
//    Messages
//    Chat Strings
//
// killough 10/98: added Compatibility and General menus
//

/////////////////////////////
//
// booleans for setup screens
// these tell you what state the setup screens are in, and whether any of
// the overlay screens (automap colors, reset button message) should be
// displayed

static boolean setup_active      = false; // in one of the setup screens
static boolean set_keybnd_active = false; // in key binding setup screens
static boolean set_weapon_active = false; // in weapons setup screen
static boolean setup_select      = false; // changing an item
static boolean setup_gather      = false; // gathering keys for value
static boolean default_verify    = false; // verify reset defaults decision

/////////////////////////////
//
// set_menu_itemon is an index that starts at zero, and tells you which
// item on the current screen the cursor is sitting on.
//
// current_setup_menu is a pointer to the current setup menu table.

static int set_menu_itemon; // which setup item is selected?   // phares 3/98
static setup_menu_t *current_setup_menu; // points to current setup menu table
static int mult_screens_index; // the index of the current screen in a set

typedef struct
{
  const char *text;
  setup_menu_t *page;
  mrect_t rect;
  int flags;
} setup_tab_t;

static setup_tab_t *current_setup_tabs;
static int set_tab_on;

// [FG] save the setup menu's itemon value in the S_END element's x coordinate

static int M_GetSetupMenuItemOn (void)
{
  const setup_menu_t* menu = current_setup_menu;

  if (menu)
  {
    while (!(menu->m_flags & S_END))
      menu++;

    return menu->m_x;
  }

  return 0;
}

static void M_SetSetupMenuItemOn (const int x)
{
  setup_menu_t* menu = current_setup_menu;

  if (menu)
  {
    while (!(menu->m_flags & S_END))
      menu++;

    menu->m_x = x;
  }
}

// [FG] save the index of the current screen in the first page's S_END element's y coordinate

static int M_GetMultScreenIndex (setup_menu_t *const *const pages)
{
  if (pages)
  {
    const setup_menu_t* menu = pages[0];

    if (menu)
    {
      while (!(menu->m_flags & S_END))
        menu++;

      return menu->m_y;
    }
  }

  return 0;
}

static void M_SetMultScreenIndex (const int y)
{
    if (!current_setup_tabs)
    {
        return;
    }

    setup_menu_t * menu = current_setup_tabs[0].page;

    while (!(menu->m_flags & S_END))
        menu++;

    menu->m_y = y;
}

/////////////////////////////
//
// The menu_buffer is used to construct strings for display on the screen.

static char menu_buffer[66];

/////////////////////////////
//
// The setup_e enum is used to provide a unique number for each group of Setup
// Screens.

enum
{
  set_general, // killough 10/98
  set_compat,
  set_key_bindings,
  set_weapons,
  set_statbar,
  set_automap,
  set_enemy,
  set_setup_end
} setup_e;

static int setup_screen; // the current setup screen. takes values from setup_e 

/////////////////////////////
//
// SetupMenu is the definition of what the main Setup Screen should look
// like. Each entry shows that the cursor can land on each item (1), the
// built-in graphic lump (i.e. "M_KEYBND") that should be displayed,
// the program which takes over when an item is selected, and the hotkey
// associated with the item.

static menuitem_t SetupMenu[]=
{
  // [FG] alternative text for missing menu graphics lumps
  {1,"M_GENERL",M_General,    'g', "GENERAL"},      // killough 10/98
  {1,"M_KEYBND",M_KeyBindings,'k', "KEY BINDINGS"},
  {1,"M_COMPAT",M_Compat,     'p', "DOOM COMPATIBILITY"},
  {1,"M_STAT"  ,M_StatusBar,  's', "STATUS BAR / HUD"},
  {1,"M_AUTO"  ,M_Automap,    'a', "AUTOMAP"},
  {1,"M_WEAP"  ,M_Weapons,    'w', "WEAPONS"},
  {1,"M_ENEM"  ,M_Enemy,      'e', "ENEMIES"},
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

static menuitem_t Generic_Setup[] =
{
  {1,"",M_DoNothing,0}
};

/////////////////////////////
//
// SetupDef is the menu definition that the mainstream Menu code understands.
// This is used by M_Setup (below) to define what is drawn and what is done
// with the main Setup screen.

static menu_t SetupDef =
{
  set_setup_end, // number of Setup Menu items (Key Bindings, etc.)
  &MainDef,      // menu to return to when BACKSPACE is hit on this menu
  SetupMenu,     // definition of items to show on the Setup Screen
  M_DrawSetup,   // program that draws the Setup Screen
  60,37,         // x,y position of the skull (modified when the skull is
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
  M_DrawKeybnd,
  34,5,      // skull drawn here
  0
};

static menu_t WeaponDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawWeapons,
  34,5,      // skull drawn here
  0
};

static menu_t StatusHUDDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawStatusHUD,
  34,5,      // skull drawn here
  0
};

static menu_t AutoMapDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawAutoMap,
  34,5,      // skull drawn here
  0
};

static menu_t EnemyDef =                                           // phares 4/08/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawEnemy,
  34,5,      // skull drawn here
  0
};

static menu_t GeneralDef =                                           // killough 10/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawGeneral,
  34,5,      // skull drawn here
  0
};

static menu_t CompatDef =                                           // killough 10/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawCompat,
  34,5,      // skull drawn here
  0
};

/////////////////////////////
//
// M_DrawBackground tiles a 64x64 patch over the entire screen, providing the
// background for the Help and Setup screens.
//
// killough 11/98: rewritten to support hires

static void M_DrawBackground(char *patchname)
{
  if (setup_active && menu_backdrop != MENU_BG_TEXTURE)
    return;

  V_DrawBackground(patchname);
}

/////////////////////////////
//
// Draws the Title for the main Setup screen

static void M_DrawSetup(void)
{
  M_DrawTitle(124, 15, "M_OPTTTL", "OPTIONS");
}

/////////////////////////////
//
// Uses the SetupDef structure to draw the menu items for the main
// Setup screen

static void M_Setup(int choice)
{
  options_active = true;

  M_SetupNextMenu(&SetupDef);
}

/////////////////////////////
//
// Data that's used by the Setup screen code
//
// Establish the message colors to be used

#define CR_TITLE  CR_GOLD
#define CR_SET    CR_GREEN
#define CR_ITEM   CR_NONE
#define CR_HILITE CR_NONE //CR_ORANGE
#define CR_SELECT CR_GRAY

// chat strings must fit in this screen space
// killough 10/98: reduced, for more general uses
#define MAXCHATWIDTH         272

/////////////////////////////
//
// phares 4/17/98:
// Added 'Reset to Defaults' Button to Setup Menu screens
// This is a small button that sits in the upper-right-hand corner of
// the first screen for each group. It blinks when selected, thus the
// two patches, which it toggles back and forth.

static char ResetButtonName[2][8] = {"M_BUTT1","M_BUTT2"};

/////////////////////////////
//
// phares 4/18/98:
// Consolidate Item drawing code
//
// M_DrawItem draws the description of the provided item (the left-hand
// part). A different color is used for the text depending on whether the
// item is selected or not, or whether it's about to change.

enum
{
    str_empty,
    str_layout,
    str_curve,
    str_center_weapon,
    str_bobfactor,
    str_screensize,
    str_hudtype,
    str_hudmode,
    str_show_widgets,
    str_crosshair,
    str_crosshair_target,
    str_hudcolor,
    str_overlay,
    str_automap_preset,

    str_resolution_scale,
    str_midi_player,

    str_gamma,
    str_sound_module,

    str_mouse_accel,

    str_default_skill,
    str_default_complevel,
    str_endoom,
    str_death_use_action,
    str_menu_backdrop,
    str_widescreen,
};

static const char **GetStrings(int id);

typedef enum
{
    null_mode,
    mouse_mode,
    pad_mode,
    key_mode
} menu_input_mode_t;

static menu_input_mode_t menu_input;
static int mouse_state_x, mouse_state_y;

static void M_DrawMenuStringEx(int flags, int x, int y, int color);

static boolean ItemDisabled(int flags)
{
    if ((flags & S_DISABLE) ||
        (flags & S_STRICT && default_strictmode) ||
        (flags & S_BOOM && default_complevel < CL_BOOM) ||
        (flags & S_MBF && default_complevel < CL_MBF) ||
        (flags & S_VANILLA && default_complevel != CL_VANILLA))
    {
        return true;
    }

    return false;
}

static boolean ItemSelected(setup_menu_t *s)
{
  if (s == current_setup_menu + set_menu_itemon && whichSkull)
    return true;

  return false;
}

static boolean PrevItemAvailable(setup_menu_t *s)
{
    int value = s->var.def->location->i;
    int min   = s->var.def->limit.min;

    return value > min;
}

static boolean NextItemAvailable(setup_menu_t *s)
{
    int value = s->var.def->location->i;
    int max   = s->var.def->limit.max;

    if (max == UL)
    {
        const char **strings = GetStrings(s->strings_id);
        if (strings)
        {
            max = array_size(strings) - 1;
        }
    }

    return value < max;
}

static void BlinkingArrowLeft(setup_menu_t *s)
{
    if (!ItemSelected(s))
    {
        return;
    }

    int flags = s->m_flags;

    if (menu_input == mouse_mode)
    {
        if (flags & S_HILITE)
            strcpy(menu_buffer, "< ");
    }
    else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
    {
        if (setup_select && PrevItemAvailable(s))
            strcpy(menu_buffer, "< ");
        else if (!setup_select)
            strcpy(menu_buffer, "> ");
    }
    else
    {
        strcpy(menu_buffer, "> ");
    }
}

static void BlinkingArrowRight(setup_menu_t *s)
{
    if (!ItemSelected(s))
    {
        return;
    }

    int flags = s->m_flags;

    if (menu_input == mouse_mode)
    {
        if (flags & S_HILITE)
            strcat(menu_buffer, " >");
    }
    else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
    {
        if (setup_select && NextItemAvailable(s))
            strcat(menu_buffer, " >");
        else if (!setup_select)
            strcat(menu_buffer, " <");
    }
    else if (!setup_select)
    {
        strcat(menu_buffer, " <");
    }
}

#define M_TAB_Y      22
#define M_TAB_OFFSET 8

static void M_DrawTabs(void)
{
    setup_tab_t *tabs = current_setup_tabs;

    int width = 0;

    for (int i = 0; tabs[i].text; ++i)
    {
        mrect_t *rect = &tabs[i].rect;
        if (!rect->w)
        {
            rect->w = M_GetPixelWidth(tabs[i].text);
            rect->y = M_TAB_Y;
            rect->h = M_SPC;
        }
        width += rect->w + M_TAB_OFFSET;
    }

    int x = (SCREENWIDTH - width) / 2;

    for (int i = 0; tabs[i].text; ++i)
    {
        mrect_t *rect = &tabs[i].rect;

        x += M_TAB_OFFSET;
        menu_buffer[0] = '\0';
        strcpy(menu_buffer, tabs[i].text);
        M_DrawMenuStringEx(tabs[i].flags, x, rect->y, CR_GOLD);
        if (i == mult_screens_index)
        {
            V_FillRect(x + video.deltaw, rect->y + M_SPC, rect->w, 1,
                       cr_gold[cr_shaded[v_lightest_color]]);
        }

        rect->x = x;

        x += rect->w;
    }
}

static void M_DrawItem(setup_menu_t* s, int accum_y)
{
    int x = s->m_x;
    int y = s->m_y;
    int flags = s->m_flags;
    mrect_t *rect = &s->rect;

    if (flags & S_RESET)
    {
        // This item is the reset button
        // Draw the 'off' version if this isn't the current menu item
        // Draw the blinking version in tune with the blinking skull otherwise

        const int index = (flags & (S_HILITE|S_SELECT)) ? whichSkull : 0;
        patch_t *patch = W_CacheLumpName(ResetButtonName[index], PU_CACHE);
        rect->x = x;
        rect->y = y;
        rect->w = SHORT(patch->width);
        rect->h = SHORT(patch->height);
        V_DrawPatch(x, y, patch);
        return;
    }

    if (!(flags & S_DIRECT))
    {
        y = accum_y;
    }

    // Draw the item string

    menu_buffer[0] = '\0';

    int w = 0;
    const char *text = s->m_text;
    int color =
          flags & S_TITLE ? CR_TITLE :
          flags & S_SELECT ? CR_SELECT :
          flags & S_HILITE ? CR_HILITE : CR_ITEM; // killough 10/98

    if (!(flags & S_NEXT_LINE))
    {
        BlinkingArrowLeft(s);
    }

    // killough 10/98: support left-justification:
    strcat(menu_buffer, text);
    w = M_GetPixelWidth(menu_buffer);
    if (!(flags & S_LEFTJUST))
    {
        x -= (w + 4);
    }

    rect->x = 0;
    rect->y = y;
    rect->w = SCREENWIDTH;
    rect->h = M_SPC;

    if (flags & S_THERMO)
    {
        y += M_THRM_TXT_OFFSET;
    }

    M_DrawMenuStringEx(flags, x, y, color);
}

// If a number item is being changed, allow up to N keystrokes to 'gather'
// the value. Gather_count tells you how many you have so far. The legality
// of what is gathered is determined by the low/high settings for the item.

#define MAXGATHER 5
static int  gather_count;
static char gather_buffer[MAXGATHER+1];  // killough 10/98: make input character-based

/////////////////////////////
//
// phares 4/18/98:
// Consolidate Item Setting drawing code
//
// M_DrawSetting draws the setting of the provided item (the right-hand
// part. It determines the text color based on whether the item is
// selected or being changed. Then, depending on the type of item, it
// displays the appropriate setting value: yes/no, a key binding, a number,
// a paint chip, etc.

static void M_DrawSetupThermo(int x, int y, int width, int size, int dot, byte *cr)
{
  int xx;
  int  i;

  xx = x;
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERML", PU_CACHE), cr);
  xx += M_THRM_STEP;

  patch_t *patch = W_CacheLumpName("M_THERMM", PU_CACHE);
  for (i = 0; i < width + 1; i++)
  {
    V_DrawPatchTranslated(xx, y, patch, cr);
    xx += M_THRM_STEP;
  }
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERMR", PU_CACHE), cr);

  if (dot > size)
    dot = size;

  int step = width * M_THRM_STEP * FRACUNIT / size;

  V_DrawPatchTranslated(x + M_THRM_STEP + dot * step / FRACUNIT, y,
                        W_CacheLumpName("M_THERMO", PU_CACHE), cr);
}

static void M_DrawSetting(setup_menu_t *s, int accum_y)
{
  int x = s->m_x, y = s->m_y, flags = s->m_flags, color;

  if (!(flags & S_DIRECT))
  {
    y = accum_y;
  }

  // Determine color of the text. This may or may not be used
  // later, depending on whether the item is a text string or not.

  color = flags & S_SELECT ? CR_SELECT : CR_SET;

  // Is the item a YES/NO item?

  if (flags & S_YESNO)
    {
      strcpy(menu_buffer, s->var.def->location->i ? "YES" : "NO");
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  if (flags & S_ONOFF)
    {
      strcpy(menu_buffer, s->var.def->location->i ? "ON" : "OFF");
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // Is the item a simple number?

  if (flags & S_NUM)
    {
      // killough 10/98: We must draw differently for items being gathered.
      if (flags & (S_HILITE|S_SELECT) && setup_gather)
      {
        gather_buffer[gather_count] = 0;
        strcpy(menu_buffer, gather_buffer);
      }
      else
        sprintf(menu_buffer,"%d",s->var.def->location->i);
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // Is the item a key binding?

  if (flags & S_INPUT)
  {
    int i;
    int offset = 0;

    const input_t *inputs = M_Input(s->input_id);

    // Draw the input bound to the action
    menu_buffer[0] = '\0';

    for (i = 0; i < array_size(inputs); ++i)
    {
      if (i > 0)
      {
        menu_buffer[offset++] = ' ';
        menu_buffer[offset++] = '+';
        menu_buffer[offset++] = ' ';
        menu_buffer[offset] = '\0';
      }

      switch (inputs[i].type)
      {
        case INPUT_KEY:
          offset = M_GetKeyString(inputs[i].value, offset);
          break;
        case INPUT_MOUSEB:
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForMouseB(inputs[i].value));
          break;
        case INPUT_JOYB:
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForJoyB(inputs[i].value));
          break;
        default:
          break;
      }
    }

    // "NONE"
    if (i == 0)
      M_GetKeyString(0, 0);

    BlinkingArrowRight(s);
    M_DrawMenuStringEx(flags, x, y, color);
  }

  // Is the item a weapon number?
  // OR, Is the item a colored text string from the Automap?
  //
  // killough 10/98: removed special code, since the rest of the engine
  // already takes care of it, and this code prevented the user from setting
  // their overall weapons preferences while playing Doom 1.
  //
  // killough 11/98: consolidated weapons code with color range code
  
  if (flags & S_WEAP) // weapon number
    {
      sprintf(menu_buffer,"%d", s->var.def->location->i);
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // [FG] selection of choices

  if (flags & (S_CHOICE|S_CRITEM))
    {
      int i = s->var.def->location->i;
      mrect_t *rect = &s->rect;
      int width;
      const char **strings = GetStrings(s->strings_id);

      menu_buffer[0] = '\0';

      if (flags & S_NEXT_LINE)
        BlinkingArrowLeft(s);

      if (i >= 0 && strings)
        strcat(menu_buffer, strings[i]);
      width = M_GetPixelWidth(menu_buffer);
      if (flags & S_NEXT_LINE)
      {
        y += M_SPC;
        x = M_X - width - 4;
        rect->y = y;
      }

      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, flags & S_CRITEM ? i : color);
      return;
    }

  if (flags & S_THERMO)
    {
      int value = s->var.def->location->i;
      int min = s->var.def->limit.min;
      int max = s->var.def->limit.max;
      const char **strings = GetStrings(s->strings_id);

      int width;
      if (flags & S_THRM_SIZE11)
        width = M_THRM_SIZE11;
      else if (flags & S_THRM_SIZE4)
        width = M_THRM_SIZE4;
      else
        width = M_THRM_SIZE8;

      if (max == UL)
      {
        if (strings)
          max = array_size(strings) - 1;
        else
          max = M_THRM_UL_VAL;
      }

      value = BETWEEN(min, max, value);

      byte *cr;
      if (ItemDisabled(flags))
        cr = cr_dark;
      else if (flags & S_HILITE)
        cr = cr_bright;
      else
        cr = NULL;

      mrect_t *rect = &s->rect;
      rect->x = x;
      rect->y = y;
      rect->w = (width + 2) * M_THRM_STEP;
      rect->h = M_THRM_HEIGHT;
      M_DrawSetupThermo(x, y, width, max - min, value - min, cr);

      if (strings)
        strcpy(menu_buffer, strings[value]);
      else
        M_snprintf(menu_buffer, 4, "%d", value);

      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x + M_THRM_STEP + rect->w, y + M_THRM_TXT_OFFSET, color);
    }
}

/////////////////////////////
//
// M_DrawScreenItems takes the data for each menu item and gives it to
// the drawing routines above.

static void M_DrawScreenItems(setup_menu_t* src)
{
  if (print_warning_about_changes > 0)   // killough 8/15/98: print warning
  {
    int x_warn;

    if (warning_about_changes & S_BADVAL)
    {
      strcpy(menu_buffer, "Value out of Range");
    }
    else if (warning_about_changes & S_PRGWARN)
    {
      strcpy(menu_buffer, "Warning: Restart to see changes");
    }
    else
    {
      strcpy(menu_buffer, "Warning: Changes apply to next game");
    }

    x_warn = SCREENWIDTH/2 - M_GetPixelWidth(menu_buffer)/2;
    M_DrawMenuString(x_warn, M_Y_WARN, CR_RED);
  }

  int accum_y = 0;

  while (!(src->m_flags & S_END))
  {
      if (!(src->m_flags & S_DIRECT))
      {
          if (!accum_y)
              accum_y = src->m_y;
          else
              accum_y += src->m_y;
      }

      // See if we're to draw the item description (left-hand part)

      if (src->m_flags & S_SHOWDESC)
      {
          M_DrawItem(src, accum_y);
      }

      // See if we're to draw the setting (right-hand part)

      if (src->m_flags & S_SHOWSET)
      {
          M_DrawSetting(src, accum_y);
      }

      src++;
  }
}

/////////////////////////////
//
// Data used to draw the "are you sure?" dialogue box when resetting
// to defaults.

#define VERIFYBOXXORG 66
#define VERIFYBOXYORG 88

static void M_DrawDefVerify()
{
  V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG, W_CacheLumpName("M_VBOX", PU_CACHE));

  // The blinking messages is keyed off of the blinking of the
  // cursor skull.

  if (whichSkull) // blink the text
    {
      strcpy(menu_buffer, "Restore defaults? (Y or N)");
      M_DrawMenuString(VERIFYBOXXORG+8,VERIFYBOXYORG+8,CR_RED);
    }
}

// [FG] delete a savegame

static void M_DrawDelVerify(void)
{
  V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG, W_CacheLumpName("M_VBOX", PU_CACHE));

  if (whichSkull) {
    strcpy(menu_buffer,"Delete savegame? (Y or N)");
    M_DrawMenuString(VERIFYBOXXORG+8,VERIFYBOXYORG+8,CR_RED);
  }
}

/////////////////////////////
//
// phares 4/18/98:
// M_DrawInstructions writes the instruction text just below the screen title
//
// killough 8/15/98: rewritten

static void M_DrawInstructions()
{
    int flags = current_setup_menu[set_menu_itemon].m_flags;

    if (ItemDisabled(flags) || print_warning_about_changes > 0)
    {
        return;
    }

    if (menu_input == mouse_mode && !(flags & S_HILITE))
    {
        return;
    }

    // There are different instruction messages depending on whether you
    // are changing an item or just sitting on it.

    const char *s = "";

    if (setup_select)
    {
        if (flags & S_INPUT)
        {
            s = "Press key or button to bind/unbind";
        }
        else if (flags & S_YESNO)
        {
            if (menu_input == pad_mode)
                s = "[ PadA ] to toggle";
            else
                s = "[ Enter ] to toggle, [ Esc ] to cancel";
        }
        else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
        {
            if (menu_input == pad_mode)
                s = "[ Left/Right ] to choose, [ PadB ] to cancel";
            else
                s = "[ Left/Right ] to choose, [ Esc ] to cancel";
        }
        else if (flags & S_NUM)
        {
            s = "Enter value";
        }
        else if (flags & S_WEAP)
        {
            s = "Enter weapon number";
        }
        else if (flags & S_RESET)
        {
            s = "Restore defaults";
        }
    }
    else
    {
        if (flags & S_INPUT)
        {
            switch (menu_input)
            {
                case mouse_mode:
                    s = "[ Del ] to clear";
                    break;
                case pad_mode:
                    s = "[ PadA ] to change, [ PadY ] to clear";
                    break;
                default:
                case key_mode:
                    s = "[ Enter ] to change, [ Del ] to clear";
                    break;
            }
        }
        else if (flags & S_RESET)
        {
            s = "Restore defaults";
        }
        else
        {
            switch (menu_input)
            {
                case pad_mode:
                    s = "[ PadA ] to change, [ PadB ] to return";
                    break;
                case key_mode:
                    s = "[ Enter ] to change";
                    break;
                default:
                    break;
            }
        }
    }

    M_DrawStringCR((SCREENWIDTH - M_GetPixelWidth(s)) / 2, M_Y_WARN, cr_shaded, NULL, s);
}

// [FG] reload current level / go to next level
// adapted from prboom-plus/src/e6y.c:369-449
int G_GotoNextLevel(int *pEpi, int *pMap)
{
  byte doom_next[4][9] = {
    {12, 13, 19, 15, 16, 17, 18, 21, 14},
    {22, 23, 24, 25, 29, 27, 28, 31, 26},
    {32, 33, 34, 35, 36, 39, 38, 41, 37},
    {42, 49, 44, 45, 46, 47, 48, 11, 43}
  };
  byte doom2_next[32] = {
     2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 31, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30,  1,
    32, 16
  };

  int epsd;
  int map = -1;

  if (gamemapinfo)
  {
    const char *next = NULL;

    if (gamemapinfo->nextsecret[0])
      next = gamemapinfo->nextsecret;
    else if (gamemapinfo->nextmap[0])
      next = gamemapinfo->nextmap;
    else if (U_CheckField(gamemapinfo->endpic))
    {
      epsd = 1;
      map = 1;
    }

    if (next)
      G_ValidateMapName(next, &epsd, &map);
  }

  if (map == -1)
  {
    // secret level
    doom2_next[14] = (haswolflevels ? 31 : 16);

    // shareware doom has only episode 1
    doom_next[0][7] = (gamemode == shareware ? 11 : 21);

    doom_next[2][7] = (gamemode == registered ? 11 : 41);

    //doom2_next and doom_next are 0 based, unlike gameepisode and gamemap
    epsd = gameepisode - 1;
    map = gamemap - 1;

    if (gamemode == commercial)
    {
      epsd = 1;
      if (map >= 0 && map <= 31)
        map = doom2_next[map];
      else
        map = gamemap + 1;
    }
    else
    {
      if (epsd >= 0 && epsd <= 3 && map >= 0 && map <= 8)
      {
        int next = doom_next[epsd][map];
        epsd = next / 10;
        map = next % 10;
      }
      else
      {
        epsd = gameepisode;
        map = gamemap + 1;
      }
    }
  }

  // [FG] report next level without changing
  if (pEpi || pMap)
  {
    if (pEpi)
      *pEpi = epsd;
    if (pMap)
      *pMap = map;
  }
  else if ((gamestate == GS_LEVEL) &&
            !deathmatch && !netgame &&
            !demorecording && !demoplayback &&
            !menuactive)
  {
    char *name = MAPNAME(epsd, map);

    if (W_CheckNumForName(name) == -1)
      displaymsg("Next level not found: %s", name);
    else
    {
      G_DeferedInitNew(gameskill, epsd, map);
      return true;
    }
  }

  return false;
}

/////////////////////////////
//
// The Key Binding Screen tables.

#define KB_X  130

// phares 4/16/98:
// X,Y position of reset button. This is the same for every screen, and is
// only defined once here.

#define X_BUTTON 301
#define Y_BUTTON (SCREENHEIGHT - 15 - 3)

// Definitions of the (in this case) five key binding screens.

setup_menu_t keys_settings1[];
setup_menu_t keys_settings2[];
setup_menu_t keys_settings3[];
setup_menu_t keys_settings4[];
setup_menu_t keys_settings5[];
setup_menu_t keys_settings6[];

// The table which gets you from one screen table to the next.

setup_menu_t* keys_settings[] =
{
  keys_settings1,
  keys_settings2,
  keys_settings3,
  keys_settings4,
  keys_settings5,
  keys_settings6,
  NULL
};

static setup_tab_t keys_tabs[] =
{
   { "action", keys_settings1 },
   { "weapon", keys_settings2 },
   { "misc", keys_settings3 },
   { "func", keys_settings4 },
   { "automap", keys_settings5 },
   { "cheats", keys_settings6 },
   { NULL }
};

// Here's an example from this first screen, with explanations.
//
//  {
//  "STRAFE",      // The description of the item ('strafe' key)
//  S_KEY,         // This is a key binding item
//  m_scrn,        // It belongs to the m_scrn group. Its key cannot be
//                 // bound to two items in this group.
//  KB_X,          // The X offset of the start of the right-hand side
//  KB_Y+ 8*8,     // The Y offset of the start of the right-hand side.
//                 // Always given in multiples off a baseline.
//  &key_strafe,   // The variable that holds the key value bound to this
//                    OR a string that holds the config variable name.
//                    OR a pointer to another setup_menu
//  &mousebstrafe, // The variable that holds the mouse button bound to
                   // this. If zero, no mouse button can be bound here.
//  &joybstrafe,   // The variable that holds the joystick button bound to
                   // this. If zero, no mouse button can be bound here.
//  }

// The first Key Binding screen table.
// Note that the Y values are ascending. If you need to add something to
// this table, (well, this one's not a good example, because it's full)
// you need to make sure the Y values still make sense so everything gets
// displayed.
//
// Note also that the first screen of each set has a line for the reset
// button. If there is more than one screen in a set, the others don't get
// the reset button.
//
// Note also that this screen has a "NEXT ->" line. This acts like an
// item, in that 'activating' it moves you along to the next screen. If
// there's a "<- PREV" item on a screen, it behaves similarly, moving you
// to the previous screen. If you leave these off, you can't move from
// screen to screen.

setup_menu_t keys_settings1[] =  // Key Binding screen strings
{
  {"Fire"        , S_INPUT,      m_scrn, KB_X, M_Y, {0}, input_fire},
  {"Forward"     , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_forward},
  {"Backward"    , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_backward},
  {"Strafe Left" , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_strafeleft},
  {"Strafe Right", S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_straferight},
  {"Use"         , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_use},
  {"Run"         , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_speed},
  {"Strafe"      , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_strafe},
  {"Turn Left"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_turnleft},
  {"Turn Right"  , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_turnright},
  {"180 Turn", S_INPUT|S_STRICT, m_scrn, KB_X, M_SPC, {0}, input_reverse},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Toggles", S_SKIP|S_TITLE,    m_null, KB_X, M_SPC},
  {"Autorun"     , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_autorun},
  {"Free Look"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_freelook},
  {"Vertmouse"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_novert},

  MI_RESET,

  MI_END
};

setup_menu_t keys_settings2[] =
{
  {"Fist"    , S_INPUT,       m_scrn, KB_X, M_Y,   {0}, input_weapon1},
  {"Pistol"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon2},
  {"Shotgun" , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon3},
  {"Chaingun", S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon4},
  {"Rocket"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon5},
  {"Plasma"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon6},
  {"BFG",      S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon7},
  {"Chainsaw", S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon8},
  {"SSG"     , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon9},
  {"Best"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapontoggle},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  // [FG] prev/next weapon keys and buttons
  {"Prev"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_prevweapon},
  {"Next"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_nextweapon},

  MI_END
};

setup_menu_t keys_settings3[] =
{
  // [FG] reload current level / go to next level
  {"Reload Map/Demo",  S_INPUT, m_scrn, KB_X, M_Y,   {0}, input_menu_reloadlevel},
  {"Next Map",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_menu_nextlevel},
  {"Show Stats/Time",  S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_hud_timestats},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Fast-FWD Demo",     S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_fforward},
  {"Finish Demo",      S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_quit},
  {"Join Demo",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_join},
  {"Increase Speed",   S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_up},
  {"Decrease Speed",   S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_down},
  {"Default Speed",    S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_default},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Begin Chat",       S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat},
  {"Player 1",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest0},
  {"Player 2",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest1},
  {"Player 3",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest2},
  {"Player 4",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest3},

  MI_END
};

setup_menu_t keys_settings4[] =
{
  // phares 4/13/98:
  // key_help and key_escape can no longer be rebound. This keeps the
  // player from getting themselves in a bind where they can't remember how
  // to get to the menus, and can't remember how to get to the help screen
  // to give them a clue as to how to get to the menus. :)

  // Also, the keys assigned to these functions cannot be bound to other
  // functions. Introduce an S_KEEP flag to show that you cannot swap this
  // key with other keys in the same 'group'. (m_scrn, etc.)

  {"Help",       S_SKIP|S_KEEP, m_scrn, 0,    0,     {&key_help}},
  {"Menu",       S_SKIP|S_KEEP, m_scrn, 0,    0,     {&key_escape}},
  {"Pause",            S_INPUT, m_scrn, KB_X, M_Y,   {0}, input_pause},
  {"Save",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_savegame},
  {"Load",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_loadgame},
  {"Volume",           S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_soundvolume},
  {"Hud",              S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_hud},
  {"Quicksave",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quicksave},
  {"End Game",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_endgame},
  {"Messages",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_messages},
  {"Quickload",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quickload},
  {"Quit",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quit},
  {"Gamma Fix",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_gamma},
  {"Spy",              S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_spy},
  {"Screenshot",       S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_screenshot},
  {"Clean Screenshot", S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_clean_screenshot},
  {"Larger View",      S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_zoomin},
  {"Smaller View",     S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_zoomout},
  MI_END
};

setup_menu_t keys_settings5[] =  // Key Binding screen strings       
{
  {"Toggle Automap",  S_INPUT, m_map, KB_X, M_Y,   {0}, input_map},
  {"Follow",          S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_follow},
  {"Overlay",         S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_overlay},
  {"Rotate",          S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_rotate},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Zoom In",         S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_zoomin},
  {"Zoom Out",        S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_zoomout},
  {"Shift Up",        S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_up},
  {"Shift Down",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_down},
  {"Shift Left",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_left},
  {"Shift Right",     S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_right},
  {"Mark Place",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_mark},
  {"Clear Last Mark", S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_clear},
  {"Full/Zoom",       S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_gobig},
  {"Grid",            S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_grid},

  MI_END
};

#define CHEAT_X 160

setup_menu_t keys_settings6[] =
{
  {"Fake Archvile Jump",   S_INPUT, m_scrn, CHEAT_X, M_Y,   {0}, input_avj},
  {"God mode/Resurrect",   S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_iddqd},
  {"Ammo & Keys",          S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idkfa},
  {"Ammo",                 S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idfa},
  {"No Clipping",          S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idclip},
  {"Health",               S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdh},
  {"Armor",                S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdm},
  {"Invulnerability",      S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdv},
  {"Berserk",              S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholds},
  {"Partial Invisibility", S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdi},
  {"Radiation Suit",       S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdr},
  {"Reveal Map",           S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_iddt},
  {"Light Amplification",  S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdl},
  {"No Target",            S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_notarget},
  {"Freeze",               S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_freeze},

  MI_END
};

// Setting up for the Key Binding screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_KeyBindings(int choice)
{
  M_SetupNextMenu(&KeybndDef);

  setup_active = true;
  setup_screen = ss_keys;
  set_keybnd_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(keys_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = keys_settings[mult_screens_index];
  current_setup_tabs = keys_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Key Bindings Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawKeybnd(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  // Set up the Key Binding screen 

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(84, 2, "M_KEYBND", "KEY BINDINGS");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Weapon Screen tables.

// There's only one weapon settings screen (for now). But since we're
// trying to fit a common description for screens, it gets a setup_menu_t,
// which only has one screen definition in it.
//
// Note that this screen has no PREV or NEXT items, since there are no
// neighboring screens.

enum {           // killough 10/98: enum for y-offset info
  weap1_pref1,
  weap1_pref2,
  weap1_pref3,
  weap1_pref4,
  weap1_pref5,
  weap1_pref6,
  weap1_pref7,
  weap1_pref8,
  weap1_pref9,
  weap1_stub1,
  weap1_toggle,
  weap1_stub2,
  weap1_bfg,
};

enum {
  weap2_bobbing,
  weap2_hide_weapon,
  weap2_center, // [FG] centered weapon sprite
  weap2_recoilpitch,
};

setup_menu_t weap_settings1[], weap_settings2[];

static setup_menu_t* weap_settings[] =
{
  weap_settings1,
  weap_settings2,
  NULL
};

static setup_tab_t weap_tabs[] =
{
   { "preferences", weap_settings1 },
   { "cosmetic",    weap_settings2 },
   { NULL }
};

// [FG] centered or bobbing weapon sprite
static const char *center_weapon_strings[] = {
    "Off", "Centered", "Bobbing"
};

static const char *bobfactor_strings[] = {
    "Off", "Full", "75%"
};

static void M_UpdateCenteredWeaponItem(void)
{
  DISABLE_ITEM(!cosmetic_bobbing, weap_settings2[weap2_center]);
}

setup_menu_t weap_settings1[] =  // Weapons Settings screen
{
  {"1St Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_Y,   {"weapon_choice_1"}},
  {"2Nd Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_2"}},
  {"3Rd Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_3"}},
  {"4Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_4"}},
  {"5Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_5"}},
  {"6Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_6"}},
  {"7Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_7"}},
  {"8Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_8"}},
  {"9Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_9"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Use Weapon Toggles", S_YESNO|S_BOOM, m_null, M_X, M_SPC,
   {"doom_weapon_toggles"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Pre-Beta BFG", S_YESNO, m_null, M_X,  // killough 8/8/98
   M_SPC, {"classic_bfg"}},

  // Button for resetting to defaults
  MI_RESET,

  MI_END
};

setup_menu_t weap_settings2[] =
{
  {"View/Weapon Bobbing", S_CHOICE, m_null, M_X, M_Y,
   {"cosmetic_bobbing"}, 0, M_UpdateCenteredWeaponItem, str_bobfactor},

  {"Hide Weapon", S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"hide_weapon"}},

  // [FG] centered or bobbing weapon sprite
  {"Weapon Alignment", S_CHOICE|S_STRICT, m_null, M_X, M_SPC,
   {"center_weapon"}, 0, NULL, str_center_weapon},

  {"Weapon Recoil", S_YESNO, m_null, M_X, M_SPC, {"weapon_recoilpitch"}},

  MI_END
};

// Setting up for the Weapons screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_Weapons(int choice)
{
  M_SetupNextMenu(&WeaponDef);

  setup_active = true;
  setup_screen = ss_weap;
  set_weapon_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(weap_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = weap_settings[mult_screens_index];
  current_setup_tabs = weap_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Weapons Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawWeapons(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(109, 2, "M_WEAP", "WEAPONS");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Status Bar / HUD tables.

// Screen table definitions

setup_menu_t stat_settings1[];
setup_menu_t stat_settings2[];
setup_menu_t stat_settings3[];
setup_menu_t stat_settings4[];

static setup_menu_t* stat_settings[] =
{
  stat_settings1,
  stat_settings2,
  stat_settings3,
  stat_settings4,
  NULL
};

static setup_tab_t stat_tabs[] =
{
   { "HUD",       stat_settings1 },
   { "Widgets",   stat_settings2 },
   { "Crosshair", stat_settings3 },
   { "Messages",  stat_settings4 },
   { NULL }
};

enum {
  stat1_screensize,
  stat1_stub1,
  stat1_title1,
  stat1_colornum,
  stat1_graypcnt,
  stat1_solid,
  stat1_stub2,
  stat1_title2,
  stat1_type,
  stat1_mode,
  stat1_stub3,
  stat1_backpack,
  stat1_armortype,
  stat1_smooth,
};

static void M_SizeDisplayAlt(void)
{
    int choice = -1;

    if (screenblocks > saved_screenblocks)
    {
        choice = 1;
    }
    else if (screenblocks < saved_screenblocks)
    {
        choice = 0;
    }

    screenblocks = saved_screenblocks;

    if (choice != -1)
    {
        M_SizeDisplay(choice);
    }

    hud_displayed = (screenblocks == 11);
}

static const char *screensize_strings[] = {
    "", "", "", "Status Bar", "Status Bar", "Status Bar", "Status Bar",
    "Status Bar", "Status Bar", "Status Bar", "Status Bar", "Fullscreen"
};

static const char *hudtype_strings[] = {
    "Crispy", "Boom No Bars", "Boom"
};

static const char **M_GetHUDModeStrings(void)
{
    static const char *crispy_strings[] = {"Off", "Original", "Widescreen"};
    static const char *boom_strings[] = {"Minimal", "Compact", "Distributed"};
    return hud_type ? boom_strings : crispy_strings;
}

static void M_UpdateHUDModeStrings(void);

#define H_X_THRM8 (M_X_THRM8 - 14)
#define H_X (M_X - 14)

setup_menu_t stat_settings1[] =  // Status Bar and HUD Settings screen
{
  {"Screen Size", S_THERMO, m_null, H_X_THRM8, M_Y, {"screenblocks"}, 0, M_SizeDisplayAlt, str_screensize},

  {"", S_SKIP, m_null, H_X, M_THRM_SPC},

  {"Status Bar", S_SKIP|S_TITLE, m_null, H_X, M_SPC},
  {"Colored Numbers", S_YESNO|S_COSMETIC, m_null, H_X, M_SPC, {"sts_colored_numbers"}},
  {"Gray Percent Sign", S_YESNO|S_COSMETIC, m_null, H_X, M_SPC, {"sts_pct_always_gray"}},
  {"Solid Background Color", S_YESNO, m_null, H_X, M_SPC, {"st_solidbackground"}},

  {"", S_SKIP, m_null, H_X, M_SPC},

  {"Fullscreen HUD", S_SKIP|S_TITLE, m_null, H_X, M_SPC},
  {"HUD Type", S_CHOICE, m_null, H_X, M_SPC, {"hud_type"}, 0, M_UpdateHUDModeStrings, str_hudtype},
  {"HUD Mode", S_CHOICE, m_null, H_X, M_SPC, {"hud_active"}, 0, NULL, str_hudmode},

  {"", S_SKIP, m_null, H_X, M_SPC},

  {"Backpack Shifts Ammo Color", S_YESNO, m_null, H_X, M_SPC, {"hud_backpack_thresholds"}},
  {"Armor Color Matches Type", S_YESNO, m_null, H_X, M_SPC, {"hud_armor_type"}},
  {"Animated Health/Armor Count", S_YESNO, m_null, H_X, M_SPC, {"hud_animated_counts"}},
  {"Blink Missing Keys", S_YESNO, m_null, H_X, M_SPC, {"hud_blink_keys"}},

  MI_RESET,

  MI_END
};

enum {
  stat2_title1,
  stat2_stats,
  stat2_time,
  stat2_coords,
  stat2_timeuse,
  stat2_stub1,
  stat2_title2,
  stat2_hudfont,
  stat2_widescreen,
  stat2_layout,
};

static const char *show_widgets_strings[] = {
    "Off", "Automap", "HUD", "Always"
};

setup_menu_t stat_settings2[] =
{
  {"Widget Types", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Show Level Stats", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_level_stats"}, 0, NULL, str_show_widgets},
  {"Show Level Time", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_level_time"}, 0, NULL, str_show_widgets},
  {"Show Player Coords", S_CHOICE|S_STRICT, m_null, M_X, M_SPC,
   {"hud_player_coords"}, 0, NULL, str_show_widgets},
  {"Use-Button Timer", S_YESNO, m_null, M_X, M_SPC, {"hud_time_use"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Widget Appearance", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Use Doom Font", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_widget_font"}, 0, NULL, str_show_widgets},
  {"Widescreen Alignment", S_YESNO, m_null, M_X, M_SPC,
   {"hud_widescreen_widgets"}, 0, HU_Start},
  {"Vertical Layout", S_YESNO, m_null, M_X, M_SPC,
   {"hud_widget_layout"}, 0, HU_Start},

  MI_END
};

enum {
  stat3_xhair,
  stat3_xhairhealth,
  stat3_xhairtarget,
  stat3_xhairlockon,
  stat3_xhaircolor,
  stat3_xhairtcolor,
};

static void M_UpdateCrosshairItems (void)
{
    DISABLE_ITEM(!hud_crosshair, stat_settings3[stat3_xhairhealth]);
    DISABLE_ITEM(!hud_crosshair, stat_settings3[stat3_xhairtarget]);
    DISABLE_ITEM(!hud_crosshair, stat_settings3[stat3_xhairlockon]);
    DISABLE_ITEM(!hud_crosshair, stat_settings3[stat3_xhaircolor]);
    DISABLE_ITEM(!(hud_crosshair && hud_crosshair_target == crosstarget_highlight),
        stat_settings3[stat3_xhairtcolor]);
}

static const char *crosshair_target_strings[] = {
    "Off", "Highlight", "Health"
};

static const char *hudcolor_strings[] = {
    "BRICK", "TAN", "GRAY", "GREEN", "BROWN", "GOLD", "RED", "BLUE", "ORANGE",
    "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE"
};

setup_menu_t stat_settings3[] =
{
  {"Enable Crosshair", S_CHOICE, m_null, M_X, M_Y,
   {"hud_crosshair"}, 0, M_UpdateCrosshairItems, str_crosshair},

  {"Color By Player Health",S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"hud_crosshair_health"}},
  {"Color By Target", S_CHOICE|S_STRICT, m_null, M_X, M_SPC,
   {"hud_crosshair_target"}, 0, M_UpdateCrosshairItems, str_crosshair_target},
  {"Lock On Target", S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"hud_crosshair_lockon"}},
  {"Default Color", S_CRITEM, m_null,M_X, M_SPC,
   {"hud_crosshair_color"}, 0, NULL, str_hudcolor},
  {"Highlight Color", S_CRITEM|S_STRICT, m_null, M_X, M_SPC,
   {"hud_crosshair_target_color"}, 0, NULL, str_hudcolor},

  MI_END
};

setup_menu_t stat_settings4[] =
{
  {"\"A Secret is Revealed!\" Message", S_YESNO, m_null, M_X, M_Y,
   {"hud_secret_message"}},

  {"Show Toggle Messages", S_YESNO, m_null, M_X, M_SPC, {"show_toggle_messages"}},

  {"Show Pickup Messages", S_YESNO, m_null, M_X, M_SPC, {"show_pickup_messages"}},

  {"Show Obituaries", S_YESNO, m_null, M_X, M_SPC, {"show_obituary_messages"}},

  {"Center Messages", S_YESNO, m_null, M_X, M_SPC, {"message_centered"}},

  {"Colorize Messages", S_YESNO, m_null, M_X, M_SPC,
   {"message_colorized"}, 0, HU_ResetMessageColors},

  MI_END
};

// Setting up for the Status Bar / HUD screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_StatusBar(int choice)
{
  M_SetupNextMenu(&StatusHUDDef);

  setup_active = true;
  setup_screen = ss_stat;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(stat_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = stat_settings[mult_screens_index];
  current_setup_tabs = stat_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Status Bar / HUD Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawStatusHUD(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(59, 2, "M_STAT", "STATUS BAR / HUD");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Automap tables.

setup_menu_t auto_settings1[];

static setup_menu_t* auto_settings[] =
{
  auto_settings1,
  NULL
};

enum {
  auto1_title1,
  auto1_preset,
  auto1_follow,
  auto1_rotate,
  auto1_overlay,
  auto1_pointer,
  auto1_stub1,
  auto1_title2,
  auto1_smooth,
  auto1_secrets,
  auto1_flash,
};

static const char *overlay_strings[] = {
    "Off", "On", "Dark"
};

static const char *automap_preset_strings[] = {
    "Vanilla", "Boom", "ZDoom"
};

setup_menu_t auto_settings1[] =  // 1st AutoMap Settings screen       
{
  {"Modes", S_SKIP|S_TITLE, m_null, M_X, M_Y},
  {"Follow Player" ,        S_YESNO,  m_null, M_X, M_SPC, {"followplayer"}},
  {"Rotate Automap",        S_YESNO,  m_null, M_X, M_SPC, {"automaprotate"}},
  {"Overlay Automap",       S_CHOICE, m_null, M_X, M_SPC, {"automapoverlay"},
   0, NULL, str_overlay},
  {"Coords Follow Pointer", S_YESNO,  m_null, M_X, M_SPC, {"map_point_coord"}},  // killough 10/98

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Miscellaneous", S_SKIP|S_TITLE, m_null, M_X, M_SPC},
  {"Color Preset", S_CHOICE|S_COSMETIC, m_null, M_X, M_SPC,
   {"mapcolor_preset"}, 0, AM_ColorPreset, str_automap_preset},
  {"Smooth automap lines", S_YESNO, m_null, M_X, M_SPC,
   {"map_smooth_lines"}, 0, AM_EnableSmoothLines},
  {"Show Found Secrets Only", S_YESNO, m_null, M_X, M_SPC, {"map_secret_after"}},
  {"Flashing Keyed Doors" , S_YESNO, m_null, M_X, M_SPC, {"map_keyed_door_flash"}},

  MI_RESET,

  MI_END
};

// Setting up for the Automap screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_Automap(int choice)
{
  M_SetupNextMenu(&AutoMapDef);

  setup_active = true;
  setup_screen = ss_auto;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(auto_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = auto_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Automap Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawAutoMap(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(109, 2, "M_AUTO", "AUTOMAP");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Enemies table.

setup_menu_t enem_settings1[];

static setup_menu_t* enem_settings[] =
{
  enem_settings1,
  NULL
};

enum {
  enem1_helpers,
  enem1_gap1,

  enem1_title1,
  enem1_colored_blood,
  enem1_flipcorpses,
  enem1_ghost,
  enem1_fuzz,
};

static void M_BarkSound(void)
{
    if (default_dogs)
    {
        S_StartSound(NULL, sfx_dgact);
    }
}

setup_menu_t enem_settings1[] =  // Enemy Settings screen
{
  {"Helper Dogs", S_MBF|S_THERMO|S_THRM_SIZE4|S_LEVWARN|S_ACTION,
   m_null, M_X_THRM4, M_Y, {"player_helpers"}, 0, M_BarkSound},

  {"", S_SKIP, m_null, M_X, M_THRM_SPC},

  {"Cosmetic", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  // [FG] colored blood and gibs
  {"Colored Blood", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"colored_blood"}, 0, D_SetBloodColor},

  // [crispy] randomly flip corpse, blood and death animation sprites
  {"Randomly Mirrored Corpses", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"flipcorpses"}},

  // [crispy] resurrected pools of gore ("ghost monsters") are translucent
  {"Translucent Ghost Monsters", S_YESNO|S_STRICT|S_VANILLA, m_null, M_X, M_SPC,
   {"ghost_monsters"}},

  // [FG] spectre drawing mode
  {"Blocky Spectre Drawing", S_YESNO, m_null, M_X, M_SPC,
   {"fuzzcolumn_mode"}, 0, R_SetFuzzColumnMode},

  MI_RESET,

  MI_END

};

/////////////////////////////

// Setting up for the Enemies screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_Enemy(int choice)
{
  M_SetupNextMenu(&EnemyDef);

  setup_active = true;
  setup_screen = ss_enem;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(enem_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = enem_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Enemies Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawEnemy(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(114, 2, "M_ENEM", "ENEMIES");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Compatibility table.
// killough 10/10/98

setup_menu_t comp_settings1[];

static setup_menu_t* comp_settings[] =
{
  comp_settings1,
  NULL
};

enum
{
  comp1_title1,
  comp1_complevel,
  comp1_strictmode,
  comp1_gap1,

  comp1_title2,
  comp1_verticalaim,
  comp1_autostrafe50,
  comp1_pistolstart,
  comp1_gap2,

  comp1_blockmapfix,
  comp1_hangsolid,
  comp1_intercepts,
};

static const char *default_complevel_strings[] = {
  "Vanilla", "Boom", "MBF", "MBF21"
};

setup_menu_t comp_settings1[] =  // Compatibility Settings screen #1
{
  {"Compatibility", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Default Compatibility Level", S_CHOICE|S_LEVWARN, m_null, M_X, M_SPC,
   {"default_complevel"}, 0, NULL, str_default_complevel},

  {"Strict Mode", S_YESNO|S_LEVWARN, m_null, M_X, M_SPC, {"strictmode"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Compatibility-breaking Features", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Direct Vertical Aiming", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"direct_vertical_aiming"}},

  {"Auto Strafe 50", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"autostrafe50"}, 0, G_UpdateSideMove},

  {"Pistol Start", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"pistolstart"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Improved Hit Detection", S_YESNO|S_STRICT|S_BOOM, m_null, M_X,
   M_SPC, {"blockmapfix"}},

  {"Walk Under Solid Hanging Bodies", S_YESNO|S_STRICT, m_null, M_X,
   M_SPC, {"hangsolid"}},

  {"Emulate INTERCEPTS overflow", S_YESNO|S_VANILLA, m_null, M_X,
   M_SPC, {"emu_intercepts"}},


  MI_RESET,

  MI_END
};

// Setting up for the Compatibility screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_Compat(int choice)
{
  M_SetupNextMenu(&CompatDef);

  setup_active = true;
  setup_screen = ss_comp;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(comp_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = comp_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Compatibility Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawCompat(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(52, 2, "M_COMPAT", "DOOM COMPATIBILITY");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The General table.
// killough 10/10/98

extern int realtic_clock_rate, tran_filter_pct;

setup_menu_t gen_settings1[];
setup_menu_t gen_settings2[];
setup_menu_t gen_settings3[];
setup_menu_t gen_settings4[];
setup_menu_t gen_settings5[];
setup_menu_t gen_settings6[];

static setup_menu_t* gen_settings[] =
{
  gen_settings1,
  gen_settings2,
  gen_settings3,
  gen_settings4,
  gen_settings5,
  gen_settings6,
  NULL
};

static setup_tab_t gen_tabs[] =
{
   { "video",   gen_settings1 },
   { "audio",   gen_settings2 },
   { "mouse",   gen_settings3 },
   { "gamepad", gen_settings4 },
   { "display", gen_settings5 },
   { "misc",    gen_settings6 },
   { NULL }
};

// Page 1

enum {
  gen1_resolution_scale,
  gen1_dynamic_resolution,
  gen1_widescreen,
  gen1_fov,
  gen1_gap1,

  gen1_fullscreen,
  gen1_exclusive_fullscreen,
  gen1_gap2,

  gen1_uncapped,
  gen1_fpslimit,
  gen1_vsync,
  gen1_gap3,

  gen1_gamma
};

// Page 2

enum {
  gen2_sfx_vol,
  gen2_music_vol,
  gen2_gap1,

  gen2_sndmodule,
  gen2_sndhrtf,
  gen2_pitch,
  gen2_fullsnd,
  gen2_gap2,

  gen2_musicbackend,
};

int resolution_scale;

static const char **M_GetResolutionScaleStrings(void)
{
    const char **strings = NULL;

    resolution_scaling_t rs;
    I_GetResolutionScaling(&rs);

    array_push(strings, "100%");

    int val = SCREENHEIGHT * 2;
    char buf[8];

    for (int i = 1; val < rs.max; ++i)
    {
        if (val == current_video_height)
        {
            resolution_scale = i;
        }

        int pct = val * 100 / SCREENHEIGHT;
        M_snprintf(buf, sizeof(buf), "%d%%", pct);
        array_push(strings, M_StringDuplicate(buf));

        val += rs.step;
    }

    array_push(strings, "native");

    return strings;
}

static void M_ResetVideoHeight(void)
{
    const char **strings = GetStrings(str_resolution_scale);
    resolution_scaling_t rs;
    I_GetResolutionScaling(&rs);

    if (default_reset)
    {
        current_video_height = 600;
        int val = SCREENHEIGHT * 2;
        for (int i = 1; val < rs.max; ++i)
        {
            if (val == current_video_height)
            {
                resolution_scale = i;
                break;
            }

            val += rs.step;
        }
    }
    else
    {
        if (resolution_scale == array_size(strings))
        {
            current_video_height = rs.max;
        }
        else if (resolution_scale == 0)
        {
            current_video_height = SCREENHEIGHT;
        }
        else
        {
            current_video_height = SCREENHEIGHT * 2 + (resolution_scale - 1) * rs.step;
        }
    }

    if (!dynamic_resolution)
    {
        VX_ResetMaxDist();
    }

    DISABLE_ITEM(current_video_height <= DRS_MIN_HEIGHT,
                 gen_settings1[gen1_dynamic_resolution]);

    resetneeded = true;
}

static const char *widescreen_strings[] = {
    "Off", "Auto", "16:10", "16:9", "21:9"
};

int midi_player_menu;

static const char **M_GetMidiDevicesStrings(void)
{
    return I_DeviceList(&midi_player_menu);
}

static void M_SmoothLight(void)
{
  setsmoothlight = true;
  setsizeneeded = true; // run R_ExecuteSetViewSize
}

static const char *gamma_strings[] = {
    // Darker
    "-4", "-3.6", "-3.2", "-2.8", "-2.4", "-2.0", "-1.6", "-1.2", "-0.8",

    // No gamma correction
    "0",

    // Lighter
    "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4"
};

static void M_ResetGamma(void)
{
  I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static void M_UpdateAdvancedSoundItems(void)
{
  DISABLE_ITEM(snd_module != SND_MODULE_3D, gen_settings2[gen2_sndhrtf]);
}

static void M_SetSoundModule(void)
{
  M_UpdateAdvancedSoundItems();

  if (!I_AllowReinitSound())
  {
    // The OpenAL implementation doesn't support the ALC_SOFT_HRTF extension
    // which is required for alcResetDeviceSOFT(). Warn the user to restart.
    warn_about_changes(S_PRGWARN);
    return;
  }

  I_SetSoundModule(snd_module);
}

static void M_SetMidiPlayer(void)
{
  S_StopMusic();
  I_SetMidiPlayer(midi_player_menu);
  S_SetMusicVolume(snd_MusicVolume);
  S_RestartMusic();
}

static void M_ToggleUncapped(void)
{
  DISABLE_ITEM(!default_uncapped, gen_settings1[gen1_fpslimit]);
  setrefreshneeded = true;
}

static void M_ToggleFullScreen(void)
{
  toggle_fullscreen = true;
}

static void M_ToggleExclusiveFullScreen(void)
{
  toggle_exclusive_fullscreen = true;
}

static void M_CoerceFPSLimit(void)
{
  if (fpslimit < TICRATE)
    fpslimit = 0;
  setrefreshneeded = true;
}

static void M_UpdateFOV(void)
{
  setsizeneeded = true; // run R_ExecuteSetViewSize;
}

static void M_ResetScreen(void)
{
  resetneeded = true;
}

static void M_UpdateSfxVolume(void)
{
  S_SetSfxVolume(snd_SfxVolume);
}

static void M_UpdateMusicVolume(void)
{
  S_SetMusicVolume(snd_MusicVolume);
}

setup_menu_t gen_settings1[] = { // General Settings screen1

  {"Resolution Scale", S_THERMO|S_THRM_SIZE11|S_ACTION, m_null, M_X_THRM11, M_Y,
   {"resolution_scale"}, 0, M_ResetVideoHeight, str_resolution_scale},

  {"Dynamic Resolution", S_YESNO, m_null, M_X, M_THRM_SPC,
   {"dynamic_resolution"}, 0, M_ResetVideoHeight},

  {"Widescreen", S_CHOICE, m_null, M_X, M_SPC,
   {"widescreen"}, 0, M_ResetScreen, str_widescreen},

  {"FOV", S_THERMO, m_null, M_X_THRM8, M_SPC, {"fov"}, 0, M_UpdateFOV},

  {"", S_SKIP, m_null, M_X, M_THRM_SPC},

  {"Fullscreen", S_YESNO, m_null, M_X, M_SPC,
   {"fullscreen"}, 0, M_ToggleFullScreen},

  {"Exclusive Fullscreen", S_YESNO, m_null, M_X, M_SPC,
   {"exclusive_fullscreen"}, 0, M_ToggleExclusiveFullScreen},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Uncapped Framerate", S_YESNO, m_null, M_X, M_SPC,
   {"uncapped"}, 0, M_ToggleUncapped},

  {"Framerate Limit", S_NUM, m_null, M_X, M_SPC,
   {"fpslimit"}, 0, M_CoerceFPSLimit},

  {"VSync", S_YESNO, m_null, M_X, M_SPC, {"use_vsync"}, 0, I_ToggleVsync},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Gamma Correction", S_THERMO, m_null, M_X_THRM8, M_SPC,
   {"gamma2"}, 0, M_ResetGamma, str_gamma},

  MI_RESET,

  MI_END
};

setup_menu_t gen_settings2[] = { // General Settings screen2

  {"Sound Volume", S_THERMO, m_null, M_X_THRM8, M_Y,
   {"sfx_volume"}, 0, M_UpdateSfxVolume},

  {"Music Volume", S_THERMO, m_null, M_X_THRM8, M_THRM_SPC,
   {"music_volume"}, 0, M_UpdateMusicVolume},

  {"", S_SKIP, m_null, M_X, M_THRM_SPC},

  {"Sound Module", S_CHOICE, m_null, M_X, M_SPC,
   {"snd_module"}, 0, M_SetSoundModule, str_sound_module},

  {"Headphones Mode", S_YESNO, m_null, M_X, M_SPC, {"snd_hrtf"}, 0, M_SetSoundModule},

  {"Pitch-Shifted Sounds", S_YESNO, m_null, M_X, M_SPC, {"pitched_sounds"}},

  // [FG] play sounds in full length
  {"Disable Sound Cutoffs", S_YESNO, m_null, M_X, M_SPC, {"full_sounds"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  // [FG] music backend
  {"MIDI player", S_CHOICE|S_ACTION|S_NEXT_LINE, m_null, M_X, M_SPC,
   {"midi_player_menu"}, 0, M_SetMidiPlayer, str_midi_player},

  MI_END
};

// Page 3

enum {
  gen3_dclick,
  gen3_free_look,
  gen3_invert_look,
  gen3_gap1,

  gen3_turn_sens,
  gen3_look_sens,
  gen3_forward_sens,
  gen3_strafe_sens,
  gen3_gap2,

  gen3_mouse_accel,
};

// Page 5

enum {
  gen5_smooth_scaling,
  gen5_trans,
  gen5_transpct,
  gen5_gap1,

  gen5_voxels,
  gen5_brightmaps,
  gen5_stretch_sky,
  gen5_linear_sky,
  gen5_swirl,
  gen5_smoothlight,
  gen5_gap2,

  gen5_menu_background,
  gen5_endoom,
};

// Page 6

enum {
  gen6_title1,
  gen6_screen_melt,
  gen6_death_action,
  gen6_demobar,
  gen6_palette_changes,
  gen6_level_brightness,
  gen6_organize_savefiles,
  gen6_gap1,

  gen6_title2,
  gen6_realtic_clock_rate,
  gen6_default_skill,
};


#define MOUSE_ACCEL_STRINGS_SIZE (40 + 1)

static const char **M_GetMouseAccelStrings(void)
{
    static const char *strings[MOUSE_ACCEL_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MOUSE_ACCEL_STRINGS_SIZE; ++i)
    {
        int val = i + 10;
        M_snprintf(buf, sizeof(buf), "%1d.%1d", val / 10, val % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

void M_ResetTimeScale(void)
{
    if (strictmode || D_CheckNetConnect())
    {
        I_SetTimeScale(100);
        return;
    }

    int time_scale = realtic_clock_rate;

    //!
    // @arg <n>
    // @category game
    //
    // Increase or decrease game speed, percentage of normal.
    //

    int p = M_CheckParmWithArgs("-speed", 1);

    if (p)
    {
        time_scale = M_ParmArgToInt(p);
        if (time_scale < 10 || time_scale > 1000)
        {
            I_Error("Invalid parameter '%d' for -speed, valid values are 10-1000.", time_scale);
        }
    }

    I_SetTimeScale(time_scale);
}

static void M_UpdateFreeLook(void)
{
  P_UpdateDirectVerticalAiming();

  if (!mouselook || !padlook)
  {
    int i;
    for (i = 0; i < MAXPLAYERS; ++i)
      if (playeringame[i])
        players[i].centering = true;
  }
}

static const char *layout_strings[] = {
  "Default", "Swap", "Legacy", "Legacy Swap"
};

static const char *curve_strings[] = {
  "", "", "", "", "", "", "", "", "", "", // Dummy values, start at 1.0.
  "Linear", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9",
  "Squared", "2.1", "2.2", "2.3", "2.4", "2.5", "2.6", "2.7", "2.8", "2.9",
  "Cubed"
};

const char *default_skill_strings[] = {
  // dummy first option because defaultskill is 1-based
  "", "ITYTD", "HNTR", "HMP", "UV", "NM"
};

static const char *endoom_strings[] = {
  "off", "on", "PWAD only"
};

static const char *death_use_action_strings[] = {
  "default", "last save", "nothing"
};

static const char *menu_backdrop_strings[] = {
  "Off", "Dark", "Texture"
};

void M_DisableVoxelsRenderingItem(void)
{
    gen_settings5[gen5_voxels].m_flags |= S_DISABLE;
}

#define CNTR_X 162

setup_menu_t gen_settings3[] = {

  // [FG] double click to "use"
  {"Double-Click to \"Use\"", S_YESNO, m_null, CNTR_X, M_Y, {"dclick_use"}},

  {"Free Look", S_YESNO, m_null, CNTR_X, M_SPC,
   {"mouselook"}, 0, M_UpdateFreeLook},

  // [FG] invert vertical axis
  {"Invert Look", S_YESNO, m_null, CNTR_X, M_SPC,
   {"mouse_y_invert"}},

  {"", S_SKIP, m_null, CNTR_X, M_SPC},

  {"Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_SPC,
   {"mouse_sensitivity"}},

  {"Look Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_y_look"}},

  {"Move Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_y"}},

  {"Strafe Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_strafe"}},

  {"", S_SKIP, m_null, CNTR_X, M_THRM_SPC},

  {"Mouse acceleration", S_THERMO, m_null, CNTR_X, M_SPC,
   {"mouse_acceleration"}, 0, NULL, str_mouse_accel},

  MI_END
};

setup_menu_t gen_settings4[] = {

  {"Stick Layout", S_CHOICE, m_scrn, CNTR_X, M_Y,
   {"joy_layout"}, 0, I_ResetController, str_layout},

  {"Free Look", S_YESNO, m_null, CNTR_X, M_SPC,
   {"padlook"}, 0, M_UpdateFreeLook},

  {"Invert Look", S_YESNO, m_scrn, CNTR_X, M_SPC, {"joy_invert_look"}},

  {"", S_SKIP, m_null, CNTR_X, M_SPC},

  {"Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_SPC,
   {"joy_sensitivity_turn"}, 0, I_ResetController},

  {"Look Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_sensitivity_look"}, 0, I_ResetController},

  {"Extra Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_extra_sensitivity_turn"}, 0, I_ResetController},

  {"", S_SKIP, m_null, CNTR_X, M_THRM_SPC},

  {"Movement Curve", S_THERMO, m_scrn, CNTR_X, M_SPC,
   {"joy_response_curve_movement"}, 0, I_ResetController, str_curve},

  {"Camera Curve", S_THERMO, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_response_curve_camera"}, 0, I_ResetController, str_curve},

  {"Movement Deadzone", S_THERMO, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_deadzone_movement"}, 0, I_ResetController},

  {"Camera Deadzone", S_THERMO, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_deadzone_camera"}, 0, I_ResetController},

  MI_END
};

setup_menu_t gen_settings5[] = {

  {"Smooth Pixel Scaling", S_YESNO, m_null, M_X, M_Y,
   {"smooth_scaling"}, 0, M_ResetScreen},

  {"Enable Translucency", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"translucency"}, 0, M_Trans},

  {"Translucency Percent", S_NUM, m_null, M_X, M_SPC,
   {"tran_filter_pct"}, 0, M_Trans},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Voxels", S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"voxels_rendering"}},

  {"Brightmaps", S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"brightmaps"}},

  {"Stretch Short Skies", S_YESNO, m_null, M_X, M_SPC,
   {"stretchsky"}, 0, R_InitSkyMap},

  {"Linear Sky Scrolling", S_YESNO, m_null, M_X, M_SPC,
   {"linearsky"}, 0, R_InitPlanes},

  {"Swirling Flats", S_YESNO, m_null, M_X, M_SPC, {"r_swirl"}},

  {"Smooth Diminishing Lighting", S_YESNO, m_null, M_X, M_SPC,
   {"smoothlight"}, 0, M_SmoothLight},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Menu Backdrop", S_CHOICE, m_null, M_X, M_SPC,
   {"menu_backdrop"}, 0, NULL, str_menu_backdrop},

  {"Show ENDOOM Screen", S_CHOICE, m_null, M_X, M_SPC,
   {"show_endoom"}, 0, NULL, str_endoom},

  MI_END
};

setup_menu_t gen_settings6[] = {

  {"Quality of life", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Screen melt", S_YESNO|S_STRICT, m_null, M_X, M_SPC, {"screen_melt"}},

  {"On death action", S_CHOICE, m_null, M_X, M_SPC,
   {"death_use_action"}, 0, NULL, str_death_use_action},

  {"Demo progress bar", S_YESNO, m_null, M_X, M_SPC, {"demobar"}},

  {"Screen flashes", S_YESNO|S_STRICT, m_null, M_X, M_SPC,
   {"palette_changes"}},

  {"Level Brightness", S_THERMO|S_THRM_SIZE4|S_STRICT, m_null, M_X_THRM4, M_SPC,
   {"extra_level_brightness"}},

  {"Organize save files", S_YESNO|S_PRGWARN, m_null, M_X, M_THRM_SPC,
   {"organize_savefiles"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Miscellaneous", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Game speed", S_NUM|S_STRICT, m_null, M_X, M_SPC,
   {"realtic_clock_rate"}, 0, M_ResetTimeScale},

  {"Default Skill", S_CHOICE|S_LEVWARN, m_null, M_X, M_SPC,
   {"default_skill"}, 0, NULL, str_default_skill},

  MI_END
};

void M_Trans(void) // To reset translucency after setting it in menu
{
    R_InitTranMap(0);

    D_SetPredefinedTranslucency();
}

// Setting up for the General screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

static void M_General(int choice)
{
  M_SetupNextMenu(&GeneralDef);

  setup_active = true;
  setup_screen = ss_gen;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(gen_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = gen_settings[mult_screens_index];
  current_setup_tabs = gen_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

static void M_DrawGeneral(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(114, 2, "M_GENERL", "GENERAL");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// General routines used by the Setup screens.
//

static boolean shiftdown = false; // phares 4/10/98: SHIFT key down or not

// phares 4/17/98:
// M_SelectDone() gets called when you have finished entering your
// Setup Menu item change.

static void M_SelectDone(setup_menu_t* ptr)
{
  ptr->m_flags &= ~S_SELECT;
  ptr->m_flags |= S_HILITE;
  S_StartSound(NULL,sfx_itemup);
  setup_select = false;
  if (print_warning_about_changes)     // killough 8/15/98
    print_warning_about_changes--;
}

// phares 4/21/98:
// Array of setup screens used by M_ResetDefaults()

static setup_menu_t **setup_screens[] =
{
  keys_settings,
  weap_settings,
  stat_settings,
  auto_settings,
  enem_settings,
  gen_settings,      // killough 10/98
  comp_settings,
};

// phares 4/19/98:
// M_ResetDefaults() resets all values for a setup screen to default values
// 
// killough 10/98: rewritten to fix bugs and warn about pending changes

static void M_ResetDefaults()
{
    default_t *dp;
    int warn = 0;

    default_reset = true; // needed to propely reset some dynamic items

    // Look through the defaults table and reset every variable that
    // belongs to the group we're interested in.
    //
    // killough: However, only reset variables whose field in the
    // current setup screen is the same as in the defaults table.
    // i.e. only reset variables really in the current setup screen.

    for (dp = defaults; dp->name; dp++)
    {
        if (dp->setupscreen != setup_screen)
        {
            continue;
        }

        setup_menu_t **screens = setup_screens[setup_screen - 1];

        for ( ; *screens; screens++)
        {
            setup_menu_t *current_item = *screens;

            for (; !(current_item->m_flags & S_END); current_item++)
            {
                int flags = current_item->m_flags;

                if (flags & S_HASDEFPTR && current_item->var.def == dp)
                {
                    if (dp->type == string)
                    {
                        free(dp->location->s);
                        dp->location->s = strdup(dp->defaultvalue.s);
                    }
                    else if (dp->type == number)
                    {
                        dp->location->i = dp->defaultvalue.i;
                    }

                    if (flags & (S_LEVWARN | S_PRGWARN))
                    {
                        warn |= flags & (S_LEVWARN | S_PRGWARN);
                    }
                    else if (dp->current)
                    {
                        if (dp->type == string)
                            dp->current->s = dp->location->s;
                        else if (dp->type == number)
                            dp->current->i = dp->location->i;
                    }

                    if (current_item->action)
                    {
                        current_item->action();
                    }
                }
                else if (current_item->input_id == dp->input_id)
                {
                    M_InputSetDefault(dp->input_id, dp->inputs);
                }
            }
        }
    }

    default_reset = false;

    if (warn)
        warn_about_changes(warn);
}

//
// M_InitDefaults()
//
// killough 11/98:
//
// This function converts all setup menu entries consisting of cfg
// variable names, into pointers to the corresponding default[]
// array entry. var.name becomes converted to var.def.
//

static void M_InitDefaults(void)
{
  setup_menu_t *const *p, *t;
  default_t *dp;
  int i;
  for (i = 0; i < ss_max-1; i++)
    for (p = setup_screens[i]; *p; p++)
      for (t = *p; !(t->m_flags & S_END); t++)
	if (t->m_flags & S_HASDEFPTR)
	{
	  if (!(dp = M_LookupDefault(t->var.name)))
	    I_Error("Could not find config variable \"%s\"", t->var.name);
	  else
	    (t->var.def = dp)->setup_menu = t;
	}
}

//
// End of Setup Screens.
//
/////////////////////////////////////////////////////////////////////////////

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

static int extended_help_count;   // number of user-defined help screens found
static int extended_help_index;   // index of current extended help screen

static menuitem_t ExtHelpMenu[] =
{
  {1,"",M_ExtHelpNextScreen,0}
};

static menu_t ExtHelpDef =
{
  1,             // # of menu items
  &ReadDef1,     // previous menu
  ExtHelpMenu,   // menuitem_t ->
  M_DrawExtHelp, // drawing routine ->
  330,181,       // x,y
  0              // lastOn
};

// M_ExtHelpNextScreen establishes the number of the next HELP screen in
// the series.

static void M_ExtHelpNextScreen(int choice)
{
  choice = 0;
  if (++extended_help_index > extended_help_count)
    {

      // when finished with extended help screens, return to Main Menu

      extended_help_index = 1;
      M_SetupNextMenu(&MainDef);
    }
}

// phares 3/30/98:
// Routine to look for HELPnn screens and create a menu
// definition structure that defines extended help screens.

static void M_InitExtendedHelp(void)
{
  int index,i;
  char namebfr[] = "HELPnn"; // haleyjd: can't write into constant

  extended_help_count = 0;
  for (index = 1 ; index < 100 ; index++)
    {
      namebfr[4] = index/10 + 0x30;
      namebfr[5] = index%10 + 0x30;
      i = W_CheckNumForName(namebfr);
      if (i == -1)
	{
	  if (extended_help_count)
	  {
	    // Restore extended help functionality
	    // for all game versions
	    ExtHelpDef.prevMenu  = &HelpDef; // previous menu
	    HelpMenu[0].routine = M_ExtHelp;

	    if (gamemode != commercial)
	      {
		ReadMenu2[0].routine = M_ExtHelp;
	      }
	  }
	  return;
	}
      extended_help_count++;
    }

}

// Initialization for the extended HELP screens.

static void M_ExtHelp(int choice)
{
  choice = 0;
  extended_help_index = 1; // Start with first extended help screen
  M_SetupNextMenu(&ExtHelpDef);
}

// Initialize the drawing part of the extended HELP screens.

static void M_DrawExtHelp(void)
{
  char namebfr[] = "HELPnn"; // [FG] char array!

  inhelpscreens = true;              // killough 5/1/98
  namebfr[4] = extended_help_index/10 + 0x30;
  namebfr[5] = extended_help_index%10 + 0x30;
  V_DrawPatchFullScreen(W_CacheLumpName(namebfr, PU_CACHE));
}

//
// M_DrawHelp
//
// This displays the help screen

static void M_DrawHelp(void)
{
  // Display help screen from PWAD
  int helplump;
  if (gamemode == commercial)
    helplump = W_CheckNumForName("HELP");
  else
    helplump = W_CheckNumForName("HELP1");

  inhelpscreens = true;                        // killough 10/98
  V_DrawPatchFullScreen(W_CacheLumpNum(helplump, PU_CACHE));
}

//
// End of Extended HELP screens               // phares 3/30/98
//
////////////////////////////////////////////////////////////////////////////

static int M_GetKeyString(int c, int offset)
{
  const char *s;

  if (c >= 33 && c <= 126)
  {
      // The '=', ',', and '.' keys originally meant the shifted
      // versions of those keys, but w/o having to shift them in
      // the game. Any actions that are mapped to these keys will
      // still mean their shifted versions. Could be changed later
      // if someone can come up with a better way to deal with them.

      if (c == '=')      // probably means the '+' key?
          c = '+';
      else if (c == ',') // probably means the '<' key?
          c = '<';
      else if (c == '.') // probably means the '>' key?
          c = '>';
      menu_buffer[offset++] = c; // Just insert the ascii key
      menu_buffer[offset] = 0;
  }
  else
  {
      s = M_GetNameForKey(c);
      if (!s)
      {
          s = "JUNK";
      }
      strcpy(&menu_buffer[offset], s); // string to display
      offset += strlen(s);
  }

  return offset;
}

#define SPACEWIDTH 4

static int menu_font_spacing = 0;

// M_DrawMenuString() draws the string in menu_buffer[]

static void M_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch)
{
    int   w;
    int   c;

    byte *cr = cr1;

    while (*ch)
    {
        c = *ch++;         // get next char

        if (c == '\x1b')
        {
            if (ch)
            {
                c = *ch++;
                if (c >= '0' && c <= '0'+CR_NONE)
                    cr = colrngs[c - '0'];
                else if (c == '0'+CR_ORIG)
                    cr = cr1;
                continue;
            }
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            cx += SPACEWIDTH;    // space
            continue;
        }

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        // V_DrawpatchTranslated() will draw the string in the
        // desired color, colrngs[color]
        if (cr && cr2)
            V_DrawPatchTRTR(cx, cy, hu_font[c], cr, cr2);
        else
            V_DrawPatchTranslated(cx, cy, hu_font[c], cr);

        // The screen is cramped, so trim one unit from each
        // character so they butt up against each other.
        cx += w + menu_font_spacing;
    }
}

void M_DrawString(int cx, int cy, int color, const char *ch)
{
  M_DrawStringCR(cx, cy, colrngs[color], NULL, ch);
}

// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it is not tied to menu_buffer

static void M_DrawMenuString(int cx, int cy, int color)
{
  M_DrawString(cx, cy, color, menu_buffer);
}

static void M_DrawMenuStringEx(int flags, int x, int y, int color)
{
    if (ItemDisabled(flags))
    {
        M_DrawStringCR(x, y, cr_dark, NULL, menu_buffer);
    }
    else if (flags & S_HILITE)
    {
        if (color == CR_NONE)
            M_DrawStringCR(x, y, cr_bright, NULL, menu_buffer);
        else
            M_DrawStringCR(x, y, colrngs[color], cr_bright, menu_buffer);
    }
    else
    {
        M_DrawMenuString(x, y, color);
    }
}

// M_GetPixelWidth() returns the number of pixels in the width of
// the string, NOT the number of chars in the string.

int M_GetPixelWidth(const char *ch)
{
    int len = 0;
    int c;

    while (*ch)
    {
        c = *ch++;    // pick up next char

        if (c == '\x1b') // skip color
        {
            if (*ch)
                ch++;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            len += SPACEWIDTH;   // space
            continue;
        }

        len += SHORT(hu_font[c]->width);
        len += menu_font_spacing; // adjust so everything fits
    }
    len -= menu_font_spacing; // replace what you took away on the last char only
    return len;
}

enum {
  prog,
  art,
  test, test_stub, test_stub2,
  canine,
  musicsfx, /*musicsfx_stub,*/
  woof, // [FG] shamelessly add myself to the Credits page ;)
  adcr, adcr_stub,
  special, special_stub, special_stub2,
};

enum {
  cr_prog=1,
  cr_art,
  cr_test,
  cr_canine,
  cr_musicsfx,
  cr_woof, // [FG] shamelessly add myself to the Credits page ;)
  cr_adcr,
  cr_special,
};

#define CR_S 9
#define CR_X 152
#define CR_X2 (CR_X+8)
#define CR_Y 31
#define CR_SH 2

static setup_menu_t cred_settings[] = {

  {"Programmer",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*prog + CR_SH*cr_prog},
  {"Lee Killough",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*prog + CR_SH*cr_prog},

  {"Artist",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*art + CR_SH*cr_art},
  {"Len Pitre",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*art + CR_SH*cr_art},

  {"PlayTesters",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*test + CR_SH*cr_test},
  {"Ky (Rez) Moffet",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*test + CR_SH*cr_test},
  {"Len Pitre",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(test+1) + CR_SH*cr_test},
  {"James (Quasar) Haley",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(test+2) + CR_SH*cr_test},

  {"Canine Consulting",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*canine + CR_SH*cr_canine},
  {"Longplain Kennels, Reg'd",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*canine + CR_SH*cr_canine},

  // haleyjd 05/12/09: changed Allegro credits to Team Eternity
  {"SDL Port By",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*musicsfx + CR_SH*cr_musicsfx},
  {"Team Eternity",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*musicsfx + CR_SH*cr_musicsfx},

  // [FG] shamelessly add myself to the Credits page ;)
  {"Woof! by",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*woof + CR_SH*cr_woof},
  {"Fabian Greffrath",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*woof + CR_SH*cr_woof},

  {"Additional Credit To",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*adcr + CR_SH*cr_adcr},
  {"id Software",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*adcr+CR_SH*cr_adcr},
  {"TeamTNT",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(adcr+1)+CR_SH*cr_adcr},

  {"Special Thanks To",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*special + CR_SH*cr_special},
  {"John Romero",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(special+0)+CR_SH*cr_special},
  {"Joel Murdoch",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(special+1)+CR_SH*cr_special},

  {0,S_SKIP|S_END,m_null}
};

void M_DrawCredits(void)     // killough 10/98: credit screen
{
  char mbftext_s[32];
  sprintf(mbftext_s, PROJECT_STRING);
  inhelpscreens = true;
  M_DrawBackground(gamemode==shareware ? "CEIL5_1" : "MFLR8_4");
  M_DrawTitle(42, 9, "MBFTEXT", mbftext_s);
  M_DrawScreenItems(cred_settings);
}

static boolean M_ShortcutResponder(const event_t *ev)
{
    // If there is no active menu displayed...

    if (menuactive || chat_on)
    {
        return false;
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
        if (ev->type == ev_joyb_down)
        {
            // Gamepad free look toggle only affects gamepad.
            padlook = !padlook;
            togglemsg("Gamepad Free Look %s", padlook ? "On" : "Off");
        }
        else
        {
            // Keyboard or mouse free look toggle only affects mouse.
            mouselook = !mouselook;
            togglemsg("Free Look %s", mouselook ? "On" : "Off");
        }
        M_UpdateFreeLook();
        // return true; // [FG] don't let toggles eat keys
    }

    if (M_InputActivated(input_speed_up) && !D_CheckNetConnect() && !strictmode)
    {
        realtic_clock_rate += 10;
        realtic_clock_rate = BETWEEN(10, 1000, realtic_clock_rate);
        displaymsg("Game Speed: %d", realtic_clock_rate);
        I_SetTimeScale(realtic_clock_rate);
    }

    if (M_InputActivated(input_speed_down) && !D_CheckNetConnect() && !strictmode)
    {
        realtic_clock_rate -= 10;
        realtic_clock_rate = BETWEEN(10, 1000, realtic_clock_rate);
        displaymsg("Game Speed: %d", realtic_clock_rate);
        I_SetTimeScale(realtic_clock_rate);
    }

    if (M_InputActivated(input_speed_default) && !D_CheckNetConnect() && !strictmode)
    {
        realtic_clock_rate = 100;
        displaymsg("Game Speed: %d", realtic_clock_rate);
        I_SetTimeScale(realtic_clock_rate);
    }

    if (M_InputActivated(input_help))      // Help key
    {
        M_StartControlPanel ();

        currentMenu = &HelpDef;         // killough 10/98: new help screen

        itemOn = 0;
        S_StartSound(NULL,sfx_swtchn);
        return true;
    }

    if (M_InputActivated(input_savegame))     // Save Game
    {
        M_StartControlPanel();
        S_StartSound(NULL,sfx_swtchn);
        M_SaveGame(0);
        return true;
    }

    if (M_InputActivated(input_loadgame))     // Load Game
    {
        M_StartControlPanel();
        S_StartSound(NULL,sfx_swtchn);
        M_LoadGame(0);
        return true;
    }

    if (M_InputActivated(input_soundvolume))      // Sound Volume
    {
        M_StartControlPanel ();
        currentMenu = &SoundDef;
        itemOn = sfx_vol;
        S_StartSound(NULL,sfx_swtchn);
        return true;
    }

    if (M_InputActivated(input_quicksave))      // Quicksave
    {
        S_StartSound(NULL,sfx_swtchn);
        M_QuickSave();
        return true;
    }

    if (M_InputActivated(input_endgame))      // End game
    {
        S_StartSound(NULL,sfx_swtchn);
        M_EndGame(0);
        return true;
    }

    if (M_InputActivated(input_messages))      // Toggle messages
    {
        M_ChangeMessages(0);
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }

    if (M_InputActivated(input_quickload))      // Quickload
    {
        S_StartSound(NULL, sfx_swtchn);
        M_QuickLoad();
        return true;
    }

    if (M_InputActivated(input_quit))       // Quit DOOM
    {
        S_StartSound(NULL, sfx_swtchn);
        M_QuitDOOM(0);
        return true;
    }

    if (M_InputActivated(input_gamma))       // gamma toggle
    {
        gamma2++;
        if (gamma2 > 17)
        {
            gamma2 = 0;
        }
        togglemsg("Gamma correction level %s", gamma_strings[gamma2]);
        M_ResetGamma();
        return true;
    }

    if (M_InputActivated(input_zoomout))     // zoom out
    {
        if (automapactive)
        {
            return false;
        }
        M_SizeDisplay(0);
        S_StartSound(NULL, sfx_stnmov);
        return true;
    }

    if (M_InputActivated(input_zoomin))               // zoom in
    {                                   // jff 2/23/98
        if (automapactive)                // allow
        {                                 // key_hud==key_zoomin
            return false;
        }
        M_SizeDisplay(1);
        S_StartSound(NULL, sfx_stnmov);
        return true;
    }

    if (M_InputActivated(input_hud))   // heads-up mode
    {
        if (automap_on)    // jff 2/22/98
        {
            return false;                  // HUD mode control
        }

        if (screenSize < 8)                // function on default F5
        {
            while (screenSize<8 || !hud_displayed) // make hud visible
                M_SizeDisplay(1);            // when configuring it
        }
        else
        {
            hud_displayed = 1;               //jff 3/3/98 turn hud on
            hud_active = (hud_active + 1) % 3; // cycle hud_active
            HU_disable_all_widgets();
        }
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

    // killough 10/98: allow key shortcut into Setup menu
    if (M_InputActivated(input_setup))
    {
        M_StartControlPanel();
        S_StartSound(NULL,sfx_swtchn);
        M_SetupNextMenu(&SetupDef);
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

    if (M_InputActivated(input_demo_fforward))
    {
        if (demoplayback && !PLAYBACK_SKIP && !fastdemo && !D_CheckNetConnect())
        {
            fastdemo_timer = !fastdemo_timer;
            I_SetFastdemoTimer(fastdemo_timer);
            return true;
        }
    }

    return false;
}

static boolean M_PointInsideRect(mrect_t *rect, int x, int y)
{
    return x > rect->x + video.deltaw &&
           x < rect->x + rect->w + video.deltaw &&
           y > rect->y &&
           y < rect->y + rect->h;
}

static void M_MenuMouseCursorPosition(int x, int y)
{
    if (!menuactive || messageToPrint)
    {
        return;
    }

    if (setup_active && !setup_select)
    {
        if (current_setup_tabs)
        {
            for (int i = 0; current_setup_tabs[i].text; ++i)
            {
                setup_tab_t *tab = &current_setup_tabs[i];

                tab->flags &= ~S_HILITE;

                if (M_PointInsideRect(&tab->rect, x, y))
                {
                    tab->flags |= S_HILITE;

                    if (set_tab_on != i)
                    {
                        set_tab_on = i;
                        S_StartSound(NULL, sfx_itemup);
                    }
                }
            }
        }

        for (int i = 0; !(current_setup_menu[i].m_flags & S_END); i++)
        {
            setup_menu_t *item = &current_setup_menu[i];
            int flags = item->m_flags;

            if (flags & S_SKIP)
            {
                continue;
            }

            item->m_flags &= ~S_HILITE;

            if (M_PointInsideRect(&item->rect, x, y))
            {
                item->m_flags |= S_HILITE;

                if (set_menu_itemon != i)
                {
                    print_warning_about_changes = false;
                    set_menu_itemon = i;
                    S_StartSound(NULL, sfx_itemup);
                }
            }
        }

        return;
    }

    for (int i = 0; i < currentMenu->numitems; i++)
    {
        menuitem_t *item = &currentMenu->menuitems[i];
        mrect_t *rect = &item->rect;

        item->flags &= ~MF_HILITE;

        if (M_PointInsideRect(rect, x, y))
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

            if (itemOn != cursor)
            {
                itemOn = cursor;
                S_StartSound(NULL, sfx_pstop);
            }
        }
    }
}

typedef enum
{
    MENU_NULL,
    MENU_UP,
    MENU_DOWN,
    MENU_LEFT,
    MENU_RIGHT,
    MENU_BACKSPACE,
    MENU_ENTER,
    MENU_ESCAPE,
    MENU_CLEAR
} menu_action_t;

static int setup_cancel = -1;

static void M_SetupYesNo(void)
{
    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;

    def->location->i = !def->location->i; // killough 8/15/98

    // killough 8/15/98: add warning messages

    if (flags & (S_LEVWARN | S_PRGWARN))
    {
        warn_about_changes(flags);
    }
    else if (def->current)
    {
        def->current->i = def->location->i;
    }

    if (current_item->action)      // killough 10/98
    {
        current_item->action();
    }
}

static void M_SetupChoice(menu_action_t action)
{
    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;
    int value = def->location->i;

    if (flags & S_ACTION && setup_cancel == -1)
    {
        setup_cancel = value;
    }

    if (action == MENU_LEFT)
    {
        value--;

        if (def->limit.min != UL && value < def->limit.min)
        {
            value = def->limit.min;
        }

        if (def->location->i != value)
        {
            S_StartSound(NULL, sfx_stnmov);
        }
        def->location->i = value;

        if (!(flags & S_ACTION) && current_item->action)
        {
            current_item->action();
        }
    }

    if (action == MENU_RIGHT)
    {
        int max = def->limit.max;

        value++;

        if (max == UL)
        {
            const char **strings = GetStrings(current_item->strings_id);
            if (strings && value == array_size(strings))
                value--;
        }
        else if (value > max)
        {
            value = max;
        }

        if (def->location->i != value)
        {
            S_StartSound(NULL, sfx_stnmov);
        }
        def->location->i = value;

        if (!(flags & S_ACTION) && current_item->action)
        {
            current_item->action();
        }
    }

    if (action == MENU_ENTER)
    {
        if (flags & (S_LEVWARN | S_PRGWARN))
        {
            warn_about_changes(flags);
        }
        else if (def->current)
        {
            def->current->i = def->location->i;
        }

        if (current_item->action)
        {
            current_item->action();
        }
        M_SelectDone(current_item);
        setup_cancel = -1;
    }
}

static boolean M_SetupChangeEntry(menu_action_t action, int ch)
{
    if (!setup_select)
    {
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;

    if (action == MENU_ESCAPE) // Exit key = no change
    {
        if (flags & (S_CHOICE|S_CRITEM|S_THERMO) && setup_cancel != -1)
        {
            def->location->i = setup_cancel;
            setup_cancel = -1;
        }

        M_SelectDone(current_item);                           // phares 4/17/98
        setup_gather = false;   // finished gathering keys, if any
        return true;
    }

    if (flags & (S_YESNO|S_ONOFF)) // yes or no setting?
    {
        if (action == MENU_ENTER)
        {
           M_SetupYesNo();
        }
        M_SelectDone(current_item);        // phares 4/17/98
        return true;
    }

    // [FG] selection of choices

    if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
    {
        M_SetupChoice(action);
        return true;
    }

    if (flags & S_NUM) // number?
    {
        if (!setup_gather) // gathering keys for a value?
        {
            return false;
        }
        // killough 10/98: Allow negatives, and use a more
        // friendly input method (e.g. don't clear value early,
        // allow backspace, and return to original value if bad
        // value is entered).

        if (action == MENU_ENTER)
        {
            if (gather_count)      // Any input?
            {
                int value;

                gather_buffer[gather_count] = 0;
                value = atoi(gather_buffer);  // Integer value

                if ((def->limit.min != UL && value < def->limit.min) ||
                    (def->limit.max != UL && value > def->limit.max))
                {
                    warn_about_changes(S_BADVAL);
                }
                else
                {
                    def->location->i = value;

                    // killough 8/9/98: fix numeric vars
                    // killough 8/15/98: add warning message

                    if (flags & (S_LEVWARN | S_PRGWARN))
                    {
                        warn_about_changes(flags);
                    }
                    else if (def->current)
                    {
                        def->current->i = value;
                    }

                    if (current_item->action)      // killough 10/98
                    {
                        current_item->action();
                    }
                }
            }
            M_SelectDone(current_item);     // phares 4/17/98
            setup_gather = false; // finished gathering keys
            return true;
        }

        if (action == MENU_BACKSPACE && gather_count)
        {
            gather_count--;
            return true;
        }

        if (gather_count >= MAXGATHER)
        {
            return true;
        }

        if (!isdigit(ch) && ch != '-')
        {
            return true; // ignore
        }

        // killough 10/98: character-based numerical input
        gather_buffer[gather_count++] = ch;
        return true;
    }

    return false;
}

static boolean M_SetupBindInput(void)
{
    if (!set_keybnd_active || !setup_select) // on a key binding setup screen
    {                                        // incoming key or button gets bound
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;

    int input_id = (current_item->m_flags & S_INPUT) ? current_item->input_id : 0;

    if (!input_id)
    {
        return true; // not a legal action here (yet)
    }

    // see if the button is already bound elsewhere. if so, you
    // have to swap bindings so the action where it's currently
    // bound doesn't go dead. Since there is more than one
    // keybinding screen, you have to search all of them for
    // any duplicates. You're only interested in the items
    // that belong to the same group as the one you're changing.

    // if you find that you're trying to swap with an action
    // that has S_KEEP set, you can't bind ch; it's already
    // bound to that S_KEEP action, and that action has to
    // keep that key.

    if (M_InputActivated(input_id))
    {
        M_InputRemoveActivated(input_id);
        M_SelectDone(current_item);
        return true;
    }

    boolean search = true;

    for (int i = 0; keys_settings[i] && search; i++)
    {
        for (setup_menu_t *p = keys_settings[i]; !(p->m_flags & S_END); p++)
        {
            if (p->m_group == current_item->m_group &&
                p != current_item &&
                (p->m_flags & (S_INPUT|S_KEEP)) &&
                M_InputActivated(p->input_id))
            {
                if (p->m_flags & S_KEEP)
                {
                    return true; // can't have it!
                }
                M_InputRemoveActivated(p->input_id);
                search = false;
                break;
            }
        }
    }

    if (!M_InputAddActivated(input_id))
    {
        return true;
    }

    M_SelectDone(current_item);       // phares 4/17/98
    return true;
}

static boolean M_SetupNextPage(int inc)
{
    // Some setup screens may have multiple screens.
    // When there are multiple screens, m_prev and m_next items need to
    // be placed on the appropriate screen tables so the user can
    // move among the screens using the left and right arrow keys.
    // The m_var1 field contains a pointer to the appropriate screen
    // to move to.

    if (!current_setup_tabs)
    {
        return false;
    }

    int i = mult_screens_index + inc;

    if (i < 0 || current_setup_tabs[i].text == NULL)
    {
        return false;
    }

    mult_screens_index += inc;

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    current_item->m_flags &= ~S_HILITE;

    M_SetSetupMenuItemOn(set_menu_itemon);
    set_tab_on = mult_screens_index;
    current_setup_menu = current_setup_tabs[set_tab_on].page;
    set_menu_itemon = M_GetSetupMenuItemOn();

    print_warning_about_changes = false; // killough 10/98
    while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
    current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;

    S_StartSound(NULL, sfx_pstop);  // killough 10/98
    return true;

}

static boolean M_SetupResponder(event_t *ev, menu_action_t action, int ch)
{
    // phares 3/26/98 - 4/11/98:
    // Setup screen key processing

    if (!setup_active)
    {
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;

    // phares 4/19/98:
    // Catch the response to the 'reset to default?' verification
    // screen

    if (default_verify)
    {
        if (toupper(ch) == 'Y')
        {
            M_ResetDefaults();
            default_verify = false;
            M_SelectDone(current_item);
        }
        else if (toupper(ch) == 'N')
        {
            default_verify = false;
            M_SelectDone(current_item);
        }
        return true;
    }

    if (M_SetupChangeEntry(action, ch))
    {
        return true;
    }

    if (M_SetupBindInput())
    {
        return true;
    }

    // Weapons

    if (set_weapon_active && // on the weapons setup screen
        setup_select) // changing an entry
    {
        if (action != MENU_ENTER)
        {
            ch -= '0'; // out of ascii
            if (ch < 1 || ch > 9)
            {
                return true; // ignore
            }

            // Plasma and BFG don't exist in shareware
            // killough 10/98: allow it anyway, since this
            // isn't the game itself, just setting preferences

            // see if 'ch' is already assigned elsewhere. if so,
            // you have to swap assignments.

            // killough 11/98: simplified

            for (int i = 0; weap_settings[i]; i++)
            {
                setup_menu_t *p = weap_settings[i];
                for (; !(p->m_flags & S_END); p++)
                {
                    if (p->m_flags & S_WEAP &&
                        p->var.def->location->i == ch &&
                        p != current_item)
                    {
                        p->var.def->location->i = current_item->var.def->location->i;
                        goto end;
                    }
                }
            }
        end:
            current_item->var.def->location->i = ch;
        }

        M_SelectDone(current_item);       // phares 4/17/98
        return true;
    }

    // Not changing any items on the Setup screens. See if we're
    // navigating the Setup menus or selecting an item to change.

    if (action == MENU_DOWN)
    {
        current_item->m_flags &= ~S_HILITE;     // phares 4/17/98
        do
        {
            if (current_item->m_flags & S_END)
            {
                set_menu_itemon = 0;
                current_item = current_setup_menu;
            }
            else
            {
                set_menu_itemon++;
                current_item++;
            }
        } while (current_item->m_flags & S_SKIP);

        M_SelectDone(current_item);         // phares 4/17/98
        return true;
    }

    if (action == MENU_UP)
    {
        current_item->m_flags &= ~S_HILITE;     // phares 4/17/98
        do
        {
            if (set_menu_itemon == 0)
            {
                do
                {
                    set_menu_itemon++;
                    current_item++;
                } while(!(current_item->m_flags & S_END));
            }
            set_menu_itemon--;
            current_item--;
        } while(current_item->m_flags & S_SKIP);

        M_SelectDone(current_item);         // phares 4/17/98
        return true;
    }

    // [FG] clear key bindings with the DEL key
    if (action == MENU_CLEAR)
    {
        int flags = current_item->m_flags;

        if (flags & S_INPUT)
        {
            M_InputReset(current_item->input_id);
        }
        return true;
    }

    if (action == MENU_ENTER)
    {
        int flags = current_item->m_flags;

        // You've selected an item to change. Highlight it, post a new
        // message about what to do, and get ready to process the
        // change.

        if (ItemDisabled(flags))
        {
            S_StartSound(NULL, sfx_oof);
            return true;
        }
        else if (flags & S_NUM)
        {
            setup_gather = true;
            print_warning_about_changes = false;
            gather_count = 0;
        }
        else if (flags & S_RESET)
        {
            default_verify = true;
        }

        current_item->m_flags |= S_SELECT;
        setup_select = true;
        S_StartSound(NULL, sfx_itemup);
        return true;
    }

    if (action == MENU_ESCAPE || action == MENU_BACKSPACE)
    {
        M_SetSetupMenuItemOn(set_menu_itemon);
        M_SetMultScreenIndex(mult_screens_index);

        if (action == MENU_ESCAPE) // Clear all menus
        {
            M_ClearMenus();
        }
        else if (currentMenu->prevMenu) // key_menu_backspace = return to Setup Menu
        {
            currentMenu = currentMenu->prevMenu;
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
        }

        current_item->m_flags &= ~(S_HILITE|S_SELECT);// phares 4/19/98
        setup_active = false;
        set_keybnd_active = false;
        set_weapon_active = false;
        default_verify = false;       // phares 4/19/98
        print_warning_about_changes = false; // [FG] reset
        HU_Start();    // catch any message changes // phares 4/19/98
        S_StartSound(NULL, sfx_swtchx);
        return true;
    }

    if (action == MENU_LEFT)
    {
        if (M_SetupNextPage(-1))
        {
            return true;
        }
    }

    if (action == MENU_RIGHT)
    {
        if (M_SetupNextPage(1))
        {
            return true;
        }
    }

    return false;

} // End of Setup Screen processing

static boolean M_SaveLoadResponder(menu_action_t action, int ch);

static boolean M_MainMenuMouseResponder(void)
{
    if (messageToPrint)
    {
        return false;
    }

    menuitem_t *current_item = &currentMenu->menuitems[itemOn];

    if (current_item->flags & MF_PAGE)
    {
        if (M_InputActivated(input_menu_enter))
        {
            if (savepage == savepage_max)
                savepage = -1;
            M_SaveLoadResponder(MENU_RIGHT, 0);
            return true;
        }
        return false;
    }

    if (current_item->flags & MF_THRM_STR)
    {
        current_item++;
    }

    mrect_t *rect = &current_item->rect;

    if (M_InputActivated(input_menu_enter)
        && !M_PointInsideRect(rect, mouse_state_x, mouse_state_y))
    {
        return true; // eat event
    }

    if (!(current_item->flags & MF_THRM))
    {
        return false;
    }

    static boolean active_thermo;

    if (M_InputActivated(input_menu_enter))
    {
        active_thermo = true;
    }
    else if (M_InputDeactivated(input_menu_enter))
    {
        active_thermo = false;
    }

    if (active_thermo)
    {
        int dot = mouse_state_x - (rect->x + M_THRM_STEP + video.deltaw);
        int step = M_MAX_VOL * FRACUNIT / (rect->w - M_THRM_STEP * 3);
        int value = dot * step / FRACUNIT;
        value = BETWEEN(0, M_MAX_VOL, value);

        current_item--;
        if (current_item->routine)
        {
            current_item->routine(value);
            S_StartSound(NULL, sfx_stnmov);
        }

        return true;
    }

    return false;
}

static boolean M_SetupTab(void)
{
    if (!current_setup_tabs)
    {
        return false;
    }

    setup_tab_t *tab = current_setup_tabs + set_tab_on;

    if (!(M_InputActivated(input_menu_enter) && tab->flags & S_HILITE))
    {
        return false;
    }

    current_setup_menu = tab->page;
    mult_screens_index = set_tab_on;
    set_menu_itemon = 0;
    while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
    set_menu_itemon--;

    S_StartSound(NULL, sfx_pstop);
    return true;
}

static boolean M_MenuMouseResponder(void)
{
    if (!menuactive)
    {
        return false;
    }

    I_ShowMouseCursor(true);

    if (!setup_active)
    {
        if (M_MainMenuMouseResponder())
        {
            return true;
        }

        return false;
    }

    if (M_SetupTab())
    {
        return true;
    }

    static setup_menu_t *active_thermo = NULL;

    if (M_InputDeactivated(input_menu_enter) && active_thermo)
    {
        int flags = active_thermo->m_flags;
        default_t *def = active_thermo->var.def;

        if (flags & S_ACTION)
        {
            if (flags & (S_LEVWARN | S_PRGWARN))
            {
                warn_about_changes(flags);
            }
            else if (def->current)
            {
                def->current->i = def->location->i;
            }

            if (active_thermo->action)
            {
                active_thermo->action();
            }
        }
        active_thermo = NULL;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;
    mrect_t *rect = &current_item->rect;

    if (ItemDisabled(flags))
    {
        return false;
    }

    if (M_InputActivated(input_menu_enter)
        && !M_PointInsideRect(rect, mouse_state_x, mouse_state_y))
    {
        return true; // eat event
    }

    if (flags & S_THERMO)
    {
        if (active_thermo && active_thermo != current_item)
        {
            active_thermo = NULL;
        }

        if (M_InputActivated(input_menu_enter))
        {
            active_thermo = current_item;
        }
    }

    if (flags & S_THERMO && active_thermo)
    {
        int dot = mouse_state_x - (rect->x + M_THRM_STEP + video.deltaw);

        int min = def->limit.min;
        int max = def->limit.max;

        if (max == UL)
        {
            const char **strings = GetStrings(active_thermo->strings_id);
            if (strings)
                max = array_size(strings) - 1;
            else
                max = M_THRM_UL_VAL;
        }

        int step = (max - min) * FRACUNIT / (rect->w - M_THRM_STEP * 2);
        int value = dot * step / FRACUNIT + min;
        value = BETWEEN(min, max, value);

        if (value != def->location->i)
        {
            def->location->i = value;

            if (!(flags & S_ACTION) && active_thermo->action)
            {
                active_thermo->action();
            }
            S_StartSound(NULL, sfx_stnmov);
        }
        return true;
    }

    if (!M_InputActivated(input_menu_enter))
    {
        return false;
    }

    if (flags & (S_YESNO|S_ONOFF)) // yes or no setting?
    {
        M_SetupYesNo();
        S_StartSound(NULL, sfx_itemup);
        return true;
    }

    if (flags & (S_CRITEM|S_CHOICE))
    {
        default_t *def = current_item->var.def;

        int value = def->location->i;

        if (NextItemAvailable(current_item))
        {
            value++;
        }
        else if (def->limit.min != UL)
        {
            value = def->limit.min;
        }

        if (def->location->i != value)
        {
            S_StartSound(NULL, sfx_stnmov);
        }
        def->location->i = value;

        if (current_item->action)
        {
            current_item->action();
        }

        if (flags & (S_LEVWARN | S_PRGWARN))
        {
            warn_about_changes(flags);
        }
        else if (def->current)
        {
            def->current->i = def->location->i;
        }

        return true;
    }

    return false;
}

static boolean M_SaveLoadResponder(menu_action_t action, int ch)
{
    if (currentMenu != &LoadDef && currentMenu != &SaveDef)
    {
        return false;
    }

    // [FG] delete a savegame

    if (delete_verify)
    {
        if (toupper(ch) == 'Y')
        {
            M_DeleteGame(itemOn);
            S_StartSound(NULL, sfx_itemup);
            delete_verify = false;
        }
        else if (toupper(ch) == 'N')
        {
            S_StartSound(NULL, sfx_itemup);
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
            M_ReadSaveStrings();
            S_StartSound(NULL, sfx_pstop);
        }
        return true;
    }
    else if (action == MENU_RIGHT)
    {
        if (savepage < savepage_max)
        {
            savepage++;
            quickSaveSlot = -1;
            M_ReadSaveStrings();
            S_StartSound(NULL, sfx_pstop);
        }
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
// M_Responder
//
// Examines incoming keystrokes and button pushes and determines some
// action based on the state of the system.
//

boolean M_Responder (event_t* ev)
{
    int    ch;
    static int joywait = 0;

    static menu_action_t repeat = MENU_NULL;
    menu_action_t action = MENU_NULL;

    ch = 0; // will be changed to a legit char if we're going to use it here

    switch (ev->type)
    {
        // "close" button pressed on window?
        case ev_quit:
            // First click on close button = bring up quit confirm message.
            // Second click on close button = confirm quit
            if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
            {
                M_QuitResponse('y');
            }
            else
            {
                S_StartSound(NULL,sfx_swtchn);
                M_QuitDOOM(0);
            }
            return true;

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
            menu_input = key_mode;
            ch = ev->data1;
            if (ch == KEY_RSHIFT)
            {
                shiftdown = true;
            }
            break;

        case ev_keyup:
            if (ev->data1 == KEY_RSHIFT)
            {
                shiftdown = false;
            }
            return false;

        case ev_mouseb_down:
            menu_input = mouse_mode;
            if (M_MenuMouseResponder())
            {
                return true;
            }
            break;

        case ev_mouseb_up:
            return M_MenuMouseResponder();

        case ev_mouse_state:
            if (ev->data1 && menu_input != mouse_mode)
            {
                return true;
            }
            menu_input = mouse_mode;
            mouse_state_x = ev->data2;
            mouse_state_y = ev->data3;
            M_MenuMouseCursorPosition(mouse_state_x, mouse_state_y);
            M_MenuMouseResponder();
            return true;

        default:
            return false;
    }

    if (menuactive)
    {
        if (menu_input == mouse_mode)
        {
            I_ShowMouseCursor(true);
        }
        else
        {
            if (current_setup_tabs)
            {
                current_setup_tabs[set_tab_on].flags &= ~S_HILITE;
            }
            I_ShowMouseCursor(false);
        }
    }

    if (M_InputActivated(input_menu_up))
        action = MENU_UP;
    else if (M_InputActivated(input_menu_down))
        action = MENU_DOWN;
    else if (M_InputActivated(input_menu_left))
        action = MENU_LEFT;
    else if (M_InputActivated(input_menu_right))
        action = MENU_RIGHT;
    else if (M_InputActivated(input_menu_backspace))
        action = MENU_BACKSPACE;
    else if (M_InputActivated(input_menu_enter))
        action = MENU_ENTER;
    else if (M_InputActivated(input_menu_escape))
        action = MENU_ESCAPE;
    else if (M_InputActivated(input_menu_clear))
        action = MENU_CLEAR;

    if (ev->type == ev_joyb_down && action >= MENU_UP && action <= MENU_RIGHT)
    {
        repeat = action;
        joywait = I_GetTime() + 15;
    }

    // Save Game string input

    if (saveStringEnter)
    {
        if (action == MENU_BACKSPACE)                      // phares 3/7/98
        {
            if (saveCharIndex > 0)
            {
                if (StartsWithMapIdentifier(savegamestrings[saveSlot]) &&
                    saveStringEnter == 1)
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
        else if (action == MENU_ESCAPE)                    // phares 3/7/98
        {
            saveStringEnter = 0;
            strcpy(&savegamestrings[saveSlot][0], saveOldString);
        }
        else if (action == MENU_ENTER)                     // phares 3/7/98
        {
            saveStringEnter = 0;
            if (savegamestrings[saveSlot][0])
            {
                M_DoSave(saveSlot);
            }
        }
        else
        {
            ch = toupper(ch);

            if (ch >= 32 && ch <= 127 &&
                saveCharIndex < SAVESTRINGSIZE - 1 &&
                M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE - 2) * 8)
            {
                savegamestrings[saveSlot][saveCharIndex++] = ch;
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

        if (messageNeedsInput == true &&
            !(ch == ' ' || ch == 'n' || ch == 'y' || ch == key_escape ||
            action == MENU_BACKSPACE)) // phares
        {
            return false;
        }

        menuactive = messageLastMenuActive;
        messageToPrint = 0;
        if (messageRoutine)
        {
            messageRoutine(ch);
        }

        menuactive = false;
        S_StartSound(NULL, sfx_swtchx);
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

    if (M_ShortcutResponder(ev))
    {
        return true;
    }

    // Pop-up Main menu?

    if (!menuactive)
    {
        if (action == MENU_ESCAPE)                                     // phares
        {
            M_StartControlPanel ();
            S_StartSound(NULL,sfx_swtchn);
            return true;
        }
        return false;
    }

    if (M_SaveLoadResponder(action, ch))
    {
        return true;
    }

    if (M_SetupResponder(ev, action, ch))
    {
        return true;
    }

    // From here on, these navigation keys are used on the BIG FONT menus
    // like the Main Menu.

    if (action == MENU_DOWN)                             // phares 3/7/98
    {
        currentMenu->menuitems[itemOn].flags &= ~MF_HILITE;
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
            S_StartSound(NULL, sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }

    if (action == MENU_UP)                               // phares 3/7/98
    {
        currentMenu->menuitems[itemOn].flags &= ~MF_HILITE;
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
            S_StartSound(NULL, sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }

    if (action == MENU_LEFT)                             // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(CHOICE_LEFT);
        }
        return true;
    }

    if (action == MENU_RIGHT)                            // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(CHOICE_RIGHT);
        }
        return true;
    }

    if (action == MENU_ENTER)                            // phares 3/7/98
    {
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;
            currentMenu->menuitems[itemOn].flags &= ~MF_HILITE;
            if (currentMenu->menuitems[itemOn].status == 2)
            {
                currentMenu->menuitems[itemOn].routine(CHOICE_RIGHT);
                S_StartSound(NULL, sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                S_StartSound(NULL, sfx_pistol);
            }
        }
        else
        {
            S_StartSound(NULL, sfx_oof); // [FG] disabled menu item
        }
        //jff 3/24/98 remember last skill selected
        // killough 10/98 moved to skill-specific functions
        return true;
    }

    if (action == MENU_ESCAPE)                           // phares 3/7/98  
    {
        if (!(currentMenu->menuitems[itemOn].flags & MF_PAGE))
            currentMenu->lastOn = itemOn;
        M_ClearMenus();
        S_StartSound(NULL, sfx_swtchx);
        return true;
    }

    if (action == MENU_BACKSPACE)                        // phares 3/7/98
    {
        menuitem_t *current_item = &currentMenu->menuitems[itemOn];
        if (!(current_item->flags & MF_PAGE))
            currentMenu->lastOn = itemOn;
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
            S_StartSound(NULL, sfx_swtchn);
        }
        return true;
    }

    // [FG] delete a savegame

    if (action == MENU_CLEAR)
    {
        if (currentMenu == &LoadDef || currentMenu == &SaveDef)
        {
            if (LoadMenu[itemOn].status)
            {
                S_StartSound(NULL, sfx_itemup);
                currentMenu->lastOn = itemOn;
                delete_verify = true;
                return true;
            }
            else
            {
                S_StartSound(NULL, sfx_oof);
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
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }

        for (i = 0; i <= itemOn; i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
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

void M_StartControlPanel (void)
{
  // intro might call this repeatedly

  if (menuactive)
    return;

  //jff 3/24/98 make default skill menu choice follow -skill or defaultskill
  //from command line or config file
  //
  // killough 10/98:
  // Fix to make "always floating" with menu selections, and to always follow
  // defaultskill, instead of -skill.

  NewDef.lastOn = defaultskill - 1;
  
  default_verify = 0;                  // killough 10/98
  menuactive = 1;
  currentMenu = &MainDef;         // JDC
  itemOn = currentMenu->lastOn;   // JDC
  print_warning_about_changes = false;   // killough 11/98

  G_ClearInput();
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
// killough 9/29/98: Significantly reformatted source
//

boolean M_MenuIsShaded(void)
{
  return options_active && menu_backdrop == MENU_BG_DARK;
}

void M_Drawer (void)
{
    inhelpscreens = false;

    // Horiz. & Vertically center string and print it.
    // killough 9/29/98: simplified code, removed 40-character width limit
    if(messageToPrint)
    {
      // haleyjd 11/11/04: must strdup message, cannot write into
      // string constants!
      char *d = strdup(messageString);
      char *p;
      int y = 100 - M_StringHeight(messageString) / 2;

      p = d;

      while(*p)
      {
        char *string = p, c;
        while((c = *p) && *p != '\n')
          p++;
        *p = 0;
        M_WriteText(160 - M_StringWidth(string)/2, y, string);
        y += SHORT(hu_font[0]->height);
        if ((*p = c))
          p++;
      }

      // haleyjd 11/11/04: free duplicate string
      free(d);
      return;
    }

    if (!menuactive)
    {
        return;
    }

    if (M_MenuIsShaded())
    {
        inhelpscreens = true;
        V_ShadeScreen();
    }

    if (currentMenu->routine)
    {
        currentMenu->routine();     // call Draw routine
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
            const char *name = currentMenu->menuitems[i].name;

            if ((name[0] == 0 || W_CheckNumForName(name) < 0) &&
                currentMenu->menuitems[i].alttext)
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
        mrect_t *rect = &item->rect;

        const char *alttext = item->alttext;
        const char *name = item->name;

        byte *cr;
        if (item->status == 0)
            cr = cr_dark;
        else if (item->flags & MF_HILITE)
            cr = cr_bright;
        else
            cr = NULL;

        // [FG] at least one menu graphics lump is missing, draw alternative text
        if (currentMenu->lumps_missing > 0)
        {
            if (alttext)
            {
                rect->x = 0;
                rect->y = y;
                rect->w = SCREENWIDTH;
                rect->h = LINEHEIGHT;
                M_DrawStringCR(x, y + 8 - (M_StringHeight(alttext) / 2),
                               cr, NULL, alttext);
            }
        }
        else if (name[0])
        {
            patch_t *patch = W_CacheLumpName(name, PU_CACHE);
            rect->x = 0;
            rect->y = y - SHORT(patch->topoffset);
            rect->w = SCREENWIDTH;
            rect->h = SHORT(patch->height);
            V_DrawPatchTranslated(x, y, patch, cr);
        }

        y += LINEHEIGHT;
    }

    // DRAW SKULL

    y = setup_active ? SCREENHEIGHT - 19 : currentMenu->y;

    V_DrawPatchDirect(x + SKULLXOFF, y - 5 + itemOn * LINEHEIGHT,
                      W_CacheLumpName(skullName[whichSkull], PU_CACHE));

    if (delete_verify)
    {
        M_DrawDelVerify();
    }
}

//
// M_ClearMenus
//
// Called when leaving the menu screens for the real world

static void M_ClearMenus(void)
{
  menuactive = 0;
  options_active = false;
  print_warning_about_changes = 0;     // killough 8/15/98
  default_verify = 0;                  // killough 10/98

  // if (!netgame && usergame && paused)
  //     sendpause = true;

  G_ClearInput();
  I_ResetRelativeMouseState();
}

//
// M_SetupNextMenu
//
static void M_SetupNextMenu(menu_t *menudef)
{
  currentMenu = menudef;
  itemOn = currentMenu->lastOn;
}

/////////////////////////////
//
// M_Ticker
//
void M_Ticker (void)
{
  if (--skullAnimCounter <= 0)
    {
      whichSkull ^= 1;
      skullAnimCounter = 8;
    }
}

/////////////////////////////
//
// Message Routines
//

static void M_StartMessage(char *string,void (*routine)(int),boolean input)
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
  int  i;
  char num[4];

  xx = x;
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERML",PU_CACHE), cr);
  xx += 8;
  for (i=0;i<thermWidth;i++)
    {
      V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERMM",PU_CACHE), cr);
      xx += 8;
    }
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERMR",PU_CACHE), cr);

  // [FG] write numerical values next to thermometer
  M_snprintf(num, 4, "%3d", thermDot);
  M_WriteText(xx + 8, y + 3, num);

  // [FG] do not crash anymore if value exceeds thermometer range
  if (thermDot >= thermWidth)
      thermDot = thermWidth - 1;

  V_DrawPatchTranslated((x + 8) + thermDot * 8, y,
                        W_CacheLumpName("M_THERMO",PU_CACHE), cr);
}

/////////////////////////////
//
// String-drawing Routines
//

//
// Find string width from hu_font chars
//

static int M_StringWidth(const char *string)
{
  int c, w = 0;

  while (*string)
  {
    c = *string++;
    if (c == '\x1b') // skip code for color change
    {
      if (*string)
        string++;
      continue;
    }
    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c > HU_FONTSIZE)
    {
      w += SPACEWIDTH;
      continue;
    }
    w += SHORT(hu_font[c]->width);
  }

  return w;
}

//
//    Find string height from hu_font chars
//

static int M_StringHeight(const char *string)
{
  return SHORT(hu_font[0]->height);
}

//
//    Write a string using the hu_font
//
static void M_WriteText (int x,int y,const char *string)
{
  int   w;
  const char *ch;
  int   c;
  int   cx;
  int   cy;
  
  ch = string;
  cx = x;
  cy = y;
  
  while(1)
    {
      c = *ch++;
      if (!c)
	break;
      if (c == '\n')
	{
	  cx = x;
	  cy += 12;
	  continue;
	}
  
      c = toupper(c) - HU_FONTSTART;
      if (c < 0 || c>= HU_FONTSIZE)
	{
	  cx += 4;
	  continue;
	}
  
      w = SHORT (hu_font[c]->width);
      if (cx+w > SCREENWIDTH)
	break;
      V_DrawPatchDirect(cx, cy, hu_font[c]);
      cx+=w;
    }
}

// [FG] alternative text for missing menu graphics lumps

static void M_DrawTitle(int x, int y, const char *patch, const char *alttext)
{
  if (W_CheckNumForName(patch) >= 0)
    V_DrawPatchDirect(x,y,W_CacheLumpName(patch,PU_CACHE));
  else
  {
    // patch doesn't exist, draw some text in place of it
    M_snprintf(menu_buffer, sizeof(menu_buffer), "%s", alttext);
    M_DrawMenuString(SCREENWIDTH/2 - M_StringWidth(alttext)/2,
                     y + 8 - M_StringHeight(alttext)/2, // assumes patch height 16
                     CR_TITLE);
  }
}

/////////////////////////////
//
// Initialization Routines to take care of one-time setup
//

static const char **selectstrings[] = {
    NULL, // str_empty
    layout_strings,
    curve_strings,
    center_weapon_strings,
    bobfactor_strings,
    screensize_strings,
    hudtype_strings,
    NULL, // str_hudmode
    show_widgets_strings,
    crosshair_strings,
    crosshair_target_strings,
    hudcolor_strings,
    overlay_strings,
    automap_preset_strings,
    NULL, // str_resolution_scale
    NULL, // str_midi_player
    gamma_strings,
    sound_module_strings,
    NULL, // str_mouse_accel
    default_skill_strings,
    default_complevel_strings,
    endoom_strings,
    death_use_action_strings,
    menu_backdrop_strings,
    widescreen_strings,
};

static const char **GetStrings(int id)
{
    if (id > str_empty && id < arrlen(selectstrings))
    {
        return selectstrings[id];
    }

    return NULL;
}

static void M_UpdateHUDModeStrings(void)
{
    selectstrings[str_hudmode] = M_GetHUDModeStrings();
}

void M_InitMenuStrings(void)
{
    M_UpdateHUDModeStrings();
    selectstrings[str_resolution_scale] = M_GetResolutionScaleStrings();
    selectstrings[str_midi_player] = M_GetMidiDevicesStrings();
    selectstrings[str_mouse_accel] = M_GetMouseAccelStrings();
}

//
// M_Init
//
void M_Init(void)
{
  M_InitDefaults();                // killough 11/98
  currentMenu = &MainDef;
  menuactive = 0;
  itemOn = currentMenu->lastOn;
  whichSkull = 0;
  skullAnimCounter = 10;
  saved_screenblocks = screenblocks;
  screenSize = screenblocks - 3;
  messageToPrint = 0;
  messageString = NULL;
  messageLastMenuActive = menuactive;
  quickSaveSlot = -1;

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
      HelpDef.y = 165;
      ReadMenu1[0].routine = M_FinishReadThis;
  }

  // Versions of doom.exe before the Ultimate Doom release only had
  // three episodes; if we're emulating one of those then don't try
  // to show episode four. If we are, then do show episode four
  // (should crash if missing).
  if (gameversion < exe_ultimate)
  {
      EpiDef.numitems--;
  }
  
  M_ResetMenu();        // killough 10/98
  M_ResetSetupMenu();
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
  #if defined(WIN_LAUNCHER)
    string = M_StringReplace(replace, "prompt", "console");
  #else
    string = M_StringReplace(replace, "prompt", "desktop");
  #endif
#else
    if (isatty(STDOUT_FILENO))
      string = M_StringReplace(replace, "prompt", "shell");
    else
      string = M_StringReplace(replace, "prompt", "desktop");
#endif
    free(replace);
    *endmsg[9] = string;
  }
}

void M_SetMenuFontSpacing(void)
{
  if (M_StringWidth("abcdefghijklmnopqrstuvwxyz01234") > 230)
    menu_font_spacing = -1;
}

// killough 10/98: allow runtime changing of menu order

void M_ResetMenu(void)
{
  // killough 4/17/98:
  // Doom traditional menu, for arch-conservatives like yours truly

  while ((traditional_menu ? M_SaveGame : M_Setup)
	 != MainMenu[options].routine)
    {
      menuitem_t t       = MainMenu[loadgame];
      MainMenu[loadgame] = MainMenu[options];
      MainMenu[options]  = MainMenu[savegame];
      MainMenu[savegame] = t;
    }
}

void M_ResetSetupMenu(void)
{
  extern boolean deh_set_blood_color;

  if (M_ParmExists("-strict"))
  {
    comp_settings1[comp1_strictmode].m_flags |= S_DISABLE;
  }

  if (force_complevel)
  {
    comp_settings1[comp1_complevel].m_flags |= S_DISABLE;
  }

  if (M_ParmExists("-pistolstart"))
  {
    comp_settings1[comp1_pistolstart].m_flags |= S_DISABLE;
  }

  if (M_ParmExists("-uncapped") || M_ParmExists("-nouncapped"))
  {
    gen_settings1[gen1_uncapped].m_flags |= S_DISABLE;
  }

  if (deh_set_blood_color)
  {
    enem_settings1[enem1_colored_blood].m_flags |= S_DISABLE;
  }

  if (!brightmaps_found || force_brightmaps)
  {
    gen_settings5[gen5_brightmaps].m_flags |= S_DISABLE;
  }

  if (current_video_height <= DRS_MIN_HEIGHT)
  {
    gen_settings1[gen1_dynamic_resolution].m_flags |= S_DISABLE;
  }

  M_CoerceFPSLimit();
  M_UpdateCrosshairItems();
  M_UpdateCenteredWeaponItem();
  M_UpdateAdvancedSoundItems();
}

void M_ResetSetupMenuVideo(void)
{
  M_ToggleUncapped();
}

//
// End of General Routines
//
/////////////////////////////////////////////////////////////////////////////

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
// Recoil, Bobbing, Monsters Remember changes in Setup now take effect immediately
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
