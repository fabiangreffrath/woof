// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: m_menu.c,v 1.54 1998/05/28 05:27:13 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//  DOOM selection menu, options, episode etc. (aka Big Font menus)
//  Sliders and icons. Kinda widget stuff.
//  Setup Menus.
//  Extended HELP screens.
//  Dynamic HELP screen.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_menu.c,v 1.54 1998/05/28 05:27:13 killough Exp $";

#include <fcntl.h>
#include "d_io.h" // haleyjd

#include "doomdef.h"
#include "doomstat.h"
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

extern patch_t* hu_font[HU_FONTSIZE];
extern boolean  message_dontfuckwithme;
          
extern boolean chat_on;          // in heads-up code
extern int     hud_active;       // in heads-up code
extern int     hud_displayed;    // in heads-up code
extern int     hud_distributed;  // in heads-up code
extern int     HU_MoveHud(void); // jff 3/9/98 avoid glitch in HUD display

//
// defaulted values
//

int mouseSensitivity_horiz; // has default   //  killough
int mouseSensitivity_vert;  // has default

int showMessages;    // Show messages has default, 0 = off, 1 = on
  
int traditional_menu;

int hide_setup=1; // killough 5/15/98

// Blocky mode, has default, 0 = high, 1 = normal
//int     detailLevel;    obsolete -- killough
int screenblocks;    // has default

int screenSize;      // temp for screenblocks (0-9)    

int quickSaveSlot;   // -1 = no quicksave slot picked!          

int messageToPrint;  // 1 = message to be printed

char* messageString; // ...and here is the message string!    

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

int saveStringEnter;              
int saveSlot;        // which slot to save in
int saveCharIndex;   // which char we're editing
// old save description before edit
char saveOldString[SAVESTRINGSIZE];  

boolean inhelpscreens; // indicates we are in or just left a help screen

boolean menuactive;    // The menus are up

#define SKULLXOFF  -32
#define LINEHEIGHT  16

char savegamestrings[10][SAVESTRINGSIZE];

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
} menu_t;

short itemOn;           // menu item skull is on (for Big Font menus)
short skullAnimCounter; // skull animation counter
short whichSkull;       // which skull to draw (he blinks)

// graphic name of skulls

char skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

menu_t* currentMenu; // current menudef                          

// jff 3/24/98
extern int defaultskill; // config file specified skill

// killough 3/6/98: preserve autorun across games
extern int autorun;      // always running?
extern int key_right;                                              
extern int key_left;
extern int key_up;
extern int key_down;
extern int key_menu_right;                                   // phares 3/7/98
extern int key_menu_left;                                    //     |
extern int key_menu_up;                                      //     V
extern int key_menu_down;                                     
extern int key_menu_backspace;                               //     ^
extern int key_menu_escape;                                  //     |
extern int key_menu_enter;                                   // phares 3/7/98
extern int key_strafeleft;                                   
extern int key_straferight;
extern int key_fire;
extern int key_use;
extern int key_strafe;
extern int key_speed;
extern int key_escape;
extern int key_savegame;
extern int key_loadgame;
extern int key_autorun;
extern int key_reverse;
extern int key_zoomin;
extern int key_zoomout;
extern int key_chat;
extern int key_backspace;
extern int key_enter;
extern int key_help;
extern int key_soundvolume;
extern int key_hud;
extern int key_quicksave;
extern int key_endgame;
extern int key_messages;
extern int key_quickload;
extern int key_quit;
extern int key_gamma;
extern int key_spy;
extern int key_pause;
extern int key_forward;
extern int key_leftturn;
extern int key_rightturn;
extern int key_backward;
extern int key_weapontoggle;
extern int key_weapon1;
extern int key_weapon2;
extern int key_weapon3;
extern int key_weapon4;
extern int key_weapon5;
extern int key_weapon6;
extern int key_weapon7;
extern int key_weapon8;
extern int key_weapon9;                                            
extern int key_map_right;                                           // phares
extern int key_map_left;                                            //    |
extern int key_map_up;                                              //    V
extern int key_map_down;
extern int key_map_zoomin;
extern int key_map_zoomout;
extern int key_map;
extern int key_map_gobig;
extern int key_map_follow;
extern int key_map_mark;                                            //    ^
extern int key_map_clear;                                           //    |
extern int key_map_grid;                                            // phares
extern int key_screenshot;    // killough 2/22/98
extern int key_setup;         // killough 10/98

// phares 3/30/98
// externs added for setup menus

extern int destination_keys[MAXPLAYERS];
extern int mousebfire;                                   
extern int mousebstrafe;                               
extern int mousebforward;
extern int joybfire;
extern int joybstrafe;                               
extern int joybuse;                                   
extern int joybspeed;                                     
extern int default_weapon_recoil;   // weapon recoil        
extern int weapon_recoil;           // weapon recoil           
extern int default_player_bobbing;  // whether player bobs or not         
extern int player_bobbing;          // whether player bobs or not       
extern int weapon_preferences[2][NUMWEAPONS+1];                   
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
extern int hud_nosecrets; // status does not list secrets/items/kills
extern int sts_traditional_keys;  // display keys the traditional way
extern int hud_list_bgon; // solid window background for list of messages
extern int hud_msg_lines; // number of message lines in window up to 16
extern int hud_msg_scrollup; // killough 11/98: whether list scrolls upwards
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
extern int hudcolor_titl; // color range of automap level title
extern int hudcolor_xyco; // color range of new coords on automap
extern int hudcolor_mesg; // color range of scrolling messages
extern int hudcolor_chat; // color range of chat lines
extern int hudcolor_list; // color of list of past messages

extern int mapcolor_frnd;  // friends colors  // killough 8/8/98
extern int default_monsters_remember;                     
extern int monsters_remember;                            

extern int map_point_coordinates; // killough 10/98

extern char* chat_macros[];  // chat macros
extern char *wad_files[], *deh_files[]; // killough 10/98
extern const char* shiftxform;
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
void M_ChangeSensitivity(int choice);
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
void M_DrawCredits(void);                                   // killough 10/98

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
void M_WriteText(int x, int y, char *string);
int  M_StringWidth(char *string);
int  M_StringHeight(char *string);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,boolean input);
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
int  M_GetPixelWidth(char*);
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
void M_Trans(void);       // killough 10/98
void M_ResetMenu(void);   // killough 10/98

menu_t NewDef;                                              // phares 5/04/98

// end of prototypes added to support Setup Menus and Extended HELP screens

static char menu_buffer[64];

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
    V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
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
    V_DrawPatchDirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
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

menuitem_t EpisodeMenu[]=
{
  {1,"M_EPI1", M_Episode,'k'},
  {1,"M_EPI2", M_Episode,'t'},
  {1,"M_EPI3", M_Episode,'i'},
  {1,"M_EPI4", M_Episode,'t'}
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

//
//    M_Episode
//
int epi;

void M_DrawEpisode(void)
{
  V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

void M_Episode(int choice)
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
   
  epi = choice;
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
  {1,"M_JKILL", M_ChooseSkill, 'i'},
  {1,"M_ROUGH", M_ChooseSkill, 'h'},
  {1,"M_HURT",  M_ChooseSkill, 'h'},
  {1,"M_ULTRA", M_ChooseSkill, 'u'},
  {1,"M_NMARE", M_ChooseSkill, 'n'}
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
  
  if ( gamemode == commercial )
    M_SetupNextMenu(&NewDef);
  else
    M_SetupNextMenu(&EpiDef);
}

void M_VerifyNightmare(int ch)
{
  if (ch != 'y')
    return;

  //jff 3/24/98 remember last skill selected
  // killough 10/98 moved to here
  defaultskill = nightmare+1;

  G_DeferedInitNew(nightmare,epi+1,1);
  M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
  if (choice == nightmare)
    {   // Ty 03/27/98 - externalized
      M_StartMessage(s_NIGHTMARE,M_VerifyNightmare,true);
      return;
    }
  
  //jff 3/24/98 remember last skill selected
  // killough 10/98 moved to here
  defaultskill = choice+1;

  G_DeferedInitNew(choice,epi+1,1);
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
  80,34, //jff 3/15/98 move menu up
  0
};

#define LOADGRAPHIC_Y 8

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
  char name[PATH_MAX+1];     // killough 3/22/98

  G_SaveGameName(name,choice);

  G_LoadGame(name, choice, false); // killough 3/16/98, 5/15/98: add slot, cmd

  M_ClearMenus ();
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
  80,34, //jff 3/15/98 move menu up
  0
};

//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
  int i;

  for (i = 0 ; i < load_end ; i++)
    {
      char name[PATH_MAX+1];    // killough 3/22/98
      FILE *fp;  // killough 11/98: change to use stdio

      G_SaveGameName(name,i);    // killough 3/22/98
      fp = fopen(name,"rb");
      if (!fp)
	{   // Ty 03/27/98 - externalized:
	  strcpy(&savegamestrings[i][0],s_EMPTYSTRING);
	  LoadMenu[i].status = 0;
	  continue;
	}
      fread(&savegamestrings[i], SAVESTRINGSIZE, 1, fp);
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
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
  G_SaveGame (slot,savegamestrings[slot]);
  M_ClearMenus ();

  // PICK QUICKSAVE SLOT YET?
  if (quickSaveSlot == -2)
    quickSaveSlot = slot;
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
  if (!strcmp(savegamestrings[choice],s_EMPTYSTRING)) // Ty 03/27/98 - externalized
    savegamestrings[choice][0] = 0;
  saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
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
  {1,"M_GENERL", M_General, 'g'},      // killough 10/98
  {1,"M_SETUP",  M_Setup,   's'},                          // phares 3/21/98
  {1,"M_ENDGAM", M_EndGame,'e'},
  {1,"M_MESSG",  M_ChangeMessages,'m'},
  /*    {1,"M_DETAIL",  M_ChangeDetail,'g'},  unused -- killough */  
  {2,"M_SCRNSZ", M_SizeDisplay,'s'},
  {-1,"",0},
  {1,"M_MSENS",  M_ChangeSensitivity,'m'},
  /* {-1,"",0},  replaced with submenu -- killough */ 
  {1,"M_SVOL",   M_Sound,'s'}
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
  extern int snd_card;      // killough 10/98
  if (ch != 'y')
    return;
  if ((!netgame || demoplayback) // killough 12/98
      && !nosfxparm && snd_card) // avoid delay if no sound card
    {
      if (gamemode == commercial)
	S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
      else
	S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
      I_WaitVBL(105);
    }
  exit(0); // killough
}

void M_QuitDOOM(int choice)
{
  static char endstring[160];

  // We pick index 0 which is language sensitive,
  // or one at random, between 1 and maximum number.
  // Ty 03/27/98 - externalized DOSY as a string s_DOSY that's in the sprintf
  if (language != english)
    sprintf(endstring,"%s\n\n%s",s_DOSY, endmsg[0] );
  else         // killough 1/18/98: fix endgame message calculation:
    sprintf(endstring,"%s\n\n%s", endmsg[gametic%(NUM_QUITMESSAGES-1)+1], s_DOSY);
  
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
  mouse_vert,
  mouse_empty2,
  mouse_end
} mouse_e;

// The definitions of the Mouse Sensitivity menu

menuitem_t MouseMenu[]=
{
  {2,"M_HORSEN",M_MouseHoriz,'h'},
  {-1,"",0},
  {2,"M_VERSEN",M_MouseVert,'v'},
  {-1,"",0}
};

menu_t MouseDef =
{
  mouse_end,
  &OptionsDef,
  MouseMenu,
  M_DrawMouse,
  60,64,
  0
};


// I'm using a scale of 100 since I don't know what's normal -- killough.

#define MOUSE_SENS_MAX 100

//
// Change Mouse Sensitivities -- killough
//

void M_DrawMouse(void)
{
  int mhmx,mvmx; //jff 4/3/98 clamp drawn position to 23 max

  V_DrawPatchDirect (60,38,0,W_CacheLumpName("M_MSENS",PU_CACHE));

  //jff 4/3/98 clamp horizontal sensitivity display
  mhmx = mouseSensitivity_horiz>23? 23 : mouseSensitivity_horiz;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_horiz+1),24,mhmx);
  //jff 4/3/98 clamp vertical sensitivity display
  mvmx = mouseSensitivity_vert>23? 23 : mouseSensitivity_vert;
  M_DrawThermo(MouseDef.x,MouseDef.y+LINEHEIGHT*(mouse_vert+1),24,mvmx);
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

void M_MouseVert(int choice)
{
  M_Mouse(choice, &mouseSensitivity_vert);
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
      if (*sens < 23)
        ++*sens;
      break;
    }
}

/////////////////////////////
//
//    M_QuickSave
//

char tempstring[80];

void M_QuickSaveResponse(int ch)
{
  if (ch == 'y')
    {
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
  sprintf(tempstring,s_QSPROMPT,savegamestrings[quickSaveSlot]); // Ty 03/27/98 - externalized
  M_StartMessage(tempstring,M_QuickSaveResponse,true);
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
      M_StartMessage(s_QSAVESPOT,NULL,false); // Ty 03/27/98 - externalized
      return;
    }
  sprintf(tempstring,s_QLPROMPT,savegamestrings[quickSaveSlot]); // Ty 03/27/98 - externalized
  M_StartMessage(tempstring,M_QuickLoadResponse,true);
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
    players[consoleplayer].message = s_MSGOFF; // Ty 03/27/98 - externalized
  else
    players[consoleplayer].message = s_MSGON ; // Ty 03/27/98 - externalized

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

/////////////////////////////
//
// The menu_buffer is used to construct strings for display on the screen.

static char menu_buffer[64];

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
  {1,"M_COMPAT",M_Compat,     'p'},
  {1,"M_KEYBND",M_KeyBindings,'k'},
  {1,"M_WEAP"  ,M_Weapons,    'w'},
  {1,"M_STAT"  ,M_StatusBar,  's'},               
  {1,"M_AUTO"  ,M_Automap,    'a'},                    
  {1,"M_ENEM"  ,M_Enemy,      'e'},                     
  {1,"M_MESS"  ,M_Messages,   'm'},                     
  {1,"M_CHAT"  ,M_ChatStrings,'c'},                     
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

void M_DrawBackground(char* patchname, byte *back_dest)
{
  int x,y;
  byte *back_src, *src;

  V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);

  src = back_src = 
    W_CacheLumpNum(firstflat+R_FlatNumForName(patchname),PU_CACHE);

  if (hires)       // killough 11/98: hires support
#if 0              // this tiles it in hires
    for (y = 0 ; y < SCREENHEIGHT*2 ; src = ((++y & 63)<<6) + back_src)
      for (x = 0 ; x < SCREENWIDTH*2/64 ; x++)
	{
	  memcpy (back_dest,back_src+((y & 63)<<6),64);
	  back_dest += 64;
	}
#else              // while this pixel-doubles it
      for (y = 0 ; y < SCREENHEIGHT ; src = ((++y & 63)<<6) + back_src,
	     back_dest += SCREENWIDTH*2)
	for (x = 0 ; x < SCREENWIDTH/64 ; x++)
	  {
	    int i = 63;
	    do
	      back_dest[i*2] = back_dest[i*2+SCREENWIDTH*2] =
		back_dest[i*2+1] = back_dest[i*2+SCREENWIDTH*2+1] = src[i];
	    while (--i>=0);
	    back_dest += 128;
	  }
#endif
  else
    for (y = 0 ; y < SCREENHEIGHT ; src = ((++y & 63)<<6) + back_src)
      for (x = 0 ; x < SCREENWIDTH/64 ; x++)
	{
	  memcpy (back_dest,back_src+((y & 63)<<6),64);
	  back_dest += 64;
	}
}

/////////////////////////////
//
// Draws the Title for the main Setup screen

void M_DrawSetup(void)
{
  V_DrawPatchDirect(124,15,0,W_CacheLumpName("M_SETUP",PU_CACHE));
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
#define CR_ITEM   CR_RED
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
char* chat_string_buffer; // points to new chat strings while editing

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
	{      // killough 10/98: support left-justification:
	  strcpy(menu_buffer,p);
	  if (!(flags & S_LEFTJUST))
	    w = M_GetPixelWidth(menu_buffer) + 4;
	  M_DrawMenuString(x - w, y ,color);
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

void M_DrawSetting(setup_menu_t* s)
{
  int x = s->m_x, y = s->m_y, flags = s->m_flags, color;

  // Determine color of the text. This may or may not be used
  // later, depending on whether the item is a text string or not.

  color = flags & S_SELECT ? CR_SELECT : flags & S_HILITE ? CR_HILITE : CR_SET;

  // Is the item a YES/NO item?

  if (flags & S_YESNO)
    {
      strcpy(menu_buffer,*s->var.def->location ? "YES" : "NO");
      M_DrawMenuString(x,y,color);
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
	sprintf(menu_buffer,"%d",*s->var.def->location);
      M_DrawMenuString(x,y,color);
      return;
    }

  // Is the item a key binding?

  if (flags & S_KEY) // Key Binding
    {
      int *key = s->var.m_key;

      // Draw the key bound to the action

      if (key)
	{
	  M_GetKeyString(*key,0); // string to display
	  if (key == &key_use)
	    {
	      // For the 'use' key, you have to build the string
      
	      if (s->m_mouse)
		sprintf(menu_buffer+strlen(menu_buffer), "/DBL-CLK MB%d",
			*s->m_mouse+1);
	      if (s->m_joy)
		sprintf(menu_buffer+strlen(menu_buffer), "/JSB%d", 
			*s->m_joy+1);
	    }
	  else
	    if (key == &key_up   || key == &key_speed ||
		key == &key_fire || key == &key_strafe)
	      {
		if (s->m_mouse)
		  sprintf(menu_buffer+strlen(menu_buffer), "/MB%d",
			  *s->m_mouse+1);
		if (s->m_joy)
		  sprintf(menu_buffer+strlen(menu_buffer), "/JSB%d",
			  *s->m_joy+1);
	      }
	  M_DrawMenuString(x,y,color);
	}
      return;
    }

  // Is the item a weapon number?
  // OR, Is the item a colored text string from the Automap?
  //
  // killough 10/98: removed special code, since the rest of the engine
  // already takes care of it, and this code prevented the user from setting
  // their overall weapons preferences while playing Doom 1.
  //
  // killough 11/98: consolidated weapons code with color range code
  
  if (flags & (S_WEAP|S_CRITEM)) // weapon number or color range
    {
      sprintf(menu_buffer,"%d", *s->var.def->location);
      M_DrawMenuString(x,y, flags & S_CRITEM ? *s->var.def->location : color);
      return;
    }

  // Is the item a paint chip?

  if (flags & S_COLOR) // Automap paint chip
    {
      int i, ch;
      char *ptr = colorblock;

      // draw the border of the paint chip
       
      for (i = 0 ; i < (CHIP_SIZE+2)*(CHIP_SIZE+2) ; i++)
	*ptr++ = PAL_BLACK;
      V_DrawBlock(x,y-1,0,CHIP_SIZE+2,CHIP_SIZE+2,colorblock);
      
      // draw the paint chip
       
      ch = *s->var.def->location;
      if (!ch) // don't show this item in automap mode
	V_DrawPatchDirect (x+1,y,0,W_CacheLumpName("M_PALNO",PU_CACHE));
      else
	{
	  ptr = colorblock;
	  for (i = 0 ; i < CHIP_SIZE*CHIP_SIZE ; i++)
	    *ptr++ = ch;
	  V_DrawBlock(x+1,y,0,CHIP_SIZE,CHIP_SIZE,colorblock);
	}
      return;
    }

  // Is the item a chat string?
  // killough 10/98: or a filename?

  if (flags & S_STRING)
    {
      char *text = *(char **) s->var.def->location;

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

	  *c = text[chat_index]; // hold temporarily
	  c[1] = 0;
	  char_width = M_GetPixelWidth(c);
	  if (char_width == 1)
	    char_width = 7; // default for end of line
	  text[chat_index] = 0; // NULL to get cursor position
	  cursor_start = M_GetPixelWidth(text);
	  text[chat_index] = *c; // replace stored char

	  // Now draw the cursor

	  for (i = 0 ; i < char_width ; i++)
	    colorblock[i] = PAL_BLACK;
	  V_DrawBlock(x+cursor_start-1,y+7,0,char_width,1,colorblock);
	}

      // Draw the setting for the item

      strcpy(menu_buffer,text);
      M_DrawMenuString(x,y,color);
      return;
    }
}

/////////////////////////////
//
// M_DrawScreenItems takes the data for each menu item and gives it to
// the drawing routines above.

void M_DrawScreenItems(setup_menu_t* src)
{
  if (print_warning_about_changes > 0)   // killough 8/15/98: print warning
    if (warning_about_changes & S_BADVAL)
      {
	strcpy(menu_buffer, "Value out of Range");
	M_DrawMenuString(100,176,CR_RED);
      }
    else
      if (warning_about_changes & S_PRGWARN)
	{
	  strcpy(menu_buffer,
		 "Warning: Program must be restarted to see changes");
	  M_DrawMenuString(3, 176, CR_RED);
	}
      else
	if (warning_about_changes & S_BADVID)
	  {
	    strcpy(menu_buffer, "Video mode not supported");
	    M_DrawMenuString(80,176,CR_RED);
	  }
	else
	  {
	    strcpy(menu_buffer,
		   "Warning: Changes are pending until next game");
	    M_DrawMenuString(18,184,CR_RED);
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
#define PAL_GRAY1  91
#define PAL_GRAY2  98
#define PAL_GRAY3 105

// And the routine to draw it.

void M_DrawDefVerify()
{
  byte block[188];
  int i;

  for (i = 0 ; i < 181 ; i++)
    block[i] = PAL_BLACK;
  for (i = 0 ; i < 17 ; i++)
    V_DrawBlock(VERIFYBOXXORG+3,VERIFYBOXYORG+3+i,0,181,1,block);
  for (i = 0 ; i < 187 ; i++)
    block[i] = PAL_GRAY1;
  V_DrawBlock(VERIFYBOXXORG,VERIFYBOXYORG,0,187,1,block);
  V_DrawBlock(VERIFYBOXXORG,VERIFYBOXYORG,0,1,23,block);
  V_DrawBlock(VERIFYBOXXORG+184,VERIFYBOXYORG+2,0,1,19,block);
  V_DrawBlock(VERIFYBOXXORG+2,VERIFYBOXYORG+20,0,183,1,block);
  for (i = 0 ; i < 185 ; i++)
    block[i] = PAL_GRAY2;
  V_DrawBlock(VERIFYBOXXORG+1,VERIFYBOXYORG+1,0,185,1,block);
  V_DrawBlock(VERIFYBOXXORG+185,VERIFYBOXYORG+1,0,1,21,block);
  V_DrawBlock(VERIFYBOXXORG+1,VERIFYBOXYORG+21,0,185,1,block);
  V_DrawBlock(VERIFYBOXXORG+1,VERIFYBOXYORG+1,0,1,21,block);
  for (i = 0 ; i < 187 ; i++)
    block[i] = PAL_GRAY3;
  V_DrawBlock(VERIFYBOXXORG+2,VERIFYBOXYORG+2,0,183,1,block);
  V_DrawBlock(VERIFYBOXXORG+2,VERIFYBOXYORG+2,0,1,19,block);
  V_DrawBlock(VERIFYBOXXORG+186,VERIFYBOXYORG,0,1,23,block);
  V_DrawBlock(VERIFYBOXXORG,VERIFYBOXYORG+22,0,187,1,block);

  // The blinking messages is keyed off of the blinking of the
  // cursor skull.

  if (whichSkull) // blink the text
    {
      strcpy(menu_buffer,"Reset to defaults? (Y or N)");
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

  // killough 8/15/98: warn when values are different
  if (flags & (S_NUM|S_YESNO) && def->current && *def->current!=*def->location)
    {
      int allow = allow_changes() ? 8 : 0;
      if (!(setup_gather | print_warning_about_changes | demoplayback))
	{
	  strcpy(menu_buffer,
		 "Warning: Current actual setting differs from the");
	  M_DrawMenuString(4, 184-allow, CR_RED);
	  strcpy(menu_buffer,
		 "default setting, which is the one being edited here.");
	  M_DrawMenuString(4, 192-allow, CR_RED);
	  if (allow)
	    {
	      strcpy(menu_buffer,
		     "However, changes made here will take effect now.");
	      M_DrawMenuString(4, 192, CR_RED);
	    }
	}
      if (allow && setup_select)            // killough 8/15/98: Set new value
	if (!(flags & (S_LEVWARN | S_PRGWARN)))
	  *def->current = *def->location;
    }

  // There are different instruction messages depending on whether you
  // are changing an item or just sitting on it.

  { // killough 11/98: reformatted
    const char *s = "";
    int color = CR_HILITE,x = setup_select ? color = CR_SELECT, flags & S_KEY ?
      current_setup_menu[set_menu_itemon].m_mouse ||
      current_setup_menu[set_menu_itemon].m_joy ?
      (s = "Press key or button for this action", 49)                        :
      (s = "Press key for this action", 84)                                  :
      flags & S_YESNO  ? (s = "Press ENTER key to toggle", 78)               :
      flags & S_WEAP   ? (s = "Enter weapon number", 97)                     :
      flags & S_NUM    ? (s = "Enter value. Press ENTER when finished.", 37) :
      flags & S_COLOR  ? (s = "Select color and press enter", 70)            :
      flags & S_CRITEM ? (s = "Enter value", 125)                            :
      flags & S_CHAT   ? (s = "Type/edit chat string and Press ENTER", 43)   :
      flags & S_FILE   ? (s = "Type/edit filename and Press ENTER", 52)      :
      flags & S_RESET  ? 43 : 0  /* when you're changing something */        :
      flags & S_RESET  ? (s = "Press ENTER key to reset to defaults", 43)    :
      (s = "Press Enter to Change", 91);
    strcpy(menu_buffer, s);
    M_DrawMenuString(x,20,color);
  }
}


/////////////////////////////
//
// The Key Binding Screen tables.

#define KB_X  160
#define KB_PREV  57
#define KB_NEXT 310
#define KB_Y   31

// phares 4/16/98:
// X,Y position of reset button. This is the same for every screen, and is
// only defined once here.

#define X_BUTTON 301
#define Y_BUTTON   3

// Definitions of the (in this case) four key binding screens.

setup_menu_t keys_settings1[];       
setup_menu_t keys_settings2[];       
setup_menu_t keys_settings3[];       
setup_menu_t keys_settings4[];       

// The table which gets you from one screen table to the next.

setup_menu_t* keys_settings[] =
{
  keys_settings1,
  keys_settings2,
  keys_settings3,
  keys_settings4,
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

setup_menu_t keys_settings1[] =  // Key Binding screen strings       
{
  {"MOVEMENT"    ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y},
  {"FORWARD"     ,S_KEY       ,m_scrn,KB_X,KB_Y+1*8,{&key_up},&mousebforward},
  {"BACKWARD"    ,S_KEY       ,m_scrn,KB_X,KB_Y+2*8,{&key_down}},
  {"TURN LEFT"   ,S_KEY       ,m_scrn,KB_X,KB_Y+3*8,{&key_left}},
  {"TURN RIGHT"  ,S_KEY       ,m_scrn,KB_X,KB_Y+4*8,{&key_right}},
  {"RUN"         ,S_KEY       ,m_scrn,KB_X,KB_Y+5*8,{&key_speed},0,&joybspeed},
  {"STRAFE LEFT" ,S_KEY       ,m_scrn,KB_X,KB_Y+6*8,{&key_strafeleft}},
  {"STRAFE RIGHT",S_KEY       ,m_scrn,KB_X,KB_Y+7*8,{&key_straferight}},
  {"STRAFE"      ,S_KEY       ,m_scrn,KB_X,KB_Y+8*8,{&key_strafe},&mousebstrafe,&joybstrafe},
  {"AUTORUN"     ,S_KEY       ,m_scrn,KB_X,KB_Y+9*8,{&key_autorun}},
  {"180 TURN"    ,S_KEY       ,m_scrn,KB_X,KB_Y+10*8,{&key_reverse}},
  {"USE"         ,S_KEY       ,m_scrn,KB_X,KB_Y+11*8,{&key_use},&mousebforward,&joybuse},

  {"MENUS"       ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y+12*8},
  {"NEXT ITEM"   ,S_KEY       ,m_menu,KB_X,KB_Y+13*8,{&key_menu_down}},
  {"PREV ITEM"   ,S_KEY       ,m_menu,KB_X,KB_Y+14*8,{&key_menu_up}},
  {"LEFT"        ,S_KEY       ,m_menu,KB_X,KB_Y+15*8,{&key_menu_left}},
  {"RIGHT"       ,S_KEY       ,m_menu,KB_X,KB_Y+16*8,{&key_menu_right}},
  {"BACKSPACE"   ,S_KEY       ,m_menu,KB_X,KB_Y+17*8,{&key_menu_backspace}},
  {"SELECT ITEM" ,S_KEY       ,m_menu,KB_X,KB_Y+18*8,{&key_menu_enter}},
  {"EXIT"        ,S_KEY       ,m_menu,KB_X,KB_Y+19*8,{&key_menu_escape}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,KB_NEXT,KB_Y+20*8, {keys_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings2[] =  // Key Binding screen strings       
{
  {"SCREEN"      ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y},

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
  // killough 10/98: hotkey for entering setup menu:
  {"SETUP"       ,S_KEY       ,m_scrn,KB_X,KB_Y+ 1*8,{&key_setup}},
  {"PAUSE"       ,S_KEY       ,m_scrn,KB_X,KB_Y+ 2*8,{&key_pause}},
  {"AUTOMAP"     ,S_KEY       ,m_scrn,KB_X,KB_Y+ 3*8,{&key_map}},
  {"VOLUME"      ,S_KEY       ,m_scrn,KB_X,KB_Y+ 4*8,{&key_soundvolume}},
  {"HUD"         ,S_KEY       ,m_scrn,KB_X,KB_Y+ 5*8,{&key_hud}},
  {"MESSAGES"    ,S_KEY       ,m_scrn,KB_X,KB_Y+ 6*8,{&key_messages}},
  {"GAMMA FIX"   ,S_KEY       ,m_scrn,KB_X,KB_Y+ 7*8,{&key_gamma}},
  {"SPY"         ,S_KEY       ,m_scrn,KB_X,KB_Y+ 8*8,{&key_spy}},
  {"LARGER VIEW" ,S_KEY       ,m_scrn,KB_X,KB_Y+ 9*8,{&key_zoomin}},
  {"SMALLER VIEW",S_KEY       ,m_scrn,KB_X,KB_Y+10*8,{&key_zoomout}},
  {"SCREENSHOT"  ,S_KEY       ,m_scrn,KB_X,KB_Y+11*8,{&key_screenshot}},
  {"GAME"        ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y+12*8},
  {"SAVE"        ,S_KEY       ,m_scrn,KB_X,KB_Y+13*8,{&key_savegame}},
  {"LOAD"        ,S_KEY       ,m_scrn,KB_X,KB_Y+14*8,{&key_loadgame}},
  {"QUICKSAVE"   ,S_KEY       ,m_scrn,KB_X,KB_Y+15*8,{&key_quicksave}},
  {"QUICKLOAD"   ,S_KEY       ,m_scrn,KB_X,KB_Y+16*8,{&key_quickload}},
  {"END GAME"    ,S_KEY       ,m_scrn,KB_X,KB_Y+17*8,{&key_endgame}},
  {"QUIT"        ,S_KEY       ,m_scrn,KB_X,KB_Y+18*8,{&key_quit}},
  {"<- PREV", S_SKIP|S_PREV,m_null,KB_PREV,KB_Y+20*8, {keys_settings1}},
  {"NEXT ->", S_SKIP|S_NEXT,m_null,KB_NEXT,KB_Y+20*8, {keys_settings3}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

setup_menu_t keys_settings3[] =  // Key Binding screen strings       
{
  {"WEAPONS" ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y},
  {"FIST"    ,S_KEY       ,m_scrn,KB_X,KB_Y+ 1*8,{&key_weapon1}},
  {"PISTOL"  ,S_KEY       ,m_scrn,KB_X,KB_Y+ 2*8,{&key_weapon2}},
  {"SHOTGUN" ,S_KEY       ,m_scrn,KB_X,KB_Y+ 3*8,{&key_weapon3}},
  {"CHAINGUN",S_KEY       ,m_scrn,KB_X,KB_Y+ 4*8,{&key_weapon4}},
  {"ROCKET"  ,S_KEY       ,m_scrn,KB_X,KB_Y+ 5*8,{&key_weapon5}},
  {"PLASMA"  ,S_KEY       ,m_scrn,KB_X,KB_Y+ 6*8,{&key_weapon6}},
  {"BFG",     S_KEY       ,m_scrn,KB_X,KB_Y+ 7*8,{&key_weapon7}},
  {"CHAINSAW",S_KEY       ,m_scrn,KB_X,KB_Y+ 8*8,{&key_weapon8}},
  {"SSG"     ,S_KEY       ,m_scrn,KB_X,KB_Y+ 9*8,{&key_weapon9}},
  {"BEST"    ,S_KEY       ,m_scrn,KB_X,KB_Y+10*8,{&key_weapontoggle}},
  {"FIRE"    ,S_KEY       ,m_scrn,KB_X,KB_Y+11*8,{&key_fire},&mousebfire,&joybfire},

  {"<- PREV",S_SKIP|S_PREV,m_null,KB_PREV,KB_Y+20*8, {keys_settings2}},
  {"NEXT ->",S_SKIP|S_NEXT,m_null,KB_NEXT,KB_Y+20*8, {keys_settings4}},

  // Final entry

  {0,S_SKIP|S_END,m_null}

};

setup_menu_t keys_settings4[] =  // Key Binding screen strings       
{
  {"AUTOMAP"    ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y},
  {"FOLLOW"     ,S_KEY       ,m_map ,KB_X,KB_Y+ 1*8,{&key_map_follow}},
  {"ZOOM IN"    ,S_KEY       ,m_map ,KB_X,KB_Y+ 2*8,{&key_map_zoomin}},
  {"ZOOM OUT"   ,S_KEY       ,m_map ,KB_X,KB_Y+ 3*8,{&key_map_zoomout}},
  {"SHIFT UP"   ,S_KEY       ,m_map ,KB_X,KB_Y+ 4*8,{&key_map_up}},
  {"SHIFT DOWN" ,S_KEY       ,m_map ,KB_X,KB_Y+ 5*8,{&key_map_down}},
  {"SHIFT LEFT" ,S_KEY       ,m_map ,KB_X,KB_Y+ 6*8,{&key_map_left}},
  {"SHIFT RIGHT",S_KEY       ,m_map ,KB_X,KB_Y+ 7*8,{&key_map_right}},
  {"MARK PLACE" ,S_KEY       ,m_map ,KB_X,KB_Y+ 8*8,{&key_map_mark}},
  {"CLEAR MARKS",S_KEY       ,m_map ,KB_X,KB_Y+ 9*8,{&key_map_clear}},
  {"FULL/ZOOM"  ,S_KEY       ,m_map ,KB_X,KB_Y+10*8,{&key_map_gobig}},
  {"GRID"       ,S_KEY       ,m_map ,KB_X,KB_Y+11*8,{&key_map_grid}},

  {"CHATTING"   ,S_SKIP|S_TITLE,m_null,KB_X,KB_Y+12*8},
  {"BEGIN CHAT" ,S_KEY       ,m_scrn,KB_X,KB_Y+13*8,{&key_chat}},
  {"PLAYER 1"   ,S_KEY       ,m_scrn,KB_X,KB_Y+14*8,{&destination_keys[0]}},
  {"PLAYER 2"   ,S_KEY       ,m_scrn,KB_X,KB_Y+15*8,{&destination_keys[1]}},
  {"PLAYER 3"   ,S_KEY       ,m_scrn,KB_X,KB_Y+16*8,{&destination_keys[2]}},
  {"PLAYER 4"   ,S_KEY       ,m_scrn,KB_X,KB_Y+17*8,{&destination_keys[3]}},
  {"BACKSPACE"  ,S_KEY       ,m_scrn,KB_X,KB_Y+18*8,{&key_backspace}},
  {"ENTER"      ,S_KEY       ,m_scrn,KB_X,KB_Y+19*8,{&key_enter}},

  {"<- PREV" ,S_SKIP|S_PREV,m_null,KB_PREV,KB_Y+20*8, {keys_settings3}},

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
  mult_screens_index = 0;
  current_setup_menu = keys_settings[0];
  set_menu_itemon = 0;
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
  V_DrawPatchDirect (84,2,0,W_CacheLumpName("M_KEYBND",PU_CACHE));
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

#define WP_X 203
#define WP_Y  33

// There's only one weapon settings screen (for now). But since we're
// trying to fit a common description for screens, it gets a setup_menu_t,
// which only has one screen definition in it.
//
// Note that this screen has no PREV or NEXT items, since there are no
// neighboring screens.

enum {           // killough 10/98: enum for y-offset info
  weap_recoil,
  weap_bobbing,
  weap_bfg,
  weap_stub1,
  weap_pref1,
  weap_pref2,
  weap_pref3,
  weap_pref4,
  weap_pref5,
  weap_pref6,
  weap_pref7,
  weap_pref8,
  weap_pref9,
  weap_stub2,
  weap_toggle,
  weap_toggle2,
};

setup_menu_t weap_settings1[];

setup_menu_t* weap_settings[] =
{
  weap_settings1,
  NULL
};

setup_menu_t weap_settings1[] =  // Weapons Settings screen       
{
  {"ENABLE RECOIL", S_YESNO,m_null,WP_X, WP_Y+ weap_recoil*8, {"weapon_recoil"}},
  {"ENABLE BOBBING",S_YESNO,m_null,WP_X, WP_Y+weap_bobbing*8, {"player_bobbing"}},

#ifdef BETA
  {"CLASSIC BFG"      ,S_YESNO,m_null,WP_X,  // killough 8/8/98
   WP_Y+ weap_bfg*8, {"classic_bfg"}},
#endif

  {"1ST CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref1*8, {"weapon_choice_1"}},
  {"2nd CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref2*8, {"weapon_choice_2"}},
  {"3rd CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref3*8, {"weapon_choice_3"}},
  {"4th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref4*8, {"weapon_choice_4"}},
  {"5th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref5*8, {"weapon_choice_5"}},
  {"6th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref6*8, {"weapon_choice_6"}},
  {"7th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref7*8, {"weapon_choice_7"}},
  {"8th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref8*8, {"weapon_choice_8"}},
  {"9th CHOICE WEAPON",S_WEAP,m_null,WP_X,WP_Y+weap_pref9*8, {"weapon_choice_9"}},

  {"Enable Fist/Chainsaw\n& SG/SSG toggle", S_YESNO, m_null, WP_X,
   WP_Y+ weap_toggle*8, {"doom_weapon_toggles"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

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
  mult_screens_index = 0;
  current_setup_menu = weap_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Weapons Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawWeapons(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (109,2,0,W_CacheLumpName("M_WEAP",PU_CACHE));
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

#define ST_X 203
#define ST_Y  31

// Screen table definitions

setup_menu_t stat_settings1[];

setup_menu_t* stat_settings[] =
{
  stat_settings1,
  NULL
};

setup_menu_t stat_settings1[] =  // Status Bar and HUD Settings screen       
{
  {"STATUS BAR"        ,S_SKIP|S_TITLE,m_null,ST_X,ST_Y+ 1*8 },

  {"USE RED NUMBERS"   ,S_YESNO, m_null,ST_X,ST_Y+ 2*8, {"sts_always_red"}},
  {"GRAY %"            ,S_YESNO, m_null,ST_X,ST_Y+ 3*8, {"sts_pct_always_gray"}},
  {"SINGLE KEY DISPLAY",S_YESNO, m_null,ST_X,ST_Y+ 4*8, {"sts_traditional_keys"}},

  {"HEADS-UP DISPLAY"  ,S_SKIP|S_TITLE,m_null,ST_X,ST_Y+ 6*8},

  {"HIDE SECRETS"      ,S_YESNO     ,m_null,ST_X,ST_Y+ 7*8, {"hud_nosecrets"}},
  {"HEALTH LOW/OK"     ,S_NUM       ,m_null,ST_X,ST_Y+ 8*8, {"health_red"}},
  {"HEALTH OK/GOOD"    ,S_NUM       ,m_null,ST_X,ST_Y+ 9*8, {"health_yellow"}},
  {"HEALTH GOOD/EXTRA" ,S_NUM       ,m_null,ST_X,ST_Y+10*8, {"health_green"}},
  {"ARMOR LOW/OK"      ,S_NUM       ,m_null,ST_X,ST_Y+11*8, {"armor_red"}},
  {"ARMOR OK/GOOD"     ,S_NUM       ,m_null,ST_X,ST_Y+12*8, {"armor_yellow"}},
  {"ARMOR GOOD/EXTRA"  ,S_NUM       ,m_null,ST_X,ST_Y+13*8, {"armor_green"}},
  {"AMMO LOW/OK"       ,S_NUM       ,m_null,ST_X,ST_Y+14*8, {"ammo_red"}},
  {"AMMO OK/GOOD"      ,S_NUM       ,m_null,ST_X,ST_Y+15*8, {"ammo_yellow"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

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
  mult_screens_index = 0;
  current_setup_menu = stat_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Status Bar / HUD Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawStatusHUD(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (59,2,0,W_CacheLumpName("M_STAT",PU_CACHE));
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

#define AU_X    250
#define AU_Y     31
#define AU_PREV KB_PREV
#define AU_NEXT KB_NEXT

setup_menu_t auto_settings1[];       
setup_menu_t auto_settings2[];

setup_menu_t* auto_settings[] =
{
  auto_settings1,
  auto_settings2,
  NULL
};

setup_menu_t auto_settings1[] =  // 1st AutoMap Settings screen       
{
  {"background", S_COLOR, m_null, AU_X, AU_Y, {"mapcolor_back"}},
  {"grid lines", S_COLOR, m_null, AU_X, AU_Y + 1*8, {"mapcolor_grid"}},
  {"normal 1s wall", S_COLOR, m_null,AU_X,AU_Y+ 2*8, {"mapcolor_wall"}},
  {"line at floor height change", S_COLOR, m_null, AU_X, AU_Y+ 3*8, {"mapcolor_fchg"}},
  {"line at ceiling height change"      ,S_COLOR,m_null,AU_X,AU_Y+ 4*8, {"mapcolor_cchg"}},
  {"line at sector with floor = ceiling",S_COLOR,m_null,AU_X,AU_Y+ 5*8, {"mapcolor_clsd"}},
  {"red key"                            ,S_COLOR,m_null,AU_X,AU_Y+ 6*8, {"mapcolor_rkey"}},
  {"blue key"                           ,S_COLOR,m_null,AU_X,AU_Y+ 7*8, {"mapcolor_bkey"}},
  {"yellow key"                         ,S_COLOR,m_null,AU_X,AU_Y+ 8*8, {"mapcolor_ykey"}},
  {"red door"                           ,S_COLOR,m_null,AU_X,AU_Y+ 9*8, {"mapcolor_rdor"}},
  {"blue door"                          ,S_COLOR,m_null,AU_X,AU_Y+10*8, {"mapcolor_bdor"}},
  {"yellow door"                        ,S_COLOR,m_null,AU_X,AU_Y+11*8, {"mapcolor_ydor"}},

  {"AUTOMAP LEVEL TITLE COLOR"      ,S_CRITEM,m_null,AU_X,AU_Y+13*8, {"hudcolor_titl"}},
  {"AUTOMAP COORDINATES COLOR"      ,S_CRITEM,m_null,AU_X,AU_Y+14*8, {"hudcolor_xyco"}},

  {"Show Secrets only after entering",S_YESNO,m_null,AU_X,AU_Y+15*8, {"map_secret_after"}},

  {"Show coordinates of automap pointer",S_YESNO,m_null,AU_X,AU_Y+16*8, {"map_point_coord"}},  // killough 10/98

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,AU_NEXT,AU_Y+20*8, {auto_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}

};

setup_menu_t auto_settings2[] =  // 2nd AutoMap Settings screen
{
  {"teleporter line"                ,S_COLOR ,m_null,AU_X,AU_Y, {"mapcolor_tele"}},
  {"secret sector boundary"         ,S_COLOR ,m_null,AU_X,AU_Y+ 1*8, {"mapcolor_secr"}},
  //jff 4/23/98 add exit line to automap
  {"exit line"                      ,S_COLOR ,m_null,AU_X,AU_Y+ 2*8, {"mapcolor_exit"}},
  {"computer map unseen line"       ,S_COLOR ,m_null,AU_X,AU_Y+ 3*8, {"mapcolor_unsn"}},
  {"line w/no floor/ceiling changes",S_COLOR ,m_null,AU_X,AU_Y+ 4*8, {"mapcolor_flat"}},
  {"general sprite"                 ,S_COLOR ,m_null,AU_X,AU_Y+ 5*8, {"mapcolor_sprt"}},
  {"crosshair"                      ,S_COLOR ,m_null,AU_X,AU_Y+ 6*8, {"mapcolor_hair"}},
  {"single player arrow"            ,S_COLOR ,m_null,AU_X,AU_Y+ 7*8, {"mapcolor_sngl"}},
  {"player 1 arrow"                 ,S_COLOR ,m_null,AU_X,AU_Y+ 8*8, {"mapcolor_ply1"}},
  {"player 2 arrow"                 ,S_COLOR ,m_null,AU_X,AU_Y+ 9*8, {"mapcolor_ply2"}},
  {"player 3 arrow"                 ,S_COLOR ,m_null,AU_X,AU_Y+10*8, {"mapcolor_ply3"}},
  {"player 4 arrow"                 ,S_COLOR ,m_null,AU_X,AU_Y+11*8, {"mapcolor_ply4"}},

  {"friends"                        ,S_COLOR ,m_null,AU_X,AU_Y+12*8, {"mapcolor_frnd"}},        // killough 8/8/98

  {"<- PREV",S_SKIP|S_PREV,m_null,AU_PREV,AU_Y+20*8, {auto_settings1}},

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
  set_menu_itemon = 0;
  mult_screens_index = 0;
  current_setup_menu = auto_settings[0];
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
  int i,cpx,cpy;
  byte *ptr;

  // Draw a background, border, and paint chips

  V_DrawPatchDirect (COLORPALXORIG-5,COLORPALYORIG-5,0,W_CacheLumpName("M_COLORS",PU_CACHE));

  // Draw the cursor around the paint chip
  // (cpx,cpy) is the upper left-hand corner of the paint chip

  ptr = colorblock;
  for (i = 0 ; i < CHIP_SIZE+2 ; i++)
    *ptr++ = PAL_WHITE;
  cpx = COLORPALXORIG+color_palette_x*(CHIP_SIZE+1)-1;
  cpy = COLORPALYORIG+color_palette_y*(CHIP_SIZE+1)-1;
  V_DrawBlock(cpx,cpy,0,CHIP_SIZE+2,1,colorblock);
  V_DrawBlock(cpx+CHIP_SIZE+1,cpy,0,1,CHIP_SIZE+2,colorblock);
  V_DrawBlock(cpx,cpy+CHIP_SIZE+1,0,CHIP_SIZE+2,1,colorblock);
  V_DrawBlock(cpx,cpy,0,1,CHIP_SIZE+2,colorblock);
}

// The drawing part of the Automap Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawAutoMap(void)

{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (109,2,0,W_CacheLumpName("M_AUTO",PU_CACHE));
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

#define E_X 250
#define E_Y  31

setup_menu_t enem_settings1[];

setup_menu_t* enem_settings[] =
{
  enem_settings1,
  NULL
};

enum {
  enem_infighting,

  enem_remember = 1,

  enem_backing,
  enem_monkeys,
  enem_avoid_hazards,
  enem_friction,
  enem_help_friends,

#ifdef DOGS 
  enem_helpers,
#endif

  enem_distfriend,

#ifdef DOGS 
  enem_dog_jumping,
#endif

  enem_end
};

setup_menu_t enem_settings1[] =  // Enemy Settings screen       
{
  // killough 7/19/98
  {"Monster Infighting When Provoked",S_YESNO,m_null,E_X,E_Y+ enem_infighting*8, {"monster_infighting"}},

  {"Remember Previous Enemy",S_YESNO,m_null,E_X,E_Y+ enem_remember*8, {"monsters_remember"}},

  // killough 9/8/98
  {"Monster Backing Out",S_YESNO,m_null,E_X,E_Y+ enem_backing*8, {"monster_backing"}},

  {"Climb Steep Stairs", S_YESNO,m_null,E_X,E_Y+enem_monkeys*8, {"monkeys"}},

  // killough 9/9/98
  {"Intelligently Avoid Hazards",S_YESNO,m_null,E_X,E_Y+ enem_avoid_hazards*8, {"monster_avoid_hazards"}},

  // killough 10/98
  {"Affected by Friction",S_YESNO,m_null,E_X,E_Y+ enem_friction*8, {"monster_friction"}},

  {"Rescue Dying Friends",S_YESNO,m_null,E_X,E_Y+ enem_help_friends*8, {"help_friends"}},

#ifdef DOGS
  // killough 7/19/98
  {"Number Of Single-Player Helper Dogs",S_NUM|S_LEVWARN,m_null,E_X,E_Y+ enem_helpers*8, {"player_helpers"}},

  // killough 8/8/98
  {"Distance Friends Stay Away",S_NUM,m_null,E_X,E_Y+ enem_distfriend*8, {"friend_distance"}},

  {"Allow dogs to jump down",S_YESNO,m_null,E_X,E_Y+ enem_dog_jumping*8, {"dog_jumping"}},
#endif

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
  mult_screens_index = 0;
  current_setup_menu = enem_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Enemies Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawEnemy(void)

{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (114,2,0,W_CacheLumpName("M_ENEM",PU_CACHE));
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

extern int usejoystick, usemouse, default_mus_card, default_snd_card;
extern int detect_voices, realtic_clock_rate, tran_filter_pct;

setup_menu_t gen_settings1[], gen_settings2[];

setup_menu_t* gen_settings[] =
{
  gen_settings1,
  gen_settings2,
  NULL
};

enum {
  general_hires,  
  general_pageflip,
  general_vsync,
  general_trans,
  general_transpct,
  general_pcx,
  general_diskicon,
  general_hom
};

enum {
  general_sndcard,
  general_muscard,
  general_detvoices,
  general_sndchan,
  general_pitch
};

#define G_X 250
#define G_Y  44
#define G_Y2 (G_Y+82)
#define G_Y3 (G_Y+44)
#define G_Y4 (G_Y3+52)
#define GF_X 76

setup_menu_t gen_settings1[] = { // General Settings screen1

  {"Video"       ,S_SKIP|S_TITLE, m_null, G_X, G_Y - 12},

  {"High Resolution", S_YESNO, m_null, G_X, G_Y + general_hires*8,
   {"hires"}, 0, 0, I_ResetScreen},

  {"Use Page-Flipping", S_YESNO, m_null, G_X, G_Y + general_pageflip*8,
   {"page_flip"}, 0, 0, I_ResetScreen},

  {"Wait for Vertical Retrace", S_YESNO, m_null, G_X,
   G_Y + general_vsync*8, {"use_vsync"}, 0, 0, I_ResetScreen},

  {"Enable Translucency", S_YESNO, m_null, G_X,
   G_Y + general_trans*8, {"translucency"}, 0, 0, M_Trans},

  {"Translucency filter percentage", S_NUM, m_null, G_X,
   G_Y + general_transpct*8, {"tran_filter_pct"}, 0, 0, M_Trans},

  {"PCX instead of BMP for screenshots", S_YESNO, m_null, G_X,
   G_Y + general_pcx*8, {"screenshot_pcx"}},

  {"Flash Icon During Disk IO", S_YESNO, m_null, G_X,
   G_Y + general_diskicon*8, {"disk_icon"}},

  {"Flashing HOM indicator", S_YESNO, m_null, G_X,
   G_Y + general_hom*8, {"flashing_hom"}},

  {"Sound & Music", S_SKIP|S_TITLE, m_null, G_X, G_Y2 - 12},

  {"Sound Card", S_NUM|S_PRGWARN, m_null, G_X,
   G_Y2 + general_sndcard*8, {"sound_card"}},

  {"Music Card", S_NUM|S_PRGWARN, m_null, G_X,
   G_Y2 + general_muscard*8, {"music_card"}},

  {"Autodetect Number of Voices", S_YESNO|S_PRGWARN, m_null, G_X,
   G_Y2 + general_detvoices*8, {"detect_voices"}},

  {"Number of Sound Channels", S_NUM|S_PRGWARN, m_null, G_X,
   G_Y2 + general_sndchan*8, {"snd_channels"}},

  {"Enable v1.1 Pitch Effects", S_YESNO, m_null, G_X,
   G_Y2 + general_pitch*8, {"pitched_sounds"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT,m_null,KB_NEXT,KB_Y+20*8, {gen_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

enum {
  general_mouse,
  general_joy,
  general_leds
};

enum {
  general_wad1,
  general_wad2,
  general_deh1,
  general_deh2
};

enum {
  general_corpse,
  general_realtic,
  general_end
};

setup_menu_t gen_settings2[] = { // General Settings screen2

  {"Input Devices"     ,S_SKIP|S_TITLE, m_null, G_X, G_Y - 12},

  {"Enable Mouse", S_YESNO, m_null, G_X,
   G_Y + general_mouse*8, {"use_mouse"}},

  {"Enable Joystick", S_YESNO, m_null, G_X,
   G_Y + general_joy*8, {"use_joystick"}},

#if 0
  {"Keyboard LEDs Always Off", S_YESNO, m_null, G_X,
   G_Y + general_leds*8, {"leds_always_off"}, 0, 0, I_ResetLEDs},
#endif

  {"Files Preloaded at Game Startup",S_SKIP|S_TITLE, m_null, G_X,
   G_Y3 - 12},

  {"WAD # 1", S_FILE, m_null, GF_X, G_Y3 + general_wad1*8, {"wadfile_1"}},

  {"WAD #2", S_FILE, m_null, GF_X, G_Y3 + general_wad2*8, {"wadfile_2"}},

  {"DEH/BEX # 1", S_FILE, m_null, GF_X, G_Y3 + general_deh1*8, {"dehfile_1"}},

  {"DEH/BEX #2", S_FILE, m_null, GF_X, G_Y3 + general_deh2*8, {"dehfile_2"}},

  {"Miscellaneous"  ,S_SKIP|S_TITLE, m_null, G_X, G_Y4 - 12},

  {"Maximum number of player corpses", S_NUM|S_PRGWARN, m_null, G_X,
   G_Y4 + general_corpse*8, {"max_player_corpse"}},

  {"Game speed, percentage of normal", S_NUM|S_PRGWARN, m_null, G_X,
   G_Y4 + general_realtic*8, {"realtic_clock_rate"}},

  {"<- PREV",S_SKIP|S_PREV, m_null, KB_PREV, KB_Y+20*8, {gen_settings1}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

void M_Trans(void) // To reset translucency after setting it in menu
{
  if (general_translucency)
    R_InitTranMap(0);
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
  mult_screens_index = 0;
  current_setup_menu = gen_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawGeneral(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (114,2,0,W_CacheLumpName("M_GENERL",PU_CACHE));
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
#define C_Y  32
#define COMP_SPC 12
#define C_NEXTPREV 131

setup_menu_t comp_settings1[], comp_settings2[];

setup_menu_t* comp_settings[] =
{
  comp_settings1,
  comp_settings2,
  NULL
};

enum
{
  compat_telefrag,
  compat_dropoff,
  compat_falloff,
  compat_staylift,
  compat_doorstuck,
  compat_pursuit,
  compat_vile,
  compat_pain,
  compat_skull,
  compat_blazing,
  compat_doorlight = 0,
  compat_god,
  compat_infcheat,
  compat_zombie,
  compat_skymap,
  compat_stairs,
  compat_floors,
  compat_model,
  compat_zerotags,
  compat_menu,
};

setup_menu_t comp_settings1[] =  // Compatibility Settings screen #1
{
  {"Any monster can telefrag on MAP30", S_YESNO, m_null, C_X,
   C_Y + compat_telefrag * COMP_SPC, {"comp_telefrag"}},

  {"Some objects never hang over tall ledges", S_YESNO, m_null, C_X,
   C_Y + compat_dropoff * COMP_SPC, {"comp_dropoff"}},

  {"Objects don't fall under their own weight", S_YESNO, m_null, C_X,
   C_Y + compat_falloff * COMP_SPC, {"comp_falloff"}},

  {"Monsters randomly walk off of moving lifts", S_YESNO, m_null, C_X,
   C_Y + compat_staylift * COMP_SPC, {"comp_staylift"}},

  {"Monsters get stuck on doortracks", S_YESNO, m_null, C_X,
   C_Y + compat_doorstuck * COMP_SPC, {"comp_doorstuck"}},

  {"Monsters don't give up pursuit of targets", S_YESNO, m_null, C_X,
   C_Y + compat_pursuit * COMP_SPC, {"comp_pursuit"}},

  {"Arch-Vile resurrects invincible ghosts", S_YESNO, m_null, C_X,
   C_Y + compat_vile * COMP_SPC, {"comp_vile"}},

  {"Pain Elemental limited to 20 lost souls", S_YESNO, m_null, C_X,
   C_Y + compat_pain * COMP_SPC, {"comp_pain"}},

  {"Lost souls get stuck behind walls", S_YESNO, m_null, C_X,
   C_Y + compat_skull * COMP_SPC, {"comp_skull"}},

  {"Blazing doors make double closing sounds", S_YESNO, m_null, C_X,
   C_Y + compat_blazing * COMP_SPC, {"comp_blazing"}},

  // Button for resetting to defaults
  {0,S_RESET,m_null,X_BUTTON,Y_BUTTON},

  {"NEXT ->",S_SKIP|S_NEXT, m_null, KB_NEXT, C_Y+C_NEXTPREV, {comp_settings2}},

  // Final entry
  {0,S_SKIP|S_END,m_null}
};

setup_menu_t comp_settings2[] =  // Compatibility Settings screen #2
{
  {"Tagged doors don't trigger special lighting", S_YESNO, m_null, C_X,
   C_Y + compat_doorlight * COMP_SPC, {"comp_doorlight"}},

  {"God mode isn't absolute", S_YESNO, m_null, C_X,
   C_Y + compat_god * COMP_SPC, {"comp_god"}},

  {"Powerup cheats are not infinite duration", S_YESNO, m_null, C_X,
   C_Y + compat_infcheat * COMP_SPC, {"comp_infcheat"}},

  {"Zombie players can exit levels", S_YESNO, m_null, C_X,
   C_Y + compat_zombie * COMP_SPC, {"comp_zombie"}},

  {"Sky is unaffected by invulnerability", S_YESNO, m_null, C_X,
   C_Y + compat_skymap * COMP_SPC, {"comp_skymap"}},

  {"Use exactly Doom's stairbuilding method", S_YESNO, m_null, C_X,
   C_Y + compat_stairs * COMP_SPC, {"comp_stairs"}},

  {"Use exactly Doom's floor motion behavior", S_YESNO, m_null, C_X,
   C_Y + compat_floors * COMP_SPC, {"comp_floors"}},

  {"Use exactly Doom's linedef trigger model", S_YESNO, m_null, C_X,
   C_Y + compat_model * COMP_SPC, {"comp_model"}},

  {"Linedef effects work with sector tag = 0", S_YESNO, m_null, C_X,
   C_Y + compat_zerotags * COMP_SPC, {"comp_zerotags"}},

  {"Use Doom's main menu ordering", S_YESNO, m_null, C_X,
   C_Y + compat_menu * COMP_SPC, {"traditional_menu"}, 0, 0, M_ResetMenu},

  {"<- PREV", S_SKIP|S_PREV, m_null, KB_PREV, C_Y+C_NEXTPREV,{comp_settings1}},

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
  mult_screens_index = 0;
  current_setup_menu = comp_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Compatibility Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawCompat(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect(52,2,0,W_CacheLumpName("M_COMPAT",PU_CACHE));
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

#define M_X 230
#define M_Y  39

// killough 11/98: enumerated

enum {
  mess_color_play,
  mess_timer,
  mess_color_chat,
  mess_chat_timer,
  mess_color_review,
  mess_timed,
  mess_hud_timer,
  mess_lines,
  mess_scrollup,
  mess_background,
};

setup_menu_t mess_settings1[];

setup_menu_t* mess_settings[] =
{
  mess_settings1,
  NULL
};

setup_menu_t mess_settings1[] =  // Messages screen       
{
  {"Message Color During Play", S_CRITEM, m_null, M_X,
   M_Y + mess_color_play*8, {"hudcolor_mesg"}},

  {"Message Duration During Play (ms)", S_NUM, m_null, M_X,
   M_Y  + mess_timer*8, {"message_timer"}},

  {"Chat Message Color", S_CRITEM, m_null, M_X,
   M_Y + mess_color_chat*8, {"hudcolor_chat"}},

  {"Chat Message Duration (ms)", S_NUM, m_null, M_X,
   M_Y  + mess_chat_timer*8, {"chat_msg_timer"}},

  {"Message Review Color", S_CRITEM, m_null, M_X,
   M_Y + mess_color_review*8, {"hudcolor_list"}},

  {"Message Listing Review is Temporary",  S_YESNO,  m_null,  M_X,
   M_Y + mess_timed*8, {"hud_msg_timed"}},

  {"Message Review Duration (ms)", S_NUM, m_null, M_X,
   M_Y  + mess_hud_timer*8, {"hud_msg_timer"}},

  {"Number of Review Message Lines", S_NUM, m_null,  M_X,
   M_Y + mess_lines*8, {"hud_msg_lines"}},

  {"Message Listing Scrolls Upwards",  S_YESNO,  m_null,  M_X,
   M_Y + mess_scrollup*8, {"hud_msg_scrollup"}},

  {"Message Background",  S_YESNO,  m_null,  M_X,  
   M_Y + mess_background*8, {"hud_list_bgon"}},

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
  mult_screens_index = 0;
  current_setup_menu = mess_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Messages Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawMessages(void)

{
  inhelpscreens = true;
  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (103,2,0,W_CacheLumpName("M_MESS",PU_CACHE));
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);
  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Chat Strings table.

#define CS_X 20
#define CS_Y (31+8)

setup_menu_t chat_settings1[];

setup_menu_t* chat_settings[] =
{
  chat_settings1,
  NULL
};

setup_menu_t chat_settings1[] =  // Chat Strings screen       
{
  {"1",S_CHAT,m_null,CS_X,CS_Y+ 1*8, {"chatmacro1"}},
  {"2",S_CHAT,m_null,CS_X,CS_Y+ 2*8, {"chatmacro2"}},
  {"3",S_CHAT,m_null,CS_X,CS_Y+ 3*8, {"chatmacro3"}},
  {"4",S_CHAT,m_null,CS_X,CS_Y+ 4*8, {"chatmacro4"}},
  {"5",S_CHAT,m_null,CS_X,CS_Y+ 5*8, {"chatmacro5"}},
  {"6",S_CHAT,m_null,CS_X,CS_Y+ 6*8, {"chatmacro6"}},
  {"7",S_CHAT,m_null,CS_X,CS_Y+ 7*8, {"chatmacro7"}},
  {"8",S_CHAT,m_null,CS_X,CS_Y+ 8*8, {"chatmacro8"}},
  {"9",S_CHAT,m_null,CS_X,CS_Y+ 9*8, {"chatmacro9"}},
  {"0",S_CHAT,m_null,CS_X,CS_Y+10*8, {"chatmacro0"}},

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
  mult_screens_index = 0;
  current_setup_menu = chat_settings[0];
  set_menu_itemon = 0;
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Chat Strings Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawChatStrings(void)

{
  inhelpscreens = true;
  M_DrawBackground("FLOOR4_6", screens[0]); // Draw background
  V_DrawPatchDirect (83,2,0,W_CacheLumpName("M_CHAT",PU_CACHE));
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
		p->var.m_key == dp->location || 
		p->m_mouse == dp->location ||
		p->m_joy == dp->location)
	      {
		if (dp->isstr)
		  free(*(char **) dp->location),
		    *(char **) dp->location = strdup((char *)dp->defaultvalue);
		else
		  *dp->location = dp->defaultvalue;
	
		if (p->m_flags & (S_LEVWARN | S_PRGWARN))
		  warn |= p->m_flags & (S_LEVWARN | S_PRGWARN);
		else
		  if (dp->current)
		    if (allow_changes())
		      *dp->current = *dp->location;
		    else
		      warn |= S_LEVWARN;
		
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
	  if (!(dp = M_LookupDefault(t->var.name)))
	    I_Error("Could not find config variable \"%s\"", t->var.name);
	  else
	    (t->var.def = dp)->setup_menu = t;
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
	    if (gamemode == commercial)
	      {
		ExtHelpDef.prevMenu  = &ReadDef1; // previous menu
		ReadMenu1[0].routine = M_ExtHelp;
	      }
	    else
	      {
		ExtHelpDef.prevMenu  = &ReadDef2; // previous menu
		ReadMenu2[0].routine = M_ExtHelp;
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
  char* namebfr = "HELPnn";

  inhelpscreens = true;              // killough 5/1/98
  namebfr[4] = extended_help_index/10 + 0x30;
  namebfr[5] = extended_help_index%10 + 0x30;
  V_DrawPatchDirect(0,0,0,W_CacheLumpName(namebfr,PU_CACHE));
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
  char* s;

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

      switch(c)
	{
	case KEYD_TAB:
	  s = "TAB";
	  break;
	case KEYD_ENTER:
	  s = "ENTR"; 
	  break;
	case KEYD_ESCAPE:
	  s = "ESC";
	  break;
	case KEYD_SPACEBAR:
	  s = "SPAC";
	  break;
	case KEYD_BACKSPACE:
	  s = "BACK";
	  break;
	case KEYD_RCTRL:
	  s = "CTRL";
	  break;
	case KEYD_LEFTARROW:
	  s = "LARR";
	  break;
	case KEYD_UPARROW:
	  s = "UARR";
	  break;
	case KEYD_RIGHTARROW:
	  s = "RARR";
	  break;
	case KEYD_DOWNARROW:
	  s = "DARR";
	  break;
	case KEYD_RSHIFT:
	  s = "SHFT";
	  break;
	case KEYD_RALT:
	  s = "ALT";
	  break;
	case KEYD_CAPSLOCK:
	  s = "CAPS";
	  break;
	case KEYD_F1:
	  s = "F1";
	  break;
	case KEYD_F2:
	  s = "F2";
	  break;
	case KEYD_F3:
	  s = "F3";
	  break;
	case KEYD_F4:
	  s = "F4";
	  break;
	case KEYD_F5:
	  s = "F5";
	  break;
	case KEYD_F6:
	  s = "F6";
	  break;
	case KEYD_F7:
	  s = "F7";
	  break;
	case KEYD_F8:
	  s = "F8";
	  break;
	case KEYD_F9:
	  s = "F9";
	  break;
	case KEYD_F10:
	  s = "F10";
	  break;
	case KEYD_SCROLLLOCK:
	  s = "SCRL";
	  break;
	case KEYD_HOME:
	  s = "HOME";
	  break;
	case KEYD_PAGEUP:
	  s = "PGUP";
	  break;
	case KEYD_END:
	  s = "END";
	  break;
	case KEYD_PAGEDOWN:
	  s = "PGDN";
	  break;
	case KEYD_INSERT:
	  s = "INST";
	  break;
	case KEYD_F11:
	  s = "F11";
	  break;
	case KEYD_F12:
	  s = "F12";
	  break;
	case KEYD_PAUSE:
	  s = "PAUS";
	  break;
	default:
	  s = "JUNK";
	  break;
	}
      strcpy(&menu_buffer[offset],s); // string to display
      offset += strlen(s);
    }
  return offset;
}

//
// The Dynamic HELP screen table.

#define KT_X1 283
#define KT_X2 172
#define KT_X3  87

#define KT_Y1   2
#define KT_Y2 118
#define KT_Y3 102

setup_menu_t helpstrings[] =  // HELP screen strings       
{
  {"SCREEN"      ,S_SKIP|S_TITLE,m_null,KT_X1,KT_Y1},
  {"HELP"        ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 1*8,{&key_help}},
  {"MENU"        ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 2*8,{&key_escape}},
  {"SETUP"       ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 3*8,{&key_setup}},
  {"PAUSE"       ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 4*8,{&key_pause}},
  {"AUTOMAP"     ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 5*8,{&key_map}},
  {"SOUND VOLUME",S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 6*8,{&key_soundvolume}},
  {"HUD"         ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 7*8,{&key_hud}},
  {"MESSAGES"    ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 8*8,{&key_messages}},
  {"GAMMA FIX"   ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+ 9*8,{&key_gamma}},
  {"SPY"         ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+10*8,{&key_spy}},
  {"LARGER VIEW" ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+11*8,{&key_zoomin}},
  {"SMALLER VIEW",S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+12*8,{&key_zoomout}},
  {"SCREENSHOT"  ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y1+13*8,{&key_screenshot}},

  {"AUTOMAP"     ,S_SKIP|S_TITLE,m_null,KT_X1,KT_Y2},
  {"FOLLOW MODE" ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 1*8,{&key_map_follow}},
  {"ZOOM IN"     ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 2*8,{&key_map_zoomin}},
  {"ZOOM OUT"    ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 3*8,{&key_map_zoomout}},
  {"MARK PLACE"  ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 4*8,{&key_map_mark}},
  {"CLEAR MARKS" ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 5*8,{&key_map_clear}},
  {"FULL/ZOOM"   ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 6*8,{&key_map_gobig}},
  {"GRID"        ,S_SKIP|S_KEY,m_null,KT_X1,KT_Y2+ 7*8,{&key_map_grid}},

  {"WEAPONS"     ,S_SKIP|S_TITLE,m_null,KT_X3,KT_Y1},
  {"FIST"        ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 1*8,{&key_weapon1}},
  {"PISTOL"      ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 2*8,{&key_weapon2}},
  {"SHOTGUN"     ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 3*8,{&key_weapon3}},
  {"CHAINGUN"    ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 4*8,{&key_weapon4}},
  {"ROCKET"      ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 5*8,{&key_weapon5}},
  {"PLASMA"      ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 6*8,{&key_weapon6}},
  {"BFG 9000"    ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 7*8,{&key_weapon7}},
  {"CHAINSAW"    ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 8*8,{&key_weapon8}},
  {"SSG"         ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+ 9*8,{&key_weapon9}},
  {"BEST"        ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+10*8,{&key_weapontoggle}},
  {"FIRE"        ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y1+11*8,{&key_fire},&mousebfire,&joybfire},

  {"MOVEMENT"    ,S_SKIP|S_TITLE,m_null,KT_X3,KT_Y3},
  {"FORWARD"     ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 1*8,{&key_up},&mousebforward},
  {"BACKWARD"    ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 2*8,{&key_down}},
  {"TURN LEFT"   ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 3*8,{&key_left}},
  {"TURN RIGHT"  ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 4*8,{&key_right}},
  {"RUN"         ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 5*8,{&key_speed},0,&joybspeed},
  {"STRAFE LEFT" ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 6*8,{&key_strafeleft}},
  {"STRAFE RIGHT",S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 7*8,{&key_straferight}},
  {"STRAFE"      ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 8*8,{&key_strafe},&mousebstrafe,&joybstrafe},
  {"AUTORUN"     ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+ 9*8,{&key_autorun}},
  {"180 TURN"    ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+10*8,{&key_reverse}},
  {"USE"         ,S_SKIP|S_KEY,m_null,KT_X3,KT_Y3+11*8,{&key_use},&mousebforward,&joybuse},

  {"GAME"        ,S_SKIP|S_TITLE,m_null,KT_X2,KT_Y1},
  {"SAVE"        ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 1*8,{&key_savegame}},
  {"LOAD"        ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 2*8,{&key_loadgame}},
  {"QUICKSAVE"   ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 3*8,{&key_quicksave}},
  {"END GAME"    ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 4*8,{&key_endgame}},
  {"QUICKLOAD"   ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 5*8,{&key_quickload}},
  {"QUIT"        ,S_SKIP|S_KEY,m_null,KT_X2,KT_Y1+ 6*8,{&key_quit}},

  // Final entry

  {0,S_SKIP|S_END,m_null}
};

#define SPACEWIDTH 4

// M_DrawMenuString() draws the string in menu_buffer[]

void M_DrawMenuString(int cx, int cy, int color)
{
  int   w;
  int   c;
  char* ch = menu_buffer;

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
    
      V_DrawPatchTranslated(cx,cy,0,hu_font[c],colrngs[color],0);

      // The screen is cramped, so trim one unit from each
      // character so they butt up against each other.
      cx += w - 1; 
    }
}

// M_GetPixelWidth() returns the number of pixels in the width of
// the string, NOT the number of chars in the string.

int M_GetPixelWidth(char* ch)
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
      len--; // adjust so everything fits
    }
  len++; // replace what you took away on the last char only
  return len;
}

//
// M_DrawHelp
//
// This displays the help screen

void M_DrawHelp (void)
{
  inhelpscreens = true;                        // killough 10/98
  M_DrawBackground("FLOOR4_6", screens[0]);
  V_MarkRect (0,0,SCREENWIDTH,SCREENHEIGHT);
  M_DrawScreenItems(helpstrings);
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
  adcr, adcr_stub,
  special, special_stub, special_stub2,
};

enum {
  cr_prog=1,
  cr_art,
  cr_test,
  cr_canine,
  cr_musicsfx,
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
  inhelpscreens = true;
  M_DrawBackground(gamemode==shareware ? "CEIL5_1" : "MFLR8_4", screens[0]);
  V_DrawPatchTranslated(42,9,0, W_CacheLumpName("MBFTEXT",PU_CACHE),
			colrngs[CR_GOLD],0);
  V_MarkRect(0,0,SCREENWIDTH,SCREENHEIGHT);
  M_DrawScreenItems(cred_settings);
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
  int    i;
  static int joywait   = 0;
  static int mousewait = 0;
  static int mousey    = 0;
  static int lasty     = 0;
  static int mousex    = 0;
  static int lastx     = 0;
  
  ch = -1; // will be changed to a legit char if we're going to use it here

  // Process joystick input

  if (ev->type == ev_joystick && joywait < I_GetTime())
    {
      if (ev->data3 == -1)
	{
	  ch = key_menu_up;                                // phares 3/7/98
	  joywait = I_GetTime() + 5;
	}
      else if (ev->data3 == 1)
	{
	  ch = key_menu_down;                              // phares 3/7/98
	  joywait = I_GetTime() + 5;
	}
  
      if (ev->data2 == -1)
	{
	  ch = key_menu_left;                              // phares 3/7/98
	  joywait = I_GetTime() + 2;
	}
      else if (ev->data2 == 1)
	{
	  ch = key_menu_right;                             // phares 3/7/98
	  joywait = I_GetTime() + 2;
	}
  
      if (ev->data1&1)
	{
	  ch = key_menu_enter;                             // phares 3/7/98
	  joywait = I_GetTime() + 5;
	}

      if (ev->data1&2)
	{
	  ch = key_menu_backspace;                         // phares 3/7/98
	  joywait = I_GetTime() + 5;
	}

// phares 4/4/98:
      // Handle joystick buttons 3 and 4, and allow them to pass down
      // to where key binding can eat them.

      if (setup_active && set_keybnd_active)
	{
	  if (ev->data1&4)
	    {
	      ch = 0; // meaningless, just to get you past the check for -1
	      joywait = I_GetTime() + 5;
	    }
	  if (ev->data1&8)
	    {
	      ch = 0; // meaningless, just to get you past the check for -1
	      joywait = I_GetTime() + 5;
	    }
	}

    }
  else
    {

      // Process mouse input

      if (ev->type == ev_mouse && mousewait < I_GetTime())
	{
	  mousey += ev->data3;
	  if (mousey < lasty-30)
	    {
	      ch = key_menu_down;                            // phares 3/7/98
	      mousewait = I_GetTime() + 5;
	      mousey = lasty -= 30;
	    }
	  else if (mousey > lasty+30)
	    {
	      ch = key_menu_up;                              // phares 3/7/98
	      mousewait = I_GetTime() + 5;
	      mousey = lasty += 30;
	    }
  
	  mousex += ev->data2;
	  if (mousex < lastx-30)
	    {
	      ch = key_menu_left;                            // phares 3/7/98
	      mousewait = I_GetTime() + 5;
	      mousex = lastx -= 30;
	    }
	  else if (mousex > lastx+30)
	    {
	      ch = key_menu_right;                           // phares 3/7/98
	      mousewait = I_GetTime() + 5;
	      mousex = lastx += 30;
	    }
  
	  if (ev->data1&1)
	    {
	      ch = key_menu_enter;                           // phares 3/7/98
	      mousewait = I_GetTime() + 15;
	    }
    
	  if (ev->data1&2)
	    {
	      ch = key_menu_backspace;                       // phares 3/7/98
	      mousewait = I_GetTime() + 15;
	    }

	  // phares 4/4/98:
	  // Handle mouse button 3, and allow it to pass down
	  // to where key binding can eat it.

	  if (setup_active && set_keybnd_active)
	    if (ev->data1&4)
	      {
		ch = 0; // meaningless, just to get you past the check for -1
		mousewait = I_GetTime() + 15;
	      }
	}
      else
        
	// Process keyboard input

	if (ev->type == ev_keydown)
	  {
	    ch = ev->data1;         // phares 4/11/98:
	    if (ch == KEYD_RSHIFT)        // For chat string processing, need
	      shiftdown = true;           // to know when shift key is up or
	  }                             // down so you can get at the !,#,
	else if (ev->type == ev_keyup)  // etc. keys. Keydowns are allowed
	  if (ev->data1 == KEYD_RSHIFT) // past this point, but keyups aren't
	    shiftdown = false;          // so we need to note the difference
    }                                 // here using the 'shiftdown' boolean.
  
  if (ch == -1)
    return false; // we can't use the event here

  // Save Game string input

  if (saveStringEnter)
    {
      if (ch == key_menu_backspace)                      // phares 3/7/98
	{
	  if (saveCharIndex > 0)
	    {
	      saveCharIndex--;
	      savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	}

      else if (ch == key_menu_escape)                    // phares 3/7/98
	{
	  saveStringEnter = 0;
	  strcpy(&savegamestrings[saveSlot][0],saveOldString);
	}
    
      else if (ch == key_menu_enter)                     // phares 3/7/98
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
	}
      return true;
    }
  
  // Take care of any messages that need input

  if (messageToPrint)
    {
      if (messageNeedsInput == true &&
	  !(ch == ' ' || ch == 'n' || ch == 'y' || ch == key_escape)) // phares
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

  if ((devparm && ch == key_help) || ch == key_screenshot)
    {
      G_ScreenShot ();
      return true;
    }

  // If there is no active menu displayed...

  if (!menuactive)                                            // phares
    {                                                               //  |
      if (ch == key_autorun)      // Autorun                          //  V
	{
	  autorun = !autorun;
	  return true;
	}

      if (ch == key_help)      // Help key
	{
	  M_StartControlPanel ();

	  currentMenu = &HelpDef;         // killough 10/98: new help screen

	  itemOn = 0;
	  S_StartSound(NULL,sfx_swtchn);
	  return true;
	}

      if (ch == key_savegame)     // Save Game
	{                             
	  M_StartControlPanel();
	  S_StartSound(NULL,sfx_swtchn);
	  M_SaveGame(0);
	  return true;
	}

      if (ch == key_loadgame)     // Load Game
	{
	  M_StartControlPanel();
	  S_StartSound(NULL,sfx_swtchn);
	  M_LoadGame(0);
	  return true;
	}

      if (ch == key_soundvolume)      // Sound Volume
	{
	  M_StartControlPanel ();
	  currentMenu = &SoundDef;
	  itemOn = sfx_vol;
	  S_StartSound(NULL,sfx_swtchn);
	  return true;
	}

      if (ch == key_quicksave)      // Quicksave
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_QuickSave();
	  return true;
	}
    
      if (ch == key_endgame)      // End game
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_EndGame(0);
	  return true;
	}
    
      if (ch == key_messages)      // Toggle messages
	{
	  M_ChangeMessages(0);
	  S_StartSound(NULL,sfx_swtchn);
	  return true;
	}
    
      if (ch == key_quickload)      // Quickload
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_QuickLoad();
	  return true;
	}
    
      if (ch == key_quit)       // Quit DOOM
	{
	  S_StartSound(NULL,sfx_swtchn);
	  M_QuitDOOM(0);
	  return true;
	}
    
      if (ch == key_gamma)       // gamma toggle
	{
	  usegamma++;
	  if (usegamma > 4)
	    usegamma = 0;
	  players[consoleplayer].message =
	    usegamma == 0 ? s_GAMMALVL0 :
	    usegamma == 1 ? s_GAMMALVL1 :
	    usegamma == 2 ? s_GAMMALVL2 :
	    usegamma == 3 ? s_GAMMALVL3 :
	    s_GAMMALVL4;
	  I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
	  return true;                      
	}


      if (ch == key_zoomout)     // zoom out
	{
	  if (automapactive || chat_on)
	    return false;
	  M_SizeDisplay(0);
	  S_StartSound(NULL,sfx_stnmov);
	  return true;
	}
    
      if (ch == key_zoomin)               // zoom in
	{                                 // jff 2/23/98
	  if (automapactive || chat_on)     // allow 
	    return false;                   // key_hud==key_zoomin
	  M_SizeDisplay(1);                                             //  ^
	  S_StartSound(NULL,sfx_stnmov);                                //  |
	  return true;                                            // phares
	}
                                  
      if (ch == key_hud)   // heads-up mode       
	{                    
	  if (automapactive || chat_on)    // jff 2/22/98
	    return false;                  // HUD mode control
	  if (screenSize<8)                // function on default F5
	    while (screenSize<8 || !hud_displayed) // make hud visible
	      M_SizeDisplay(1);            // when configuring it
	  else
	    {
	      hud_displayed = 1;               //jff 3/3/98 turn hud on
	      hud_active = (hud_active+1)%3;   // cycle hud_active
	      if (!hud_active)                 //jff 3/4/98 add distributed
		{
		  hud_distributed = !hud_distributed; // to cycle
		  HU_MoveHud(); //jff 3/9/98 move it now to avoid glitch
		}
	    }
	  return true;
	}

      // killough 10/98: allow key shortcut into Setup menu
      if (ch == key_setup)
	{
	  M_StartControlPanel();
	  S_StartSound(NULL,sfx_swtchn);
	  M_SetupNextMenu(&SetupDef);
	  return true;
	}
    }                               
  
  // Pop-up Main menu?

  if (!menuactive)
    {
      if (ch == key_escape)                                     // phares
	{
	  M_StartControlPanel ();
	  S_StartSound(NULL,sfx_swtchn);
	  return true;
	}
      return false;
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
	  if (ch == key_menu_escape) // Exit key = no change
	    {
	      M_SelectDone(ptr1);                           // phares 4/17/98
	      setup_gather = false;   // finished gathering keys, if any
	      return true;
	    }

	  if (ptr1->m_flags & S_YESNO) // yes or no setting?
	    {
	      if (ch == key_menu_enter)
		{
		  *ptr1->var.def->location = !*ptr1->var.def->location; // killough 8/15/98

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
		      if (allow_changes())  // killough 8/15/98
			*ptr1->var.def->current = *ptr1->var.def->location;
		      else
			if (*ptr1->var.def->current != *ptr1->var.def->location)
			  warn_about_changes(S_LEVWARN); // killough 8/15/98

		  if (ptr1->action)      // killough 10/98
		    ptr1->action();
		}
	      M_SelectDone(ptr1);                           // phares 4/17/98
	      return true;
	    }

	  if (ptr1->m_flags & S_CRITEM)
	    {
	      if (ch != key_menu_enter)
		{
		  ch -= 0x30; // out of ascii
		  if (ch < 0 || ch > 9)
		    return true; // ignore
		  *ptr1->var.def->location = ch;
		}
	      if (ptr1->action)      // killough 10/98
		ptr1->action();
	      M_SelectDone(ptr1);                      // phares 4/17/98
	      return true;
	    }

	  if (ptr1->m_flags & S_NUM) // number?
	    {
	      if (setup_gather) // gathering keys for a value?
		{
		  // killough 10/98: Allow negatives, and use a more
		  // friendly input method (e.g. don't clear value early,
		  // allow backspace, and return to original value if bad
		  // value is entered).

		  if (ch == key_menu_enter)
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
			      *ptr1->var.def->location = value;

			      // killough 8/9/98: fix numeric vars
			      // killough 8/15/98: add warning message

			      if (ptr1->m_flags & (S_LEVWARN | S_PRGWARN))
				warn_about_changes(ptr1->m_flags &
						   (S_LEVWARN | S_PRGWARN));
			      else
				if (ptr1->var.def->current)
				  if (allow_changes())  // killough 8/15/98
				    *ptr1->var.def->current = value;
				  else
				    if (*ptr1->var.def->current != value)
				      warn_about_changes(S_LEVWARN);

			      if (ptr1->action)      // killough 10/98
				ptr1->action();
			    }
			}
		      M_SelectDone(ptr1);     // phares 4/17/98
		      setup_gather = false; // finished gathering keys
		      return true;
		    }

		  if (ch == key_menu_backspace && gather_count)
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

      if (set_keybnd_active) // on a key binding setup screen
	if (setup_select)    // incoming key or button gets bound
	  {
	    if (ev->type == ev_joystick)
	      {
		int i,oldbutton,group;
		boolean search = true;
      
		if (!ptr1->m_joy)
		  return true; // not a legal action here (yet)
      
		// see if the button is already bound elsewhere. if so, you
		// have to swap bindings so the action where it's currently
		// bound doesn't go dead. Since there is more than one
		// keybinding screen, you have to search all of them for
		// any duplicates. You're only interested in the items
		// that belong to the same group as the one you're changing.
      
		oldbutton = *ptr1->m_joy;
		group  = ptr1->m_group;
		if (ev->data1 & 1)
		  ch = 0;
		else if (ev->data1 & 2)
		  ch = 1;
		else if (ev->data1 & 4)
		  ch = 2;
		else if (ev->data1 & 8)
		  ch = 3;
		else
		  return true;
		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_group == group && ptr1 != ptr2)
		      if (ptr2->m_flags & S_KEY && ptr2->m_joy)
			if (*ptr2->m_joy == ch)
			  {
			    *ptr2->m_joy = oldbutton;
			    search = false;
			    break;
			  }
		*ptr1->m_joy = ch;
	      }
	    else if (ev->type == ev_mouse)
	      {
		int i,oldbutton,group;
		boolean search = true;

		if (!ptr1->m_mouse)
		  return true; // not a legal action here (yet)

		// see if the button is already bound elsewhere. if so, you
		// have to swap bindings so the action where it's currently
		// bound doesn't go dead. Since there is more than one
		// keybinding screen, you have to search all of them for
		// any duplicates. You're only interested in the items
		// that belong to the same group as the one you're changing.

		oldbutton = *ptr1->m_mouse;
		group  = ptr1->m_group;
		if (ev->data1 & 1)
		  ch = 0;
		else if (ev->data1 & 2)
		  ch = 1;
		else if (ev->data1 & 4)
		  ch = 2;
		else
		  return true;
		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_group == group && ptr1 != ptr2)
		      if (ptr2->m_flags & S_KEY && ptr2->m_mouse)
			if (*ptr2->m_mouse == ch)
			  {
			    *ptr2->m_mouse = oldbutton;
			    search = false;
			    break;
			  }
		*ptr1->m_mouse = ch;
	      }
	    else  // keyboard key
	      {
		int i,oldkey,group;
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

		oldkey = *ptr1->var.m_key;
		group  = ptr1->m_group;
		for (i = 0 ; keys_settings[i] && search ; i++)
		  for (ptr2 = keys_settings[i] ; !(ptr2->m_flags & S_END) ; ptr2++)
		    if (ptr2->m_flags & (S_KEY|S_KEEP) &&
			ptr2->m_group == group && 
			ptr1 != ptr2)
		      if (*ptr2->var.m_key == ch)
			{
			  if (ptr2->m_flags & S_KEEP)
			    return true; // can't have it!
			  *ptr2->var.m_key = oldkey;
			  search = false;
			  break;
			}
		*ptr1->var.m_key = ch;
	      }

	    M_SelectDone(ptr1);       // phares 4/17/98
	    return true;
	  }

      // Weapons

      if (set_weapon_active) // on the weapons setup screen
	if (setup_select) // changing an entry
	  {
	    if (ch != key_menu_enter)
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
			*ptr2->var.def->location == ch && ptr1 != ptr2)
		      {
			*ptr2->var.def->location = *ptr1->var.def->location;
			goto end;
		      }
	      end:
		*ptr1->var.def->location = ch;
	      }

	    M_SelectDone(ptr1);       // phares 4/17/98
	    return true;
	  }

      // Automap

      if (set_auto_active) // on the automap setup screen
	if (setup_select) // incoming key
	  {
	    if (ch == key_menu_down)                
	      {
		if (++color_palette_y == 16)
		  color_palette_y = 0;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (ch == key_menu_up)                
	      {
		if (--color_palette_y < 0)
		  color_palette_y = 15;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (ch == key_menu_left)                
	      {
		if (--color_palette_x < 0)
		  color_palette_x = 15;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (ch == key_menu_right)                
	      {
		if (++color_palette_x == 16)
		  color_palette_x = 0;
		S_StartSound(NULL,sfx_itemup);
		return true;
	      }

	    if (ch == key_menu_enter)               
	      {
		*ptr1->var.def->location = color_palette_x + 16*color_palette_y;
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
	      if (ch == key_menu_backspace) // backspace and DEL
		{
		  if (chat_string_buffer[chat_index] == 0)
		    {
		      if (chat_index > 0)
			chat_string_buffer[--chat_index] = 0;
		    }
		  // shift the remainder of the text one char left
		  else
		    strcpy(&chat_string_buffer[chat_index],
			   &chat_string_buffer[chat_index+1]);
		}
	      else if (ch == key_menu_left) // move cursor left
		{
		  if (chat_index > 0)
		    chat_index--;
		}
	      else if (ch == key_menu_right) // move cursor right
		{
		  if (chat_string_buffer[chat_index] != 0)
		    chat_index++;
		}
	      else if ((ch == key_menu_enter) ||
		       (ch == key_menu_escape))
		{
		  *(char **) ptr1->var.def->location = chat_string_buffer;
		  M_SelectDone(ptr1);   // phares 4/17/98
		}
	      
	      // Adding a char to the text. Has to be a printable
	      // char, and you can't overrun the buffer. If the
	      // chat string gets larger than what the screen can hold,
	      // it is dealt with when the string is drawn (above).
	      
	      else if ((ch >= 32) && (ch <= 126))
		if ((chat_index+1) < CHAT_STRING_BFR_SIZE)
		  {
		    if (shiftdown)
		      ch = shiftxform[ch];
		    if (chat_string_buffer[chat_index] == 0) 
		      {
			chat_string_buffer[chat_index++] = ch;
			chat_string_buffer[chat_index] = 0;
		      }
		    else
		      chat_string_buffer[chat_index++] = ch;
		  }
	      return true;
	    }
	  
	  M_SelectDone(ptr1);       // phares 4/17/98
	  return true;
	}

      // Not changing any items on the Setup screens. See if we're
      // navigating the Setup menus or selecting an item to change.

      if (ch == key_menu_down)
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
  
      if (ch == key_menu_up)                 
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

      if (ch == key_menu_enter)               
	{
	  int flags = ptr1->m_flags;

	  // You've selected an item to change. Highlight it, post a new
	  // message about what to do, and get ready to process the
	  // change.
	  //
	  // killough 10/98: use friendlier char-based input buffer

	  if (flags & S_NUM)
	    {
	      setup_gather = true;
	      print_warning_about_changes = false;
	      gather_count = 0;
	    }
	  else if (flags & S_COLOR)
	    {
	      int color = *ptr1->var.def->location;
        
	      if (color < 0 || color > 255) // range check the value
		color = 0;        // 'no show' if invalid

	      color_palette_x = *ptr1->var.def->location & 15;
	      color_palette_y = *ptr1->var.def->location >> 4;
	      colorbox_active = true;
	    }
	  else if (flags & S_STRING)
	    {
	      // copy chat string into working buffer; trim if needed.
	      // free the old chat string memory and replace it with
	      // the (possibly larger) new memory for editing purposes
	      //
	      // killough 10/98: fix bugs, simplify

	      chat_string_buffer = malloc(CHAT_STRING_BFR_SIZE);
	      strncpy(chat_string_buffer,
		      *(char **) ptr1->var.def->location, CHAT_STRING_BFR_SIZE);

	      // guarantee null delimiter
	      chat_string_buffer[CHAT_STRING_BFR_SIZE-1] = 0;

	      // set chat table pointer to working buffer
	      // and free old string's memory.

	      free(*(char **) ptr1->var.def->location);
	      *(char **) ptr1->var.def->location = chat_string_buffer;
	      chat_index = 0; // current cursor position in chat_string_buffer
	    }
	  else if (flags & S_RESET)
	    default_verify = true;

	  ptr1->m_flags |= S_SELECT;
	  setup_select = true;
	  S_StartSound(NULL,sfx_itemup);
	  return true;
	}

      if ((ch == key_menu_escape) || (ch == key_menu_backspace))
	{
	  if (ch == key_menu_escape) // Clear all menus
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

      if (ch == key_menu_left)
	{
	  ptr2 = ptr1;
	  do
	    {
	      ptr2++;
	      if (ptr2->m_flags & S_PREV)
		{
		  ptr1->m_flags &= ~S_HILITE;
		  mult_screens_index--;
		  current_setup_menu = ptr2->var.menu;
		  set_menu_itemon = 0;
		  print_warning_about_changes = false; // killough 10/98
		  while (current_setup_menu[set_menu_itemon++].m_flags&S_SKIP);
		  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
		  S_StartSound(NULL,sfx_pstop);  // killough 10/98
		  return true;
		}
	    }
	  while (!(ptr2->m_flags & S_END));
	}

      if (ch == key_menu_right)                
	{
	  ptr2 = ptr1;
	  do
	    {
	      ptr2++;
	      if (ptr2->m_flags & S_NEXT)
		{
		  ptr1->m_flags &= ~S_HILITE;
		  mult_screens_index++;
		  current_setup_menu = ptr2->var.menu;
		  set_menu_itemon = 0;
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

  if (ch == key_menu_down)                             // phares 3/7/98
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
  
  if (ch == key_menu_up)                               // phares 3/7/98
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

  if (ch == key_menu_left)                             // phares 3/7/98
    {
      if (currentMenu->menuitems[itemOn].routine &&
	  currentMenu->menuitems[itemOn].status == 2)
	{
	  S_StartSound(NULL,sfx_stnmov);
	  currentMenu->menuitems[itemOn].routine(0);
	}
      return true;
    }
  
  if (ch == key_menu_right)                            // phares 3/7/98
    {
      if (currentMenu->menuitems[itemOn].routine &&
	  currentMenu->menuitems[itemOn].status == 2)
	{
	  S_StartSound(NULL,sfx_stnmov);
	  currentMenu->menuitems[itemOn].routine(1);
	}
      return true;
    }

  if (ch == key_menu_enter)                            // phares 3/7/98
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
      //jff 3/24/98 remember last skill selected
      // killough 10/98 moved to skill-specific functions
      return true;
    }
  
  if (ch == key_menu_escape)                           // phares 3/7/98  
    {
      currentMenu->lastOn = itemOn;
      M_ClearMenus ();
      S_StartSound(NULL,sfx_swtchx);
      return true;
    }
  
  if (ch == key_menu_backspace)                        // phares 3/7/98
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
  
  else
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
      
      for (i=0;i<max;i++)
      {
         if (currentMenu->menuitems[i].name[0])
            V_DrawPatchDirect(x,y,0,
            W_CacheLumpName(currentMenu->menuitems[i].name,
            PU_CACHE));
         y += LINEHEIGHT;
      }
      
      // DRAW SKULL
      
      V_DrawPatchDirect(x + SKULLXOFF,
         currentMenu->y - 5 + itemOn*LINEHEIGHT,0,
         W_CacheLumpName(skullName[whichSkull],PU_CACHE));
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

void M_StartMessage (char* string,void* routine,boolean input)
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

  xx = x;
  V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERML",PU_CACHE));
  xx += 8;
  for (i=0;i<thermWidth;i++)
    {
      V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMM",PU_CACHE));
      xx += 8;
    }
  V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMR",PU_CACHE));

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

int M_StringWidth(char* string)
{
  int i, c, w = 0;
  for (i = 0;i < strlen(string);i++)
    w += (c = toupper(string[i]) - HU_FONTSTART) < 0 || c >= HU_FONTSIZE ?
      4 : SHORT(hu_font[c]->width);
  return w;
}

//
//    Find string height from hu_font chars
//

int M_StringHeight(char* string)
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
void M_WriteText (int x,int y,char* string)
{
  int   w;
  char* ch;
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
      if ((strncmp(src->m_text,"SSG",3) == 0) && (gamemode != commercial))
	src->m_flags = S_SKIP; // Don't show setting or item
      src++;
    }
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
      NewDef.prevMenu = &MainDef;
      ReadDef1.routine = M_DrawReadThis1;
      ReadDef1.x = 330;
      ReadDef1.y = 165;
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

  M_ResetMenu();        // killough 10/98
  M_InitHelpScreen();   // init the help screen       // phares 4/08/98
  M_InitExtendedHelp(); // init extended help screens // phares 3/30/98
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
