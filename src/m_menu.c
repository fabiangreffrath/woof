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

#include <fcntl.h>
#include "m_io.h" // haleyjd

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h" // [FG] inline
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_main.h"
#include "hu_obituary.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "s_sound.h"
#include "sounds.h"
#include "m_menu.h"
#include "d_deh.h"
#include "m_misc.h"
#include "m_misc2.h" // [FG] M_StringDuplicate()
#include "m_swap.h"
#include "w_wad.h" // [FG] W_IsIWADLump() / W_WadNameForLump()
#include "p_saveg.h" // saveg_compat
#include "m_input.h"
#include "r_draw.h" // [FG] R_SetFuzzColumnMode
#include "r_sky.h" // [FG] R_InitSkyMap()
#include "r_plane.h" // [FG] R_InitPlanes()
#include "m_argv.h"
#include "m_snapshot.h"
#include "i_sound.h"
#include "r_bmaps.h"

// [crispy] remove DOS reference from the game quit confirmation dialogs
#include "SDL_platform.h"
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

int hide_setup=1; // killough 5/15/98

// Blocky mode, has default, 0 = high, 1 = normal
//int     detailLevel;    obsolete -- killough
int screenblocks;    // has default

int screenSize;      // temp for screenblocks (0-9)    

static int quickSaveSlot;   // -1 = no quicksave slot picked!

int messageToPrint;  // 1 = message to be printed

char *messageString; // ...and here is the message string!

// message x & y
int     messx;      
int     messy;
int     messageLastMenuActive;

boolean messageNeedsInput; // timed message = no input from user     

void (*messageRoutine)(int response);

#define SAVESTRINGSIZE  24

// killough 8/15/98: when changes are allowed to sync-critical variables
static int allow_changes(void)
{
 return !(demoplayback || demorecording || netgame);
}

int warning_about_changes, print_warning_about_changes;

// we are going to be entering a savegame string

static int saveStringEnter;
int saveSlot;        // which slot to save in
int saveCharIndex;   // which char we're editing
// old save description before edit
char saveOldString[SAVESTRINGSIZE];  

boolean inhelpscreens; // indicates we are in or just left a help screen

boolean menuactive;    // The menus are up

background_t menu_background;

#define SKULLXOFF  -32
#define LINEHEIGHT  16

#define M_SPC        9
#define M_X          240
#define M_Y          (29 + M_SPC)
#define M_X_PREV     (57)
#define M_X_NEXT     (310)
#define M_Y_PREVNEXT (29 + 18 * M_SPC)
#define M_Y_WARN     (M_Y_PREVNEXT - M_SPC)
#define M_THRM_SIZE  10
#define M_THRM_STEP  8
#define M_THRM_WIDTH (M_THRM_STEP * (M_THRM_SIZE + 2))
#define M_X_THRM     (M_X - M_THRM_WIDTH)
#define M_X_LOADSAVE 80
#define M_LOADSAVE_WIDTH (24 * 8 + 8) // [FG] c.f. M_DrawSaveLoadBorder()

#define DISABLE_ITEM(condition, item) \
        ((condition) ? (item.m_flags |= S_DISABLE) : (item.m_flags &= ~S_DISABLE))

char savegamestrings[10][SAVESTRINGSIZE];

// [FG] support up to 8 pages of savegames
int savepage = 0;
static const int savepage_max = 7;

//
// MENU TYPEDEFS
//

typedef struct
{
  short status; // 0 = no cursor here, 1 = ok, 2 = arrows ok
  char  name[10];
    
  // choice = menu item #.
  // if status = 2,
  //   choice=0:leftarrow,1:rightarrow
  void  (*routine)(int choice);
  char  alphaKey; // hotkey in menu     
  const char *alttext; // [FG] alternative text for missing menu graphics lumps
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

short itemOn;           // menu item skull is on (for Big Font menus)
short skullAnimCounter; // skull animation counter
short whichSkull;       // which skull to draw (he blinks)

// graphic name of skulls

char skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

menu_t SaveDef, LoadDef;
menu_t* currentMenu; // current menudef                          

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
extern int sts_always_red;// status numbers do not change colors
extern int sts_pct_always_gray;// status percents do not change colors
extern int sts_traditional_keys;  // display keys the traditional way
extern int mapcolor_back; // map background
extern int mapcolor_grid; // grid lines color
extern int mapcolor_wall; // normal 1s wall color
extern int mapcolor_fchg; // line at floor height change color
extern int mapcolor_cchg; // line at ceiling height change color
extern int mapcolor_clsd; // line at sector with floor=ceiling color
extern int mapcolor_rkey; // red key color
extern int mapcolor_bkey; // blue key color
extern int mapcolor_ykey; // yellow key color
extern int mapcolor_rdor; // red door color  (diff from keys to allow option)
extern int mapcolor_bdor; // blue door color (of enabling one but not other )
extern int mapcolor_ydor; // yellow door color
extern int mapcolor_tele; // teleporter line color
extern int mapcolor_secr; // secret sector boundary color
extern int mapcolor_exit; // jff 4/23/98 exit line
extern int mapcolor_unsn; // computer map unseen line color
extern int mapcolor_flat; // line with no floor/ceiling changes
extern int mapcolor_sprt; // general sprite color
extern int mapcolor_hair; // crosshair color
extern int mapcolor_sngl; // single player arrow color
extern int mapcolor_plyr[4];// colors for player arrows in multiplayer

extern int mapcolor_frnd;  // friends colors  // killough 8/8/98

extern int map_point_coordinates; // killough 10/98

extern char *chat_macros[];  // chat macros
extern const char shiftxform[];
extern int map_secret_after; //secrets do not appear til after bagged
extern default_t defaults[];

// end of externs added for setup menus

//
// PROTOTYPES
//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_ChangeMessages(int choice);
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_MusicVol(int choice);
/* void M_ChangeDetail(int choice);  unused -- killough */
void M_SizeDisplay(int choice);
void M_StartGame(int choice);
void M_Sound(int choice);

void M_Mouse(int choice, int *sens);      /* killough */
void M_MouseVert(int choice);
void M_MouseHoriz(int choice);
void M_MouseVert2(int choice);
void M_MouseHoriz2(int choice);
void M_DrawMouse(void);

void M_FinishReadThis(int choice);
void M_FinishHelp(int choice);            // killough 10/98
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);
void M_DrawSetup(void);                                     // phares 3/21/98
void M_DrawHelp (void);                                     // phares 5/04/98

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
void M_WriteText(int x, int y, const char *string);
int  M_StringWidth(const char *string);
int  M_StringHeight(const char *string);
void M_StartMessage(char *string,void (*routine)(int),boolean input);
void M_StopMessage(void);
void M_ClearMenus (void);

// phares 3/30/98
// prototypes added to support Setup Menus and Extended HELP screens

int  M_GetKeyString(int,int);
void M_Setup(int choice);                               
void M_KeyBindings(int choice);                        
void M_Weapons(int);
void M_StatusBar(int);
void M_Automap(int);
void M_Enemy(int);
void M_Messages(int);
void M_ChatStrings(int);
void M_InitExtendedHelp(void);
void M_ExtHelpNextScreen(int);
void M_ExtHelp(int);
int  M_GetPixelWidth(const char*);
void M_DrawKeybnd(void);
void M_DrawWeapons(void);
void M_DrawMenuString(int,int,int);                    
void M_DrawStatusHUD(void);
void M_DrawExtHelp(void);
void M_DrawAutoMap(void);
void M_DrawEnemy(void);
void M_DrawMessages(void);
void M_DrawChatStrings(void);
void M_Compat(int);       // killough 10/98
void M_General(int);      // killough 10/98
void M_DrawCompat(void);  // killough 10/98
void M_DrawGeneral(void); // killough 10/98
// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it is not tied to menu_buffer
void M_DrawString(int,int,int,const char*);

// [FG] alternative text for missing menu graphics lumps
void M_DrawTitle(int x, int y, const char *patch, const char *alttext);

menu_t NewDef;                                              // phares 5/04/98

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

menuitem_t MainMenu[]=
{
  {1,"M_NGAME", M_NewGame, 'n'},
  {1,"M_LOADG", M_LoadGame,'l'},
  {1,"M_SAVEG", M_SaveGame,'s'},
  {1,"M_OPTION",M_Options, 'o'},
  // Another hickup with Special edition.
  {1,"M_RDTHIS",M_ReadThis,'r'},
  {1,"M_QUITG", M_QuitDOOM,'q'}
};

menu_t MainDef =
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

void M_DrawMainMenu(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  V_DrawPatchDirect (94,2,0,W_CacheLumpName("M_DOOM",PU_CACHE));
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

menuitem_t ReadMenu1[] =
{
  {1,"",M_ReadThis2,0}
};

menuitem_t ReadMenu2[]=
{
  {1,"",M_FinishReadThis,0}
};

menuitem_t HelpMenu[]=    // killough 10/98
{
  {1,"",M_FinishHelp,0}
};

menu_t ReadDef1 =
{
  read1_end,
  &MainDef,
  ReadMenu1,
  M_DrawReadThis1,
  330,175,
  //280,185,              // killough 2/21/98: fix help screens
  0
};

menu_t ReadDef2 =
{
  read2_end,
  &ReadDef1,
  ReadMenu2,
  M_DrawReadThis2,
  330,175,
  0
};

menu_t HelpDef =           // killough 10/98
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

void M_ReadThis(int choice)
{
  M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
  M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
  M_SetupNextMenu(&MainDef);
}

void M_FinishHelp(int choice)        // killough 10/98
{
  M_SetupNextMenu(&MainDef);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
// killough 10/98: updated with new screens

void M_DrawReadThis1(void)
{
  inhelpscreens = true;
  if (gamemode == shareware)
    V_DrawPatchFullScreen (0,W_CacheLumpName("HELP2",PU_CACHE));
  else
    M_DrawCredits();
}

//
// Read This Menus - optional second page.
//
// killough 10/98: updated with new screens

void M_DrawReadThis2(void)
{
  inhelpscreens = true;
  if (gamemode == shareware)
    M_DrawCredits();
  else
    V_DrawPatchFullScreen (0,W_CacheLumpName("CREDIT",PU_CACHE));
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

menuitem_t EpisodeMenu[]=   // added a few free entries for UMAPINFO
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

menu_t EpiDef =
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
short EpiMenuMap[8] = { 1, 1, 1, 1, -1, -1, -1, -1 }, EpiMenuEpi[8] = { 1, 2, 3, 4, -1, -1, -1, -1 };

//
//    M_Episode
//
int epiChoice;

void M_ClearEpisodes(void)
{
  EpiDef.numitems = 0;
  NewDef.prevMenu = &MainDef;
}

void M_AddEpisode(const char *map, const char *gfx, const char *txt, const char *alpha)
{
  if (!EpiCustom)
  {
      EpiCustom = true;
      NewDef.prevMenu = &EpiDef;

      if (gamemode == commercial)
          EpiDef.numitems = 0;
  }

  {
    int epi, mapnum;
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
  }
  if (EpiDef.numitems <= 4)
  {
    EpiDef.y = 63;
  }
  else
  {
    EpiDef.y = 63 - (EpiDef.numitems - 4) * (LINEHEIGHT / 2);
  }
}


void M_DrawEpisode(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  M_DrawTitle(54,EpiDef.y - 25,"M_EPISOD","WHICH EPISODE?");
}

void M_Episode(int choice)
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

menuitem_t NewGameMenu[]=
{
  {1,"M_JKILL", M_ChooseSkill, 'i', "I'm too young to die."},
  {1,"M_ROUGH", M_ChooseSkill, 'h', "Hey, not too rough."},
  {1,"M_HURT",  M_ChooseSkill, 'h', "Hurt me plenty."},
  {1,"M_ULTRA", M_ChooseSkill, 'u', "Ultra-Violence."},
  {1,"M_NMARE", M_ChooseSkill, 'n', "Nightmare!"}
};

menu_t NewDef =
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

void M_DrawNewGame(void)
{
  // [crispy] force status bar refresh
  inhelpscreens = true;

  V_DrawPatchDirect (96,14,0,W_CacheLumpName("M_NEWG",PU_CACHE));
  V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_SKILL",PU_CACHE));
}

void M_NewGame(int choice)
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

void M_VerifyNightmare(int ch)
{
  if (ch != 'y')
    return;

  G_DeferedInitNew(nightmare,epiChoice+1,1);
  M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
  if (choice == nightmare)
    {   // Ty 03/27/98 - externalized
      M_StartMessage(s_NIGHTMARE,M_VerifyNightmare,true);
      return;
    }

  if (!EpiCustom)
  G_DeferedInitNew(choice,epiChoice+1,1);
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
  load_end
} load_e;

// The definitions of the Load Game screen

menuitem_t LoadMenu[]=
{
  {1,"", M_LoadSelect,'1'},
  {1,"", M_LoadSelect,'2'},
  {1,"", M_LoadSelect,'3'},
  {1,"", M_LoadSelect,'4'},
  {1,"", M_LoadSelect,'5'},
  {1,"", M_LoadSelect,'6'},
  {1,"", M_LoadSelect,'7'}, //jff 3/15/98 extend number of slots
  {1,"", M_LoadSelect,'8'},
};

menu_t LoadDef =
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

  const int snapshot_x = MAX((WIDESCREENDELTA + SaveDef.x + SKULLXOFF - snapshot_width) / 2, 8);
  const int snapshot_y = LoadDef.y + MAX((load_end * LINEHEIGHT - snapshot_height) * n / load_end, 0);

  // [FG] a snapshot window smaller than 80*48 px is considered too small
  if (snapshot_width < ORIGWIDTH/4)
    return;

  if (!M_DrawSnapshot(n, snapshot_x, snapshot_y, snapshot_width, snapshot_height))
  {
    M_WriteText(snapshot_x + snapshot_width/2 - M_StringWidth(txt)/2 - WIDESCREENDELTA,
                snapshot_y + snapshot_height/2 - M_StringHeight(txt)/2,
                txt);
  }

  txt = M_GetSavegameTime(n);
  M_DrawString(snapshot_x + snapshot_width/2 - M_GetPixelWidth(txt)/2 - WIDESCREENDELTA,
               snapshot_y + snapshot_height + M_StringHeight(txt),
               CR_GOLD, txt);

  // [FG] draw the view border around the snapshot
  R_DrawBorder(snapshot_x, snapshot_y, snapshot_width, snapshot_height, 0);
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
void M_DrawSaveLoadBottomLine(void)
{
  char pagestr[16];
  const int y = LoadDef.y+LINEHEIGHT*load_end;

  // [crispy] force status bar refresh
  inhelpscreens = true;

  M_DrawSaveLoadBorder(LoadDef.x,y);

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

void M_DrawLoad(void)
{
  int i;

  //jff 3/15/98 use symbolic load position
  V_DrawPatchDirect (72,LOADGRAPHIC_Y,0,W_CacheLumpName("M_LOADG",PU_CACHE));
  for (i = 0 ; i < load_end ; i++)
    {
      M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
      M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }

  M_DrawBorderedSnapshot(itemOn);

  M_DrawSaveLoadBottomLine();
}

//
// Draw border for the savegame description
//

void M_DrawSaveLoadBorder(int x,int y)
{
  int i;
  
  V_DrawPatchDirect (x-8,y+7,0,W_CacheLumpName("M_LSLEFT",PU_CACHE));
  
  for (i = 0 ; i < 24 ; i++)
    {
      V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSCNTR",PU_CACHE));
      x += 8;
    }

  V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSRGHT",PU_CACHE));
}

//
// User wants to load this game
//

void M_LoadSelect(int choice)
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

void M_LoadGame (int choice)
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

menuitem_t SaveMenu[]=
{
  {1,"", M_SaveSelect,'1'},
  {1,"", M_SaveSelect,'2'},
  {1,"", M_SaveSelect,'3'},
  {1,"", M_SaveSelect,'4'},
  {1,"", M_SaveSelect,'5'},
  {1,"", M_SaveSelect,'6'},
  {1,"", M_SaveSelect,'7'}, //jff 3/15/98 extend number of slots
  {1,"", M_SaveSelect,'8'},
};

menu_t SaveDef =
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
void M_ReadSaveStrings(void)
{
  int i;

  // [FG] shift savegame descriptions a bit to the right
  //      to make room for the snapshots on the left
  SaveDef.x = LoadDef.x = M_X_LOADSAVE + MIN(M_LOADSAVE_WIDTH/2, WIDESCREENDELTA);

  // [FG] fit the snapshots into the resulting space
  snapshot_width = MIN((WIDESCREENDELTA + SaveDef.x + 2 * SKULLXOFF) & ~7, ORIGWIDTH/2); // [FG] multiple of 8
  snapshot_height = MIN((snapshot_width * ORIGHEIGHT / ORIGWIDTH) & ~7, ORIGHEIGHT/2);

  for (i = 0 ; i < load_end ; i++)
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
void M_DrawSave(void)
{
  int i;

  //jff 3/15/98 use symbolic load position
  V_DrawPatchDirect (72,LOADGRAPHIC_Y,0,W_CacheLumpName("M_SAVEG",PU_CACHE));
  for (i = 0 ; i < load_end ; i++)
    {
      M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
      M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
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
void M_DoSave(int slot)
{
  G_SaveGame (slot,savegamestrings[slot]);
  M_ClearMenus ();
}

void M_SetQuickSaveSlot (int slot)
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
void M_SaveSelect(int choice)
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
void M_SaveGame (int choice)
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
// OPTIONS MENU
//

// numerical values for the Options menu items

enum
{
  general, // killough 10/98
  // killough 4/6/98: move setup to be a sub-menu of OPTIONs
  setup,                                                    // phares 3/21/98
  endgame,
  messages,
  /*    detail, obsolete -- killough */
  scrnsize,
  option_empty1,
  mousesens,
  /* option_empty2, submenu now -- killough */
  soundvol,
  opt_end
} options_e;

// The definitions of the Options menu

menuitem_t OptionsMenu[]=
{
  // killough 4/6/98: move setup to be a sub-menu of OPTIONs
  // [FG] alternative text for missing menu graphics lumps
  {1,"M_GENERL", M_General, 'g', "GENERAL"},      // killough 10/98
  {1,"M_SETUP",  M_Setup,   's', "SETUP"},                          // phares 3/21/98
  {1,"M_ENDGAM", M_EndGame,'e', "END GAME"},
  {1,"M_MESSG",  M_ChangeMessages,'m', "MESSAGES:"},
  /*    {1,"M_DETAIL",  M_ChangeDetail,'g'},  unused -- killough */  
  {2,"M_SCRNSZ", M_SizeDisplay,'s', "SCREEN SIZE"},
  {-1,"",0},
  {1,"M_MSENS",  M_ChangeSensitivity,'m', "MOUSE SENSITIVITY"},
  /* {-1,"",0},  replaced with submenu -- killough */ 
  {1,"M_SVOL",   M_Sound,'s', "SOUND VOLUME"}
};

menu_t OptionsDef =
{
  opt_end,
  &MainDef,
  OptionsMenu,
  M_DrawOptions,
  60,37,
  0
};

//
// M_Options
//
char detailNames[2][9] = {"M_GDHIGH","M_GDLOW"};
char msgNames[2][9]  = {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
  V_DrawPatchDirect (108,15,0,W_CacheLumpName("M_OPTTTL",PU_CACHE));

  /*  obsolete -- killough
      V_DrawPatchDirect (OptionsDef.x + 175,OptionsDef.y+LINEHEIGHT*detail,0,
      W_CacheLumpName(detailNames[detailLevel],PU_CACHE));
      */

  // [FG] alternative text for missing menu graphics lumps
  if (OptionsDef.lumps_missing > 0)
    M_WriteText(OptionsDef.x + M_StringWidth("MESSAGES: "),
                OptionsDef.y + LINEHEIGHT*messages + 8-(M_StringHeight("ONOFF")/2),
                showMessages ? "ON" : "OFF");
  else
  if (OptionsDef.lumps_missing == -1)
  V_DrawPatchDirect (OptionsDef.x + 120,OptionsDef.y+LINEHEIGHT*messages,0,
		     W_CacheLumpName(msgNames[showMessages],PU_CACHE));

  /* M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(mousesens+1),
     10,mouseSensitivity);   killough */
  
  M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
	       9,screenSize);
}

void M_Options(int choice)
{
  M_SetupNextMenu(&OptionsDef);
}

/////////////////////////////
//
// M_QuitDOOM
//
int quitsounds[8] =
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

int quitsounds2[8] =
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

void M_QuitResponse(int ch)
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

void M_QuitDOOM(int choice)
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
  sfx_empty1,
  music_vol,
  sfx_empty2,
  sound_end
} sound_e;

// The definitions of the Sound Volume menu

menuitem_t SoundMenu[]=
{
  {2,"M_SFXVOL",M_SfxVol,'s'},
  {-1,"",0},
  {2,"M_MUSVOL",M_MusicVol,'m'},
  {-1,"",0}
};

menu_t SoundDef =
{
  sound_end,
  &OptionsDef,
  SoundMenu,
  M_DrawSound,
  80,64,
  0
};

//
// Change Sfx & Music volumes
//

void M_DrawSound(void)
{
  V_DrawPatchDirect (60,38,0,W_CacheLumpName("M_SVOL",PU_CACHE));

  M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),16,snd_SfxVolume);

  M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),16,snd_MusicVolume);
}

void M_Sound(int choice)
{
  M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
  switch(choice)
    {
    case 0:
      if (snd_SfxVolume)
        snd_SfxVolume--;
      break;
    case 1:
      if (snd_SfxVolume < 15)
        snd_SfxVolume++;
      break;
    }
  
  S_SetSfxVolume(snd_SfxVolume /* *8 */);
}

void M_MusicVol(int choice)
{
  switch(choice)
    {
    case 0:
      if (snd_MusicVolume)
        snd_MusicVolume--;
      break;
    case 1:
      if (snd_MusicVolume < 15)
        snd_MusicVolume++;
      break;
    }
  
  S_SetMusicVolume(snd_MusicVolume /* *8 */);
}

/////////////////////////////
//
// MOUSE SENSITIVITY MENU -- killough
//

// numerical values for the Mouse Sensitivity menu items
// The 'empty' slots are where the sliding scales appear.

enum
{
  mouse_horiz,
  mouse_empty1,
  mouse_horiz2,
  mouse_empty2,
  mouse_vert,
  mouse_empty3,
  mouse_vert2,
  mouse_empty4,
  mouse_end
} mouse_e;

// The definitions of the Mouse Sensitivity menu

menuitem_t MouseMenu[]=
{
  // [FG] alternative text for missing menu graphics lumps
  {2,"",M_MouseHoriz,'h', "HORIZONTAL: TURN"},
  {-1,"",0},
  {2,"",M_MouseHoriz2,'h', "HORIZONTAL: STRAFE"},
  {-1,"",0},
  {2,"",M_MouseVert,'v', "VERTICAL: MOVE"},
  {-1,"",0},
  {2,"",M_MouseVert2,'v', "VERTICAL: LOOK"},
  {-1,"",0}
};

menu_t MouseDef =
{
  mouse_end,
  &OptionsDef,
  MouseMenu,
  M_DrawMouse,
  60,34,
  0
};


// I'm using a scale of 100 since I don't know what's normal -- killough.

#define MOUSE_SENS_MAX 100

extern int axis_turn_sens;

//
// Change Mouse Sensitivities -- killough
//

void M_DrawMouse(void)
{
  int mhmx,mvmx,mhmx2,mvmx2; //jff 4/3/98 clamp drawn position to 23 max

  V_DrawPatchDirect (60,LOADGRAPHIC_Y,0,W_CacheLumpName("M_MSENS",PU_CACHE));

  //jff 4/3/98 clamp horizontal sensitivity display
  mhmx = mouseSensitivity_horiz; // >23? 23 : mouseSensitivity_horiz;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_horiz+1),24,mhmx);
  mhmx2 = mouseSensitivity_horiz_strafe; // >23? 23 : mouseSensitivity_horiz;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_horiz2+1),24,mhmx2);
  //jff 4/3/98 clamp vertical sensitivity display
  mvmx = mouseSensitivity_vert; // >23? 23 : mouseSensitivity_vert;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_vert+1),24,mvmx);
  mvmx2 = mouseSensitivity_vert_look; // >23? 23 : mouseSensitivity_vert;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_vert2+1),24,mvmx2);
}

void M_ChangeSensitivity(int choice)
{
  M_SetupNextMenu(&MouseDef);      // killough

  //  switch(choice)
  //      {
  //    case 0:
  //      if (mouseSensitivity)
  //        mouseSensitivity--;
  //      break;
  //    case 1:
  //      if (mouseSensitivity < 9)
  //        mouseSensitivity++;
  //      break;
  //      }
}

void M_MouseHoriz(int choice)
{
  M_Mouse(choice, &mouseSensitivity_horiz);
}

void M_MouseHoriz2(int choice)
{
  M_Mouse(choice, &mouseSensitivity_horiz_strafe);
}

void M_MouseVert(int choice)
{
  M_Mouse(choice, &mouseSensitivity_vert);
}

void M_MouseVert2(int choice)
{
  M_Mouse(choice, &mouseSensitivity_vert_look);
}

void M_Mouse(int choice, int *sens)
{
  switch(choice)
    {
    case 0:
      if (*sens)
        --*sens;
      break;
    case 1:
//    if (*sens < 23)
        ++*sens;
      break;
    }
}

/////////////////////////////
//
//    M_QuickSave
//

void M_QuickSaveResponse(int ch)
{
  if (ch == 'y')
    {
      if (StartsWithMapIdentifier(savegamestrings[quickSaveSlot]))
          SetDefaultSaveName(quickSaveSlot);
      M_DoSave(quickSaveSlot);
      S_StartSound(NULL,sfx_swtchx);
    }
}

void M_QuickSave(void)
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

void M_QuickLoadResponse(int ch)
{
  if (ch == 'y')
    {
      M_LoadSelect(quickSaveSlot);
      S_StartSound(NULL,sfx_swtchx);
    }
}

void M_QuickLoad(void)
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

void M_EndGameResponse(int ch)
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

void M_EndGame(int choice)
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

void M_ChangeMessages(int choice)
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

void M_SizeDisplay(int choice)
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
	hud_displayed = !hud_displayed;
      break;
    }
  R_SetViewSize (screenblocks /*, detailLevel obsolete -- killough */);
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

boolean setup_active      = false; // in one of the setup screens
boolean set_keybnd_active = false; // in key binding setup screens
boolean set_weapon_active = false; // in weapons setup screen
boolean set_status_active = false; // in status bar/hud setup screen
boolean set_auto_active   = false; // in automap setup screen
boolean set_enemy_active  = false; // in enemies setup screen
boolean set_mess_active   = false; // in messages setup screen
boolean set_chat_active   = false; // in chat string setup screen
boolean setup_select      = false; // changing an item
boolean setup_gather      = false; // gathering keys for value
boolean colorbox_active   = false; // color palette being shown
boolean default_verify    = false; // verify reset defaults decision
boolean set_general_active = false;
boolean set_compat_active = false;

/////////////////////////////
//
// set_menu_itemon is an index that starts at zero, and tells you which
// item on the current screen the cursor is sitting on.
//
// current_setup_menu is a pointer to the current setup menu table.

static int set_menu_itemon; // which setup item is selected?   // phares 3/98
setup_menu_t* current_setup_menu; // points to current setup menu table

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

static void M_SetMultScreenIndex (setup_menu_t *menu, const int y)
{
    if (menu)
    {
      while (!(menu->m_flags & S_END))
      {
        if (menu->m_flags & S_PREV)
        {
          menu = menu->var.menu;
          continue;
        }
        menu++;
      }

      menu->m_y = y;
    }
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
  set_compat,
  set_key_bindings,                                     
  set_weapons,                                           
  set_statbar,                                           
  set_automap,
  set_enemy,
  set_messages,
  set_chatstrings,
  set_setup_end
} setup_e;

int setup_screen; // the current setup screen. takes values from setup_e 

/////////////////////////////
//
// SetupMenu is the definition of what the main Setup Screen should look
// like. Each entry shows that the cursor can land on each item (1), the
// built-in graphic lump (i.e. "M_KEYBND") that should be displayed,
// the program which takes over when an item is selected, and the hotkey
// associated with the item.

menuitem_t SetupMenu[]=
{
  // [FG] alternative text for missing menu graphics lumps
  {1,"M_COMPAT",M_Compat,     'p', "DOOM COMPATIBILITY"},
  {1,"M_KEYBND",M_KeyBindings,'k', "KEY BINDINGS"},
  {1,"M_WEAP"  ,M_Weapons,    'w', "WEAPONS"},
  {1,"M_STAT"  ,M_StatusBar,  's', "STATUS BAR / HUD"},
  {1,"M_AUTO"  ,M_Automap,    'a', "AUTOMAP"},
  {1,"M_ENEM"  ,M_Enemy,      'e', "ENEMIES"},
  {1,"M_MESS"  ,M_Messages,   'm', "MESSAGES"},
  {1,"M_CHAT"  ,M_ChatStrings,'c', "CHAT STRINGS"},
};

/////////////////////////////
//
// M_DoNothing does just that: nothing. Just a placeholder.

void M_DoNothing(int choice)
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

menuitem_t Generic_Setup[] =
{
  {1,"",M_DoNothing,0}
};

/////////////////////////////
//
// SetupDef is the menu definition that the mainstream Menu code understands.
// This is used by M_Setup (below) to define what is drawn and what is done
// with the main Setup screen.

menu_t  SetupDef =
{
  set_setup_end, // number of Setup Menu items (Key Bindings, etc.)
  &OptionsDef,   // menu to return to when BACKSPACE is hit on this menu
  SetupMenu,     // definition of items to show on the Setup Screen
  M_DrawSetup,   // program that draws the Setup Screen
  59,37,         // x,y position of the skull (modified when the skull is
                 // drawn). The skull is parked on the upper-left corner
                 // of the Setup screens, since it isn't needed as a cursor
  0              // last item the user was on for this menu
};

/////////////////////////////
//
// Here are the definitions of the individual Setup Menu screens. They
// follow the format of the 'Big Font' menu structures. See the comments
// for SetupDef (above) to help understand what each of these says.

menu_t KeybndDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawKeybnd,
  34,5,      // skull drawn here
  0
};

menu_t WeaponDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawWeapons,
  34,5,      // skull drawn here
  0
};

menu_t StatusHUDDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawStatusHUD,
  34,5,      // skull drawn here
  0
};

menu_t AutoMapDef =
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawAutoMap,
  34,5,      // skull drawn here
  0
};

menu_t EnemyDef =                                           // phares 4/08/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawEnemy,
  34,5,      // skull drawn here
  0
};

menu_t MessageDef =                                         // phares 4/08/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawMessages,
  34,5,      // skull drawn here
  0
};

menu_t ChatStrDef =                                         // phares 4/10/98
{
  generic_setup_end,
  &SetupDef,
  Generic_Setup,
  M_DrawChatStrings,
  34,5,      // skull drawn here
  0
};

menu_t GeneralDef =                                           // killough 10/98
{
  generic_setup_end,
  &OptionsDef,
  Generic_Setup,
  M_DrawGeneral,
  34,5,      // skull drawn here
  0
};

menu_t CompatDef =                                           // killough 10/98
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

void M_DrawBackground(char *patchname, byte *back_dest)
{
  if (setup_active && menu_background != background_on)
    return;

  R_DrawBackground(patchname, back_dest);
}

/////////////////////////////
//
// Draws the Title for the main Setup screen

void M_DrawSetup(void)
{
  M_DrawTitle(124,15,"M_SETUP","SETUP");
}

/////////////////////////////
//
// Uses the SetupDef structure to draw the menu items for the main
// Setup screen

void M_Setup(int choice)
{
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
#define CR_HILITE CR_ORANGE
#define CR_SELECT CR_GRAY

// Data used by the Automap color selection code

#define CHIP_SIZE 7 // size of color block for colored items

#define COLORPALXORIG ((320 - 16*(CHIP_SIZE+1))/2)
#define COLORPALYORIG ((200 - 16*(CHIP_SIZE+1))/2)

#define PAL_BLACK   0
#define PAL_WHITE   4

static byte colorblock[(CHIP_SIZE+4)*(CHIP_SIZE+4)];

// Data used by the Chat String editing code

#define CHAT_STRING_BFR_SIZE 128

// chat strings must fit in this screen space
// killough 10/98: reduced, for more general uses
#define MAXCHATWIDTH         272

int   chat_index;
char *chat_string_buffer; // points to new chat strings while editing

/////////////////////////////
//
// phares 4/17/98:
// Added 'Reset to Defaults' Button to Setup Menu screens
// This is a small button that sits in the upper-right-hand corner of
// the first screen for each group. It blinks when selected, thus the
// two patches, which it toggles back and forth.

char ResetButtonName[2][8] = {"M_BUTT1","M_BUTT2"};

/////////////////////////////
//
// phares 4/18/98:
// Consolidate Item drawing code
//
// M_DrawItem draws the description of the provided item (the left-hand
// part). A different color is used for the text depending on whether the
// item is selected or not, or whether it's about to change.

static void M_DrawMenuStringEx(int flags, int x, int y, int color);

static boolean ItemDisabled(int flags)
{
  if ((flags & S_DISABLE) ||
      (flags & S_STRICT && strictmode) ||
      (flags & S_CRITICAL && critical) ||
      (flags & S_MBF && demo_version < 203) ||
      (flags & S_BOOM && demo_version < 202) ||
      (flags & S_VANILLA && !demo_compatibility))
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

static boolean PrevItemAvailable (setup_menu_t *s)
{
  const int value = s->var.def->location->i;
  return ((s->var.def->limit.min == UL || value > s->var.def->limit.min) &&
         (!s->selectstrings || value > 0));
}

static boolean NextItemAvailable (setup_menu_t *s)
{
  const int value = s->var.def->location->i;
  return ((s->var.def->limit.max == UL || value < s->var.def->limit.max) &&
         (!s->selectstrings || s->selectstrings[value + 1]));
}

void M_DrawItem(setup_menu_t* s)
{
  int x = s->m_x;
  int y = s->m_y;
  int flags = s->m_flags;
  if (flags & S_RESET)

    // This item is the reset button
    // Draw the 'off' version if this isn't the current menu item
    // Draw the blinking version in tune with the blinking skull otherwise

    V_DrawPatchDirect(x,y,0,W_CacheLumpName(ResetButtonName
					    [flags & (S_HILITE|S_SELECT) ?
					    whichSkull : 0], PU_CACHE));
  else  // Draw the item string
    {
      char *p, *t;
      int w = 0;
      int color = 
	flags & S_SELECT ? CR_SELECT : 
	flags & S_HILITE ? CR_HILITE :
	flags & (S_TITLE|S_NEXT|S_PREV) ? CR_TITLE : CR_ITEM; // killough 10/98

      // killough 10/98: 
      // Enhance to support multiline text separated by newlines.
      // This supports multiline items on horizontally-crowded menus.

      for (p = t = strdup(s->m_text); (p = strtok(p,"\n")); y += 8, p = NULL)
	{
	  menu_buffer[0] = '\0';
	  // [FG] print a blinking "arrow" next to the currently highlighted menu item
	  if (!(flags & S_NEXT_LINE) && ItemSelected(s))
	  {
	    if ((flags & (S_CHOICE|S_CRITEM|S_THERMO)) && setup_select)
	    {
	      if (PrevItemAvailable(s))
	        strcpy(menu_buffer, "< ");
	    }
	    else
	      strcpy(menu_buffer, "> ");
	  }
	  // killough 10/98: support left-justification:
	  strcat(menu_buffer, p);
	  if (!(flags & S_LEFTJUST))
	    w = M_GetPixelWidth(menu_buffer) + 4;
	  M_DrawMenuStringEx(flags, x - w, y, color);
	}
      free(t);
    }
}

// If a number item is being changed, allow up to N keystrokes to 'gather'
// the value. Gather_count tells you how many you have so far. The legality
// of what is gathered is determined by the low/high settings for the item.

#define MAXGATHER 5
int  gather_count;
char gather_buffer[MAXGATHER+1];  // killough 10/98: make input character-based

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

static void M_DrawMiniThermo(int x, int y, int size, int dot, char *color)
{
  int xx;
  int  i;
  const int step = M_THRM_STEP * M_THRM_SIZE * FRACUNIT / size;

  xx = x;
  V_DrawPatch(xx, y, 0, W_CacheLumpName("M_MTHRML", PU_CACHE));
  xx += M_THRM_STEP;
  for (i = 0; i < M_THRM_SIZE; i++)
  {
    V_DrawPatch(xx, y, 0, W_CacheLumpName("M_MTHRMM", PU_CACHE));
    xx += M_THRM_STEP;
  }
  V_DrawPatch(xx, y, 0, W_CacheLumpName("M_MTHRMR", PU_CACHE));

  // [FG] do not crash anymore if value exceeds thermometer range
  if (dot > size)
      dot = size;

  V_DrawPatchTranslated(x + M_THRM_STEP / 2 + dot * step / FRACUNIT, y, 0,
                        W_CacheLumpName("M_MTHRMO", PU_CACHE), color);
}

void M_DrawSetting(setup_menu_t* s)
{
  int x = s->m_x, y = s->m_y, flags = s->m_flags, color;

  // Determine color of the text. This may or may not be used
  // later, depending on whether the item is a text string or not.

  color = flags & S_SELECT ? CR_SELECT : flags & S_HILITE ? CR_HILITE : CR_SET;

  // Is the item a YES/NO item?

  if (flags & S_YESNO)
    {
      strcpy(menu_buffer,s->var.def->location->i ? "YES" : "NO");
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (!setup_select && ItemSelected(s))
        strcat(menu_buffer, " <");
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
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (!setup_select && ItemSelected(s))
        strcat(menu_buffer, " <");
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // Is the item a key binding?

  if (flags & S_INPUT)
  {
    int i;
    int offset = 0;

    input_t* input = M_Input(s->ident);

    // Draw the input bound to the action
    menu_buffer[0] = '\0';

    for (i = 0; i < input->num_inputs; ++i)
    {
      input_value_t *v = &input->inputs[i];

      if (i > 0)
      {
        menu_buffer[offset++] = ' ';
        menu_buffer[offset++] = '+';
        menu_buffer[offset++] = ' ';
        menu_buffer[offset] = '\0';
      }

      switch (v->type)
      {
        case input_type_key:
          offset = M_GetKeyString(v->value, offset);
          break;
        case input_type_mouseb:
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForMouseB(v->value));
          break;
        case input_type_joyb:
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForJoyB(v->value));
          break;
        default:
          break;
      }
    }

    // "NONE"
    if (i == 0)
      M_GetKeyString(0, 0);

    // [FG] print a blinking "arrow" next to the currently highlighted menu item
    if (!setup_select && ItemSelected(s))
      strcat(menu_buffer, " <");
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
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (!setup_select && ItemSelected(s))
        strcat(menu_buffer, " <");
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // Is the item a paint chip?

  if (flags & S_COLOR) // Automap paint chip
    {
      int i, ch;
      byte *ptr = colorblock;

      // draw the border of the paint chip
       
      for (i = 0 ; i < (CHIP_SIZE+2)*(CHIP_SIZE+2) ; i++)
	*ptr++ = PAL_BLACK;
      V_DrawBlock(x+WIDESCREENDELTA,y-1,0,CHIP_SIZE+2,CHIP_SIZE+2,colorblock);
      
      // draw the paint chip
       
      ch = s->var.def->location->i;
      if (!ch) // don't show this item in automap mode
	V_DrawPatchDirect (x+1,y,0,W_CacheLumpName("M_PALNO",PU_CACHE));
      else
	{
	  ptr = colorblock;
	  for (i = 0 ; i < CHIP_SIZE*CHIP_SIZE ; i++)
	    *ptr++ = ch;
	  V_DrawBlock(x+1+WIDESCREENDELTA,y,0,CHIP_SIZE,CHIP_SIZE,colorblock);
	}
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (!setup_select && ItemSelected(s))
        M_DrawString(x + CHIP_SIZE, y, color, " <");
      return;
    }

  // Is the item a chat string?
  // killough 10/98: or a filename?

  if (flags & S_STRING)
    {
      char *text = s->var.def->location->s;

      // Are we editing this string? If so, display a cursor under
      // the correct character.

      if (setup_select && (s->m_flags & (S_HILITE|S_SELECT)))
	{
	  int cursor_start, char_width, i;
	  char c[2];

	  // If the string is too wide for the screen, trim it back,
	  // one char at a time until it fits. This should only occur
	  // while you're editing the string.

	  while (M_GetPixelWidth(text) >= MAXCHATWIDTH)
	    {
	      int len = strlen(text); 
	      text[--len] = 0;
	      if (chat_index > len)
		chat_index--;
	    }

	  // Find the distance from the beginning of the string to
	  // where the cursor should be drawn, plus the width of
	  // the char the cursor is under..

	  c[0] = text[chat_index]; // hold temporarily
	  c[1] = 0;
	  char_width = M_GetPixelWidth(c);
	  if (char_width == 1)
	    char_width = 7; // default for end of line
	  text[chat_index] = 0; // NULL to get cursor position
	  cursor_start = M_GetPixelWidth(text);
	  text[chat_index] = c[0]; // replace stored char

	  // Now draw the cursor

	  for (i = 0 ; i < char_width ; i++)
	    colorblock[i] = PAL_WHITE;
	  if (x+cursor_start-1+WIDESCREENDELTA+char_width < SCREENWIDTH)
	    V_DrawBlock(x+cursor_start-1+WIDESCREENDELTA,y+7,0,char_width,1,colorblock);
	}

      // Draw the setting for the item

      strcpy(menu_buffer,text);
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (!setup_select && ItemSelected(s))
        strcat(menu_buffer, " <");
      M_DrawMenuString(x,y,color);
      return;
    }

  // [FG] selection of choices

  if (flags & (S_CHOICE|S_CRITEM))
    {
      int i = s->var.def->location->i;
      int width;
      menu_buffer[0] = '\0';

      if (i >= 0 && s->selectstrings[i])
        strcpy(menu_buffer, s->selectstrings[i]);
      width = M_GetPixelWidth(menu_buffer);
      if (flags & S_NEXT_LINE)
      {
        y += M_SPC;
        x = M_X - width - 4;
      }
      M_DrawMenuStringEx(flags, x, y, flags & S_CRITEM ? i : color);
      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (ItemSelected(s))
      {
        menu_buffer[0] = '\0';
        if (setup_select)
        {
          if (NextItemAvailable(s))
            strcpy(menu_buffer, " >");
        }
        else
          strcpy(menu_buffer, " <");
        M_DrawMenuStringEx(flags, x + width, y, color);

        if (flags & S_NEXT_LINE)
        {
          menu_buffer[0] = '\0';
          if (setup_select)
          {
            if (PrevItemAvailable(s))
              strcpy(menu_buffer, "< ");
          }
          else
            strcpy(menu_buffer, "> ");
          x -= M_GetPixelWidth(menu_buffer);
          M_DrawMenuStringEx(flags, x, y, color);
        }
      }
      return;
    }

  if (flags & S_THERMO)
    {
      const int value = s->var.def->location->i;
      const int max = s->var.def->limit.max;
      const int size = (max == UL ? M_THRM_SIZE * 2 : max);
      const int offsetx = SHORT(hu_font[0]->width);
      int offsety = M_SPC - SHORT(hu_font[0]->height);

      if (offsety > 2)
        offsety = 2;
      else if (offsety < 1)
        offsety = 0;
      else
        offsety = 1;

      M_DrawMiniThermo(x - offsetx, y - offsety, size, value,
                       ItemDisabled(flags) ? cr_dark : colrngs[color]);

      if (s->selectstrings && value >= 0 && s->selectstrings[value])
        strcpy(menu_buffer, s->selectstrings[value]);
      else
        M_snprintf(menu_buffer, 4, "%d", value);

      // [FG] print a blinking "arrow" next to the currently highlighted menu item
      if (ItemSelected(s))
      {
        if (setup_select)
        {
          if (NextItemAvailable(s))
            strcat(menu_buffer, " >");
        }
        else
          strcat(menu_buffer, " <");
      }
      M_DrawMenuStringEx(flags, x + M_THRM_WIDTH, y, color);
    }
}

/////////////////////////////
//
// M_DrawScreenItems takes the data for each menu item and gives it to
// the drawing routines above.

void M_DrawScreenItems(setup_menu_t* src)
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
      strcpy(menu_buffer, "Warning: Program must be restarted to see changes");
    }
    else
    {
      strcpy(menu_buffer, "Warning: Changes are pending until next game");
    }

    x_warn = ORIGWIDTH/2 - M_GetPixelWidth(menu_buffer)/2;
    M_DrawMenuString(x_warn, M_Y_WARN, CR_RED);
  }

  while (!(src->m_flags & S_END))
    {
      // See if we're to draw the item description (left-hand part)

      if (src->m_flags & S_SHOWDESC)
	M_DrawItem(src);

      // See if we're to draw the setting (right-hand part)

      if (src->m_flags & S_SHOWSET)
	M_DrawSetting(src);
      src++;
    }
}

/////////////////////////////
//
// Data used to draw the "are you sure?" dialogue box when resetting
// to defaults.

#define VERIFYBOXXORG 66
#define VERIFYBOXYORG 88

void M_DrawDefVerify()
{
  V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG, 0, W_CacheLumpName("M_VBOX", PU_CACHE));

  // The blinking messages is keyed off of the blinking of the
  // cursor skull.

  if (whichSkull) // blink the text
    {
      strcpy(menu_buffer,"Reset to defaults? (Y or N)");
      M_DrawMenuString(VERIFYBOXXORG+8,VERIFYBOXYORG+8,CR_RED);
    }
}

// [FG] delete a savegame

static void M_DrawDelVerify(void)
{
  V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG, 0, W_CacheLumpName("M_VBOX", PU_CACHE));

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

void M_DrawInstructions()
{
  default_t *def = current_setup_menu[set_menu_itemon].var.def;
  int flags = current_setup_menu[set_menu_itemon].m_flags;

  if (ItemDisabled(flags))
    return;

  // killough 8/15/98: warn when values are different
  if (flags & (S_NUM|S_YESNO) && def->current && def->current->i!=def->location->i &&
      !(flags & S_COSMETIC)) // Don't warn about cosmetic options
    {
      int allow = allow_changes() ? M_SPC : 0;
      if (!(setup_gather | print_warning_about_changes | demoplayback))
	{
	  strcpy(menu_buffer,
		 "Current actual setting differs from the default.");
	  M_DrawMenuString(4, M_Y_WARN - allow, CR_RED);
	  if (allow)
	    {
	      strcpy(menu_buffer,
		     "However, changes made here will take effect now.");
	      M_DrawMenuString(4, M_Y_WARN, CR_RED);
	    }
	}
      if (allow && setup_select)            // killough 8/15/98: Set new value
	if (!(flags & (S_LEVWARN | S_PRGWARN)))
	  def->current->i = def->location->i;
    }

  // There are different instruction messages depending on whether you
  // are changing an item or just sitting on it.

  { // killough 11/98: reformatted
    const char *s = "";
    int color = CR_HILITE,x = setup_select ? color = CR_SELECT,
      flags & S_INPUT  ? (s = "Press key or button to bind/unbind", 50)      :
      flags & S_YESNO  ? (s = "Press ENTER key to toggle", 78)               :
      // [FG] selection of choices
      flags & (S_CHOICE|S_CRITEM|S_THERMO)
                       ? (s = "Press left or right to choose", 70)           :
      flags & S_WEAP   ? (s = "Enter weapon number", 97)                     :
      flags & S_NUM    ? (s = "Enter value. Press ENTER when finished.", 37) :
      flags & S_COLOR  ? (s = "Select color and press enter", 70)            :
      flags & S_STRING ? (s = "Type/edit and Press ENTER", 78)               :
      flags & S_RESET  ? 43 : 0  /* when you're changing something */        :
      flags & S_RESET  ? (s = "Press ENTER key to reset to defaults", 43)    :
      // [FG] clear key bindings with the DEL key
      flags & S_INPUT  ? (s = "Press Enter to Change, Del to Clear", 43)     :
      (s = "Press Enter to Change", 91);
    strcpy(menu_buffer, s);
    M_DrawMenuString(x,20,color);
  }
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

#define KB_X  120

// phares 4/16/98:
// X,Y position of reset button. This is the same for every screen, and is
// only defined once here.

#define X_BUTTON 301
#define Y_BUTTON   3

// Definitions of the (in this case) five key binding screens.

setup_menu_t keys_settings1[];       
setup_menu_t keys_settings2[];       
setup_menu_t keys_settings3[];       
setup_menu_t keys_settings4[];       
setup_menu_t keys_settings5[];
setup_menu_t keys_settings6[];
setup_menu_t keys_settings7[];
setup_menu_t keys_settings8[];
setup_menu_t keys_settings9[];

// The table which gets you from one screen table to the next.

setup_menu_t* keys_settings[] =
{
  keys_settings1,
  keys_settings2,
  keys_settings3,
  keys_settings4,
  keys_settings5,
  keys_settings6,
  keys_settings7,
  keys_settings8,
  keys_settings9,
  NULL
};

int mult_screens_index; // the index of the current screen in a set

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

static const char *controller_axes_strings[] = {
  "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y", "None", NULL
};

setup_menu_t keys_settings1[] =  // Key Binding screen strings       
{
  {"ACTION"    ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  {"FIRE"        ,S_INPUT     ,m_scrn,KB_X,M_Y+1*M_SPC,{0},input_fire},
  {"FORWARD"     ,S_INPUT     ,m_scrn,KB_X,M_Y+2*M_SPC,{0},input_forward},
  {"BACKWARD"    ,S_INPUT     ,m_scrn,KB_X,M_Y+3*M_SPC,{0},input_backward},
  {"STRAFE LEFT" ,S_INPUT     ,m_scrn,KB_X,M_Y+4*M_SPC,{0},input_strafeleft},
  {"STRAFE RIGHT",S_INPUT     ,m_scrn,KB_X,M_Y+5*M_SPC,{0},input_straferight},
  {"USE"         ,S_INPUT     ,m_scrn,KB_X,M_Y+6*M_SPC,{0},input_use},
  {"RUN"         ,S_INPUT     ,m_scrn,KB_X,M_Y+7*M_SPC,{0},input_speed},
  {"STRAFE"      ,S_INPUT     ,m_scrn,KB_X,M_Y+8*M_SPC,{0},input_strafe},
  {"TURN LEFT"   ,S_INPUT     ,m_scrn,KB_X,M_Y+9*M_SPC,{0},input_turnleft},
  {"TURN RIGHT"  ,S_INPUT     ,m_scrn,KB_X,M_Y+10*M_SPC,{0},input_turnright},
  {"180 TURN"    ,S_INPUT|S_STRICT,m_scrn,KB_X,M_Y+11*M_SPC,{0},input_reverse},

  {"TOGGLES"     ,S_SKIP|S_TITLE,m_null,KB_X,M_Y+13*M_SPC},
  {"AUTORUN"     ,S_INPUT     ,m_scrn,KB_X,M_Y+14*M_SPC,{0},input_autorun},
  {"MOUSELOOK"   ,S_INPUT     ,m_scrn,KB_X,M_Y+15*M_SPC,{0},input_mouselook},
  {"VERTMOUSE"   ,S_INPUT     ,m_scrn,KB_X,M_Y+16*M_SPC,{0},input_novert},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings2[] =  // Key Binding screen strings       
{
  {"WEAPONS" ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  {"FIST"    ,S_INPUT     ,m_scrn,KB_X,M_Y+1*M_SPC,{0},input_weapon1},
  {"PISTOL"  ,S_INPUT     ,m_scrn,KB_X,M_Y+2*M_SPC,{0},input_weapon2},
  {"SHOTGUN" ,S_INPUT     ,m_scrn,KB_X,M_Y+3*M_SPC,{0},input_weapon3},
  {"CHAINGUN",S_INPUT     ,m_scrn,KB_X,M_Y+4*M_SPC,{0},input_weapon4},
  {"ROCKET"  ,S_INPUT     ,m_scrn,KB_X,M_Y+5*M_SPC,{0},input_weapon5},
  {"PLASMA"  ,S_INPUT     ,m_scrn,KB_X,M_Y+6*M_SPC,{0},input_weapon6},
  {"BFG",     S_INPUT     ,m_scrn,KB_X,M_Y+7*M_SPC,{0},input_weapon7},
  {"CHAINSAW",S_INPUT     ,m_scrn,KB_X,M_Y+8*M_SPC,{0},input_weapon8},
  {"SSG"     ,S_INPUT     ,m_scrn,KB_X,M_Y+9*M_SPC,{0},input_weapon9},
  {"BEST"    ,S_INPUT     ,m_scrn,KB_X,M_Y+10*M_SPC,{0},input_weapontoggle},
  // [FG] prev/next weapon keys and buttons
  {"PREV"    ,S_INPUT     ,m_scrn,KB_X,M_Y+12*M_SPC,{0},input_prevweapon},
  {"NEXT"    ,S_INPUT     ,m_scrn,KB_X,M_Y+13*M_SPC,{0},input_nextweapon},

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings1}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings3}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings3[] =
{
  {"GAMEPAD", S_SKIP|S_TITLE,m_null,KB_X,M_Y},

  {"ANALOG CONTROLS", S_YESNO, m_scrn, KB_X, M_Y+1*M_SPC, {"analog_controls"}},

  {"MOVING FORWARD", S_CHOICE, m_scrn, KB_X, M_Y+2*M_SPC,
    {"axis_forward"}, 0, NULL, controller_axes_strings},
  {"INVERT", S_YESNO, m_scrn, KB_X, M_Y+3*M_SPC, {"invert_forward"}},
  {"STRAFING", S_CHOICE, m_scrn, KB_X, M_Y+4*M_SPC,
    {"axis_strafe"}, 0, NULL, controller_axes_strings},
  {"INVERT", S_YESNO, m_scrn, KB_X, M_Y+5*M_SPC, {"invert_strafe"}},
  {"SENSITIVITY", S_THERMO, m_scrn, KB_X, M_Y+6*M_SPC, {"axis_move_sens"}},

  {"TURNING", S_CHOICE, m_scrn, KB_X, M_Y+8*M_SPC,
    {"axis_turn"}, 0, NULL, controller_axes_strings},
  {"INVERT", S_YESNO, m_scrn, KB_X, M_Y+9*M_SPC, {"invert_turn"}},
  {"SENSITIVITY", S_THERMO, m_scrn, KB_X, M_Y+10*M_SPC, {"axis_turn_sens"}},

  {"PADLOOK TOGGLE", S_INPUT, m_scrn, KB_X, M_Y+12*M_SPC, {0}, input_padlook},
  {"LOOKING", S_CHOICE, m_scrn, KB_X, M_Y+13*M_SPC,
    {"axis_look"}, 0, NULL, controller_axes_strings},
  {"INVERT", S_YESNO, m_scrn, KB_X, M_Y+14*M_SPC, {"invert_look"}},
  {"SENSITIVITY", S_THERMO, m_scrn, KB_X, M_Y+15*M_SPC, {"axis_look_sens"}},

  {"<- PREV", S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings2}},
  {"NEXT ->", S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings4}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t keys_settings4[] =  // Key Binding screen strings       
{
  {"GAME SPEED",S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  {"INCREASE"     ,S_INPUT,m_scrn,KB_X,M_Y+1*M_SPC,{0},input_speed_up},
  {"DECREASE"     ,S_INPUT,m_scrn,KB_X,M_Y+2*M_SPC,{0},input_speed_down},
  {"DEFAULT"      ,S_INPUT,m_scrn,KB_X,M_Y+3*M_SPC,{0},input_speed_default},

  {"DEMOS" ,S_SKIP|S_TITLE,m_null,KB_X,M_Y+5*M_SPC},
  {"FAST-FORWARD" ,S_INPUT,m_scrn,KB_X,M_Y+6*M_SPC,{0},input_demo_fforward},
  {"FINISH DEMO"  ,S_INPUT,m_scrn,KB_X,M_Y+7*M_SPC,{0},input_demo_quit},
  {"JOIN DEMO"    ,S_INPUT,m_scrn,KB_X,M_Y+8*M_SPC,{0},input_demo_join},

  {"MISCELLANEOUS",S_SKIP|S_TITLE,m_null,KB_X,M_Y+10*M_SPC},
  // [FG] reload current level / go to next level
  {"RELOAD MAP/DEMO",S_INPUT,m_scrn,KB_X,M_Y+11*M_SPC,{0},input_menu_reloadlevel},
  {"NEXT MAP"     ,S_INPUT,m_scrn,KB_X,M_Y+12*M_SPC,{0},input_menu_nextlevel},
  {"SHOW STATS/TIME",S_INPUT,m_scrn,KB_X,M_Y+13*M_SPC,{0},input_hud_timestats},

  {"<- PREV", S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings3}},
  {"NEXT ->", S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings5}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t keys_settings5[] =  // Key Binding screen strings       
{
  {"SHORTCUTS"      ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},

  // phares 4/13/98:
  // key_help and key_escape can no longer be rebound. This keeps the
  // player from getting themselves in a bind where they can't remember how
  // to get to the menus, and can't remember how to get to the help screen
  // to give them a clue as to how to get to the menus. :)

  // Also, the keys assigned to these functions cannot be bound to other
  // functions. Introduce an S_KEEP flag to show that you cannot swap this
  // key with other keys in the same 'group'. (m_scrn, etc.)

  {"HELP"        ,S_SKIP|S_KEEP ,m_scrn,0   ,0    ,{&key_help}},
  {"MENU"        ,S_SKIP|S_KEEP ,m_scrn,0   ,0    ,{&key_escape}},
  {"PAUSE"       ,S_INPUT     ,m_scrn,KB_X,M_Y+ 1*M_SPC,{0},input_pause},
  {"SAVE"        ,S_INPUT     ,m_scrn,KB_X,M_Y+ 2*M_SPC,{0},input_savegame},
  {"QUICKSAVE"   ,S_INPUT     ,m_scrn,KB_X,M_Y+ 3*M_SPC,{0},input_quicksave},
  {"QUICKLOAD"   ,S_INPUT     ,m_scrn,KB_X,M_Y+ 4*M_SPC,{0},input_quickload},
  {"LOAD"        ,S_INPUT     ,m_scrn,KB_X,M_Y+ 5*M_SPC,{0},input_loadgame},
  {"VOLUME"      ,S_INPUT     ,m_scrn,KB_X,M_Y+ 6*M_SPC,{0},input_soundvolume},
  {"HUD"         ,S_INPUT     ,m_scrn,KB_X,M_Y+ 7*M_SPC,{0},input_hud},
  {"END GAME"    ,S_INPUT     ,m_scrn,KB_X,M_Y+ 8*M_SPC,{0},input_endgame},
  {"MESSAGES"    ,S_INPUT     ,m_scrn,KB_X,M_Y+ 9*M_SPC,{0},input_messages},
  {"QUIT"        ,S_INPUT     ,m_scrn,KB_X,M_Y+10*M_SPC,{0},input_quit},
  {"GAMMA FIX"   ,S_INPUT     ,m_scrn,KB_X,M_Y+11*M_SPC,{0},input_gamma},
  {"SPY"         ,S_INPUT     ,m_scrn,KB_X,M_Y+12*M_SPC,{0},input_spy},
  {"LARGER VIEW" ,S_INPUT     ,m_scrn,KB_X,M_Y+13*M_SPC,{0},input_zoomin},
  {"SMALLER VIEW",S_INPUT     ,m_scrn,KB_X,M_Y+14*M_SPC,{0},input_zoomout},
  {"SCREENSHOT"  ,S_INPUT     ,m_scrn,KB_X,M_Y+15*M_SPC,{0},input_screenshot},
  {"CLEAN SCREENSHOT",S_INPUT ,m_scrn,KB_X,M_Y+16*M_SPC,{0},input_clean_screenshot},

  {"<- PREV", S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings4}},
  {"NEXT ->", S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings6}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t keys_settings6[] =  // Key Binding screen strings       
{
  {"AUTOMAP"    ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  {"TOGGLE AUTOMAP",S_INPUT  ,m_map ,KB_X,M_Y+ 1*M_SPC,{0},input_map},
  {"FOLLOW"     ,S_INPUT     ,m_map ,KB_X,M_Y+ 2*M_SPC,{0},input_map_follow},
  {"OVERLAY"    ,S_INPUT     ,m_map ,KB_X,M_Y+ 3*M_SPC,{0},input_map_overlay},
  {"ROTATE"     ,S_INPUT     ,m_map ,KB_X,M_Y+ 4*M_SPC,{0},input_map_rotate},

  {"ZOOM IN"    ,S_INPUT     ,m_map ,KB_X,M_Y+ 6*M_SPC,{0},input_map_zoomin},
  {"ZOOM OUT"   ,S_INPUT     ,m_map ,KB_X,M_Y+ 7*M_SPC,{0},input_map_zoomout},
  {"SHIFT UP"   ,S_INPUT     ,m_map ,KB_X,M_Y+ 8*M_SPC,{0},input_map_up},
  {"SHIFT DOWN" ,S_INPUT     ,m_map ,KB_X,M_Y+ 9*M_SPC,{0},input_map_down},
  {"SHIFT LEFT" ,S_INPUT     ,m_map ,KB_X,M_Y+10*M_SPC,{0},input_map_left},
  {"SHIFT RIGHT",S_INPUT     ,m_map ,KB_X,M_Y+11*M_SPC,{0},input_map_right},
  {"MARK PLACE" ,S_INPUT     ,m_map ,KB_X,M_Y+12*M_SPC,{0},input_map_mark},
  {"CLEAR LAST MARK",S_INPUT ,m_map ,KB_X,M_Y+13*M_SPC,{0},input_map_clear},
  {"FULL/ZOOM"  ,S_INPUT     ,m_map ,KB_X,M_Y+14*M_SPC,{0},input_map_gobig},
  {"GRID"       ,S_INPUT     ,m_map ,KB_X,M_Y+15*M_SPC,{0},input_map_grid},

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings5}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings7}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

#define CHEAT_X 160

setup_menu_t keys_settings7[] =
{
  {"Cheats", S_SKIP|S_TITLE, m_null, CHEAT_X, M_Y},

  {"Fake Archvile Jump",S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 1*M_SPC,{0},input_avj},
  {"God mode/Resurrect",S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 2*M_SPC,{0},input_iddqd},
  {"Ammo & Keys"       ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 3*M_SPC,{0},input_idkfa},
  {"Ammo"              ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 4*M_SPC,{0},input_idfa},
  {"No Clipping"       ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 5*M_SPC,{0},input_idclip},
  {"Health"            ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 6*M_SPC,{0},input_idbeholdh},
  {"Armor"             ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 7*M_SPC,{0},input_idbeholdm},
  {"Invulnerability"   ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 8*M_SPC,{0},input_idbeholdv},
  {"Berserk"           ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+ 9*M_SPC,{0},input_idbeholds},
  {"Partial Invisibility",S_INPUT,m_scrn,CHEAT_X,M_Y+10*M_SPC,{0},input_idbeholdi},
  {"Radiation Suit"    ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+11*M_SPC,{0},input_idbeholdr},
  {"Reveal Map"        ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+12*M_SPC,{0},input_iddt},
  {"Light Amplification",S_INPUT,m_scrn ,CHEAT_X,M_Y+13*M_SPC,{0},input_idbeholdl},
  {"No Target"         ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+14*M_SPC,{0},input_notarget},
  {"Freeze"            ,S_INPUT ,m_scrn ,CHEAT_X,M_Y+15*M_SPC,{0},input_freeze},

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings6}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings8}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings8[] =  // Key Binding screen strings
{
  {"MENUS"       ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  // killough 10/98: hotkey for entering setup menu:
  {"SETUP"       ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+1*M_SPC,{0},input_setup},
  {"NEXT ITEM"   ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+2*M_SPC,{0},input_menu_down},
  {"PREV ITEM"   ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+3*M_SPC,{0},input_menu_up},
  {"LEFT"        ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+4*M_SPC,{0},input_menu_left},
  {"RIGHT"       ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+5*M_SPC,{0},input_menu_right},
  {"BACKSPACE"   ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+6*M_SPC,{0},input_menu_backspace},
  {"SELECT ITEM" ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+7*M_SPC,{0},input_menu_enter},
  {"EXIT"        ,S_INPUT|S_KEEP,m_menu,KB_X,M_Y+8*M_SPC,{0},input_menu_escape},

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings7}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {keys_settings9}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings9[] =  // Key Binding screen strings
{
  {"CHATTING"   ,S_SKIP|S_TITLE,m_null,KB_X,M_Y},
  {"BEGIN CHAT" ,S_INPUT     ,m_scrn,KB_X,M_Y+1*M_SPC,{0},input_chat},
  {"PLAYER 1"   ,S_INPUT     ,m_scrn,KB_X,M_Y+2*M_SPC,{0},input_chat_dest0},
  {"PLAYER 2"   ,S_INPUT     ,m_scrn,KB_X,M_Y+3*M_SPC,{0},input_chat_dest1},
  {"PLAYER 3"   ,S_INPUT     ,m_scrn,KB_X,M_Y+4*M_SPC,{0},input_chat_dest2},
  {"PLAYER 4"   ,S_INPUT     ,m_scrn,KB_X,M_Y+5*M_SPC,{0},input_chat_dest3},
  {"BACKSPACE"  ,S_INPUT     ,m_scrn,KB_X,M_Y+6*M_SPC,{0},input_chat_backspace},
  {"ENTER"      ,S_INPUT     ,m_scrn,KB_X,M_Y+7*M_SPC,{0},input_chat_enter},

  {"<- PREV" ,S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {keys_settings8}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

// Setting up for the Key Binding screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_KeyBindings(int choice)
{
  M_SetupNextMenu(&KeybndDef);

  setup_active = true;
  setup_screen = ss_keys;
  set_keybnd_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(keys_settings);
  current_setup_menu = keys_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Key Bindings Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawKeybnd(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  // Set up the Key Binding screen 

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(84,2,"M_KEYBND","KEY BINDINGS");
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
  weap1_toggle_line,
  weap1_stub2,
  weap1_bfg,
};

enum {
  weap2_title1,
  weap2_hide_weapon,
  weap2_center, // [FG] centered weapon sprite
  weap2_bobbing,
  weap2_recoilpitch,
};

setup_menu_t weap_settings1[];
setup_menu_t weap_settings2[];

setup_menu_t* weap_settings[] =
{
  weap_settings1,
  weap_settings2,
  NULL
};

// [FG] centered or bobbing weapon sprite
static const char *weapon_attack_alignment_strings[] = {
  "OFF", "CENTERED", "BOBBING", NULL
};

static const char *default_bobfactor_strings[] = {
  "OFF", "FULL", "75%", NULL
};

static void M_UpdateCenteredWeaponItem(void)
{
  DISABLE_ITEM(!cosmetic_bobbing, weap_settings2[weap2_center]);
}

setup_menu_t weap_settings1[] =  // Weapons Settings screen       
{
  {"1ST CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref1*M_SPC, {"weapon_choice_1"}},
  {"2nd CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref2*M_SPC, {"weapon_choice_2"}},
  {"3rd CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref3*M_SPC, {"weapon_choice_3"}},
  {"4th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref4*M_SPC, {"weapon_choice_4"}},
  {"5th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref5*M_SPC, {"weapon_choice_5"}},
  {"6th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref6*M_SPC, {"weapon_choice_6"}},
  {"7th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref7*M_SPC, {"weapon_choice_7"}},
  {"8th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref8*M_SPC, {"weapon_choice_8"}},
  {"9th CHOICE WEAPON",S_WEAP|S_BOOM,m_null,M_X,M_Y+weap1_pref9*M_SPC, {"weapon_choice_9"}},

  {"", S_SKIP, m_null, M_X, M_Y + weap1_stub1*M_SPC},

  {"Enable Fist/Chainsaw\n& SG/SSG toggle", S_YESNO|S_BOOM, m_null, M_X,
   M_Y+ weap1_toggle*M_SPC, {"doom_weapon_toggles"}},

  {"", S_SKIP, m_null, M_X, M_Y + weap1_stub2*M_SPC},

  {"Pre-Beta BFG"      ,S_YESNO,m_null,M_X,  // killough 8/8/98
   M_Y+ weap1_bfg*M_SPC, {"classic_bfg"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {weap_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

setup_menu_t weap_settings2[] =
{
  {"Cosmetic",S_SKIP|S_TITLE,m_null,M_X,M_Y+weap2_title1*M_SPC},

  {"Hide Weapon", S_YESNO|S_STRICT, m_null, M_X,
   M_Y + weap2_hide_weapon*M_SPC, {"hide_weapon"}},

  // [FG] centered or bobbing weapon sprite
  {"Weapon Attack Alignment", S_CHOICE|S_STRICT, m_null, M_X,
   M_Y + weap2_center * M_SPC, {"center_weapon"}, 0, NULL, weapon_attack_alignment_strings},

  {"Player View/Weapon Bobbing", S_CHOICE, m_null, M_X,
   M_Y + weap2_bobbing * M_SPC, {"cosmetic_bobbing"}, 0, M_UpdateCenteredWeaponItem, default_bobfactor_strings},

  {"Enable Recoil Pitch", S_YESNO, m_null, M_X,
   M_Y + weap2_recoilpitch*M_SPC, {"weapon_recoilpitch"}},

  {"<- PREV" ,S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {weap_settings1}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

// Setting up for the Weapons screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Weapons(int choice)
{
  M_SetupNextMenu(&WeaponDef);

  setup_active = true;
  setup_screen = ss_weap;
  set_weapon_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(weap_settings);
  current_setup_menu = weap_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Weapons Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawWeapons(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(109,2,"M_WEAP","WEAPONS");
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

setup_menu_t stat_settings1[], stat_settings2[], stat_settings3[];

setup_menu_t* stat_settings[] =
{
  stat_settings1,
  stat_settings2,
  stat_settings3,
  NULL
};

enum {
  stat1_title1,
  stat1_rednum,
  stat1_graypcnt,
  stat1_keys,
  stat1_stub1,
  stat1_title2,
  stat1_stats,
  stat1_time,
  stat1_healthr,
  stat1_healthy,
  stat1_healthg,
  stat1_armorr,
  stat1_armory,
  stat1_armorg,
  stat1_ammor,
  stat1_ammoy,
};

setup_menu_t stat_settings1[] =  // Status Bar and HUD Settings screen       
{
  {"STATUS BAR"        ,S_SKIP|S_TITLE,m_null,M_X,M_Y},

  {"USE STANDARD COLORS",S_YESNO|S_COSMETIC,m_null,M_X,M_Y+stat1_rednum*M_SPC,   {"sts_always_red"}},
  {"GRAY %"            ,S_YESNO|S_COSMETIC,m_null,M_X,M_Y+stat1_graypcnt*M_SPC, {"sts_pct_always_gray"}},
  {"SINGLE KEY DISPLAY",S_YESNO|S_COSMETIC,m_null,M_X,M_Y+stat1_keys*M_SPC,     {"sts_traditional_keys"}},

  {"",S_SKIP,m_null,M_X,M_Y+stat1_stub1*M_SPC},

  {"HEADS-UP DISPLAY"  ,S_SKIP|S_TITLE,m_null,M_X,M_Y+stat1_title2*M_SPC},

  {"SHOW LEVEL STATS"  ,S_YESNO|S_COSMETIC,m_null,M_X,M_Y+stat1_stats*M_SPC, {"hud_level_stats"}},
  {"SHOW LEVEL TIME"   ,S_YESNO|S_COSMETIC,m_null,M_X,M_Y+stat1_time*M_SPC,  {"hud_level_time"}},
  {"HEALTH LOW/OK"     ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_healthr*M_SPC, {"health_red"}},
  {"HEALTH OK/GOOD"    ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_healthy*M_SPC, {"health_yellow"}},
  {"HEALTH GOOD/EXTRA" ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_healthg*M_SPC, {"health_green"}},
  {"ARMOR LOW/OK"      ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_armorr*M_SPC,  {"armor_red"}},
  {"ARMOR OK/GOOD"     ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_armory*M_SPC,  {"armor_yellow"}},
  {"ARMOR GOOD/EXTRA"  ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_armorg*M_SPC,  {"armor_green"}},
  {"AMMO LOW/OK"       ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_ammor*M_SPC,   {"ammo_red"}},
  {"AMMO OK/GOOD"      ,S_NUM|S_COSMETIC,m_null,M_X,M_Y+stat1_ammoy*M_SPC,   {"ammo_yellow"}},
                                                                        
  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {stat_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

enum {
  stat2_title1,
  stat2_backpack,
  stat2_armortype,
  stat2_smooth,
  stat2_stub1,
  stat2_title2,
  stat2_crispyhud,
  stat2_hudfont,
  stat2_widescreen,
  stat2_threelined,
  stat2_bargraphs,
};

enum {
  stat3_title1,
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

static const char *show_widgets_strings[] = {
    "OFF", "ON AUTOMAP", "ALWAYS", NULL
};

static const char *crosshair_target_str[] = {
    "OFF", "HIGHLIGHT", "HEALTH", NULL
};

static const char *hudcolor_str[] = {
    "BRICK", "TAN", "GRAY", "GREEN", "BROWN", "GOLD", "RED", "BLUE", "ORANGE",
    "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE", NULL
};

setup_menu_t stat_settings2[] =
{
  {"WIDGET APPEARANCE",S_SKIP|S_TITLE,m_null,M_X,M_Y},

  {"BACKPACK CHANGES THRESHOLDS"    ,S_YESNO,m_null,M_X,M_Y+stat2_backpack*M_SPC,  {"hud_backpack_thresholds"}},
  {"COLOR OF ARMOR DEPENDS ON TYPE" ,S_YESNO,m_null,M_X,M_Y+stat2_armortype*M_SPC, {"hud_armor_type"}},
  {"SMOOTH HEALTH/ARMOR COUNT"      ,S_YESNO,m_null,M_X,M_Y+stat2_smooth*M_SPC,    {"smooth_counts"}},

  {"",S_SKIP,m_null,M_X,M_Y+stat2_stub1*M_SPC},

  {"EXTENDED HUD",S_SKIP|S_TITLE,m_null,M_X,M_Y+stat2_title2*M_SPC },

  {"PREFER CRISPY HUD OVER BOOM HUD"  ,S_YESNO ,m_null,M_X,M_Y+stat2_crispyhud*M_SPC, {"crispy_hud"}},
  {"USE STANDARD DOOM FONT FOR WIDGETS", S_CHOICE,m_null,M_X,M_Y+stat2_hudfont*M_SPC, {"hud_widget_font"}, 0, NULL, show_widgets_strings},
  {"WIDESCREEN WIDGET ARRANGEMENT", S_YESNO,m_null,M_X,M_Y+stat2_widescreen*M_SPC, {"hud_widescreen_widgets"}},
  {"3-LINED COORDS/STATS WIDGETS", S_YESNO,m_null,M_X,M_Y+stat2_threelined*M_SPC, {"hud_threelined_widgets"}},
  {"DRAW BAR GRAPHS IN WIDGETS", S_YESNO,m_null,M_X,M_Y+stat2_bargraphs*M_SPC, {"hud_draw_bargraphs"}},

  {"<- PREV" ,S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {stat_settings1}},
  {"NEXT ->", S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {stat_settings3}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

setup_menu_t stat_settings3[] =
{
  {"CROSSHAIR",S_SKIP|S_TITLE,m_null,M_X,M_Y+stat3_title1*M_SPC },

  {"ENABLE CROSSHAIR",      S_CHOICE,m_null,M_X,M_Y+stat3_xhair*M_SPC,       {"hud_crosshair"}, 0, M_UpdateCrosshairItems, crosshair_str},
  {"COLOR BY PLAYER HEALTH",S_YESNO|S_STRICT,m_null,M_X,M_Y+stat3_xhairhealth*M_SPC, {"hud_crosshair_health"}},
  {"COLOR BY TARGET",       S_CHOICE|S_STRICT,m_null,M_X,M_Y+stat3_xhairtarget*M_SPC, {"hud_crosshair_target"}, 0, M_UpdateCrosshairItems, crosshair_target_str},
  {"LOCK ON TARGET",        S_YESNO|S_STRICT,m_null,M_X,M_Y+stat3_xhairlockon*M_SPC, {"hud_crosshair_lockon"}},
  {"DEFAULT COLOR",         S_CRITEM,m_null,M_X,M_Y+stat3_xhaircolor*M_SPC,  {"hud_crosshair_color"}, 0, NULL, hudcolor_str},
  {"HIGHLIGHT COLOR",       S_CRITEM|S_STRICT,m_null,M_X,M_Y+stat3_xhairtcolor*M_SPC, {"hud_crosshair_target_color"}, 0, NULL, hudcolor_str},

  {"<- PREV" ,S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {stat_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

// Setting up for the Status Bar / HUD screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_StatusBar(int choice)
{
  M_SetupNextMenu(&StatusHUDDef);

  setup_active = true;
  setup_screen = ss_stat;
  set_status_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(stat_settings);
  current_setup_menu = stat_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Status Bar / HUD Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawStatusHUD(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(59,2,"M_STAT","STATUS BAR / HUD");
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
setup_menu_t auto_settings2[];
setup_menu_t auto_settings3[];

setup_menu_t* auto_settings[] =
{
  auto_settings1,
  auto_settings2,
  auto_settings3,
  NULL
};

enum {
  auto1_title1,
  auto1_follow,
  auto1_rotate,
  auto1_overlay,
  auto1_stub1,
  auto1_title2,
  auto1_coords,
  auto1_pointer,
  auto1_stats,
  auto1_time,
  auto1_stub2,
  auto1_title3,
  auto1_smooth,
  auto1_secrets,
  auto1_flash,
};

static const char *overlay_strings[] = {
  "Off", "On", "Dark", NULL
};

extern void AM_enableSmoothLines(void);

setup_menu_t auto_settings1[] =  // 1st AutoMap Settings screen       
{
  {"Modes",S_SKIP|S_TITLE,m_null,M_X,M_Y},
  {"Follow Player"        ,S_YESNO ,m_null,M_X,M_Y+auto1_follow*M_SPC,  {"followplayer"}},
  {"Rotate Automap"       ,S_YESNO ,m_null,M_X,M_Y+auto1_rotate*M_SPC,  {"automaprotate"}},
  {"Overlay Automap"      ,S_CHOICE,m_null,M_X,M_Y+auto1_overlay*M_SPC, {"automapoverlay"}, 0, NULL, overlay_strings},

  {"",S_SKIP,m_null,M_X,M_Y+auto1_stub1*M_SPC},

  {"Widgets",S_SKIP|S_TITLE,m_null,M_X,M_Y+auto1_title2*M_SPC},
  // [FG] show level statistics and level time widgets
  {"Show player coords"   ,S_CHOICE|S_STRICT,m_null,M_X,M_Y+auto1_coords*M_SPC,  {"map_player_coords"},0,NULL,show_widgets_strings},
  {"Coords follow pointer",S_YESNO ,m_null,M_X,M_Y+auto1_pointer*M_SPC, {"map_point_coord"}},  // killough 10/98
  {"Show level stats"     ,S_YESNO ,m_null,M_X,M_Y+auto1_stats*M_SPC,   {"map_level_stats"}},
  {"Show level time"      ,S_YESNO ,m_null,M_X,M_Y+auto1_time*M_SPC,    {"map_level_time"}},

  {"",S_SKIP,m_null,M_X,M_Y+auto1_stub2*M_SPC},

  {"Miscellaneous",S_SKIP|S_TITLE,m_null,M_X,M_Y+auto1_title3*M_SPC},
  {"Smooth automap lines"            ,S_YESNO,m_null,M_X,M_Y+auto1_smooth*M_SPC,  {"map_smooth_lines"},0,AM_enableSmoothLines},
  {"Show Secrets only after entering",S_YESNO,m_null,M_X,M_Y+auto1_secrets*M_SPC, {"map_secret_after"}},
  {"Keyed doors are flashing"        ,S_YESNO,m_null,M_X,M_Y+auto1_flash*M_SPC,   {"map_keyed_door_flash"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {auto_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

enum {
  auto2_col_back,
  auto2_col_grid,
  auto2_col_wall,
  auto2_col_fchg,
  auto2_col_cchg,
  auto2_col_clsd,
  auto2_col_rkey,
  auto2_col_bkey,
  auto2_col_ykey,
  auto2_col_rdor,
  auto2_col_bdor,
  auto2_col_ydor,
  auto2_stub1,
  auto2_col_titl,
  auto2_col_xyco,
};

setup_menu_t auto_settings2[] =  // 2nd AutoMap Settings screen
{
  {"background"                         ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y,                      {"mapcolor_back"}},
  {"grid lines"                         ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_grid*M_SPC, {"mapcolor_grid"}},
  {"normal 1s wall"                     ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_wall*M_SPC, {"mapcolor_wall"}},
  {"line at floor height change"        ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_fchg*M_SPC, {"mapcolor_fchg"}},
  {"line at ceiling height change"      ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_cchg*M_SPC, {"mapcolor_cchg"}},
  {"line at sector with floor = ceiling",S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_clsd*M_SPC, {"mapcolor_clsd"}},
  {"red key"                            ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_rkey*M_SPC, {"mapcolor_rkey"}},
  {"blue key"                           ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_bkey*M_SPC, {"mapcolor_bkey"}},
  {"yellow key"                         ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_ykey*M_SPC, {"mapcolor_ykey"}},
  {"red door"                           ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_rdor*M_SPC, {"mapcolor_rdor"}},
  {"blue door"                          ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_bdor*M_SPC, {"mapcolor_bdor"}},
  {"yellow door"                        ,S_COLOR|S_COSMETIC,m_null,M_X,M_Y+auto2_col_ydor*M_SPC, {"mapcolor_ydor"}},

  {"",S_SKIP,m_null,M_X,M_Y+auto2_stub1*M_SPC},

  {"AUTOMAP LEVEL TITLE COLOR",S_CRITEM|S_COSMETIC,m_null,M_X,M_Y+auto2_col_titl*M_SPC, {"hudcolor_titl"}, 0, NULL, hudcolor_str},
  {"AUTOMAP COORDINATES COLOR",S_CRITEM|S_COSMETIC,m_null,M_X,M_Y+auto2_col_xyco*M_SPC, {"hudcolor_xyco"}, 0, NULL, hudcolor_str},

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {auto_settings1}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {auto_settings3}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

enum {
  auto3_col_tele,
  auto3_col_secr,
  auto3_col_exit,
  auto3_col_unsn,
  auto3_col_flat,
  auto3_col_sprt,
  auto3_col_hair,
  auto3_col_sngl,
  auto3_col_ply1,
  auto3_col_ply2,
  auto3_col_ply3,
  auto3_col_ply4,
  auto3_stub1,
  auto3_col_frnd,
};

setup_menu_t auto_settings3[] =  // 3rd AutoMap Settings screen
{
  {"teleporter line"                ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y,                      {"mapcolor_tele"}},
  {"secret sector boundary"         ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_secr*M_SPC, {"mapcolor_secr"}},
  //jff 4/23/98 add exit line to automap
  {"exit line"                      ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_exit*M_SPC, {"mapcolor_exit"}},
  {"computer map unseen line"       ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_unsn*M_SPC, {"mapcolor_unsn"}},
  {"line w/no floor/ceiling changes",S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_flat*M_SPC, {"mapcolor_flat"}},
  {"general sprite"                 ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_sprt*M_SPC, {"mapcolor_sprt"}},
  {"crosshair"                      ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_hair*M_SPC, {"mapcolor_hair"}},
  {"single player arrow"            ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_sngl*M_SPC, {"mapcolor_sngl"}},
  {"player 1 arrow"                 ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_ply1*M_SPC, {"mapcolor_ply1"}},
  {"player 2 arrow"                 ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_ply2*M_SPC, {"mapcolor_ply2"}},
  {"player 3 arrow"                 ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_ply3*M_SPC, {"mapcolor_ply3"}},
  {"player 4 arrow"                 ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_ply4*M_SPC, {"mapcolor_ply4"}},

  {"",S_SKIP,m_null,M_X,M_Y+auto3_stub1*M_SPC},

  {"friends"                        ,S_COLOR|S_COSMETIC ,m_null,M_X,M_Y+auto3_col_frnd*M_SPC, {"mapcolor_frnd"}}, // killough 8/8/98

  {"<- PREV",S_SKIP|S_PREV,m_null,M_X_PREV,M_Y_PREVNEXT, {auto_settings2}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};


// Setting up for the Automap screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Automap(int choice)
{
  M_SetupNextMenu(&AutoMapDef);

  setup_active = true;
  setup_screen = ss_auto;
  set_auto_active = true;
  setup_select = false;
  colorbox_active = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(auto_settings);
  current_setup_menu = auto_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// Data used by the color palette that is displayed for the player to
// select colors.

int color_palette_x; // X position of the cursor on the color palette
int color_palette_y; // Y position of the cursor on the color palette
byte palette_background[16*(CHIP_SIZE+1)+8];

// M_DrawColPal() draws the color palette when the user needs to select a
// color.

// phares 4/1/98: now uses a single lump for the palette instead of
// building the image out of individual paint chips.

void M_DrawColPal()
{
  int cpx, cpy;

  // Draw a background, border, and paint chips

  V_DrawPatchDirect (COLORPALXORIG-5,COLORPALYORIG-5,0,W_CacheLumpName("M_COLORS",PU_CACHE));

  // Draw the cursor around the paint chip
  // (cpx,cpy) is the upper left-hand corner of the paint chip

  cpx = COLORPALXORIG + color_palette_x * (CHIP_SIZE + 1) - 1;
  cpy = COLORPALYORIG + color_palette_y * (CHIP_SIZE + 1) - 1;
  V_DrawPatch(cpx, cpy, 0, W_CacheLumpName("M_PALSEL", PU_CACHE));
}

// The drawing part of the Automap Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawAutoMap(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(109,2,"M_AUTO","AUTOMAP");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If a color is being selected, need to show color paint chips

  if (colorbox_active)
    M_DrawColPal();

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  else if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Enemies table.

setup_menu_t enem_settings1[];

setup_menu_t* enem_settings[] =
{
  enem_settings1,
  NULL
};

enum {
  enem1_infighting,

  enem1_remember = 1,

  enem1_backing,
  enem1_monkeys,
  enem1_avoid_hazards,
  enem1_friction,
  enem1_help_friends,

  enem1_helpers,

  enem1_distfriend,

  enem1_dog_jumping,

  enem1_stub1,
  enem1_title1,
  enem1_colored_blood,
  enem1_flipcorpses,
  enem1_ghost,
  enem1_fuzz,

  enem1_end
};

setup_menu_t enem_settings1[] =  // Enemy Settings screen       
{
  // killough 7/19/98
  {"Monster Infighting When Provoked",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_infighting*M_SPC, {"monster_infighting"}},

  {"Remember Previous Enemy",S_YESNO|S_BOOM,m_null,M_X,M_Y+ enem1_remember*M_SPC, {"monsters_remember"}},

  // killough 9/8/98
  {"Monster Backing Out",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_backing*M_SPC, {"monster_backing"}},

  {"Climb Steep Stairs", S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_monkeys*M_SPC, {"monkeys"}},

  // killough 9/9/98
  {"Intelligently Avoid Hazards",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_avoid_hazards*M_SPC, {"monster_avoid_hazards"}},

  // killough 10/98
  {"Affected by Friction",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_friction*M_SPC, {"monster_friction"}},

  {"Rescue Dying Friends",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_help_friends*M_SPC, {"help_friends"}},

  // killough 7/19/98
  {"Number Of Single-Player Helper Dogs",S_NUM|S_LEVWARN|S_MBF,m_null,M_X,M_Y+ enem1_helpers*M_SPC, {"player_helpers"}},

  // killough 8/8/98
  {"Distance Friends Stay Away",S_NUM|S_MBF,m_null,M_X,M_Y+ enem1_distfriend*M_SPC, {"friend_distance"}},

  {"Allow dogs to jump down",S_YESNO|S_MBF,m_null,M_X,M_Y+ enem1_dog_jumping*M_SPC, {"dog_jumping"}},

  {"", S_SKIP, m_null, M_X , M_Y + enem1_stub1*M_SPC},

  {"Cosmetic",S_SKIP|S_TITLE,m_null,M_X,M_Y+ enem1_title1*M_SPC},

  // [FG] colored blood and gibs
  {"Colored Blood",S_YESNO|S_STRICT,m_null,M_X,M_Y+ enem1_colored_blood*M_SPC, {"colored_blood"}, 0, D_SetBloodColor},

  // [crispy] randomly flip corpse, blood and death animation sprites
  {"Randomly Mirrored Corpses",S_YESNO|S_STRICT,m_null,M_X,M_Y+ enem1_flipcorpses*M_SPC, {"flipcorpses"}},

  // [crispy] resurrected pools of gore ("ghost monsters") are translucent
  {"Translucent Ghost Monsters",S_YESNO|S_STRICT,m_null,M_X,M_Y+ enem1_ghost*M_SPC, {"ghost_monsters"}},

  // [FG] spectre drawing mode
  {"Blocky Spectre Drawing",S_YESNO,m_null,M_X,M_Y+ enem1_fuzz*M_SPC, {"fuzzcolumn_mode"}, 0, R_SetFuzzColumnMode},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

/////////////////////////////

// Setting up for the Enemies screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Enemy(int choice)
{
  M_SetupNextMenu(&EnemyDef);

  setup_active = true;
  setup_screen = ss_enem;
  set_enemy_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(enem_settings);
  current_setup_menu = enem_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Enemies Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawEnemy(void)

{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(114,2,"M_ENEM","ENEMIES");
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

setup_menu_t gen_settings1[], gen_settings2[], gen_settings3[], gen_settings4[], gen_settings5[];

setup_menu_t* gen_settings[] =
{
  gen_settings1,
  gen_settings2,
  gen_settings3,
  gen_settings4,
  gen_settings5,
  NULL
};

// Page 1

enum {
  gen1_title1,
  gen1_hires,
  gen1_widescreen,
  gen1_gap1,

  gen1_fullscreen,
  gen1_exclusive_fullscreen,
  gen1_gap2,

  gen1_uncapped,
  gen1_fpslimit,
  gen1_vsync,
  gen1_gap3,

  gen1_gamma,
  gen1_end1,
};

// Page 2

enum {
  gen2_title1,
  gen2_sndchan,
  gen2_pitch,
  gen2_fullsnd,
  gen2_gap1,

  gen2_sndresampler,
  gen2_sndmodule,
  gen2_sndhrtf,
  gen2_gap2,

  gen2_musicbackend,
  gen2_gap3,
  gen2_gap4, // music backend spans two lines
  gen2_end1,
};

int midi_player_menu;

static const char *midi_player_menu_strings[MAX_MIDI_PLAYER_MENU_ITEMS];

void static M_SmoothLight(void)
{
  extern void P_SegLengths(boolean contrast_only);
  // [crispy] re-calculate the zlight[][] array
  R_InitLightTables();
  // [crispy] re-calculate the scalelight[][] array
  R_ExecuteSetViewSize();
  // [crispy] re-calculate fake contrast
  P_SegLengths(true);
}

static const char *gamma_strings[] = {
  // Darker
  "-4", "-3.6", "-3.2", "-2.8", "-2.4", "-2.0", "-1.6", "-1.2", "-0.8",

  // No gamma correction
  "0",

  // Lighter
  "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4",
  NULL
};

static void M_ResetGamma(void)
{
  I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

static const char *sound_module_menu_strings[] = {
  "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
  "PC Speaker",
#endif
  NULL
};

static const char *sound_resampler_menu_strings[] = {
  "Nearest", "Linear", "Cubic", NULL
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

static void M_UpdateUserSoundSettings(void)
{
  I_UpdateUserSoundSettings();
}

static void M_SetMidiPlayer(void)
{
  S_StopMusic();
  I_SetMidiPlayer(midi_player_menu);
  S_SetMusicVolume(snd_MusicVolume);
  S_RestartMusic();
}

static void M_EnableDisableFPSLimit(void)
{
  DISABLE_ITEM(!uncapped, gen_settings1[gen1_fpslimit]);
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
}

static void M_ResetScreen(void)
{
  need_reset = true;
}

setup_menu_t gen_settings1[] = { // General Settings screen1

  {"Video"       ,S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"High Resolution", S_YESNO, m_null, M_X, M_Y+ gen1_hires*M_SPC,
   {"hires"}, 0, M_ResetScreen},

  {"Widescreen Rendering", S_YESNO, m_null, M_X, M_Y+ gen1_widescreen*M_SPC,
   {"widescreen"}, 0, M_ResetScreen},

  {"", S_SKIP, m_null, M_X, M_Y + gen1_gap1*M_SPC},

  {"Fullscreen Mode", S_YESNO, m_null, M_X, M_Y+ gen1_fullscreen*M_SPC,
   {"fullscreen"}, 0, M_ToggleFullScreen},

  {"Exclusive Fullscreen", S_YESNO, m_null, M_X, M_Y+ gen1_exclusive_fullscreen*M_SPC,
   {"exclusive_fullscreen"}, 0, M_ToggleExclusiveFullScreen},

  {"", S_SKIP, m_null, M_X, M_Y + gen1_gap2*M_SPC},

  {"Uncapped Frame Rate", S_YESNO, m_null, M_X, M_Y+ gen1_uncapped*M_SPC,
   {"uncapped"}, 0, M_EnableDisableFPSLimit},

  {"Frame Rate Limit", S_NUM, m_null, M_X,
   M_Y + gen1_fpslimit*M_SPC, {"fpslimit"}, 0, M_CoerceFPSLimit},

  {"Vertical Sync", S_YESNO, m_null, M_X,
   M_Y+ gen1_vsync*M_SPC, {"use_vsync"}, 0, I_ToggleVsync},

  {"", S_SKIP, m_null, M_X, M_Y + gen1_gap3*M_SPC},

  {"Gamma Correction", S_THERMO, m_null, M_X_THRM,
   M_Y+ gen1_gamma*M_SPC, {"gamma2"}, 0, M_ResetGamma, gamma_strings},

  {"", S_SKIP, m_null, M_X, M_Y + gen1_end1*M_SPC},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,M_X_NEXT,M_Y_PREVNEXT, {gen_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

setup_menu_t gen_settings2[] = { // General Settings screen2

  {"Sound & Music", S_SKIP|S_TITLE, m_null, M_X,
   M_Y + gen2_title1*M_SPC},

  {"Number of Sound Channels", S_NUM|S_PRGWARN, m_null, M_X,
   M_Y + gen2_sndchan*M_SPC, {"snd_channels"}},

  {"Pitch-Shifted Sounds", S_YESNO, m_null, M_X,
   M_Y + gen2_pitch*M_SPC, {"pitched_sounds"}},

  // [FG] play sounds in full length
  {"Disable Sound Cutoffs", S_YESNO, m_null, M_X,
   M_Y + gen2_fullsnd*M_SPC, {"full_sounds"}},

  {"", S_SKIP, m_null, M_X, M_Y + gen2_gap1*M_SPC},

  {"Resampler", S_CHOICE, m_null, M_X,
   M_Y + gen2_sndresampler*M_SPC, {"snd_resampler"}, 0, M_UpdateUserSoundSettings, sound_resampler_menu_strings},

  {"Sound Module", S_CHOICE, m_null, M_X,
   M_Y + gen2_sndmodule*M_SPC, {"snd_module"}, 0, M_SetSoundModule, sound_module_menu_strings},

  {"Headphones Mode", S_YESNO, m_null, M_X,
   M_Y + gen2_sndhrtf*M_SPC, {"snd_hrtf"}, 0, M_SetSoundModule},

  {"", S_SKIP, m_null, M_X, M_Y + gen2_gap2*M_SPC},

  // [FG] music backend
  {"MIDI player", S_CHOICE|S_NEXT_LINE, m_null, M_X,
   M_Y + gen2_musicbackend*M_SPC, {"midi_player_menu"}, 0, M_SetMidiPlayer, midi_player_menu_strings},

  {"", S_SKIP, m_null, M_X, M_Y + gen2_gap3*M_SPC},
  {"", S_SKIP, m_null, M_X, M_Y + gen2_gap4*M_SPC},

  {"", S_SKIP, m_null, M_X, M_Y + gen2_end1*M_SPC},

  {"<- PREV",S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {gen_settings1}},
  {"NEXT ->",S_SKIP|S_NEXT, m_null, M_X_NEXT, M_Y_PREVNEXT, {gen_settings3}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

// Page 3

enum {
  gen3_title1,
  gen3_smooth_scaling,
  gen3_trans,
  gen3_transpct,
  gen3_stub1,
  gen3_sky1,
  gen3_sky2,
  gen3_swirl,
  gen3_smoothlight,
  gen3_brightmaps,
  gen3_stub2,
  gen3_solidbackground,
  gen3_menu_background,
  gen3_diskicon,
  gen3_endoom,
  gen3_end1,
};

// Page 4

enum {
  gen4_title1,
  gen4_strictmode,
  gen4_screen_melt,
  gen4_death_action,
  gen4_demobar,
  gen4_palette_changes,
  gen4_level_brightness,
  gen4_organize_savefiles,
  gen4_end1,

  gen4_title2,
  gen4_hangsolid,
  gen4_blockmapfix,
  gen4_verticalaim,
  gen4_pistolstart,
  gen4_end2,
};

// Page 5

enum {
  gen5_title1,
  gen5_mouse1,
  gen5_mouse2,
  gen5_mouse3,
  gen5_mouse_accel,
  gen5_mouse_accel_threshold,
  gen5_end1,

  gen5_title2,
  gen5_realtic,
  gen5_compat,
  gen5_skill,
  gen5_playername,
  gen5_end2,
};

#define MOUSE_ACCEL_STRINGS_SIZE (40 + 2)

static const char *mouse_accel_strings[MOUSE_ACCEL_STRINGS_SIZE];

static void M_InitMouseAccel(void)
{
  int i;
  char buf[8];

  for (i = 0; i < MOUSE_ACCEL_STRINGS_SIZE - 1; ++i)
  {
    int val = i + 10;
    M_snprintf(buf, sizeof(buf), "%1d.%1d", val / 10, val % 10);
    mouse_accel_strings[i] = M_StringDuplicate(buf);
  }

  mouse_accel_strings[i] = NULL;
}

void M_ResetTimeScale(void)
{
  if (strictmode || D_CheckNetConnect())
    I_SetTimeScale(100);
  else
  {
    int p;

    int time_scale = realtic_clock_rate;

    //!
    // @arg <n>
    // @category game
    //
    // Increase or decrease game speed, percentage of normal.
    //

    p = M_CheckParmWithArgs("-speed", 1);

    if (p)
    {
      time_scale = M_ParmArgToInt(p);
      if (time_scale < 10 || time_scale > 1000)
        I_Error("Invalid parameter '%d' for -speed, valid values are 10-1000.",
                time_scale);
    }

    I_SetTimeScale(time_scale);
  }
}

static void M_UpdateDirectVerticalAimingItem(void)
{
  DISABLE_ITEM(!mouselook && !padlook, gen_settings4[gen4_verticalaim]);
}

static void M_UpdateMouseLook(void)
{
  P_UpdateDirectVerticalAiming();
  M_UpdateDirectVerticalAimingItem();

  if (!mouselook || !padlook)
  {
    int i;
    for (i = 0; i < MAXPLAYERS; ++i)
      if (playeringame[i])
        players[i].centering = true;
  }
}

const char *default_skill_strings[] = {
  // dummy first option because defaultskill is 1-based
  "", "ITYTD", "HNTR", "HMP", "UV", "NM", NULL
};

static const char *default_compatibility_strings[] = {
  "Vanilla", "Boom", "MBF", "MBF21", NULL
};

static const char *default_endoom_strings[] = {
  "off", "on", "PWAD only", NULL
};

static const char *death_use_action_strings[] = {
  "default", "last save", "nothing", NULL
};

static const char *menu_background_strings[] = {
  "on", "off", "dark", NULL
};

setup_menu_t gen_settings3[] = { // General Settings screen3

  {"Display Options"  ,S_SKIP|S_TITLE, m_null, M_X,
   M_Y + gen3_title1*M_SPC},

  {"Smooth Pixel Scaling", S_YESNO, m_null, M_X,
   M_Y+ gen3_smooth_scaling*M_SPC, {"smooth_scaling"}, 0, M_ResetScreen},

  {"Enable predefined translucency", S_YESNO|S_STRICT, m_null, M_X,
   M_Y+ gen3_trans*M_SPC, {"translucency"}, 0, M_Trans},

  {"Translucency filter percentage", S_NUM, m_null, M_X,
   M_Y+ gen3_transpct*M_SPC, {"tran_filter_pct"}, 0, M_Trans},

  {"", S_SKIP, m_null, M_X, M_Y + gen3_stub1*M_SPC},

  {"Stretch Short Skies", S_YESNO, m_null, M_X,
   M_Y + gen3_sky1*M_SPC, {"stretchsky"}, 0, R_InitSkyMap},

  {"Linear Sky Scrolling", S_YESNO, m_null, M_X,
   M_Y + gen3_sky2*M_SPC, {"linearsky"}, 0, R_InitPlanes},

  {"Swirling Animated Flats", S_YESNO, m_null, M_X,
   M_Y + gen3_swirl*M_SPC, {"r_swirl"}},

  {"Smooth Diminishing Lighting", S_YESNO, m_null, M_X,
   M_Y + gen3_smoothlight*M_SPC, {"smoothlight"}, 0, M_SmoothLight},

  {"Brightmaps for Textures and Sprites", S_YESNO|S_STRICT, m_null, M_X,
   M_Y + gen3_brightmaps*M_SPC, {"brightmaps"}},

  {"", S_SKIP, m_null, M_X, M_Y + gen3_stub2*M_SPC},

  {"Solid Status Bar Background", S_YESNO, m_null, M_X,
   M_Y + gen3_solidbackground*M_SPC, {"st_solidbackground"}},

  {"Draw Menu Background", S_CHOICE, m_null, M_X,
   M_Y + gen3_menu_background*M_SPC, {"menu_background"}, 0, NULL, menu_background_strings},

  {"Flash Icon During Disk IO", S_YESNO, m_null, M_X,
   M_Y + gen3_diskicon*M_SPC, {"disk_icon"}},

  {"Show ENDOOM screen", S_CHOICE, m_null, M_X,
   M_Y + gen3_endoom*M_SPC, {"show_endoom"}, 0, NULL, default_endoom_strings},

  {"<- PREV",S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {gen_settings2}},
  {"NEXT ->",S_SKIP|S_NEXT, m_null, M_X_NEXT, M_Y_PREVNEXT, {gen_settings4}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t gen_settings4[] = { // General Settings screen4

  {"Quality of life"  ,S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Strict Mode", S_YESNO|S_LEVWARN, m_null, M_X,
   M_Y + gen4_strictmode*M_SPC, {"strictmode"}},

  {"Screen melt", S_YESNO|S_STRICT, m_null, M_X,
   M_Y + gen4_screen_melt*M_SPC, {"screen_melt"}},

  {"On death action", S_CHOICE, m_null, M_X,
   M_Y + gen4_death_action*M_SPC, {"death_use_action"}, 0, NULL, death_use_action_strings},

  {"Show demo progress bar", S_YESNO, m_null, M_X,
   M_Y + gen4_demobar*M_SPC, {"demobar"}},

  {"Pain/pickup/powerup flashes", S_YESNO|S_STRICT, m_null, M_X,
   M_Y + gen4_palette_changes*M_SPC, {"palette_changes"}},

  {"Level Brightness", S_THERMO|S_STRICT, m_null, M_X_THRM,
   M_Y + gen4_level_brightness*M_SPC, {"extra_level_brightness"}},

  {"Organize save files", S_YESNO|S_PRGWARN, m_null, M_X,
   M_Y + gen4_organize_savefiles*M_SPC, {"organize_savefiles"}},

  {"", S_SKIP, m_null, M_X, M_Y + gen4_end1*M_SPC},

  {"Compatibility-breaking Features"  ,S_SKIP|S_TITLE, m_null, M_X,
   M_Y + gen4_title2*M_SPC},

  {"Walk Under Solid Hanging Bodies", S_YESNO|S_STRICT|S_CRITICAL, m_null, M_X,
   M_Y + gen4_hangsolid*M_SPC, {"hangsolid"}},

  {"Improved Hit Detection", S_YESNO|S_STRICT|S_CRITICAL, m_null, M_X,
   M_Y + gen4_blockmapfix*M_SPC, {"blockmapfix"}},

  {"Direct Vertical Aiming", S_YESNO|S_STRICT|S_CRITICAL, m_null, M_X,
   M_Y + gen4_verticalaim*M_SPC, {"direct_vertical_aiming"}},

  {"Pistol Start", S_YESNO|S_STRICT|S_CRITICAL, m_null, M_X,
   M_Y + gen4_pistolstart*M_SPC, {"pistolstart"}},

  {"<- PREV",S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {gen_settings3}},
  {"NEXT ->",S_SKIP|S_NEXT, m_null, M_X_NEXT, M_Y_PREVNEXT, {gen_settings5}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t gen_settings5[] = { // General Settings screen5

  {"Mouse Settings"     ,S_SKIP|S_TITLE, m_null, M_X, M_Y},

  // [FG] double click acts as "use"
  {"Double Click acts as \"Use\"", S_YESNO, m_null, M_X,
   M_Y+ gen5_mouse1*M_SPC, {"dclick_use"}},

  {"Permanent Mouselook", S_YESNO, m_null, M_X,
   M_Y+ gen5_mouse2*M_SPC, {"mouselook"}, 0, M_UpdateMouseLook},

  // [FG] invert vertical axis
  {"Invert vertical axis", S_YESNO, m_null, M_X,
   M_Y+ gen5_mouse3*M_SPC, {"mouse_y_invert"}},

  {"Mouse acceleration", S_THERMO, m_null, M_X_THRM,
   M_Y + gen5_mouse_accel * M_SPC, {"mouse_acceleration"}, 0, NULL, mouse_accel_strings},

  {"Mouse threshold", S_NUM, m_null, M_X,
   M_Y + gen5_mouse_accel_threshold * M_SPC, {"mouse_acceleration_threshold"}},

  {"", S_SKIP, m_null, M_X, M_Y + gen5_end1*M_SPC},

  {"Miscellaneous"  ,S_SKIP|S_TITLE, m_null, M_X, M_Y + gen5_title2*M_SPC},

  {"Game speed, percentage of normal", S_NUM|S_STRICT, m_null, M_X,
   M_Y + gen5_realtic*M_SPC, {"realtic_clock_rate"}, 0, M_ResetTimeScale},

  {"Default compatibility", S_CHOICE|S_LEVWARN, m_null, M_X,
   M_Y + gen5_compat*M_SPC, {"default_complevel"}, 0, NULL, default_compatibility_strings},

  {"Default skill level", S_CHOICE|S_LEVWARN, m_null, M_X,
   M_Y + gen5_skill*M_SPC, {"default_skill"}, 0, NULL, default_skill_strings},

  {"Player Name", S_STRING, m_null, M_X,
   M_Y + gen5_playername*M_SPC, {"net_player_name"}, 0, HU_InitObituaries},

  {"<- PREV",S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {gen_settings4}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

void M_Trans(void) // To reset translucency after setting it in menu
{
    R_InitTranMap(0);

    D_SetPredefinedTranslucency();
}

// Setting up for the General screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_General(int choice)
{
  M_SetupNextMenu(&GeneralDef);

  setup_active = true;
  setup_screen = ss_gen;
  set_general_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(gen_settings);
  current_setup_menu = gen_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawGeneral(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(114,2,"M_GENERL","GENERAL");
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

#define C_X  284
#define COMP_SPC 12

setup_menu_t comp_settings1[], comp_settings2[], comp_settings3[];

setup_menu_t* comp_settings[] =
{
  comp_settings1,
  comp_settings2,
  comp_settings3,
  NULL
};

enum
{
  comp1_telefrag,
  comp1_dropoff,
  comp1_falloff,
  comp1_staylift,
  comp1_doorstuck,
  comp1_pursuit,
  comp1_vile,
  comp1_pain,
  comp1_skull,
  comp1_god,
};

setup_menu_t comp_settings1[] =  // Compatibility Settings screen #1
{
  {"Any monster can telefrag on MAP30", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_telefrag * COMP_SPC, {"comp_telefrag"}},

  {"Some objects never hang over tall ledges", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_dropoff * COMP_SPC, {"comp_dropoff"}},

  {"Objects don't fall under their own weight", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_falloff * COMP_SPC, {"comp_falloff"}},

  {"Monsters randomly walk off of moving lifts", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_staylift * COMP_SPC, {"comp_staylift"}},

  {"Monsters get stuck on doortracks", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_doorstuck * COMP_SPC, {"comp_doorstuck"}},

  {"Monsters don't give up pursuit of targets", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_pursuit * COMP_SPC, {"comp_pursuit"}},

  {"Arch-Vile resurrects invincible ghosts", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_vile * COMP_SPC, {"comp_vile"}},

  {"Pain Elemental limited to 20 lost souls", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_pain * COMP_SPC, {"comp_pain"}},

  {"Lost souls get stuck behind walls", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_skull * COMP_SPC, {"comp_skull"}},

  {"God mode isn't absolute", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp1_god * COMP_SPC, {"comp_god"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT, m_null, M_X_NEXT, M_Y_PREVNEXT, {comp_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

enum
{
  comp2_infcheat,
  comp2_zombie,
  comp2_stairs,
  comp2_floors,
  comp2_model,
  comp2_zerotags,
  comp2_cosmetic,
  comp2_blazing,
  comp2_doorlight,
  comp2_skymap,
  comp2_menu,
};

setup_menu_t comp_settings2[] =  // Compatibility Settings screen #2
{
  {"Powerup cheats are not infinite duration", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_infcheat * COMP_SPC, {"comp_infcheat"}},

  {"Zombie players can exit levels", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_zombie * COMP_SPC, {"comp_zombie"}},

  {"Use exactly Doom's stairbuilding method", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_stairs * COMP_SPC, {"comp_stairs"}},

  {"Use exactly Doom's floor motion behavior", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_floors * COMP_SPC, {"comp_floors"}},

  {"Use exactly Doom's linedef trigger model", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_model * COMP_SPC, {"comp_model"}},

  {"Linedef effects work with sector tag = 0", S_YESNO|S_MBF, m_null, C_X,
   M_Y + comp2_zerotags * COMP_SPC, {"comp_zerotags"}},

  {"Cosmetic", S_SKIP|S_TITLE, m_null, C_X,
   M_Y + comp2_cosmetic * COMP_SPC},

  {"Blazing doors make double closing sounds", S_YESNO|S_COSMETIC, m_null, C_X,
   M_Y + comp2_blazing * COMP_SPC, {"comp_blazing"}},

  {"Tagged doors don't trigger special lighting", S_YESNO|S_COSMETIC, m_null, C_X,
   M_Y + comp2_doorlight * COMP_SPC, {"comp_doorlight"}},

  {"Sky is unaffected by invulnerability", S_YESNO|S_COSMETIC, m_null, C_X,
   M_Y + comp2_skymap * COMP_SPC, {"comp_skymap"}},

  {"Use Doom's main menu ordering", S_YESNO, m_null, C_X,
   M_Y + comp2_menu * COMP_SPC, {"traditional_menu"}, 0, M_ResetMenu},

  {"<- PREV", S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {comp_settings1}},
  {"NEXT ->", S_SKIP|S_NEXT, m_null, M_X_NEXT, M_Y_PREVNEXT, {comp_settings3}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

enum
{
  comp3_emu0,
  comp3_emu1,
  comp3_emu2,
  comp3_emu3,
  comp3_emu4,
  comp3_emu5,
};

static void M_UpdateCriticalItems(void)
{
  DISABLE_ITEM(demo_compatibility && overflow[emu_intercepts].enabled,
               gen_settings4[gen4_blockmapfix]);
}

setup_menu_t comp_settings3[] =  // Compatibility Settings screen #3
{
  {"Overflow Emulation", S_SKIP|S_TITLE, m_null, C_X,
   M_Y + comp3_emu0 * COMP_SPC},

  {"Emulate SPECHITS overflow", S_YESNO|S_VANILLA, m_null, C_X,
   M_Y + comp3_emu1 * COMP_SPC, {"emu_spechits"}},

  {"Emulate REJECT overflow", S_YESNO|S_LEVWARN|S_VANILLA, m_null, C_X,
   M_Y + comp3_emu2 * COMP_SPC, {"emu_reject"}},

  {"Emulate INTERCEPTS overflow", S_YESNO|S_VANILLA, m_null, C_X,
   M_Y + comp3_emu3 * COMP_SPC, {"emu_intercepts"}, 0, M_UpdateCriticalItems},

  {"Enable missed backside emulation", S_YESNO|S_LEVWARN|S_VANILLA, m_null, C_X,
   M_Y + comp3_emu4 * COMP_SPC, {"emu_missedbackside"}},

  {"Enable donut overrun emulation", S_YESNO|S_LEVWARN|S_VANILLA, m_null, C_X,
   M_Y + comp3_emu5 * COMP_SPC, {"emu_donut"}},

  {"<- PREV", S_SKIP|S_PREV, m_null, M_X_PREV, M_Y_PREVNEXT, {comp_settings2}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};


// Setting up for the Compatibility screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Compat(int choice)
{
  M_SetupNextMenu(&CompatDef);

  setup_active = true;
  setup_screen = ss_comp;
  set_general_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(comp_settings);
  current_setup_menu = comp_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Compatibility Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawCompat(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(52,2,"M_COMPAT","DOOM COMPATIBILITY");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Messages table.

// killough 11/98: enumerated

enum {
  mess_secret,
  mess_showtoggle,
  mess_showpickup,
  mess_showobituary,
  mess_stub1,
  mess_centered,
  mess_colorized,
  mess_color_play,
  mess_timer,
  mess_color_chat,
  mess_chat_timer,
  mess_color_obituary,
  mess_stub2,
  mess_list,
  mess_lines
};

setup_menu_t mess_settings1[];

setup_menu_t* mess_settings[] =
{
  mess_settings1,
  NULL
};

static void M_UpdateMultiLineMsgItem(void)
{
  DISABLE_ITEM(!message_list, mess_settings1[mess_lines]);
}

setup_menu_t mess_settings1[] =  // Messages screen       
{
  {"\"A Secret is Revealed!\" Message", S_YESNO, m_null, M_X, 
   M_Y + mess_secret*M_SPC, {"hud_secret_message"}},

  {"Show Toggle Messages", S_YESNO, m_null, M_X, 
   M_Y + mess_showtoggle*M_SPC, {"show_toggle_messages"}},

  {"Show Pickup Messages", S_YESNO, m_null, M_X, 
   M_Y + mess_showpickup*M_SPC, {"show_pickup_messages"}},

  {"Show Obituaries", S_YESNO, m_null, M_X,
   M_Y + mess_showobituary*M_SPC, {"show_obituary_messages"}},

  {"", S_SKIP, m_null, M_X, M_Y + mess_stub1*M_SPC},

  {"Center Messages", S_YESNO, m_null, M_X,
   M_Y + mess_centered*M_SPC, {"message_centered"}},

  {"Colorize Player Messages", S_YESNO, m_null, M_X,
   M_Y + mess_colorized*M_SPC, {"message_colorized"}, 0, HU_ResetMessageColors},

  {"Message Color During Play", S_CRITEM|S_COSMETIC, m_null, M_X,
   M_Y + mess_color_play*M_SPC, {"hudcolor_mesg"}, 0, NULL, hudcolor_str},

  {"Message Duration During Play (ms)", S_NUM, m_null, M_X,
   M_Y  + mess_timer*M_SPC, {"message_timer"}},

  {"Chat Message Color", S_CRITEM|S_COSMETIC, m_null, M_X,
   M_Y + mess_color_chat*M_SPC, {"hudcolor_chat"}, 0, NULL, hudcolor_str},

  {"Chat Message Duration (ms)", S_NUM|S_COSMETIC, m_null, M_X,
   M_Y  + mess_chat_timer*M_SPC, {"chat_msg_timer"}},

  {"Obituary Color", S_CRITEM|S_COSMETIC, m_null, M_X,
   M_Y + mess_color_obituary*M_SPC, {"hudcolor_obituary"}, 0, NULL, hudcolor_str},

  {"", S_SKIP, m_null, M_X, M_Y + mess_stub2*M_SPC},

  {"Multi-Line Messages", S_YESNO, m_null, M_X,
   M_Y + mess_list*M_SPC, {"message_list"}, 0, M_UpdateMultiLineMsgItem},

  {"Number of Lines", S_NUM, m_null,  M_X,
   M_Y + mess_lines*M_SPC, {"hud_msg_lines"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};


// Setting up for the Messages screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Messages(int choice)
{
  M_SetupNextMenu(&MessageDef);

  setup_active = true;
  setup_screen = ss_mess;
  set_mess_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(mess_settings);
  current_setup_menu = mess_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Messages Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawMessages(void)

{
  inhelpscreens = true;
  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(103,2,"M_MESS","MESSAGES");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);
  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Chat Strings table.

#define CS_X 20

setup_menu_t chat_settings1[];

setup_menu_t* chat_settings[] =
{
  chat_settings1,
  NULL
};

setup_menu_t chat_settings1[] =  // Chat Strings screen       
{
  {"1",S_STRING|S_COSMETIC,m_null,CS_X,M_Y, {"chatmacro1"}},
  {"2",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 1*M_SPC, {"chatmacro2"}},
  {"3",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 2*M_SPC, {"chatmacro3"}},
  {"4",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 3*M_SPC, {"chatmacro4"}},
  {"5",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 4*M_SPC, {"chatmacro5"}},
  {"6",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 5*M_SPC, {"chatmacro6"}},
  {"7",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 6*M_SPC, {"chatmacro7"}},
  {"8",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 7*M_SPC, {"chatmacro8"}},
  {"9",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 8*M_SPC, {"chatmacro9"}},
  {"0",S_STRING|S_COSMETIC,m_null,CS_X,M_Y+ 9*M_SPC, {"chatmacro0"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

// Setting up for the Chat Strings screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_ChatStrings(int choice)
{
  M_SetupNextMenu(&ChatStrDef);
  setup_active = true;
  setup_screen = ss_chat;
  set_chat_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(chat_settings);
  current_setup_menu = chat_settings[mult_screens_index];
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Chat Strings Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawChatStrings(void)

{
  inhelpscreens = true;
  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  M_DrawTitle(83,2,"M_CHAT","CHAT STRINGS");
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

void M_SelectDone(setup_menu_t* ptr)
{
  ptr->m_flags &= ~S_SELECT;
  ptr->m_flags |= S_HILITE;
  S_StartSound(NULL,sfx_itemup);
  setup_select = false;
  colorbox_active = false;
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
  mess_settings,
  chat_settings,
  gen_settings,      // killough 10/98
  comp_settings,
};

// phares 4/19/98:
// M_ResetDefaults() resets all values for a setup screen to default values
// 
// killough 10/98: rewritten to fix bugs and warn about pending changes

void M_ResetDefaults()
{
  default_t *dp;
  int warn = 0;

  // Look through the defaults table and reset every variable that
  // belongs to the group we're interested in.
  //
  // killough: However, only reset variables whose field in the
  // current setup screen is the same as in the defaults table.
  // i.e. only reset variables really in the current setup screen.

  for (dp = defaults; dp->name; dp++)
    if (dp->setupscreen == setup_screen)
      {
	setup_menu_t **l, *p;
	for (l = setup_screens[setup_screen-1]; *l; l++)
	  for (p = *l; !(p->m_flags & S_END); p++)
	    if (p->m_flags & S_HASDEFPTR ? p->var.def == dp :
		p->var.m_key == &dp->location->i ||
		p->ident == dp->ident)
	      {
		if (dp->type == string)
		  free(dp->location->s),
		    dp->location->s = strdup(dp->defaultvalue.s);
		else if (dp->type == number)
		  dp->location->i = dp->defaultvalue.i;
		else if (dp->type == input)
		  M_InputSet(dp->ident, dp->inputs);
	
		if (p->m_flags & (S_LEVWARN | S_PRGWARN))
		  warn |= p->m_flags & (S_LEVWARN | S_PRGWARN);
		else
		  if (dp->current)
		  {
		    if (allow_changes())
		    {
		      if (dp->type == string)
		      dp->current->s = dp->location->s;
		      else if (dp->type == number)
		      dp->current->i = dp->location->i;
		    }
		    else
		      warn |= S_LEVWARN;
		  }
		
		if (p->action)
		  p->action();

		goto end;
	      }
      end:;
      }
  
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

int extended_help_count;   // number of user-defined help screens found
int extended_help_index;   // index of current extended help screen

menuitem_t ExtHelpMenu[] =
{
  {1,"",M_ExtHelpNextScreen,0}
};

menu_t ExtHelpDef =
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

void M_ExtHelpNextScreen(int choice)
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

void M_InitExtendedHelp(void)

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

void M_ExtHelp(int choice)
{
  choice = 0;
  extended_help_index = 1; // Start with first extended help screen
  M_SetupNextMenu(&ExtHelpDef);
}

// Initialize the drawing part of the extended HELP screens.

void M_DrawExtHelp(void)
{
  char namebfr[] = "HELPnn"; // [FG] char array!

  inhelpscreens = true;              // killough 5/1/98
  namebfr[4] = extended_help_index/10 + 0x30;
  namebfr[5] = extended_help_index%10 + 0x30;
  V_DrawPatchFullScreen(0,W_CacheLumpName(namebfr,PU_CACHE));
}

//
// End of Extended HELP screens               // phares 3/30/98
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// Dynamic HELP screen                     // phares 3/2/98
//
// Rather than providing the static HELP screens from DOOM and its versions,
// BOOM provides the player with a dynamic HELP screen that displays the
// current settings of major key bindings.
//
// The Dynamic HELP screen is defined in a manner similar to that used for
// the Setup Screens above.
//
// M_GetKeyString finds the correct string to represent the key binding
// for the current item being drawn.

int M_GetKeyString(int c,int offset)
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

      // Retrieve 4-letter (max) string representing the key

      s = M_GetNameForKey(c);
      if (!s)
        s = "JUNK";

      strcpy(&menu_buffer[offset],s); // string to display
      offset += strlen(s);
    }
  return offset;
}

//
// The Dynamic HELP screen table.

#define KT_X1  87
#define KT_X2  247
#define KT_X3  187

#define KT_Y0  8
#define KT_Y1  (KT_Y0 +  2 * M_SPC)
#define KT_Y2  (KT_Y0 + 12 * M_SPC)
#define KT_Y3  (KT_Y0 + 17 * M_SPC)

setup_menu_t helpstrings[] =  // HELP screen strings       
{
  {"MENU"        ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y0,{0},input_escape},
  {"AUTOMAP"     ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y0,{0},input_map},

  {"ACTION"      ,S_SKIP|S_TITLE,m_null,KT_X1,KT_Y1},
  {"FIRE"        ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 1*M_SPC,{0},input_fire},
  {"FORWARD"     ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 2*M_SPC,{0},input_forward},
  {"BACKWARD"    ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 3*M_SPC,{0},input_backward},
  {"STRAFE LEFT" ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 4*M_SPC,{0},input_strafeleft},
  {"STRAFE RIGHT",S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 5*M_SPC,{0},input_straferight},
  {"RUN"         ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 6*M_SPC,{0},input_speed},
  {"AUTORUN"     ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 7*M_SPC,{0},input_autorun},
  {"USE"         ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y1+ 8*M_SPC,{0},input_use},

  {"WEAPONS"     ,S_SKIP|S_TITLE,m_null,KT_X1,KT_Y2},
  {"FIST"        ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 1*M_SPC,{0},input_weapon1},
  {"PISTOL"      ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 2*M_SPC,{0},input_weapon2},
  {"SHOTGUN"     ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 3*M_SPC,{0},input_weapon3},
  {"CHAINGUN"    ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 4*M_SPC,{0},input_weapon4},
  {"ROCKET"      ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 5*M_SPC,{0},input_weapon5},
  {"PLASMA"      ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 6*M_SPC,{0},input_weapon6},
  {"BFG 9000"    ,S_SKIP|S_INPUT,m_null,KT_X1,KT_Y2+ 7*M_SPC,{0},input_weapon7},

  {"GAME"        ,S_SKIP|S_TITLE,m_null,KT_X2,KT_Y1},
  {"SAVE"        ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y1+ 1*M_SPC,{0},input_savegame},
  {"LOAD"        ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y1+ 2*M_SPC,{0},input_loadgame},
  {"QUICKSAVE"   ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y1+ 3*M_SPC,{0},input_quicksave},
  {"QUICKLOAD"   ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y1+ 4*M_SPC,{0},input_quickload},
  {"QUIT"        ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y1+ 5*M_SPC,{0},input_quit},

  {"SCREEN"      ,S_SKIP|S_TITLE,m_null,KT_X2,KT_Y2},
  {"SOUND VOLUME",S_SKIP|S_INPUT,m_null,KT_X2,KT_Y2+ 1*M_SPC,{0},input_soundvolume},
  {"HUD"         ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y2+ 2*M_SPC,{0},input_hud},
  {"GAMMA FIX"   ,S_SKIP|S_INPUT,m_null,KT_X2,KT_Y2+ 3*M_SPC,{0},input_gamma},

  {"NEXT"        ,S_SKIP|S_INPUT,m_null,KT_X3,KT_Y2+ 6*M_SPC,{0},input_nextweapon},
  {"PREV"        ,S_SKIP|S_INPUT,m_null,KT_X3,KT_Y2+ 7*M_SPC,{0},input_prevweapon},


  // Final entry

  {0,S_SKIP|S_END,m_null}
};

#define SPACEWIDTH 4

static int menu_font_spacing = 0;

// M_DrawMenuString() draws the string in menu_buffer[]

void M_DrawStringCR(int cx, int cy, char *color, const char *ch)
{
  int   w;
  int   c;

  while (*ch)
    {
      c = *ch++;         // get next char
      c = toupper(c) - HU_FONTSTART;
      if (c < 0 || c> HU_FONTSIZE)
	{
	  cx += SPACEWIDTH;    // space
	  continue;
	}
      w = SHORT (hu_font[c]->width);
      if (cx + w > SCREENWIDTH)
	break;
    
      // V_DrawpatchTranslated() will draw the string in the
      // desired color, colrngs[color]
    
      V_DrawPatchTranslated(cx,cy,0,hu_font[c],color);

      // The screen is cramped, so trim one unit from each
      // character so they butt up against each other.
      cx += w + menu_font_spacing;
    }
}

void M_DrawString(int cx, int cy, int color, const char *ch)
{
  M_DrawStringCR(cx, cy, colrngs[color], ch);
}

// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it is not tied to menu_buffer

void M_DrawMenuString(int cx, int cy, int color)
{
  M_DrawString(cx, cy, color, menu_buffer);
}

static void M_DrawMenuStringEx(int flags, int x, int y, int color)
{
  if (ItemDisabled(flags))
    M_DrawStringCR(x, y, cr_dark, menu_buffer);
  else
    M_DrawMenuString(x, y, color);
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
      c = toupper(c) - HU_FONTSTART;
      if (c < 0 || c > HU_FONTSIZE)
	{
	  len += SPACEWIDTH;   // space
	  continue;
	}
      len += SHORT (hu_font[c]->width);
      len += menu_font_spacing; // adjust so everything fits
    }
  len -= menu_font_spacing; // replace what you took away on the last char only
  return len;
}

//
// M_DrawHelp
//
// This displays the help screen

void M_DrawHelp (void)
{
  // Display help screen from PWAD
  int helplump;
  if (gamemode == commercial)
    helplump = W_CheckNumForName("HELP");
  else
    helplump = W_CheckNumForName("HELP1");

  inhelpscreens = true;                        // killough 10/98
  if (helplump < 0 || W_IsIWADLump(helplump))
  {
  M_DrawBackground("FLOOR4_6", screens[0]);
  M_DrawScreenItems(helpstrings);
  }
  else
  {
    V_DrawPatchFullScreen(0, W_CacheLumpNum(helplump, PU_CACHE));
  }
}
  
//
// End of Dynamic HELP screen                // phares 3/2/98
//
////////////////////////////////////////////////////////////////////////////

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

setup_menu_t cred_settings[]={

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
  M_DrawBackground(gamemode==shareware ? "CEIL5_1" : "MFLR8_4", screens[0]);
  M_DrawTitle(42,9,"MBFTEXT",mbftext_s);
  M_DrawScreenItems(cred_settings);
}

enum
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
};

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
  int    action;
  int    i;
  int    s_input;
  static int joywait   = 0;
  static int repeat    = MENU_NULL;
  static int old_value = -1;
  static char *old_str = NULL;

  // "close" button pressed on window?
  if (ev->type == ev_quit)
  {
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
  }
  
  ch = -1; // will be changed to a legit char if we're going to use it here
  action = MENU_NULL;

  // Process joystick input

  if (ev->type == ev_joystick && joywait < I_GetTime())
    {
      if (repeat != MENU_NULL)
        {
          action = repeat;
          ch = 0;
          joywait = I_GetTime() + 2;
        }
    }
  else if (ev->type == ev_joyb_up)
    {
      if (M_InputDeactivated(input_menu_up) ||
          M_InputDeactivated(input_menu_down) ||
          M_InputDeactivated(input_menu_right) ||
          M_InputDeactivated(input_menu_left))
      {
        repeat = MENU_NULL;
      }
    }
  else if (ev->type == ev_joyb_down)
    {
      ch = 0;
    }
  else
    {

      // Process mouse input

      if (ev->type == ev_mouseb_down)
	{
          ch = 0; // meaningless, just to get you past the check for -1
	}
      else
        
	// Process keyboard input

	if (ev->type == ev_keydown)
	  {
	    ch = ev->data1;         // phares 4/11/98:
	    if (ch == KEY_RSHIFT)        // For chat string processing, need
	      shiftdown = true;           // to know when shift key is up or
	  }                             // down so you can get at the !,#,
	else if (ev->type == ev_keyup)  // etc. keys. Keydowns are allowed
	  if (ev->data1 == KEY_RSHIFT) // past this point, but keyups aren't
	    shiftdown = false;          // so we need to note the difference
    }                                 // here using the 'shiftdown' boolean.
  
  if (ch == -1)
    return false; // we can't use the event here

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
	      if (StartsWithMapIdentifier(savegamestrings[saveSlot]) && saveStringEnter == 1)
	        saveCharIndex = 0;
	      else
	        saveCharIndex--;
	      savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	}

      else if (action == MENU_ESCAPE)                    // phares 3/7/98
	{
	  saveStringEnter = 0;
	  strcpy(&savegamestrings[saveSlot][0],saveOldString);
	}
    
      else if (action == MENU_ENTER)                     // phares 3/7/98
	{
	  saveStringEnter = 0;
	  if (savegamestrings[saveSlot][0])
	    M_DoSave(saveSlot);
	}
    
      else
	{
	  ch = toupper(ch);
#if 0
	  // killough 11/98: removed useless code
	  if (ch != 32)
	    if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
	      ; // if true, do nothing
#endif
	  if (ch >= 32 && ch <= 127 &&
	      saveCharIndex < SAVESTRINGSIZE-1 &&
	      M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE-2)*8)
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
        ch = 'y';

      if (messageNeedsInput == true &&
	  !(ch == ' ' || ch == 'n' || ch == 'y' || ch == key_escape ||
	    action == MENU_BACKSPACE)) // phares
	return false;
  
      menuactive = messageLastMenuActive;
      messageToPrint = 0;
      if (messageRoutine)
	messageRoutine(ch);
    
      menuactive = false;
      S_StartSound(NULL,sfx_swtchx);
      return true;
    }

  // killough 2/22/98: add support for screenshot key:

  if ((devparm && ch == key_help) || M_InputActivated(input_screenshot))
    {
      G_ScreenShot ();
      // return true; // [FG] don't let toggles eat keys
    }

  if (M_InputActivated(input_clean_screenshot))
    {
      clean_screenshot = true;
      G_ScreenShot();
    }

  // If there is no active menu displayed...

  if (!menuactive && !chat_on)                                // phares
    {                                                         //  |
      static boolean fastdemo_timer = false;
	  
      if (M_InputActivated(input_autorun)) // Autorun         //  V
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

      if (M_InputActivated(input_mouselook))
	{
	  mouselook = !mouselook;
	  togglemsg("Mouselook %s", mouselook ? "On" : "Off");
	  M_UpdateMouseLook();
	  // return true; // [FG] don't let toggles eat keys
	}

      if (M_InputActivated(input_padlook))
	{
	  padlook = !padlook;
	  togglemsg("Padlook %s", padlook ? "On" : "Off");
	  M_UpdateMouseLook();
	  // return true; // [FG] don't let toggles eat keys
	}

      if (ch == key_help)      // Help key
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
	  S_StartSound(NULL,sfx_swtchn);
	  return true;
	}
    
      if (M_InputActivated(input_quickload))      // Quickload
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_QuickLoad();
	  return true;
	}
    
      if (M_InputActivated(input_quit))       // Quit DOOM
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_QuitDOOM(0);
	  return true;
	}
    
      if (M_InputActivated(input_gamma))       // gamma toggle
	{
	  gamma2++;
	  if (gamma2 > 17)
	    gamma2 = 0;
	  togglemsg("Gamma correction level %s", gamma_strings[gamma2]);
	  M_ResetGamma();
	  return true;                      
	}


      if (M_InputActivated(input_zoomout))     // zoom out
	{
	  if (automapactive)
	    return false;
	  M_SizeDisplay(0);
	  S_StartSound(NULL,sfx_stnmov);
	  return true;
	}
    
      if (M_InputActivated(input_zoomin))               // zoom in
	{                                 // jff 2/23/98
	  if (automapactive)                // allow
	    return false;                   // key_hud==key_zoomin
	  M_SizeDisplay(1);                                             //  ^
	  S_StartSound(NULL,sfx_stnmov);                                //  |
	  return true;                                            // phares
	}
                                  
      if (M_InputActivated(input_hud))   // heads-up mode       
	{                    
	  if (automap_on)    // jff 2/22/98
	    return false;                  // HUD mode control
	  if (screenSize<8)                // function on default F5
	    while (screenSize<8 || !hud_displayed) // make hud visible
	      M_SizeDisplay(1);            // when configuring it
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
	  if (hud_level_stats || hud_level_time)
	    hud_level_stats = hud_level_time = 0;
	  else
	    hud_level_stats = hud_level_time = 1;
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
			return true;
	}

        if (M_InputActivated(input_demo_fforward))
        {
          if (demoplayback && !PLAYBACK_SKIP && !fastdemo
              && !D_CheckNetConnect())
          {
            fastdemo_timer = !fastdemo_timer;
            I_SetFastdemoTimer(fastdemo_timer);
            return true;
          }
        }

        if (M_InputActivated(input_speed_up) && !D_CheckNetConnect()
            && !strictmode)
        {
          realtic_clock_rate += 10;
          realtic_clock_rate = BETWEEN(10, 1000, realtic_clock_rate);
          displaymsg("Game Speed: %d", realtic_clock_rate);
          I_SetTimeScale(realtic_clock_rate);
        }

        if (M_InputActivated(input_speed_down) && !D_CheckNetConnect()
            && !strictmode)
        {
          realtic_clock_rate -= 10;
          realtic_clock_rate = BETWEEN(10, 1000, realtic_clock_rate);
          displaymsg("Game Speed: %d", realtic_clock_rate);
          I_SetTimeScale(realtic_clock_rate);
        }

        if (M_InputActivated(input_speed_default) && !D_CheckNetConnect()
            && !strictmode)
        {
          realtic_clock_rate = 100;
          displaymsg("Game Speed: %d", realtic_clock_rate);
          I_SetTimeScale(realtic_clock_rate);
        }
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

  // [FG] delete a savegame

  if (currentMenu == &LoadDef || currentMenu == &SaveDef)
  {
    if (delete_verify)
    {
      if (toupper(ch) == 'Y')
      {
        M_DeleteGame(itemOn);
        S_StartSound(NULL,sfx_itemup);
        delete_verify = false;
      }
      else if (toupper(ch) == 'N')
      {
        S_StartSound(NULL,sfx_itemup);
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
        S_StartSound(NULL,sfx_pstop);
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
        S_StartSound(NULL,sfx_pstop);
      }
      return true;
    }
  }

  // phares 3/26/98 - 4/11/98:
  // Setup screen key processing

  if (setup_active)
    {
      setup_menu_t* ptr1= current_setup_menu + set_menu_itemon;
      setup_menu_t* ptr2 = NULL;

      // phares 4/19/98:
      // Catch the response to the 'reset to default?' verification
      // screen

      if (default_verify)
	{
	  if (toupper(ch) == 'Y')
	    {
	      M_ResetDefaults();
	      default_verify = false;
	      M_SelectDone(ptr1);
	    }
	  else if (toupper(ch) == 'N')
	    {
	      default_verify = false;
	      M_SelectDone(ptr1);
	    }
	  return true;
	}

      // Common processing for some items

      if (setup_select) // changing an entry
	{
	  if (action == MENU_ESCAPE) // Exit key = no change
	    {
	      if (ptr1->m_flags & (S_CHOICE|S_CRITEM) &&
		  old_value != -1)
		{
		    ptr1->var.def->location->i = old_value;
		    old_value = -1;
		}
	      if (ptr1->m_flags & S_STRING && old_str != NULL)
		{
		    ptr1->var.def->location->s = old_str;
		    old_str = NULL;

		    free(chat_string_buffer);
		    chat_string_buffer = NULL;

		    // [FG] call action routine for string changes
		    if (ptr1->action)
		        ptr1->action();
		}
	      M_SelectDone(ptr1);                           // phares 4/17/98
	      setup_gather = false;   // finished gathering keys, if any
	      return true;
	    }

	  if (ptr1->m_flags & S_YESNO) // yes or no setting?
	    {
	      if (action == MENU_ENTER)
		{
		  ptr1->var.def->location->i = !ptr1->var.def->location->i; // killough 8/15/98

		  // phares 4/14/98:
		  // If not in demoplayback, demorecording, or netgame,
		  // and there's a second variable in var2, set that
		  // as well
		  
		  // killough 8/15/98: add warning messages

		  if (ptr1->m_flags & (S_LEVWARN | S_PRGWARN))
		    warn_about_changes(ptr1->m_flags &    // killough 10/98
				       (S_LEVWARN | S_PRGWARN));
		  else
		    if (ptr1->var.def->current)
		    {
		      if (allow_changes())  // killough 8/15/98
			ptr1->var.def->current->i = ptr1->var.def->location->i;
		      else
			if (ptr1->var.def->current->i != ptr1->var.def->location->i)
			  warn_about_changes(S_LEVWARN); // killough 8/15/98
		    }

		  if (ptr1->action)      // killough 10/98
		    ptr1->action();
		}
	      M_SelectDone(ptr1);                           // phares 4/17/98
	      return true;
	    }

	  // [FG] selection of choices

	  if (ptr1->m_flags & (S_CHOICE|S_CRITEM|S_THERMO))
	    {
	      int value = ptr1->var.def->location->i;

	      if (!(ptr1->m_flags & S_THERMO) && old_value == -1)
	        old_value = value;

	      if (action == MENU_LEFT)
		{
		  value--;
		  if (ptr1->var.def->limit.min != UL &&
		      value < ptr1->var.def->limit.min)
			value = ptr1->var.def->limit.min;
		  if (ptr1->var.def->location->i != value)
			S_StartSound(NULL,sfx_pstop);
		  ptr1->var.def->location->i = value;

		  if (ptr1->m_flags & S_THERMO && ptr1->action)
		    ptr1->action();
		}
	      if (action == MENU_RIGHT)
		{
		  value++;
		  if (ptr1->var.def->limit.max != UL &&
		      value > ptr1->var.def->limit.max)
			value = ptr1->var.def->limit.max;
		  else
		    if (ptr1->var.def->limit.max != UL &&
		        value < ptr1->var.def->limit.max &&
		        ptr1->selectstrings &&
		        ptr1->selectstrings[value] == NULL)
			  value--;
		  if (ptr1->var.def->location->i != value)
			S_StartSound(NULL,sfx_pstop);
		  ptr1->var.def->location->i = value;

		  if (ptr1->m_flags & S_THERMO && ptr1->action)
		    ptr1->action();
		}
	      if (action == MENU_ENTER)
		{
		  if (ptr1->m_flags & (S_LEVWARN | S_PRGWARN))
		    warn_about_changes(ptr1->m_flags & (S_LEVWARN | S_PRGWARN));
		  else
		    if (ptr1->var.def->current)
		    {
		      if (allow_changes())
			ptr1->var.def->current->i = ptr1->var.def->location->i;
		      else
			if (ptr1->var.def->current->i != ptr1->var.def->location->i)
			  warn_about_changes(S_LEVWARN);
		    }

		  if (ptr1->m_flags & (S_CHOICE|S_CRITEM) && ptr1->action)
		    ptr1->action();
		  M_SelectDone(ptr1);
		  old_value = -1;
		}
	      return true;
	    }
#if 0
	  if (ptr1->m_flags & S_CRITEM)
	    {
	      if (action != MENU_ENTER)
		{
		  ch -= 0x30; // out of ascii
		  if (ch < 0 || ch > 9)
		    return true; // ignore
		  ptr1->var.def->location->i = ch;
		}
	      if (ptr1->action)      // killough 10/98
		ptr1->action();
	      M_SelectDone(ptr1);                      // phares 4/17/98
	      return true;
	    }
#endif
	  if (ptr1->m_flags & S_NUM) // number?
	    {
	      if (setup_gather) // gathering keys for a value?
		{
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

			  if ((ptr1->var.def->limit.min != UL &&
			       value < ptr1->var.def->limit.min) ||
			      (ptr1->var.def->limit.max != UL &&
			       value > ptr1->var.def->limit.max))
			    warn_about_changes(S_BADVAL);
			  else
			    {
			      ptr1->var.def->location->i = value;

			      // killough 8/9/98: fix numeric vars
			      // killough 8/15/98: add warning message

			      if (ptr1->m_flags & (S_LEVWARN | S_PRGWARN))
				warn_about_changes(ptr1->m_flags &
						   (S_LEVWARN | S_PRGWARN));
			      else
				if (ptr1->var.def->current)
				{
				  if (allow_changes())  // killough 8/15/98
				    ptr1->var.def->current->i = value;
				  else
				    if (ptr1->var.def->current->i != value)
				      warn_about_changes(S_LEVWARN);
				}

			      if (ptr1->action)      // killough 10/98
				ptr1->action();
			    }
			}
		      M_SelectDone(ptr1);     // phares 4/17/98
		      setup_gather = false; // finished gathering keys
		      return true;
		    }

		  if (action == MENU_BACKSPACE && gather_count)
		    {
		      gather_count--;
		      return true;
		    }

		  if (gather_count >= MAXGATHER)
		    return true;

		  if (!isdigit(ch) && ch != '-')
		    return true; // ignore

		  // killough 10/98: character-based numerical input
		  gather_buffer[gather_count++] = ch;
		}
	      return true;
	    }
	}

      // Key Bindings

      s_input = (ptr1->m_flags & S_INPUT) ? ptr1->ident : 0;

      if (set_keybnd_active) // on a key binding setup screen
	if (setup_select)    // incoming key or button gets bound
	  {
	    if (ev->type == ev_joyb_down)
	      {
		int i,group;
		boolean search = true;
      
		if (!s_input)
		  return true; // not a legal action here (yet)
      
		// see if the button is already bound elsewhere. if so, you
		// have to swap bindings so the action where it's currently
		// bound doesn't go dead. Since there is more than one
		// keybinding screen, you have to search all of them for
		// any duplicates. You're only interested in the items
		// that belong to the same group as the one you're changing.
      
		group  = ptr1->m_group;
		if ((ch = ev->data1) == -1)
		  return true;

		if (M_InputMatchJoyB(s_input, ch))
		  {
		    M_InputRemoveJoyB(s_input, ch);
		    M_SelectDone(ptr1);
		    return true;
		  }

		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_group == group && ptr1 != ptr2)
		      if (ptr2->m_flags & S_INPUT)
			if (M_InputMatchJoyB(ptr2->ident, ch))
			  {
			    M_InputRemoveJoyB(ptr2->ident, ch);
			    search = false;
			    break;
			  }
		if (!M_InputAddJoyB(s_input, ch))
		  {
		    M_InputReset(s_input);
		    M_InputAddJoyB(s_input, ch);
		  }
	      }
	    else if (ev->type == ev_mouseb_down)
	      {
		int i,group;
		boolean search = true;

		if (!s_input)
		  return true; // not a legal action here (yet)

		// see if the button is already bound elsewhere. if so, you
		// have to swap bindings so the action where it's currently
		// bound doesn't go dead. Since there is more than one
		// keybinding screen, you have to search all of them for
		// any duplicates. You're only interested in the items
		// that belong to the same group as the one you're changing.

		group  = ptr1->m_group;
		if ((ch = ev->data1) == -1)
		  return true;

		if (M_InputMatchMouseB(s_input, ch))
		  {
		    M_InputRemoveMouseB(s_input, ch);
		    M_SelectDone(ptr1);
		    return true;
		  }

		// Don't bind movement and turning to mouse wheel. It needs to
		// be impossible to input a one-frame of movement automatically
		// in speedrunning.
		if (M_IsMouseWheel(ch) &&
		    s_input >= input_forward && s_input <= input_straferight)
		  return true;
		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_group == group && ptr1 != ptr2)
		      if (ptr2->m_flags & S_INPUT)
			if (M_InputMatchMouseB(ptr2->ident, ch))
			  {
			    M_InputRemoveMouseB(ptr2->ident, ch);
			    search = false;
			    break;
			  }
		if (!M_InputAddMouseB(s_input, ch))
		  {
		    M_InputReset(s_input);
		    M_InputAddMouseB(s_input, ch);
		  }
	      }
	    else if (ev->type == ev_keydown) // keyboard key
	      {
		int i,group;
		boolean search = true;
        
		// see if 'ch' is already bound elsewhere. if so, you have
		// to swap bindings so the action where it's currently
		// bound doesn't go dead. Since there is more than one
		// keybinding screen, you have to search all of them for
		// any duplicates. You're only interested in the items
		// that belong to the same group as the one you're changing.

		// if you find that you're trying to swap with an action
		// that has S_KEEP set, you can't bind ch; it's already
		// bound to that S_KEEP action, and that action has to
		// keep that key.

		if (M_InputMatchKey(s_input, ch))
		  {
		    M_InputRemoveKey(s_input, ch);
		    M_SelectDone(ptr1);
		    return true;
		  }

		group  = ptr1->m_group;
		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_flags & (S_INPUT|S_KEEP) &&
			ptr2->m_group == group && 
			ptr1 != ptr2)
		      if (M_InputMatchKey(ptr2->ident, ch))
			{
			  if (ptr2->m_flags & S_KEEP)
			    return true; // can't have it!
			  M_InputRemoveKey(ptr2->ident, ch);
			  search = false;
			  break;
			}
		if (!M_InputAddKey(s_input, ch))
		  {
		    M_InputReset(s_input);
		    M_InputAddKey(s_input, ch);
		  }
	      }

	    M_SelectDone(ptr1);       // phares 4/17/98
	    return true;
	  }

      // Weapons

      if (set_weapon_active) // on the weapons setup screen
	if (setup_select) // changing an entry
	  {
	    if (action != MENU_ENTER)
	      {
		ch -= '0'; // out of ascii
		if (ch < 1 || ch > 9)
		  return true; // ignore

		// Plasma and BFG don't exist in shareware
		// killough 10/98: allow it anyway, since this
		// isn't the game itself, just setting preferences
            
		// see if 'ch' is already assigned elsewhere. if so,
		// you have to swap assignments.

		// killough 11/98: simplified

		for (i = 0; (ptr2 = weap_settings[i]); i++)
		  for (; !(ptr2->m_flags & S_END); ptr2++)
		    if (ptr2->m_flags & S_WEAP && 
			ptr2->var.def->location->i == ch && ptr1 != ptr2)
		      {
			ptr2->var.def->location->i = ptr1->var.def->location->i;
			goto end;
		      }
	      end:
		ptr1->var.def->location->i = ch;
	      }

	    M_SelectDone(ptr1);       // phares 4/17/98
	    return true;
	  }

      // Automap

      if (set_auto_active) // on the automap setup screen
	if (setup_select) // incoming key
	  {
	    if (action == MENU_DOWN)                
	      {
		if (++color_palette_y == 16)
		  color_palette_y = 0;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (action == MENU_UP)                
	      {
		if (--color_palette_y < 0)
		  color_palette_y = 15;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (action == MENU_LEFT)                
	      {
		if (--color_palette_x < 0)
		  color_palette_x = 15;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (action == MENU_RIGHT)                
	      {
		if (++color_palette_x == 16)
		  color_palette_x = 0;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (action == MENU_ENTER)               
	      {
		ptr1->var.def->location->i = color_palette_x + 16*color_palette_y;
		M_SelectDone(ptr1);                         // phares 4/17/98
		colorbox_active = false;
		return true;
	      }
	  }
      
      // killough 10/98: consolidate handling into one place:
      if (setup_select &&
	  set_enemy_active | set_general_active | set_chat_active | 
	  set_mess_active | set_status_active | set_compat_active)
	{
	  if (ptr1->m_flags & S_STRING) // creating/editing a string?
	    {
	      if (action == MENU_BACKSPACE) // backspace and DEL
		{
		  if (chat_string_buffer[chat_index] == 0)
		    {
		      if (chat_index > 0)
			chat_string_buffer[--chat_index] = 0;
		    }
		  // shift the remainder of the text one char left
		  else if (chat_index > 0)
		    {
		      char *str = &chat_string_buffer[chat_index];
		      strcpy(str - 1, str);
		      chat_index--;
		    }
		}
	      else if (ch == KEY_DEL)
		{
		  if (chat_string_buffer[chat_index])
		    {
		      char *str = &chat_string_buffer[chat_index];
		      strcpy(str, str + 1);
		    }
		}
	      else if (action == MENU_LEFT) // move cursor left
		{
		  if (chat_index > 0)
		    chat_index--;
		}
	      else if (action == MENU_RIGHT) // move cursor right
		{
		  if (chat_string_buffer[chat_index] != 0)
		    chat_index++;
		}
	      else if (action == MENU_ENTER)
		{
		  ptr1->var.def->location->s = chat_string_buffer;

		  if (old_str != NULL)
		  {
		      free(old_str);
		      old_str = NULL;
		  }

		  // [FG] call action routine for string changes
		  if (ptr1->action)
		      ptr1->action();

		  M_SelectDone(ptr1);   // phares 4/17/98
		}
	      
	      // Adding a char to the text. Has to be a printable
	      // char, and you can't overrun the buffer. If the
	      // chat string gets larger than what the screen can hold,
	      // it is dealt with when the string is drawn (above).
	      
	      else if ((ch >= 32) && (ch <= 126))
		if ((strlen(chat_string_buffer) + 1) < CHAT_STRING_BFR_SIZE)
		  {
		    if (shiftdown)
		      ch = shiftxform[ch];
		    if (chat_string_buffer[chat_index] == 0) 
		      {
			chat_string_buffer[chat_index++] = ch;
			chat_string_buffer[chat_index] = 0;
		      }
		    else
		      {
			char *str = &chat_string_buffer[chat_index];
			memmove(str + 1, str, strlen(str));
			chat_string_buffer[chat_index++] = ch;
		      }
		  }
	      return true;
	    }
	  
	  M_SelectDone(ptr1);       // phares 4/17/98
	  return true;
	}

      // Not changing any items on the Setup screens. See if we're
      // navigating the Setup menus or selecting an item to change.

      if (action == MENU_DOWN)
	{
	  ptr1->m_flags &= ~S_HILITE;     // phares 4/17/98
	  do
	    if (ptr1->m_flags & S_END)
	      {
		set_menu_itemon = 0;
		ptr1 = current_setup_menu;
	      }
	    else
	      {
		set_menu_itemon++;
		ptr1++;
	      }
	  while (ptr1->m_flags & S_SKIP);
	  M_SelectDone(ptr1);         // phares 4/17/98
	  return true;
	}
  
      if (action == MENU_UP)                 
	{
	  ptr1->m_flags &= ~S_HILITE;     // phares 4/17/98
	  do
	    {
	      if (set_menu_itemon == 0)
		do
		  set_menu_itemon++;
		while(!((current_setup_menu + set_menu_itemon)->m_flags & S_END));
	      set_menu_itemon--;
	    }
	  while((current_setup_menu + set_menu_itemon)->m_flags & S_SKIP);
	  M_SelectDone(current_setup_menu + set_menu_itemon);         // phares 4/17/98
	  return true;
	}

      // [FG] clear key bindings with the DEL key
      if (action == MENU_CLEAR)
	{
	  if (ptr1->m_flags & S_INPUT)
	  {
	    M_InputReset(ptr1->ident);
	  }

	  if (ptr1->m_flags & S_KEEP)
	  {
	    action = MENU_ENTER;
	  }
	  else
	  {
	    return true;
	  }
	}

      if (action == MENU_ENTER)               
	{
	  int flags = ptr1->m_flags;

	  // You've selected an item to change. Highlight it, post a new
	  // message about what to do, and get ready to process the
	  // change.
	  //
	  // killough 10/98: use friendlier char-based input buffer

	  if (ItemDisabled(flags))
	    {
	      S_StartSound(NULL,sfx_oof);
	      return true;
	    }
	  else if (flags & S_NUM)
	    {
	      setup_gather = true;
	      print_warning_about_changes = false;
	      gather_count = 0;
	    }
	  else if (flags & S_COLOR)
	    {
	      int color = ptr1->var.def->location->i;
        
	      if (color < 0 || color > 255) // range check the value
		color = 0;        // 'no show' if invalid

	      color_palette_x = ptr1->var.def->location->i & 15;
	      color_palette_y = ptr1->var.def->location->i >> 4;
	      colorbox_active = true;
	    }
	  else if (flags & S_STRING)
	    {
	      // copy chat string into working buffer; trim if needed.
	      // free the old chat string memory and replace it with
	      // the (possibly larger) new memory for editing purposes
	      //
	      // killough 10/98: fix bugs, simplify

	      if (old_str == NULL && ptr1->var.def->location->s != NULL)
	          old_str = M_StringDuplicate(ptr1->var.def->location->s);

	      chat_string_buffer = malloc(CHAT_STRING_BFR_SIZE);
	      strncpy(chat_string_buffer,
		      ptr1->var.def->location->s, CHAT_STRING_BFR_SIZE);

	      // guarantee null delimiter
	      chat_string_buffer[CHAT_STRING_BFR_SIZE-1] = 0;

	      // set chat table pointer to working buffer
	      // and free old string's memory.

	      free(ptr1->var.def->location->s);
	      ptr1->var.def->location->s = chat_string_buffer;
	      // current cursor position in chat_string_buffer
	      chat_index = strlen(chat_string_buffer);
	    }
	  else if (flags & S_RESET)
	    default_verify = true;

	  ptr1->m_flags |= S_SELECT;
	  setup_select = true;
	  S_StartSound(NULL,sfx_itemup);
	  return true;
	}

      if ((action == MENU_ESCAPE) || (action == MENU_BACKSPACE))
	{
	  M_SetSetupMenuItemOn(set_menu_itemon);
	  M_SetMultScreenIndex(ptr1, mult_screens_index);
	  if (action == MENU_ESCAPE) // Clear all menus
	    M_ClearMenus();
	  else // key_menu_backspace = return to Setup Menu
	    if (currentMenu->prevMenu)
	      {
		currentMenu = currentMenu->prevMenu;
		itemOn = currentMenu->lastOn;
		S_StartSound(NULL,sfx_swtchn);
	      }
	  ptr1->m_flags &= ~(S_HILITE|S_SELECT);// phares 4/19/98
	  setup_active = false;
	  set_keybnd_active = false;
	  set_weapon_active = false;
	  set_status_active = false;
	  set_auto_active = false;
	  set_enemy_active = false;
	  set_mess_active = false;
	  set_chat_active = false;
	  colorbox_active = false;
	  default_verify = false;       // phares 4/19/98
	  set_general_active = false;    // killough 10/98
          set_compat_active = false;    // killough 10/98
	  print_warning_about_changes = false; // [FG] reset
	  HU_Start();    // catch any message changes // phares 4/19/98
	  S_StartSound(NULL,sfx_swtchx);
	  return true;
	}

      // Some setup screens may have multiple screens.
      // When there are multiple screens, m_prev and m_next items need to
      // be placed on the appropriate screen tables so the user can
      // move among the screens using the left and right arrow keys.
      // The m_var1 field contains a pointer to the appropriate screen
      // to move to.

      if (action == MENU_LEFT)
	{
	  ptr2 = ptr1;
	  do
	    {
	      ptr2++;
	      if (ptr2->m_flags & S_PREV)
		{
		  ptr1->m_flags &= ~S_HILITE;
		  mult_screens_index--;
		  M_SetSetupMenuItemOn(set_menu_itemon);
		  current_setup_menu = ptr2->var.menu;
		  set_menu_itemon = M_GetSetupMenuItemOn();
		  print_warning_about_changes = false; // killough 10/98
		  while (current_setup_menu[set_menu_itemon++].m_flags&S_SKIP);
		  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
		  S_StartSound(NULL,sfx_pstop);  // killough 10/98
		  return true;
		}
	    }
	  while (!(ptr2->m_flags & S_END));
	}

      if (action == MENU_RIGHT)                
	{
	  ptr2 = ptr1;
	  do
	    {
	      ptr2++;
	      if (ptr2->m_flags & S_NEXT)
		{
		  ptr1->m_flags &= ~S_HILITE;
		  mult_screens_index++;
		  M_SetSetupMenuItemOn(set_menu_itemon);
		  current_setup_menu = ptr2->var.menu;
		  set_menu_itemon = M_GetSetupMenuItemOn();
		  print_warning_about_changes = false; // killough 10/98
		  while (current_setup_menu[set_menu_itemon++].m_flags&S_SKIP);
		  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
		  S_StartSound(NULL,sfx_pstop);  // killough 10/98
		  return true;
		}
	    }
	  while (!(ptr2->m_flags & S_END));
	}

    } // End of Setup Screen processing

  // From here on, these navigation keys are used on the BIG FONT menus
  // like the Main Menu.

  if (action == MENU_DOWN)                             // phares 3/7/98
    {
      do
	{
	  if (itemOn+1 > currentMenu->numitems-1)
	    itemOn = 0;
	  else
	    itemOn++;
	  S_StartSound(NULL,sfx_pstop);
	}
      while(currentMenu->menuitems[itemOn].status==-1);
      return true;
    }
  
  if (action == MENU_UP)                               // phares 3/7/98
    {
      do
	{
	  if (!itemOn)
	    itemOn = currentMenu->numitems-1;
	  else
	    itemOn--;
	  S_StartSound(NULL,sfx_pstop);
	}
      while(currentMenu->menuitems[itemOn].status==-1);
      return true;
    }

  if (action == MENU_LEFT)                             // phares 3/7/98
    {
      if (currentMenu->menuitems[itemOn].routine &&
	  currentMenu->menuitems[itemOn].status == 2)
	{
	  S_StartSound(NULL,sfx_stnmov);
	  currentMenu->menuitems[itemOn].routine(0);
	}
      return true;
    }
  
  if (action == MENU_RIGHT)                            // phares 3/7/98
    {
      if (currentMenu->menuitems[itemOn].routine &&
	  currentMenu->menuitems[itemOn].status == 2)
	{
	  S_StartSound(NULL,sfx_stnmov);
	  currentMenu->menuitems[itemOn].routine(1);
	}
      return true;
    }

  if (action == MENU_ENTER)                            // phares 3/7/98
    {
      if (currentMenu->menuitems[itemOn].routine &&
	  currentMenu->menuitems[itemOn].status)
	{
	  currentMenu->lastOn = itemOn;
	  if (currentMenu->menuitems[itemOn].status == 2)
	    {
	      currentMenu->menuitems[itemOn].routine(1);   // right arrow
	      S_StartSound(NULL,sfx_stnmov);
	    }
	  else
	    {
	      currentMenu->menuitems[itemOn].routine(itemOn);
	      S_StartSound(NULL,sfx_pistol);
	    }
	}
	else
	  S_StartSound(NULL,sfx_oof); // [FG] disabled menu item
      //jff 3/24/98 remember last skill selected
      // killough 10/98 moved to skill-specific functions
      return true;
    }
  
  if (action == MENU_ESCAPE)                           // phares 3/7/98  
    {
      currentMenu->lastOn = itemOn;
      M_ClearMenus ();
      S_StartSound(NULL,sfx_swtchx);
      return true;
    }
  
  if (action == MENU_BACKSPACE)                        // phares 3/7/98
    {
      currentMenu->lastOn = itemOn;

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
	    currentMenu = currentMenu->prevMenu;
	  itemOn = currentMenu->lastOn;
	  S_StartSound(NULL,sfx_swtchn);
	}
      return true;
    }

  // [FG] delete a savegame

  else if (action == MENU_CLEAR)
  {
    if (currentMenu == &LoadDef || currentMenu == &SaveDef)
    {
      if (LoadMenu[itemOn].status)
      {
        S_StartSound(NULL,sfx_itemup);
        currentMenu->lastOn = itemOn;
        delete_verify = true;
        return true;
      }
      else
      {
        S_StartSound(NULL,sfx_oof);
      }
    }
  }
  
  else
    {
      if (ch) // fix items with alphaKey == 0
      {
      for (i = itemOn+1;i < currentMenu->numitems;i++)
	if (currentMenu->menuitems[i].alphaKey == ch)
	  {
	    itemOn = i;
	    S_StartSound(NULL,sfx_pstop);
	    return true;
	  }
      for (i = 0;i <= itemOn;i++)
	if (currentMenu->menuitems[i].alphaKey == ch)
	  {
	    itemOn = i;
	    S_StartSound(NULL,sfx_pstop);
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
  return setup_active && menu_background == background_dark;
}

void M_Drawer (void)
{
   if (M_MenuIsShaded())
      V_ShadeScreen();

   inhelpscreens = false;
   
   // Horiz. & Vertically center string and print it.
   // killough 9/29/98: simplified code, removed 40-character width limit
   if(messageToPrint)
   {
      // haleyjd 11/11/04: must strdup message, cannot write into
      // string constants!
      char *d = strdup(messageString);
      char *p;
      int y = 100 - M_StringHeight(messageString)/2;
      
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
   }
   else if(menuactive)
   {
      int x,y,max,i;
      
      if (currentMenu->routine)
         currentMenu->routine();     // call Draw routine
      
      // DRAW MENU
      
      x = currentMenu->x;
      y = currentMenu->y;
      max = currentMenu->numitems;
      
      // [FG] check current menu for missing menu graphics lumps - only once
      if (currentMenu->lumps_missing == 0)
      {
        for (i = 0; i < max; i++)
          if ((currentMenu->menuitems[i].name[0] == 0 ||
               W_CheckNumForName(currentMenu->menuitems[i].name) < 0) &&
              currentMenu->menuitems[i].alttext)
          {
            currentMenu->lumps_missing++;
            break;
          }

        // [FG] no lump missing, no need to check again
        if (currentMenu->lumps_missing == 0)
          currentMenu->lumps_missing = -1;
      }

      // [FG] at least one menu graphics lump is missing, draw alternative text
      if (currentMenu->lumps_missing > 0)
      {
        for (i = 0; i < max; i++)
        {
          const char *alttext = currentMenu->menuitems[i].alttext;
          if (alttext)
            M_DrawStringCR(x, y+8-(M_StringHeight(alttext)/2),
            currentMenu->menuitems[i].status == 0 ? cr_dark : NULL,alttext);
          y += LINEHEIGHT;
        }
      }
      else
      for (i=0;i<max;i++)
      {
         if (currentMenu->menuitems[i].name[0])
            V_DrawPatchTranslated(x,y,0,
            W_CacheLumpName(currentMenu->menuitems[i].name,PU_CACHE),
            currentMenu->menuitems[i].status == 0 ? cr_dark : NULL);
         y += LINEHEIGHT;
      }
      
      // DRAW SKULL
      
      V_DrawPatchDirect(x + SKULLXOFF,
         currentMenu->y - 5 + itemOn*LINEHEIGHT,0,
         W_CacheLumpName(skullName[whichSkull],PU_CACHE));

      if (delete_verify)
        M_DrawDelVerify();
   }
}

//
// M_ClearMenus
//
// Called when leaving the menu screens for the real world

void M_ClearMenus (void)
{
  menuactive = 0;
  print_warning_about_changes = 0;     // killough 8/15/98
  default_verify = 0;                  // killough 10/98

  // if (!netgame && usergame && paused)
  //     sendpause = true;
}

//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
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

void M_StartMessage (char *string,void (*routine)(int),boolean input)
{
  messageLastMenuActive = menuactive;
  messageToPrint = 1;
  messageString = string;
  messageRoutine = routine;
  messageNeedsInput = input;
  menuactive = true;
  return;
}

void M_StopMessage(void)
{
  menuactive = messageLastMenuActive;
  messageToPrint = 0;
}

/////////////////////////////
//
// Thermometer Routines
//

//
// M_DrawThermo draws the thermometer graphic for Mouse Sensitivity,
// Sound Volume, etc.
//
void M_DrawThermo(int x,int y,int thermWidth,int thermDot )
{
  int xx;
  int  i;
  char num[4];

  xx = x;
  V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERML",PU_CACHE));
  xx += 8;
  for (i=0;i<thermWidth;i++)
    {
      V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMM",PU_CACHE));
      xx += 8;
    }
  V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMR",PU_CACHE));

  // [FG] write numerical values next to thermometer
  M_snprintf(num, 4, "%3d", thermDot);
  M_WriteText(xx + 8, y + 3, num);

  // [FG] do not crash anymore if value exceeds thermometer range
  if (thermDot >= thermWidth)
      thermDot = thermWidth - 1;

  V_DrawPatchDirect ((x+8) + thermDot*8,y,
		     0,W_CacheLumpName("M_THERMO",PU_CACHE));
}

//
// Draw an empty cell in the thermometer
//

void M_DrawEmptyCell (menu_t* menu,int item)
{
  V_DrawPatchDirect (menu->x - 10,menu->y+item*LINEHEIGHT - 1, 0,
		     W_CacheLumpName("M_CELL1",PU_CACHE));
}

//
// Draw a full cell in the thermometer
//

void M_DrawSelCell (menu_t* menu,int item)
{
  V_DrawPatchDirect (menu->x - 10,menu->y+item*LINEHEIGHT - 1, 0,
		     W_CacheLumpName("M_CELL2",PU_CACHE));
}

/////////////////////////////
//
// String-drawing Routines
//

//
// Find string width from hu_font chars
//

int M_StringWidth(const char *string)
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

int M_StringHeight(const char *string)
{
  int i, h, height = h = SHORT(hu_font[0]->height);
  for (i = 0;string[i];i++)            // killough 1/31/98
    if (string[i] == '\n')
      h += height;
  return h;
}

//
//    Write a string using the hu_font
//
void M_WriteText (int x,int y,const char *string)
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
      V_DrawPatchDirect(cx, cy, 0, hu_font[c]);
      cx+=w;
    }
}

// [FG] alternative text for missing menu graphics lumps

void M_DrawTitle(int x, int y, const char *patch, const char *alttext)
{
  if (W_CheckNumForName(patch) >= 0)
    V_DrawPatchDirect(x,y,0,W_CacheLumpName(patch,PU_CACHE));
  else
  {
    // patch doesn't exist, draw some text in place of it
    strcpy(menu_buffer,alttext);
    M_DrawMenuString(160-(M_StringWidth(alttext)/2),
                y+8-(M_StringHeight(alttext)/2), // assumes patch height 16
                CR_TITLE);
  }
}

/////////////////////////////
//
// Initialization Routines to take care of one-time setup
//

// phares 4/08/98:
// M_InitHelpScreen() clears the weapons from the HELP
// screen that don't exist in this version of the game.

void M_InitHelpScreen()
{
  setup_menu_t* src;

  src = helpstrings;
  while (!(src->m_flags & S_END))
    {
      if ((strncmp(src->m_text,"PLASMA",6) == 0) && (gamemode == shareware))
	src->m_flags = S_SKIP; // Don't show setting or item
      if ((strncmp(src->m_text,"BFG",3) == 0) && (gamemode == shareware))
	src->m_flags = S_SKIP; // Don't show setting or item
      if ((strncmp(src->m_text,"SSG",3) == 0) && !have_ssg)
	src->m_flags = S_SKIP; // Don't show setting or item
      src++;
    }
}

void M_GetMidiDevices(void)
{
  int numdev = I_DeviceList(midi_player_menu_strings,
        MAX_MIDI_PLAYER_MENU_ITEMS - 1, &midi_player_menu);

  midi_player_menu_strings[numdev] = NULL;
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
  screenSize = screenblocks - 3;
  messageToPrint = 0;
  messageString = NULL;
  messageLastMenuActive = menuactive;
  quickSaveSlot = -1;

  // Here we could catch other version dependencies,
  //  like HELP1/2, and four episodes.

  switch(gamemode)
    {
    case commercial:
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
      ReadDef1.routine = M_DrawReadThis1;
      ReadDef1.x = 330;
      ReadDef1.y = 165;
      HelpDef.y = 165;
      ReadMenu1[0].routine = M_FinishReadThis;
      break;
    case registered:
      // Episode 2 and 3 are handled,
      //  branching to an ad screen.

      // killough 2/21/98: Fix registered Doom help screen
      // killough 10/98: moved to second screen, moved up to the top
      ReadDef2.y = 15;

    case shareware:
      // We need to remove the fourth episode.
      EpiDef.numitems--;
      break;
    case retail:
      // We are fine.
    default:
      break;
    }

  M_InitMouseAccel();

  M_ResetMenu();        // killough 10/98
  M_ResetSetupMenu();
  M_InitHelpScreen();   // init the help screen       // phares 4/08/98
  M_InitExtendedHelp(); // init extended help screens // phares 3/30/98

  // [FG] support the BFG Edition IWADs

  if (W_CheckNumForName("M_GDHIGH") != -1)
  {
    patch_t *patch = W_CacheLumpName("M_GDHIGH", PU_CACHE);
    if (OptionsDef.x + 175 + SHORT(patch->width) >= ORIGWIDTH)
    {
      if (W_CheckNumForName("M_DISP") != -1)
        strcpy(OptionsMenu[scrnsize].name, "M_DISP");
    }
  }

  // [crispy] remove DOS reference from the game quit confirmation dialogs
  {
    const char *platform = SDL_GetPlatform();
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
#ifndef _WIN32
    if (isatty(STDOUT_FILENO))
        string = M_StringReplace(replace, "prompt", "shell");
    else
#endif
    string = M_StringReplace(replace, "prompt", "desktop");
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

  while ((traditional_menu ? M_SaveGame : M_Options)
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
    gen_settings4[gen4_strictmode].m_flags |= S_DISABLE;
  }

  if (M_ParmExists("-complevel"))
  {
    gen_settings5[gen5_compat].m_flags |= S_DISABLE;
  }

  if (M_ParmExists("-pistolstart"))
  {
    gen_settings4[gen4_pistolstart].m_flags |= S_DISABLE;
  }

  if (M_ParmExists("-uncapped") || M_ParmExists("-nouncapped"))
  {
    gen_settings1[gen1_uncapped].m_flags |= S_DISABLE;
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
    gen_settings3[gen3_brightmaps].m_flags |= S_DISABLE;
  }

  DISABLE_ITEM(!comp[comp_vile], enem_settings1[enem1_ghost]);

  M_CoerceFPSLimit();
  M_UpdateCrosshairItems();
  M_UpdateCenteredWeaponItem();
  M_UpdateMultiLineMsgItem();
  M_UpdateCriticalItems();
  M_UpdateDirectVerticalAimingItem();
  M_UpdateAdvancedSoundItems();
}

void M_ResetSetupMenuVideo(void)
{
  DISABLE_ITEM(!hires, enem_settings1[enem1_fuzz]);
  M_EnableDisableFPSLimit();
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
