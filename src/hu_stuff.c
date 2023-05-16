//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 Fabian Greffrath
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
// DESCRIPTION:  Heads-up displays
//
//-----------------------------------------------------------------------------

// killough 5/3/98: remove unnecessary headers

#include "doomstat.h"
#include "doomkeys.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "st_stuff.h" /* jff 2/16/98 need loc of status bar */
#include "w_wad.h"
#include "s_sound.h"
#include "dstrings.h"
#include "sounds.h"
#include "d_deh.h"   /* Ty 03/27/98 - externalization of mapnamesx arrays */
#include "r_draw.h"
#include "m_input.h"
#include "p_map.h" // crosshair (linetarget)
#include "m_misc2.h"
#include "m_swap.h"
#include "r_main.h"
#include "u_scanner.h"

// global heads up display controls

int hud_active;       //jff 2/17/98 controls heads-up display mode 
int hud_displayed;    //jff 2/23/98 turns heads-up display on/off
int hud_secret_message; // "A secret is revealed!" message
int hud_widget_font;

int crispy_hud; // Crispy HUD
static boolean draw_crispy_hud;

//
// Locally used constants, shortcuts.
//
// Ty 03/28/98 -
// These four shortcuts modifed to reflect char ** of mapnamesx[]
#define HU_TITLE  (*mapnames[(gameepisode-1)*9+gamemap-1])
#define HU_TITLE2 (*mapnames2[gamemap-1])
#define HU_TITLEP (*mapnamesp[gamemap-1])
#define HU_TITLET (*mapnamest[gamemap-1])

#define HU_TITLEX (0 - WIDESCREENDELTA)
#define HU_TITLEY (ST_Y - 1 - SHORT(hu_font[0]->height))

#define HU_HUDX   (2 - WIDESCREENDELTA)

#define HU_INPUTX (HU_MSGX)
#define HU_INPUTY (HU_MSGY + HU_REFRESHSPACING)

char* chat_macros[] =    // Ty 03/27/98 - *not* externalized
{
  HUSTR_CHATMACRO0,
  HUSTR_CHATMACRO1,
  HUSTR_CHATMACRO2,
  HUSTR_CHATMACRO3,
  HUSTR_CHATMACRO4,
  HUSTR_CHATMACRO5,
  HUSTR_CHATMACRO6,
  HUSTR_CHATMACRO7,
  HUSTR_CHATMACRO8,
  HUSTR_CHATMACRO9
};

char **player_names[] =
{
  &s_HUSTR_PLRGREEN,
  &s_HUSTR_PLRINDIGO,
  &s_HUSTR_PLRBROWN,
  &s_HUSTR_PLRRED
};

//jff 3/17/98 translate player colmap to text color ranges
int plyrcoltran[MAXPLAYERS]={CR_GREEN,CR_GRAY,CR_BROWN,CR_RED};

char chat_char;                 // remove later.
static player_t*  plr;

// font sets
static patch_t* hu_fontA[HU_FONTSIZE+6];
static patch_t* hu_fontB[HU_FONTSIZE+6];
patch_t **hu_font  = hu_fontA;
patch_t **hu_font2 = hu_fontB;
static int CR_BLUE = CR_BLUE1;

// widgets
static hu_textline_t  w_title;
static hu_stext_t     w_message;
static hu_itext_t     w_chat;
static hu_itext_t     w_inputbuffer[MAXPLAYERS];
static hu_textline_t  w_coord;
static hu_textline_t  w_fps;
static hu_textline_t  w_ammo;   //jff 2/16/98 new ammo widget for hud
static hu_textline_t  w_health; //jff 2/16/98 new health widget for hud
static hu_textline_t  w_armor;  //jff 2/16/98 new armor widget for hud
static hu_textline_t  w_weapon; //jff 2/16/98 new weapon widget for hud
static hu_textline_t  w_keys;   //jff 2/16/98 new keys widget for hud
static hu_textline_t  w_monsec; //jff 2/16/98 new kill/secret widget for hud
static hu_mtext_t     w_rtext;  //jff 2/26/98 text message refresh widget
static hu_stext_t     w_secret; // [crispy] secret message widget
static hu_textline_t  w_sttime; // time above status bar

#define MAX_HUDS 3
#define MAX_WIDGETS 12

static widget_t widgets[MAX_HUDS][MAX_WIDGETS] = {
  {
    {&w_title,  align_bottomleft},

    {&w_monsec, align_bottomleft},
    {&w_sttime, align_bottomleft},
    {&w_coord,  align_topright},
    {&w_fps,    align_topright},
    {NULL}
  }, {
    {&w_title,  align_bottomleft},

    {&w_armor,  align_bottomleft},
    {&w_health, align_bottomleft},
    {&w_ammo,   align_bottomleft},
    {&w_weapon, align_bottomleft},
    {&w_keys,   align_bottomleft},

    {&w_monsec, align_bottomleft},
    {&w_sttime, align_bottomleft},
    {&w_coord,  align_topright},
    {&w_fps,    align_topright},
    {NULL}
  }, {
    {&w_title,  align_bottomleft},

    {&w_health, align_topright},
    {&w_armor,  align_topright},
    {&w_ammo,   align_bottomright},
    {&w_weapon, align_bottomright},
    {&w_keys,   align_bottomleft},

    {&w_monsec, align_bottomleft},
    {&w_sttime, align_bottomleft},
    {&w_coord , align_topright},
    {&w_fps,    align_topright},
    {NULL}
  }
};

static widget_t *widget = *widgets;

static void HU_ParseHUD (void);

static boolean    always_off = false;
static char       chat_dest[MAXPLAYERS];
boolean           chat_on;
static boolean    message_on;
static boolean    message_list_on;   // killough 11/98
static boolean    has_message;       // killough 12/98
boolean           message_dontfuckwithme;
static boolean    message_nottobefuckedwith;
static int        message_counter;
static int        message_list_counter;         // killough 11/98
static int        message_count;     // killough 11/98
static int        chat_count;        // killough 11/98
static boolean    secret_on;
static int        secret_counter;

boolean           message_centered;
boolean           message_colorized;

extern int        showMessages;
static boolean    headsupactive = false;

//jff 2/16/98 hud supported automap colors added
int hudcolor_titl;  // color range of automap level title
int hudcolor_xyco;  // color range of new coords on automap
//jff 2/16/98 hud text colors, controls added
int hudcolor_mesg;  // color range of scrolling messages
int hudcolor_chat;  // color range of chat lines
int hud_msg_lines;  // number of message lines in window
int message_list;      // killough 11/98: made global

int message_timer  = HU_MSGTIMEOUT * (1000/TICRATE);     // killough 11/98
int chat_msg_timer = HU_MSGTIMEOUT * (1000/TICRATE);     // killough 11/98

static void HU_InitCrosshair(void);
static void HU_StartCrosshair(void);
int hud_crosshair;

//jff 2/16/98 initialization strings for ammo, health, armor widgets
static char hud_coordstr[80];
static char hud_fpsstr[16];
static char hud_ammostr[80];
static char hud_healthstr[80];
static char hud_armorstr[80];
static char hud_weapstr[80];
static char hud_keysstr[80];
static char hud_monsecstr[80];
static char hud_timestr[48]; // time above status bar

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//
// Ty 03/27/98 - externalized map name arrays - now in d_deh.c
// and converted to arrays of pointers to char *
// See modified HUTITLEx macros
extern char **mapnames[];
extern char **mapnames2[];
extern char **mapnamesp[];
extern char **mapnamest[];

// key tables
// jff 5/10/98 french support removed, 
// as it was not being used and couldn't be easily tested
//
const char* shiftxform;

const char english_shiftxform[] =
{
  0,
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31,
  ' ', '!', '"', '#', '$', '%', '&',
  '"', // shift-'
  '(', ')', '*', '+',
  '<', // shift-,
  '_', // shift--
  '>', // shift-.
  '?', // shift-/
  ')', // shift-0
  '!', // shift-1
  '@', // shift-2
  '#', // shift-3
  '$', // shift-4
  '%', // shift-5
  '^', // shift-6
  '&', // shift-7
  '*', // shift-8
  '(', // shift-9
  ':',
  ':', // shift-;
  '<',
  '+', // shift-=
  '>', '?', '@',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '[', // shift-[
  '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
  ']', // shift-]
  '"', '_',
  '\'', // shift-`
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '{', '|', '}', '~', 127
};

static boolean VANILLAMAP(int e, int m)
{
  if (gamemode == commercial)
    return (e == 1 && m > 0 && m <=32);
  else
    return (e > 0 && e <= 4 && m > 0 && m <= 9);
}

struct {
  char **str;
  const int cr;
  const char *col;
} static const colorize_strings[] = {
  // [Woof!] colorize keycard and skull key messages
  {&s_GOTBLUECARD, CR_BLUE2, " blue "},
  {&s_GOTBLUESKUL, CR_BLUE2, " blue "},
  {&s_GOTREDCARD,  CR_RED,  " red "},
  {&s_GOTREDSKULL, CR_RED,  " red "},
  {&s_GOTYELWCARD, CR_GOLD, " yellow "},
  {&s_GOTYELWSKUL, CR_GOLD, " yellow "},
  {&s_PD_BLUEC,    CR_BLUE2, " blue "},
  {&s_PD_BLUEK,    CR_BLUE2, " blue "},
  {&s_PD_BLUEO,    CR_BLUE2, " blue "},
  {&s_PD_BLUES,    CR_BLUE2, " blue "},
  {&s_PD_REDC,     CR_RED,  " red "},
  {&s_PD_REDK,     CR_RED,  " red "},
  {&s_PD_REDO,     CR_RED,  " red "},
  {&s_PD_REDS,     CR_RED,  " red "},
  {&s_PD_YELLOWC,  CR_GOLD, " yellow "},
  {&s_PD_YELLOWK,  CR_GOLD, " yellow "},
  {&s_PD_YELLOWO,  CR_GOLD, " yellow "},
  {&s_PD_YELLOWS,  CR_GOLD, " yellow "},

  // [Woof!] colorize multi-player messages
  {&s_HUSTR_PLRGREEN,  CR_GREEN, "Green: "},
  {&s_HUSTR_PLRINDIGO, CR_GRAY,  "Indigo: "},
  {&s_HUSTR_PLRBROWN,  CR_BROWN, "Brown: "},
  {&s_HUSTR_PLRRED,    CR_RED,   "Red: "},
};

static char* PrepareColor(const char *str, const char *col)
{
    char *str_replace, col_replace[16];

    M_snprintf(col_replace, sizeof(col_replace),
               "\x1b%c%s\x1b%c", '0'+CR_ORIG, col, '0'+CR_ORIG);
    str_replace = M_StringReplace(str, col, col_replace);

    return str_replace;
}

static void UpdateColor(char *str, int cr)
{
    int i;
    int len = strlen(str);

    if (!message_colorized)
    {
        cr = CR_ORIG;
    }

    for (i = 0; i < len; ++i)
    {
        if (str[i] == '\x1b' && i + 1 < len)
        {
          str[i + 1] = '0'+cr;
          break;
        }
    }
}

void HU_ResetMessageColors(void)
{
    int i;

    for (i = 0; i < arrlen(colorize_strings); i++)
    {
        UpdateColor(*colorize_strings[i].str, colorize_strings[i].cr);
    }
}

extern boolean st_invul;

static char* ColorByHealth(int health, int maxhealth, boolean invul)
{
  if (invul)
    return colrngs[CR_GRAY];

  health = 100 * health / maxhealth;

  if (health < health_red)
    return colrngs[CR_RED];
  else if (health < health_yellow)
    return colrngs[CR_GOLD];
  else if (health <= health_green)
    return colrngs[CR_GREEN];
  else
    return colrngs[CR_BLUE];
}

static int lightest_color, darkest_color;

static void HU_InitDemoProgressBar (void)
{
  byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

  lightest_color = I_GetPaletteIndex(playpal, 0xFF, 0xFF, 0xFF);
  darkest_color  = I_GetPaletteIndex(playpal, 0x00, 0x00, 0x00);
}

//
// HU_Init()
//
// Initialize the heads-up display, text that overwrites the primary display
//
// Passed nothing, returns nothing
//
void HU_Init(void)
{
  int i, j;
  char buffer[9];

  shiftxform = english_shiftxform;

  // load the heads-up font
  j = HU_FONTSTART;
  for (i = 0; i < HU_FONTSIZE; i++, j++)
  {
    if ('0' <= j && j <= '9')
    {
      sprintf(buffer, "DIG%.1d", j - 48);
      hu_fontB[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
      sprintf(buffer, "STCFN%.3d", j);
      hu_font[i]  = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
    }
    else if ('A' <= j && j <= 'Z')
    {
      sprintf(buffer, "DIG%c", j);
      hu_fontB[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
      sprintf(buffer, "STCFN%.3d", j);
      hu_font[i]  = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
    }
    else if (j == '%')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG37", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN037", PU_STATIC);
    }
    else if (j == '+')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG43", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN043", PU_STATIC);
    }
    else if (j == '.')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG46", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN046", PU_STATIC);
    }
    else if (j == '-')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG45", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN045", PU_STATIC);
    }
    else if (j == '/')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG47", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN047", PU_STATIC);
    }
    else if (j == ':')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG58", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN058", PU_STATIC);
    }
    else if (j == '[')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG91", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN091", PU_STATIC);
    }
    else if (j == ']')
    {
      hu_fontB[i] = (patch_t *) W_CacheLumpName("DIG93", PU_STATIC);
      hu_font[i]  = (patch_t *) W_CacheLumpName("STCFN093", PU_STATIC);
    }
    else if (j < 97)
    {
      sprintf(buffer, "STCFN%.3d", j);
      // [FG] removed the embedded STCFN096 lump
      if (W_CheckNumForName(buffer) != -1)
        hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
      else
        hu_fontB[i] = hu_font[i] = hu_font[0];
    }
    else if (j > 122) //jff 2/23/98 make all font chars defined, useful or not
    {
      sprintf(buffer, "STBR%.3d", j);
      hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
    }
    else
      hu_font[i] = hu_font[0]; //jff 2/16/98 account for gap
  }

  //jff 2/26/98 load patches for keys and double keys
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS0", PU_STATIC); i++;
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS1", PU_STATIC); i++;
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS2", PU_STATIC); i++;
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS3", PU_STATIC); i++;
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS4", PU_STATIC); i++;
  hu_fontB[i] = hu_font[i] = (patch_t *) W_CacheLumpName("STKEYS5", PU_STATIC);

  // [FG] support crosshair patches from extras.wad
  HU_InitCrosshair();

  HU_InitDemoProgressBar();

  HU_ParseHUD();

  // [Woof!] prepare player messages for colorization
  for (i = 0; i < arrlen(colorize_strings); i++)
  {
    *colorize_strings[i].str = PrepareColor(*colorize_strings[i].str, colorize_strings[i].col);
  }

  HU_ResetMessageColors();
}

static inline void HU_enableWidget (hu_textline_t *line, boolean cond)
{
  if (cond)
  {
    line->visible = true;
  }
}

void HU_disableAllWidgets (void)
{
  widget_t *w = widget;

  while (w->line)
  {
    if (w->align == align_direct)
    {
      w->line->x = w->x;
      w->line->y = w->y;
    }
    w->line->visible = false;
    w++;
  }
}

//
// HU_Stop()
//
// Make the heads-up displays inactive
//
// Passed nothing, returns nothing
//
void HU_Stop(void)
{
  headsupactive = false;
}

//
// HU_Start(void)
//
// Create and initialize the heads-up widgets, software machines to
// maintain, update, and display information over the primary display
//
// This routine must be called after any change to the heads up configuration
// in order for the changes to take effect in the actual displays
//
// Passed nothing, returns nothing
//

static void HU_widget_build_fps (void);
static void HU_widget_build_coord (void);
static void HU_widget_build_sttime(void);
static void HU_widget_build_monsec(void);
static void HU_widget_build_keys (void);
static void HU_widget_build_weapon (void);
static void HU_widget_build_armor (void);
static void HU_widget_build_health (void);
static void HU_widget_build_ammo (void);

void HU_Start(void)
{
  int i;
  char *s, *n;

  if (headsupactive)                    // stop before starting
    HU_Stop();

  plr = &players[displayplayer];        // killough 3/7/98
  message_on = false;
  message_dontfuckwithme = false;
  message_nottobefuckedwith = false;
  chat_on = false;
  secret_on = false;

  // killough 11/98:
  message_list_on = false;
  message_counter = message_list_counter = 0;
  message_count = (message_timer  * TICRATE) / 1000 + 1;
  chat_count    = (chat_msg_timer * TICRATE) / 1000 + 1;

  // [crispy] re-calculate WIDESCREENDELTA
  I_GetScreenDimensions();

  // create the message widget
  // messages to player in upper-left of screen
  HUlib_initSText(&w_message, HU_MSGX, HU_MSGY, HU_MSGHEIGHT, &hu_font,
                  HU_FONTSTART, colrngs[hudcolor_mesg], &message_on);

  // create the secret message widget
  HUlib_initSText(&w_secret, 0, 100 - 2*SHORT(hu_font[0]->height), HU_MSGHEIGHT, &hu_font,
                  HU_FONTSTART, colrngs[CR_GOLD], &secret_on);

  //jff 2/26/98 add the text refresh widget initialization
  HUlib_initMText(&w_rtext, HU_MSGX, HU_MSGY, &hu_font,
                  HU_FONTSTART, colrngs[hudcolor_mesg], &message_list_on); // killough 11/98

  // create the hud text refresh widget
  // scrolling display of last hud_msg_lines messages received
  if (hud_msg_lines > HU_MAXMESSAGES)
  {
    hud_msg_lines = HU_MAXMESSAGES;
  }

  // create the chat widget
  HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, &hu_font,
                  HU_FONTSTART, colrngs[hudcolor_chat], &chat_on);

  // create the inputbuffer widgets, one per player
  for (i = 0; i < MAXPLAYERS; i++)
  {
    HUlib_initIText(&w_inputbuffer[i], 0, 0, 0,
                    0, colrngs[hudcolor_chat], &always_off);
  }

  //jff 2/16/98 added some HUD widgets
  // create the map title widget - map title display in lower left of automap
  HUlib_initTextLine(&w_title, HU_TITLEX, HU_TITLEY, &hu_font,
                     HU_FONTSTART, colrngs[hudcolor_titl], NULL);

  // create the hud health widget
  HUlib_initTextLine(&w_health, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GREEN], HU_widget_build_health);

  // create the hud armor widget
  HUlib_initTextLine(&w_armor, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GREEN], HU_widget_build_armor);

  // create the hud ammo widget
  HUlib_initTextLine(&w_ammo, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GOLD], HU_widget_build_ammo);

  // create the hud weapons widget
  HUlib_initTextLine(&w_weapon, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GRAY], HU_widget_build_weapon);

  // create the hud keys widget
  HUlib_initTextLine(&w_keys, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GRAY], HU_widget_build_keys);

  // create the hud monster/secret widget
  HUlib_initTextLine(&w_monsec, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GRAY], HU_widget_build_monsec);

  HUlib_initTextLine(&w_sttime, 0, 0, &hu_font2, HU_FONTSTART, colrngs[CR_GRAY], HU_widget_build_sttime);

  // create the automaps coordinate widget
  HUlib_initTextLine(&w_coord, 0, 0, &hu_font2, HU_FONTSTART, colrngs[hudcolor_xyco], HU_widget_build_coord);

  HUlib_initTextLine(&w_fps, 0, 0, &hu_font2, HU_FONTSTART, colrngs[hudcolor_xyco], HU_widget_build_fps);

  // initialize the automap's level title widget
  if (gamemapinfo && gamemapinfo->levelname)
  {
    if (gamemapinfo->label)
      s = gamemapinfo->label;
    else
      s = gamemapinfo->mapname;

    if (s == gamemapinfo->mapname || U_CheckField(s))
    {
      while (*s)
        HUlib_addCharToTextLine(&w_title, *(s++));

      HUlib_addCharToTextLine(&w_title, ':');
      HUlib_addCharToTextLine(&w_title, ' ');
    }
    s = gamemapinfo->levelname;
  }
  else if (gamestate == GS_LEVEL)
  {
    if (VANILLAMAP(gameepisode, gamemap))
    {
      s = (gamemode != commercial) ? HU_TITLE :
          (gamemission == pack_tnt) ? HU_TITLET :
          (gamemission == pack_plut) ? HU_TITLEP :
          HU_TITLE2;
    }
    else
    {
      // initialize the map title widget with the generic map lump name
      s = MAPNAME(gameepisode, gamemap);
    }
  }
  else
  {
    s = "";
  }

  // [FG] cap at line break
  if ((n = strchr(s, '\n')))
  {
    *n = '\0';
  }

  HUlib_addStringToTextLine(&w_title, s);

  //jff 2/16/98 initialize ammo widget
  sprintf(hud_ammostr, "AMM ");
  HUlib_addStringToTextLine(&w_ammo, hud_ammostr);

  //jff 2/16/98 initialize health widget
  sprintf(hud_healthstr, "HEL ");
  HUlib_addStringToTextLine(&w_health, hud_healthstr);

  //jff 2/16/98 initialize armor widget
  sprintf(hud_armorstr, "ARM ");
  HUlib_addStringToTextLine(&w_armor, hud_armorstr);

  //jff 2/17/98 initialize weapons widget
  sprintf(hud_weapstr, "WEA ");
  HUlib_addStringToTextLine(&w_weapon, hud_weapstr);

  //jff 2/17/98 initialize keys widget
  if (deathmatch)
    sprintf(hud_keysstr, "FRG %c%c", '\x1b', '0'+CR_ORIG);
  else
    sprintf(hud_keysstr, "KEY %c%c", '\x1b', '0'+CR_NONE);
  HUlib_addStringToTextLine(&w_keys, hud_keysstr);

  HU_disableAllWidgets();

  // init crosshair
  if (hud_crosshair)
    HU_StartCrosshair();

  // now allow the heads-up display to run
  headsupactive = true;
}

// do the hud ammo display
static void HU_widget_build_ammo (void)
{
  int i;

  // clear the widgets internal line
  HUlib_clearTextLine(&w_ammo);

  hud_ammostr[4] = '\0';

  // special case for weapon with no ammo selected - blank bargraph + N/A
  if (weaponinfo[plr->readyweapon].ammo == am_noammo)
  {
    strcat(hud_ammostr, "\x7f\x7f\x7f\x7f\x7f\x7f\x7f N/A");
    w_ammo.cr = colrngs[CR_GRAY];
  }
  else
  {
    int ammo = plr->ammo[weaponinfo[plr->readyweapon].ammo];
    int fullammo = plr->maxammo[weaponinfo[plr->readyweapon].ammo];
    int ammopct = (100 * ammo) / fullammo;
    int ammobars = ammopct / 4;

    // build the bargraph string
    // full bargraph chars
    for (i = 4; i < 4 + ammobars / 4;)
      hud_ammostr[i++] = 123;

    // plus one last character with 0, 1, 2, 3 bars
    switch (ammobars % 4)
    {
      case 0:
        break;
      case 1:
        hud_ammostr[i++] = 126;
        break;
      case 2:
        hud_ammostr[i++] = 125;
        break;
      case 3:
        hud_ammostr[i++] = 124;
        break;
    }

    // pad string with blank bar characters
    while (i < 4 + 7)
      hud_ammostr[i++] = 127;
    hud_ammostr[i] = '\0';

    // build the numeric amount init string
    sprintf(hud_ammostr + i, "%d/%d", ammo, fullammo);

    // backpack changes thresholds (ammo widget)
    if (plr->backpack && !hud_backpack_thresholds && fullammo)
      ammopct = (100 * ammo) / (fullammo / 2);

    // set the display color from the percentage of total ammo held
    if (ammopct < ammo_red)
      w_ammo.cr = colrngs[CR_RED];
    else if (ammopct < ammo_yellow)
      w_ammo.cr = colrngs[CR_GOLD];
    else if (ammopct > 100) // more than max threshold w/o backpack
      w_ammo.cr = colrngs[CR_BLUE];
    else
      w_ammo.cr = colrngs[CR_GREEN];
  }

  // transfer the init string to the widget
  HUlib_addStringToTextLine(&w_ammo, hud_ammostr);
}

// do the hud health display
static void HU_widget_build_health (void)
{
  int i;
  int healthbars = (st_health > 100) ? 25 : (st_health / 4);

  // clear the widgets internal line
  HUlib_clearTextLine(&w_health);

  // build the bargraph string
  // full bargraph chars
  for (i = 4; i < 4 + healthbars / 4;)
    hud_healthstr[i++] = 123;

  // plus one last character with 0, 1, 2, 3 bars
  switch (healthbars % 4)
  {
    case 0:
      break;
    case 1:
      hud_healthstr[i++] = 126;
      break;
    case 2:
      hud_healthstr[i++] = 125;
      break;
    case 3:
      hud_healthstr[i++] = 124;
      break;
  }

  // pad string with blank bar characters
  while (i < 4 + 7)
    hud_healthstr[i++] = 127;
  hud_healthstr[i] = '\0';

  // build the numeric amount init string
  sprintf(hud_healthstr + i, "%3d", st_health);

  // set the display color from the amount of health posessed
  w_health.cr = ColorByHealth(plr->health, 100, st_invul);

  // transfer the init string to the widget
  HUlib_addStringToTextLine(&w_health, hud_healthstr);
}

// do the hud armor display
static void HU_widget_build_armor (void)
{
  int i;
  int armorbars = (st_armor > 100) ? 25 : (st_armor / 4);

  // clear the widgets internal line
  HUlib_clearTextLine(&w_armor);

  // build the bargraph string
  // full bargraph chars
  for (i = 4; i < 4 + armorbars / 4;)
    hud_armorstr[i++] = 123;

  // plus one last character with 0, 1, 2, 3 bars
  switch (armorbars % 4)
  {
    case 0:
      break;
    case 1:
      hud_armorstr[i++] = 126;
      break;
    case 2:
      hud_armorstr[i++] = 125;
      break;
    case 3:
      hud_armorstr[i++] = 124;
      break;
  }

  // pad string with blank bar characters
  while (i < 4 + 7)
    hud_armorstr[i++] = 127;
  hud_armorstr[i] = '\0';

  // build the numeric amount init string
  sprintf(hud_armorstr + i, "%3d", st_armor);

  // color of armor depends on type
  if (hud_armor_type)
  {
    w_armor.cr =
      st_invul ? colrngs[CR_GRAY] :
      (plr->armortype == 0) ? colrngs[CR_RED] :
      (plr->armortype == 1) ? colrngs[CR_GREEN] :
      colrngs[CR_BLUE];
  }
  else
  {
    int armor = plr->armorpoints;
    
    // set the display color from the amount of armor posessed
    w_armor.cr =
      st_invul ? colrngs[CR_GRAY] :
      (armor < armor_red) ? colrngs[CR_RED] :
      (armor < armor_yellow) ? colrngs[CR_GOLD] :
      (armor <= armor_green) ? colrngs[CR_GREEN] :
      colrngs[CR_BLUE];
  }

  // transfer the init string to the widget
  HUlib_addStringToTextLine(&w_armor, hud_armorstr);
}

// do the hud weapon display
static void HU_widget_build_weapon (void)
{
  int i, w, ammo, fullammo, ammopct;

  // clear the widgets internal line
  HUlib_clearTextLine(&w_weapon);

  i = 4; hud_weapstr[i] = '\0'; //jff 3/7/98 make sure ammo goes away

  // do each weapon that exists in current gamemode
  for (w = 0; w <= wp_supershotgun; w++) //jff 3/4/98 show fists too, why not?
  {
    int ok = 1;

    //jff avoid executing for weapons that do not exist
    switch (gamemode)
    {
      case shareware:
        if (w >= wp_plasma && w != wp_chainsaw)
          ok = 0;
        break;
      case retail:
      case registered:
        if (w >= wp_supershotgun && !have_ssg)
          ok = 0;
        break;
      default:
      case commercial:
        break;
    }
    if (!ok)
      continue;

    ammo = plr->ammo[weaponinfo[w].ammo];
    fullammo = plr->maxammo[weaponinfo[w].ammo];
    ammopct = 0;

    // skip weapons not currently posessed
    if (!plr->weaponowned[w])
      continue;

    // backpack changes thresholds (weapon widget)
    if (plr->backpack && !hud_backpack_thresholds)
      fullammo /= 2;

    ammopct = fullammo ? (100 * ammo) / fullammo : 100;

    // display each weapon number in a color related to the ammo for it
    hud_weapstr[i++] = '\x1b'; //jff 3/26/98 use ESC not '\' for paths
    if (weaponinfo[w].ammo == am_noammo) //jff 3/14/98 show berserk on HUD
      hud_weapstr[i++] = plr->powers[pw_strength] ? '0'+CR_GREEN : '0'+CR_GRAY;
    else if (ammopct < ammo_red)
      hud_weapstr[i++] = '0'+CR_RED;
    else if (ammopct < ammo_yellow)
      hud_weapstr[i++] = '0'+CR_GOLD;
    else if (ammopct > 100) // more than max threshold w/o backpack
      hud_weapstr[i++] = '0'+CR_BLUE;
    else
      hud_weapstr[i++] = '0'+CR_GREEN;

    hud_weapstr[i++] = '0'+w+1;
    hud_weapstr[i++] = ' ';
    hud_weapstr[i] = '\0';
  }

  // transfer the init string to the widget
  HUlib_addStringToTextLine(&w_weapon, hud_weapstr);
}

static inline int HU_top(int i, int idx1, int top1)
{
  if (idx1 > -1)
  {
    char numbuf[32], *s;

    sprintf(numbuf, "%5d", top1);
    // make frag count in player's color via escape code

    hud_keysstr[i++] = '\x1b'; //jff 3/26/98 use ESC not '\' for paths
    hud_keysstr[i++] = '0' + plyrcoltran[idx1 & 3];
    s = numbuf;
    while (*s)
      hud_keysstr[i++] = *s++;
  }
  return i;
}

static void HU_widget_build_keys (void)
{
  int i, k;

  HUlib_clearTextLine(&w_keys); // clear the widget strings

  i = 6; hud_keysstr[i] = '\0'; //jff 3/7/98 make sure deleted keys go away

  if (!deathmatch)
  {
    // build text string whose characters call out graphic keys
    for (k = 0; k < 6; k++)
    {
      // skip keys not possessed
      if (!plr->cards[k])
        continue;

      hud_keysstr[i++] = HU_FONTEND + k + 1; // key number plus HU_FONTEND is char for key
      hud_keysstr[i++] = ' ';   // spacing
      hud_keysstr[i++] = ' ';
    }
    hud_keysstr[i] = '\0';
  }
  else //jff 3/17/98 show frags, not keys, in deathmatch
  {
    int top1 = -999, top2 = -999, top3 = -999, top4 = -999;
    int idx1 = -1, idx2 = -1, idx3 = -1, idx4 = -1;
    int fragcount, m;

    // scan thru players
    for (k = 0; k < MAXPLAYERS; k++)
    {
      // skip players not in game
      if (!playeringame[k])
        continue;

      fragcount = 0;

      // compute number of times they've fragged each player
      // minus number of times they've been fragged by them
      for (m = 0; m < MAXPLAYERS; m++)
      {
        if (!playeringame[m])
          continue;
        fragcount += (m != k) ? players[k].frags[m] : -players[k].frags[m];
      }

      // very primitive sort of frags to find top four
      if (fragcount > top1)
      {
        top4 = top3; top3 = top2; top2 = top1; top1 = fragcount;
        idx4 = idx3; idx3 = idx2; idx2 = idx1; idx1 = k;
      }
      else if (fragcount > top2)
      {
        top4 = top3; top3 = top2; top2 = fragcount;
        idx4 = idx3; idx3 = idx2; idx2 = k;
      }
      else if (fragcount > top3)
      {
        top4 = top3; top3 = fragcount;
        idx4 = idx3; idx3 = k;
      }
      else if (fragcount > top4)
      {
        top4 = fragcount;
        idx4 = k;
      }
    }

    // killough 11/98: replaced cut-and-pasted code with function

    // if the biggest number exists,
    // put it in the init string
    i = HU_top(i, idx1, top1);

    // if the second biggest number exists,
    // put it in the init string
    i = HU_top(i, idx2, top2);

    // if the third biggest number exists,
    // put it in the init string
    i = HU_top(i, idx3, top3);

    // if the fourth biggest number exists,
    // put it in the init string
    i = HU_top(i, idx4, top4);

    hud_keysstr[i] = '\0';
  }

  // transfer the built string (frags or key title) to the widget
  HUlib_addStringToTextLine(&w_keys, hud_keysstr);
}

static void HU_widget_build_monsec(void)
{
  int i, playerscount;
  char kills_str[60];
  int offset = 0;

  int kills = 0, kills_color, kills_percent, kills_percent_color;
  int items = 0, items_color;
  int secrets = 0, secrets_color;

  for (i = 0, playerscount = 0; i < MAXPLAYERS; ++i)
  {
    int color = (i == displayplayer) ? '0'+CR_GRAY : '0'+CR_GREEN;
    if (playeringame[i])
    {
      if (playerscount == 0)
      {
        offset = sprintf(kills_str,
          "\x1b%c%d", color, players[i].killcount);
      }
      else
      {
        offset += sprintf(kills_str + offset,
          "\x1b%c+\x1b%c%d", '0'+CR_GREEN, color, players[i].killcount);
      }

      kills += players[i].killcount;
      items += players[i].itemcount;
      secrets += players[i].secretcount;
      ++playerscount;
    }
  }

  kills_color = (kills - extrakills >= totalkills) ? '0'+CR_BLUE : '0'+CR_GRAY;
  kills_percent_color = (kills >= totalkills) ? '0'+CR_BLUE : '0'+CR_GRAY;
  kills_percent = (totalkills == 0) ? 100 : (kills * 100 / totalkills);
  items_color = (items >= totalitems) ? '0'+CR_BLUE : '0'+CR_GRAY;
  secrets_color = (secrets >= totalsecret) ? '0'+CR_BLUE : '0'+CR_GRAY;

  if (playerscount > 1)
  {
    offset = sprintf(hud_monsecstr,
      "\x1b%cK %s \x1b%c%d/%d",
      '0'+CR_RED, kills_str, kills_color, kills, totalkills);
  }
  else
  {
    offset = sprintf(hud_monsecstr,
      "\x1b%cK \x1b%c%d/%d",
      '0'+CR_RED, kills_color, plr->killcount, totalkills);
  }

  if (extrakills)
  {
    offset += sprintf(hud_monsecstr + offset, "+%d", extrakills);
  }

  sprintf(hud_monsecstr + offset,
    " \x1b%c%d%% \x1b%cI \x1b%c%d/%d \x1b%cS \x1b%c%d/%d",
    kills_percent_color, kills_percent,
    '0'+CR_RED, items_color, items, totalitems,
    '0'+CR_RED, secrets_color, secrets, totalsecret);

  HUlib_clearTextLine(&w_monsec);
  HUlib_addStringToTextLine(&w_monsec, hud_monsecstr);
}

static void HU_widget_build_sttime(void)
{
  int offset = 0;
  extern int time_scale;

  if (time_scale != 100)
  {
    offset += sprintf(hud_timestr, "\x1b%c%d%% ",
            '0'+CR_BLUE, time_scale);
  }

  if (totalleveltimes)
  {
    const int time = (totalleveltimes + leveltime) / TICRATE;

    offset += sprintf(hud_timestr + offset, "\x1b%c%d:%02d ",
            '0'+CR_GREEN, time/60, time%60);
  }
  sprintf(hud_timestr + offset, "\x1b%c%d:%05.2f",
    '0'+CR_GRAY, leveltime/TICRATE/60, (float)(leveltime%(60*TICRATE))/TICRATE);

  HUlib_clearTextLine(&w_sttime);
  HUlib_addStringToTextLine(&w_sttime, hud_timestr);
}

static void HU_widget_build_coord (void)
{
  fixed_t x,y,z; // killough 10/98:
  void AM_Coordinates(const mobj_t *, fixed_t *, fixed_t *, fixed_t *);

  // killough 10/98: allow coordinates to display non-following pointer
  AM_Coordinates(plr->mo, &x, &y, &z);

  //jff 2/16/98 output new coord display
  sprintf(hud_coordstr, "X \x1b%c%d \x1b%cY \x1b%c%d \x1b%cZ \x1b%c%d",
          '0'+CR_GRAY, x >> FRACBITS, '0'+hudcolor_xyco,
          '0'+CR_GRAY, y >> FRACBITS, '0'+hudcolor_xyco,
          '0'+CR_GRAY, z >> FRACBITS);

  HUlib_clearTextLine(&w_coord);
  HUlib_addStringToTextLine(&w_coord, hud_coordstr);
}

static void HU_widget_build_fps (void)
{
  extern int fps;

  sprintf(hud_fpsstr,"\x1b%c%d \x1b%cFPS", '0'+CR_GRAY, fps, '0'+CR_ORIG);
  HUlib_clearTextLine(&w_fps);
  HUlib_addStringToTextLine(&w_fps, hud_fpsstr);
}

// Crosshair

boolean hud_crosshair_health;
crosstarget_t hud_crosshair_target;
boolean hud_crosshair_lockon; // [Alaux] Crosshair locks on target
int hud_crosshair_color;
int hud_crosshair_target_color;

typedef struct
{
  patch_t *patch;
  int w, h, x, y;
  char *cr;
} crosshair_t;

static crosshair_t crosshair;

const char *crosshair_nam[HU_CROSSHAIRS] =
  { NULL, "CROSS00", "CROSS01", "CROSS02", "CROSS03" };
const char *crosshair_str[HU_CROSSHAIRS+1] =
  { "none", "cross", "angle", "dot", "big", NULL };

static void HU_InitCrosshair(void)
{
  int i, j;

  for (i = 1; i < HU_CROSSHAIRS; i++)
  {
    j = W_CheckNumForName(crosshair_nam[i]);
    if (j >= num_predefined_lumps)
    {
      if (R_IsPatchLump(j))
        crosshair_str[i] = crosshair_nam[i];
      else
        crosshair_nam[i] = NULL;
    }
  }
}

static void HU_StartCrosshair(void)
{
  if (crosshair.patch)
    Z_ChangeTag(crosshair.patch, PU_CACHE);

  if (crosshair_nam[hud_crosshair])
  {
    crosshair.patch = W_CacheLumpName(crosshair_nam[hud_crosshair], PU_STATIC);

    crosshair.w = SHORT(crosshair.patch->width)/2;
    crosshair.h = SHORT(crosshair.patch->height)/2;
  }
  else
    crosshair.patch = NULL;
}

mobj_t *crosshair_target; // [Alaux] Lock crosshair on target

static void HU_UpdateCrosshair(void)
{
  crosshair.x = ORIGWIDTH/2;
  crosshair.y = (screenblocks <= 10) ? (ORIGHEIGHT-ST_HEIGHT)/2 : ORIGHEIGHT/2;

  if (hud_crosshair_health)
    crosshair.cr = ColorByHealth(plr->health, 100, st_invul);
  else
    crosshair.cr = colrngs[hud_crosshair_color];

  if (STRICTMODE(hud_crosshair_target || hud_crosshair_lockon))
  {
    angle_t an = plr->mo->angle;
    ammotype_t ammo = weaponinfo[plr->readyweapon].ammo;
    fixed_t range = (ammo == am_noammo) ? MELEERANGE : 16*64*FRACUNIT;
    boolean intercepts_overflow_enabled = overflow[emu_intercepts].enabled;

    crosshair_target = linetarget = NULL;

    overflow[emu_intercepts].enabled = false;
    P_AimLineAttack(plr->mo, an, range, 0);
    if (ammo == am_misl || ammo == am_cell)
    {
      if (!linetarget)
        P_AimLineAttack(plr->mo, an += 1<<26, range, 0);
      if (!linetarget)
        P_AimLineAttack(plr->mo, an -= 2<<26, range, 0);
    }
    overflow[emu_intercepts].enabled = intercepts_overflow_enabled;

    if (linetarget && !(linetarget->flags & MF_SHADOW))
      crosshair_target = linetarget;

    if (hud_crosshair_target && crosshair_target)
    {
      // [Alaux] Color crosshair by target health
      if (hud_crosshair_target == crosstarget_health)
      {
        crosshair.cr = ColorByHealth(crosshair_target->health, crosshair_target->info->spawnhealth, false);
      }
      else
      {
        crosshair.cr = colrngs[hud_crosshair_target_color];
      }
    }
  }
}

void HU_UpdateCrosshairLock(int x, int y)
{
  int w = (crosshair.w << hires);
  int h = (crosshair.h << hires);

  x = viewwindowx + BETWEEN(w, viewwidth  - w - 1, x);
  y = viewwindowy + BETWEEN(h, viewheight - h - 1, y);

  crosshair.x = (x >> hires) - WIDESCREENDELTA;
  crosshair.y = (y >> hires);
}

void HU_DrawCrosshair(void)
{
  if (plr->playerstate != PST_LIVE ||
      automapactive ||
      menuactive ||
      paused ||
      secret_on)
  {
    return;
  }

  if (crosshair.patch)
    V_DrawPatchTranslated(crosshair.x - crosshair.w,
                          crosshair.y - crosshair.h,
                          0, crosshair.patch, crosshair.cr);
}

// [crispy] print a bar indicating demo progress at the bottom of the screen
boolean HU_DemoProgressBar(boolean force)
{
  const int progress = SCREENWIDTH * playback_tic / playback_totaltics;
  static int old_progress = 0;

  if (old_progress < progress)
  {
    old_progress = progress;
  }
  else if (!force)
  {
    return false;
  }

  V_DrawHorizLine(0, SCREENHEIGHT - 2, FG, progress, darkest_color);
  V_DrawHorizLine(0, SCREENHEIGHT - 1, FG, progress, lightest_color);

  return true;
}

// [FG] level stats and level time widgets
int map_player_coords, map_level_stats, map_level_time;
int hud_level_stats, hud_level_time;

//
// HU_Drawer()
//
// Draw all the pieces of the heads-up display
//
// Passed nothing, returns nothing
//
void HU_Drawer(void)
{
  widget_t *w = widget;
  align_t align_text = message_centered ? align_direct : align_topleft;

  HUlib_resetAlignOffsets();

  // jff 4/24/98 Erase current lines before drawing current
  // needed when screen not fullsize
  // killough 11/98: only do it when not fullsize
  // moved here to properly update the w_sttime and w_monsec widgets
  if (scaledviewheight < 200)
  {
    HU_Erase();
  }

  if (message_list)
    HUlib_drawMText(&w_rtext, align_text);
  else
    HUlib_drawSText(&w_message, align_text);

  HUlib_drawSText(&w_secret, align_direct);

  // display the interactive buffer for chat entry
  HUlib_drawIText(&w_chat, align_topleft);

  if (draw_crispy_hud)
  {
    ST_Drawer (false, true);
  }

  while (w->line)
  {
    if (w->line->visible)
    {
      HUlib_drawTextLine(w->line, w->align, false);
    }
    w++;
  }
}

// [FG] draw Time widget on intermission screen
void WI_DrawTimeWidget(void)
{
  if (hud_level_time)
  {
    w_sttime.x = HU_HUDX;
    w_sttime.y = 0;
    // leveltime is already added to totalleveltimes before WI_Start()
    //HU_widget_build_sttime();
    HUlib_drawTextLine(&w_sttime, align_direct, false);
  }
}

//
// HU_Erase()
//
// Erase hud display lines that can be trashed by small screen display
//
// Passed nothing, returns nothing
//

void HU_Erase(void)
{
  // erase the message display or the message review display
  if (!message_list)
    HUlib_eraseSText(&w_message);
  else
    HUlib_eraseMText(&w_rtext);

  HUlib_eraseSText(&w_secret);
  
  // erase the interactive text buffer for chat entry
  HUlib_eraseIText(&w_chat);

  // erase the automap title
  HUlib_eraseTextLine(&w_title);

  // [FG] erase FPS counter widget
  HUlib_eraseTextLine(&w_coord);
  HUlib_eraseTextLine(&w_fps);

  HUlib_eraseTextLine(&w_monsec);
  HUlib_eraseTextLine(&w_sttime);
}

//
// HU_Ticker()
//
// Update the hud displays once per frame
//
// Passed nothing, returns nothing
//

static boolean bsdown; // Is backspace down?
static int bscounter;

int M_StringWidth(char *string);

void HU_Ticker(void)
{
  widget_t *w = widget = widgets[hud_active];
  plr = &players[displayplayer];         // killough 3/7/98

  HU_disableAllWidgets();
  draw_crispy_hud = false;

  if ((automapactive && hud_widget_font == 1) || hud_widget_font == 2)
  {
    hu_font2 = hu_font;
    CR_BLUE = CR_BLUE2;
  }
  else
  {
    hu_font2 = hu_fontB;
    CR_BLUE = CR_BLUE1;
  }

  // killough 11/98: support counter for message list as well as regular msg
  if (message_list_counter && !--message_list_counter)
    message_list_on = false;

  if (message_list)
    w_chat.l.y = HU_MSGY + HU_REFRESHSPACING * hud_msg_lines;
  else
    w_chat.l.y = HU_INPUTY;

  // wait a few tics before sending a backspace character
  if (bsdown && bscounter++ > 9)
  {
    HUlib_keyInIText(&w_chat, KEY_BACKSPACE);
    bscounter = 8;
  }

  // tick down message counter if message is up
  if (message_counter && !--message_counter)
    message_on = message_nottobefuckedwith = false;

  if (secret_counter && !--secret_counter)
    secret_on = false;

  // [Woof!] "A secret is revealed!" message
  if (plr->secretmessage)
  {
    w_secret.l[0].x = ORIGWIDTH/2 - M_StringWidth(plr->secretmessage)/2;

    HUlib_addMessageToSText(&w_secret, 0, plr->secretmessage);
    plr->secretmessage = NULL;
    secret_on = true;
    secret_counter = 5*TICRATE/2; // [crispy] 2.5 seconds
  }

  // if messages on, or "Messages Off" is being displayed
  // this allows the notification of turning messages off to be seen
  // display message if necessary

  if ((showMessages || message_dontfuckwithme) && plr->message &&
      (!message_nottobefuckedwith || message_dontfuckwithme))
  {
    if (message_centered)
    {
      const int msg_x = ORIGWIDTH / 2 - M_StringWidth(plr->message) / 2;
      w_message.l->x = msg_x;
      w_rtext.x = msg_x;
    }

    //post the message to the message widget
    HUlib_addMessageToSText(&w_message, 0, plr->message);

    //jff 2/26/98 add message to refresh text widget too
    HUlib_addMessageToMText(&w_rtext, 0, plr->message);

    // clear the message to avoid posting multiple times
    plr->message = 0;

    // killough 11/98: display message list, possibly timed
    if (message_list)
    {
      message_list_on = true;
      message_list_counter = message_count;
    }
    else
    {
      message_on = true;       // note a message is displayed
      // start the message persistence counter	      
      message_counter = message_count;
    }

    has_message = true;        // killough 12/98

    // transfer "Messages Off" exception to the "being displayed" variable
    message_nottobefuckedwith = message_dontfuckwithme;

    // clear the flag that "Messages Off" is being posted
    message_dontfuckwithme = 0;
  }

  // check for incoming chat characters
  if (netgame)
  {
    int i, rc;
    char c;

    for (i = 0; i < MAXPLAYERS; i++)
    {
      if (!playeringame[i])
        continue;

      if (i != consoleplayer &&
          (c = players[i].cmd.chatchar))
      {
        if (c <= HU_BROADCAST)
          chat_dest[i] = c;
        else
        {
          if (c >= 'a' && c <= 'z')
            c = (char) shiftxform[(unsigned char) c];

          rc = HUlib_keyInIText(&w_inputbuffer[i], c);
          if (rc && c == KEY_ENTER)
          {
            if (w_inputbuffer[i].l.len &&
                (chat_dest[i] == consoleplayer + 1 ||
                chat_dest[i] == HU_BROADCAST))
            {
              HUlib_addMessageToSText(&w_message,
                                      *player_names[i],
                                      w_inputbuffer[i].l.l);

              has_message = true; // killough 12/98
              message_nottobefuckedwith = true;
              message_on = true;
              message_counter = chat_count; // killough 11/98
              S_StartSound(0, gamemode == commercial ?
                              sfx_radio : sfx_tink);
            }
            HUlib_resetIText(&w_inputbuffer[i]);
          }
        }
        players[i].cmd.chatchar = 0;
      }
    }
  }

  // draw the automap widgets if automap is displayed

  if (automapactive)
  {
    // map title
    HU_enableWidget(&w_title, true);

    HU_enableWidget(&w_monsec, map_level_stats);
    HU_enableWidget(&w_sttime, map_level_time);
    HU_enableWidget(&w_coord, STRICTMODE(map_player_coords));
  }
  else
  {
    HU_enableWidget(&w_coord, STRICTMODE(map_player_coords) == 2);
  }

  HU_enableWidget(&w_fps, plr->powers[pw_showfps]);

  if (hud_displayed &&
      scaledviewheight == SCREENHEIGHT &&
      automap_off)
  {
    if (crispy_hud)
    {
      if (hud_active > 0)
        draw_crispy_hud = true;
    }
    else
    {
      HU_enableWidget(&w_weapon, true);
      HU_enableWidget(&w_armor, true);
      HU_enableWidget(&w_health, true);
      HU_enableWidget(&w_ammo, true);
      HU_enableWidget(&w_keys, true);
    }

    HU_enableWidget(&w_monsec, hud_level_stats);
    HU_enableWidget(&w_sttime, hud_level_time);
  }
  else if (scaledviewheight &&
           scaledviewheight < SCREENHEIGHT &&
           automap_off)
  {
    HU_enableWidget(&w_monsec, hud_level_stats);
    HU_enableWidget(&w_sttime, hud_level_time);
  }

  while (w->line)
  {
    if (w->line->visible)
    {
      if (w->line->builder)
        w->line->builder();
    }
    w++;
  }

  // update crosshair properties
  if (hud_crosshair)
    HU_UpdateCrosshair();
}

#define QUEUESIZE   128

static char chatchars[QUEUESIZE];
static int  head = 0;
static int  tail = 0;

//
// HU_queueChatChar()
//
// Add an incoming character to the circular chat queue
//
// Passed the character to queue, returns nothing
//
void HU_queueChatChar(char c)
{
  if (((head + 1) & (QUEUESIZE-1)) == tail)
    doomprintf(MESSAGES_NONE, "%s", HUSTR_MSGU);
  else
    {
      chatchars[head++] = c;
      head &= QUEUESIZE-1;
    }
}

//
// HU_dequeueChatChar()
//
// Remove the earliest added character from the circular chat queue
//
// Passed nothing, returns the character dequeued
//
char HU_dequeueChatChar(void)
{
  char c;

  if (head != tail)
    {
      c = chatchars[tail++];
      tail &= QUEUESIZE-1;
    }
  else
    c = 0;
  return c;
}

//
// HU_Responder()
//
// Responds to input events that affect the heads up displays
//
// Passed the event to respond to, returns true if the event was handled
//

boolean HU_Responder(event_t *ev)
{
  static char   lastmessage[HU_MAXLINELENGTH+1];
  char*   macromessage;
  boolean   eatkey = false;
  static boolean  shiftdown = false;
  static boolean  altdown = false;
  int     c;
  int     i;
  int     numplayers;

  static int    num_nobrainers = 0;

  c = (ev->type == ev_keydown) ? ev->data1 : 0;

  numplayers = 0;
  for (i=0 ; i<MAXPLAYERS ; i++)
    numplayers += playeringame[i];

  if (ev->data1 == KEY_RSHIFT)
    {
      shiftdown = ev->type == ev_keydown;
      return false;
    }

  if (ev->data1 == KEY_RALT)
    {
      altdown = ev->type == ev_keydown;
      return false;
    }

  if (M_InputActivated(input_chat_backspace))
  {
    bsdown = true;
    bscounter = 0;
    c = KEY_BACKSPACE;
  }
  else if (M_InputDeactivated(input_chat_backspace))
  {
    bsdown = false;
    bscounter = 0;
  }

  if (ev->type == ev_keyup)
    return false;

  if (!chat_on)
    {
      if (M_InputActivated(input_chat_enter))                         // phares
        {
	  //jff 2/26/98 toggle list of messages

	  if (has_message)
	    {
	      // killough 11/98: Support timed or continuous message lists

	      if (!message_list)      // if not message list, refresh message
		{
		  message_counter = message_count;
		  message_on = true;
		}
	      else
		{
		  message_list_counter = message_count;
		  message_list_on = true;
		}
	    }
          eatkey = true;
        }  //jff 2/26/98 no chat if message review is displayed
      else // killough 10/02/98: no chat if demo playback
        if (!demoplayback)
          {
	    if (netgame && M_InputActivated(input_chat))
	      {
		eatkey = chat_on = true;
		HUlib_resetIText(&w_chat);
		HU_queueChatChar(HU_BROADCAST);
	      }//jff 2/26/98
	    else    // killough 11/98: simplify
	      if (netgame && numplayers > 2)
		for (i=0; i<MAXPLAYERS ; i++)
		  if (M_InputActivated(input_chat_dest0 + i))
		  {
		    if (i == consoleplayer)
		      doomprintf(MESSAGES_NONE, "%s", 
			++num_nobrainers <  3 ? HUSTR_TALKTOSELF1 :
	                  num_nobrainers <  6 ? HUSTR_TALKTOSELF2 :
	                  num_nobrainers <  9 ? HUSTR_TALKTOSELF3 :
	                  num_nobrainers < 32 ? HUSTR_TALKTOSELF4 :
                                                HUSTR_TALKTOSELF5 );
                  else
                    if (playeringame[i])
                      {
                        eatkey = chat_on = true;
                        HUlib_resetIText(&w_chat);
                        HU_queueChatChar((char)(i+1));
                        break;
                      }
		  }
          }
    }//jff 2/26/98 no chat functions if message review is displayed
  else
      {
        if (M_InputActivated(input_chat_enter))
        {
          c = KEY_ENTER;
        }

        // send a macro
        if (altdown)
          {
            c = c - '0';
            if (c < 0 || c > 9)
              return false;
            // fprintf(stderr, "got here\n");
            macromessage = chat_macros[c];
      
            // kill last message with a '\n'
            HU_queueChatChar(KEY_ENTER); // DEBUG!!!                // phares
      
            // send the macro message
            while (*macromessage)
              HU_queueChatChar(*macromessage++);
            HU_queueChatChar(KEY_ENTER);                            // phares
      
            // leave chat mode and notify that it was sent
            chat_on = false;
            strcpy(lastmessage, chat_macros[c]);
            doomprintf(MESSAGES_NONE, "%s", lastmessage);
            eatkey = true;
          }
        else
          {
            if (shiftdown || (c >= 'a' && c <= 'z'))
              c = shiftxform[c];
            eatkey = HUlib_keyInIText(&w_chat, c);
            if (eatkey)
              HU_queueChatChar(c);

            if (c == KEY_ENTER)                                     // phares
              {
                chat_on = false;
                if (w_chat.l.len)
                  {
                    strcpy(lastmessage, w_chat.l.l);
                    doomprintf(MESSAGES_NONE, "%s", lastmessage);
                  }
              }
            else
              if (c == KEY_ESCAPE)                               // phares
                chat_on = false;
          }
      }
  return eatkey;
}

// [FG] dynamic HUD alignment

static const struct {
  const char *name, *altname;
  hu_textline_t *const line;
} w_names[] = {
  {"title",  "levelname", &w_title},
  {"armor",   NULL,       &w_armor},
  {"health",  NULL,       &w_health},
  {"ammo",    NULL,       &w_ammo},
  {"weapon", "weapons",   &w_weapon},
  {"keys",    NULL,       &w_keys},
  {"monsec", "stats",     &w_monsec},
  {"sttime", "time",      &w_sttime},
  {"coord",  "coords",    &w_coord},
  {"fps",    "rate",      &w_fps},
};

static boolean HU_AddToWidgets (hu_textline_t *widget, int hud, align_t align, int x, int y)
{
  int i;

  if (hud < 0 || hud >= MAX_HUDS)
  {
    return false;
  }

  for (i = 0; i < MAX_WIDGETS - 1; i++)
  {
    if (widgets[hud][i].line == NULL)
    {
      break;
    }
  }

  if (i + 1 >= MAX_WIDGETS)
  {
    return false;
  }

  widgets[hud][i].line = widget;
  widgets[hud][i].align = align;
  widgets[hud][i].x = x;
  widgets[hud][i].y = y;

  widgets[hud][i + 1].line = NULL;

  return true;
}

static hu_textline_t *HU_WidgetByName (const char *name)
{
  int i;

  for (i = 0; i < arrlen(w_names); i++)
  {
    if (strcasecmp(name, w_names[i].name) == 0 ||
       (w_names[i].altname && strcasecmp(name, w_names[i].altname) == 0))
    {
      return w_names[i].line;
    }
  }

  return NULL;
}

static boolean HU_AddHUDCoords (char *name, int hud, int x, int y)
{
  hu_textline_t *widget = HU_WidgetByName(name);

  if (widget == NULL)
  {
    return false;
  }

  // [FG] relative alignment to the edges
  if (x < 0)
  {
    x += ORIGWIDTH;
  }
  if (y < 0)
  {
    y += ORIGHEIGHT;
  }

  if (x < 0 || x >= ORIGWIDTH || y < 0 || y >= ORIGHEIGHT)
  {
    return false;
  }

  return HU_AddToWidgets(widget, hud, align_direct, x, y);
}

static boolean HU_AddHUDAlignment (char *name, int hud, char *alignstr)
{
  hu_textline_t *widget = HU_WidgetByName(name);

  if (widget == NULL)
  {
    return false;
  }

  if (!strcasecmp(alignstr, "topleft")          || !strcasecmp(alignstr, "upperleft"))
  {
    return HU_AddToWidgets(widget, hud, align_topleft, 0, 0);
  }
  else if (!strcasecmp(alignstr, "topright")    || !strcasecmp(alignstr, "upperright"))
  {
    return HU_AddToWidgets(widget, hud, align_topright, 0, 0);
  }
  else if (!strcasecmp(alignstr, "bottomleft")  || !strcasecmp(alignstr, "lowerleft"))
  {
    return HU_AddToWidgets(widget, hud, align_bottomleft, 0, 0);
  }
  else if (!strcasecmp(alignstr, "bottomright") || !strcasecmp(alignstr, "lowerright"))
  {
    return HU_AddToWidgets(widget, hud, align_bottomright, 0, 0);
  }

  return false;
}

static void HU_ParseHUD (void)
{
  u_scanner_t scanner, *s;
  int hud = -1;
  int lumpnum;
  const char *data;
  int length;

  if ((lumpnum = W_CheckNumForName("WOOFHUD")) == -1)
  {
    return;
  }

  data = W_CacheLumpNum(lumpnum, PU_CACHE);
  length = W_LumpLength(lumpnum);

  scanner = U_ScanOpen(data, length, "WOOFHUD");
  s = &scanner;

  while (U_HasTokensLeft(s))
  {
    char *name;

    if (!U_CheckToken(s, TK_Identifier))
    {
      U_GetNextToken(s, true);
      continue;
    }

    if (!strcasecmp("HUD", s->string))
    {
      U_MustGetInteger(s);
      hud = s->number;

      if (hud < 0 || hud >= MAX_HUDS)
      {
        U_Error(s, "HUD (%d) must be either 0 or 1", hud);
      }

      memset(widgets[hud], 0, sizeof(widgets[hud]));
      continue;
    }

    name = M_StringDuplicate(s->string);

    if (U_CheckToken(s, TK_IntConst))
    {
      int x, y;

      x = s->number;
      U_MustGetInteger(s);
      y = s->number;

      if (!HU_AddHUDCoords(name, hud, x, y))
      {
        U_Error(s, "Cannot set coordinates for widget (%s)", name);
      }
    }
    else
    {
      char *align;

      U_MustGetToken(s, TK_Identifier);
      align = M_StringDuplicate(s->string);

      if (!HU_AddHUDAlignment(name, hud, align))
      {
        U_Error(s, "Cannot set alignment for widget (%s)", name);
      }

      free(align);
    }

    free(name);
  }

  U_ScanClose(s);
}

//----------------------------------------------------------------------------
//
// $Log: hu_stuff.c,v $
// Revision 1.27  1998/05/10  19:03:41  jim
// formatted/documented hu_stuff
//
// Revision 1.26  1998/05/03  22:25:24  killough
// Provide minimal headers at top; nothing else
//
// Revision 1.25  1998/04/28  15:53:58  jim
// Fix message list bug in small screen mode
//
// Revision 1.24  1998/04/22  12:50:14  jim
// Fix lockout from dynamic message change
//
// Revision 1.23  1998/04/05  10:09:51  jim
// added STCFN096 lump
//
// Revision 1.22  1998/03/28  05:32:12  jim
// Text enabling changes for DEH
//
// Revision 1.19  1998/03/17  20:45:23  jim
// added frags to HUD
//
// Revision 1.18  1998/03/15  14:42:16  jim
// added green fist/chainsaw in HUD when berserk
//
// Revision 1.17  1998/03/10  07:07:15  jim
// Fixed display glitch in HUD cycle
//
// Revision 1.16  1998/03/09  11:01:48  jim
// fixed string overflow for DEH, added graphic keys
//
// Revision 1.15  1998/03/09  07:10:09  killough
// Use displayplayer instead of consoleplayer
//
// Revision 1.14  1998/03/05  00:57:37  jim
// Scattered HUD
//
// Revision 1.13  1998/03/04  11:50:48  jim
// Change automap coord display
//
// Revision 1.12  1998/02/26  22:58:26  jim
// Added message review display to HUD
//
// Revision 1.11  1998/02/23  14:20:51  jim
// Merged HUD stuff, fixed p_plats.c to support elevators again
//
// Revision 1.10  1998/02/23  04:26:07  killough
// really allow new hud stuff to be turned off COMPLETELY
//
// Revision 1.9  1998/02/22  12:51:26  jim
// HUD control on F5, z coord, spacing change
//
// Revision 1.7  1998/02/20  18:46:51  jim
// cleanup of HUD control
//
// Revision 1.6  1998/02/19  16:54:53  jim
// Optimized HUD and made more configurable
//
// Revision 1.5  1998/02/18  11:55:55  jim
// Fixed issues with HUD and reduced screen size
//
// Revision 1.3  1998/02/15  02:47:47  phares
// User-defined keys
//
// Revision 1.2  1998/01/26  19:23:22  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
