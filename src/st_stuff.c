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
//
// DESCRIPTION:
//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "m_random.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "hu_stuff.h" // [FG] hud_displayed
#include "st_lib.h"
#include "r_main.h"
#include "am_map.h"
#include "m_cheat.h"
#include "m_misc2.h"
#include "m_swap.h"
#include "i_printf.h"
#include "s_sound.h"
#include "sounds.h"

// [crispy] immediately redraw status bar after help screens have been shown
extern boolean inhelpscreens;

//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS            1
#define STARTBONUSPALS          9
#define NUMREDPALS              8
#define NUMBONUSPALS            4
// Radiation suit, green shift.
#define RADIATIONPAL            13

// Location of status bar
#define ST_X                    0
#define ST_X2                   104

#define ST_FX                   143
#define ST_FY                   169

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH         (tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES         5
#define ST_NUMSTRAIGHTFACES     3
#define ST_NUMTURNFACES         2
#define ST_NUMSPECIALFACES      3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES        2
#define ST_NUMXDTHFACES         9

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES+ST_NUMXDTHFACES)

#define ST_TURNOFFSET           (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET           (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET       (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET        (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE              (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE             (ST_GODFACE+1)
#define ST_XDTHFACE             (ST_DEADFACE+1)

#define ST_FACESX               143
#define ST_FACESY               168

#define ST_EVILGRINCOUNT        (2*TICRATE)
#define ST_STRAIGHTFACECOUNT    (TICRATE/2)
#define ST_TURNCOUNT            (1*TICRATE)
#define ST_OUCHCOUNT            (1*TICRATE)
#define ST_RAMPAGEDELAY         (2*TICRATE)

#define ST_MUCHPAIN             20

// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?
// I dunno, why don't you go and find out!!!  killough

// AMMO number pos.
#define ST_AMMOWIDTH            3
#define ST_AMMOX                44
#define ST_AMMOY                171

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              90
#define ST_HEALTHY              171

// Weapon pos.
#define ST_ARMSX                111
#define ST_ARMSY                172
#define ST_ARMSBGX              104
#define ST_ARMSBGY              168
#define ST_ARMSXSPACE           12
#define ST_ARMSYSPACE           10

// Frags pos.
#define ST_FRAGSX               138
#define ST_FRAGSY               171
#define ST_FRAGSWIDTH           2

// ARMOR number pos.
#define ST_ARMORWIDTH           3
#define ST_ARMORX               221
#define ST_ARMORY               171

// Key icon positions.
#define ST_KEY0WIDTH            8
#define ST_KEY0HEIGHT           5
#define ST_KEY0X                239
#define ST_KEY0Y                171
#define ST_KEY1WIDTH            ST_KEY0WIDTH
#define ST_KEY1X                239
#define ST_KEY1Y                181
#define ST_KEY2WIDTH            ST_KEY0WIDTH
#define ST_KEY2X                239
#define ST_KEY2Y                191

// Ammunition counter.
#define ST_AMMO0WIDTH           3
#define ST_AMMO0HEIGHT          6
#define ST_AMMO0X               288
#define ST_AMMO0Y               173
#define ST_AMMO1WIDTH           ST_AMMO0WIDTH
#define ST_AMMO1X               288
#define ST_AMMO1Y               179
#define ST_AMMO2WIDTH           ST_AMMO0WIDTH
#define ST_AMMO2X               288
#define ST_AMMO2Y               191
#define ST_AMMO3WIDTH           ST_AMMO0WIDTH
#define ST_AMMO3X               288
#define ST_AMMO3Y               185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH        3
#define ST_MAXAMMO0HEIGHT       5
#define ST_MAXAMMO0X            314
#define ST_MAXAMMO0Y            173
#define ST_MAXAMMO1WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X            314
#define ST_MAXAMMO1Y            179
#define ST_MAXAMMO2WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X            314
#define ST_MAXAMMO2Y            191
#define ST_MAXAMMO3WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X            314
#define ST_MAXAMMO3Y            185

// killough 2/8/98: weapon info position macros UNUSED, removed here

// graphics are drawn to a backing screen and blitted to the real screen
static pixel_t *st_backing_screen = NULL;

// main player in game
static player_t *plyr;

// ST_Start() has just been called
static boolean st_firsttime;

// lump number for PLAYPAL
static int lu_palette;

// whether left-side main status bar is active
static boolean st_statusbaron;

// [crispy] distinguish classic status bar with background and player face from Crispy HUD
boolean st_crispyhud;
static boolean st_classicstatusbar;

// !deathmatch
static boolean st_notdeathmatch;

// !deathmatch && st_statusbaron
static boolean st_armson;

// !deathmatch
static boolean st_fragson;

// main bar left
static patch_t *sbar;

// main bar right, for doom 1.0
static patch_t *sbarr;

// 0-9, tall numbers
static patch_t *tallnum[10];

// tall % sign
static patch_t *tallpercent;

// 0-9, short, yellow (,different!) numbers
static patch_t *shortnum[10];

// 3 key-cards, 3 skulls, 3 card/skull combos
// jff 2/24/98 extend number of patches by three skull/card combos
static patch_t *keys[NUMCARDS+3];

// face status patches
static patch_t *faces[ST_NUMFACES];
static int have_xdthfaces;

// face background
static patch_t *faceback[MAXPLAYERS]; // killough 3/7/98: make array

 // main bar right
static patch_t *armsbg;

// weapon ownership patches
static patch_t *arms[6][2];

// ready-weapon widget
static st_number_t w_ready;

// [Alaux]
int smooth_counts;
int st_health = 100;
int st_armor = 0;

//jff 2/16/98 status color change levels
int ammo_red;      // ammo percent less than which status is red
int ammo_yellow;   // ammo percent less is yellow more green
int health_red;    // health amount less than which status is red
int health_yellow; // health amount less than which status is yellow
int health_green;  // health amount above is blue, below is green
int armor_red;     // armor amount less than which status is red
int armor_yellow;  // armor amount less than which status is yellow
int armor_green;   // armor amount above is blue, below is green

int hud_backpack_thresholds; // backpack changes thresholds
int hud_armor_type; // color of armor depends on type

 // in deathmatch only, summary of frags stats
static st_number_t w_frags;

// health widget
static st_percent_t w_health;

// weapon ownership widgets
static st_multicon_t w_arms[6];

// face status widget
static st_multicon_t w_faces;

// keycard widgets
static st_multicon_t w_keyboxes[3];

// armor widget
static st_percent_t  w_armor;

// ammo widgets
static st_number_t   w_ammo[4];

// max ammo widgets
static st_number_t   w_maxammo[4];

 // number of frags so far in deathmatch
static int      st_fragscount;

// used to use appopriately pained face
static int      st_oldhealth = -1;

// used for evil grin
static boolean  oldweaponsowned[NUMWEAPONS];

 // count until face changes
static int      st_facecount = 0;

// current face index, used by w_faces
static int      st_faceindex = 0;

// holds key-type for each key box on bar
static int      keyboxes[3];

// a random number per tick
static int      st_randomnumber;

extern char     *mapnames[];

//
// STATUS BAR CODE
//

void ST_Stop(void);

int st_solidbackground;

static void ST_DrawSolidBackground(int st_x)
{
  // [FG] calculate average color of the 16px left and right of the status bar
  const int vstep[][2] = {{0, 1}, {1, 2}, {2, ST_HEIGHT}};

  byte *pal = W_CacheLumpName("PLAYPAL", PU_STATIC);

  // [FG] temporarily draw status bar to background buffer
  V_DrawPatch(st_x, 0, sbar);

  const int offset = MAX(st_x + video.deltaw - SHORT(sbar->leftoffset), 0);
  const int width  = MIN(SHORT(sbar->width), video.unscaledw);
  const int depth  = 16;
  int v;

  // [FG] separate colors for the top rows
  for (v = 0; v < arrlen(vstep); v++)
  {
    int x, y;
    const int v0 = vstep[v][0], v1 = vstep[v][1];
    unsigned r = 0, g = 0, b = 0;
    byte col;

    for (y = v0; y < v1; y++)
    {
      for (x = 0; x < depth; x++)
      {
        byte *c = st_backing_screen + V_ScaleY(y) * video.pitch + V_ScaleX(x + offset);
        r += pal[3 * c[0] + 0];
        g += pal[3 * c[0] + 1];
        b += pal[3 * c[0] + 2];

        c += V_ScaleX(width - 2 * x - 1);
        r += pal[3 * c[0] + 0];
        g += pal[3 * c[0] + 1];
        b += pal[3 * c[0] + 2];
      }
    }

    r /= 2 * depth * (v1 - v0);
    g /= 2 * depth * (v1 - v0);
    b /= 2 * depth * (v1 - v0);

    // [FG] tune down to half saturation (for empiric reasons)
    col = I_GetPaletteIndex(pal, r/2, g/2, b/2);

    V_FillRect(0, v0, video.unscaledw, v1 - v0, col);
  }

  Z_ChangeTag (pal, PU_CACHE);
}

void ST_refreshBackground(boolean force)
{
    int st_x;

    if (!st_classicstatusbar)
    {
        return;
    }

    st_x = ST_X + (SCREENWIDTH - SHORT(sbar->width)) / 2 + SHORT(sbar->leftoffset);

    V_UseBuffer(st_backing_screen);

    if (video.unscaledw != ST_WIDTH)
    {
        if (st_solidbackground)
        {
            ST_DrawSolidBackground(st_x);
        }
        else
        {
            // [crispy] this is our own local copy of R_FillBackScreen() to fill
            // the entire background of st_backing_screen with the bezel
            // pattern, so it appears to the left and right of the status bar
            // in widescreen mode
            const char *name = (gamemode == commercial) ? "GRNROCK" : "FLOOR7_2";

            const byte *src = W_CacheLumpNum(firstflat + R_FlatNumForName(name), PU_CACHE);

            V_TileBlock64(SCREENHEIGHT - ST_HEIGHT, video.unscaledw, ST_HEIGHT, src);

            // [crispy] preserve bezel bottom edge
            if (scaledviewwidth == video.unscaledw)
            {
                int x;
                patch_t *patch = W_CacheLumpName("brdr_b", PU_CACHE);

                for (x = 0; x < video.deltaw; x += 8)
                {
                    V_DrawPatch(x - video.deltaw, 0, patch);
                    V_DrawPatch(SCREENWIDTH + video.deltaw - x - 8, 0, patch);
                }
            }
        }
    }

    // [crispy] center unity rerelease wide status bar
    V_DrawPatch(st_x, 0, sbar);

    // draw right side of bar if needed (Doom 1.0)
    if (sbarr)
        V_DrawPatch(ST_ARMSBGX, 0, sbarr);

    if (st_notdeathmatch)
        V_DrawPatch(ST_ARMSBGX, 0, armsbg);

    // killough 3/7/98: make face background change with displayplayer
    if (netgame)
        V_DrawPatch(ST_FX, 0, faceback[displayplayer]);

    V_RestoreBuffer();

    // [crispy] copy entire video.unscaledw, to preserve the pattern to the left
    // and right of the status bar in widescren mode
    if (!force)
    {
        V_CopyRect(ST_X, 0, st_backing_screen,
                   video.unscaledw, ST_HEIGHT,
                   ST_X, ST_Y);
    }
    else
    {
        if (video.deltaw > 0 && !st_firsttime)
        {
            V_CopyRect(0, 0, st_backing_screen,
                       video.deltaw, ST_HEIGHT,
                       0, ST_Y);
            V_CopyRect(SCREENWIDTH + video.deltaw, 0, st_backing_screen,
                       video.deltaw, ST_HEIGHT,
                       SCREENWIDTH + video.deltaw, ST_Y);
        }
    }
}

// Respond to keyboard input events,
//  intercept cheats.
boolean ST_Responder(event_t *ev)
{
  // Filter automap on/off.
  if (ev->type == ev_keyup && (ev->data1 & 0xffff0000) == AM_MSGHEADER)
    {
      if (ev->data1 == AM_MSGENTERED)
      {
        st_firsttime = true;
      }
    }
  else  // if a user keypress...
    return M_CheatResponder(ev);       // Try cheat responder in m_cheat.c
  return false;
}

int ST_calcPainOffset(void)
{
  static int lastcalc;
  static int oldhealth = -1;
  int health = plyr->health > 100 ? 100 : plyr->health;

  if (health != oldhealth)
    {
      lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
      oldhealth = health;
    }
  return lastcalc;
}

//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//

static int ST_DeadFace(void)
{
  const int state = (plyr->mo->state - states) - mobjinfo[plyr->mo->type].xdeathstate;

  // [FG] support face gib animations as in the 3DO/Jaguar/PSX ports
  if (have_xdthfaces && state >= 0)
  {
    return ST_XDTHFACE + MIN(state, have_xdthfaces - 1);
  }

  return ST_DEADFACE;
}

void ST_updateFaceWidget(void)
{
  int         i;
  angle_t     badguyangle;
  angle_t     diffang;
  static int  lastattackdown = -1;
  static int  priority = 0;
  boolean     doevilgrin;

  if (priority < 10)
    {
      // dead
      if (!plyr->health)
        {
          priority = 9;
          st_faceindex = ST_DeadFace();
          st_facecount = 1;
        }
    }

  if (priority < 9)
    {
      if (plyr->bonuscount)
        {
          // picking up bonus
          doevilgrin = false;

          for (i=0;i<NUMWEAPONS;i++)
            {
              if (oldweaponsowned[i] != plyr->weaponowned[i])
                {
                  doevilgrin = true;
                  oldweaponsowned[i] = plyr->weaponowned[i];
                }
            }
          if (doevilgrin)
            {
              // evil grin if just picked up weapon
              priority = 8;
              st_facecount = ST_EVILGRINCOUNT;
              st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }

    }

  if (priority < 8)
    {
      if (plyr->damagecount && plyr->attacker && plyr->attacker != plyr->mo)
        {
          // being attacked
          priority = 7;

          // [FG] show "Ouch Face" as intended
          if (st_oldhealth - plyr->health > ST_MUCHPAIN)
            {
              // [FG] raise "Ouch Face" priority
              priority = 8;
              st_facecount = ST_TURNCOUNT;
              st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
          else
            {
              badguyangle = R_PointToAngle2(plyr->mo->x,
                                            plyr->mo->y,
                                            plyr->attacker->x,
                                            plyr->attacker->y);

              if (badguyangle > plyr->mo->angle)
                {
                  // whether right or left
                  diffang = badguyangle - plyr->mo->angle;
                  i = diffang > ANG180;
                }
              else
                {
                  // whether left or right
                  diffang = plyr->mo->angle - badguyangle;
                  i = diffang <= ANG180;
                } // confusing, aint it?


              st_facecount = ST_TURNCOUNT;
              st_faceindex = ST_calcPainOffset();

              if (diffang < ANG45)
                {
                  // head-on
                  st_faceindex += ST_RAMPAGEOFFSET;
                }
              else if (i)
                {
                  // turn face right
                  st_faceindex += ST_TURNOFFSET;
                }
              else
                {
                  // turn face left
                  st_faceindex += ST_TURNOFFSET+1;
                }
            }
        }
    }

  if (priority < 7)
    {
      // getting hurt because of your own damn stupidity
      if (plyr->damagecount)
        {
          // [FG] show "Ouch Face" as intended
          if (st_oldhealth - plyr->health > ST_MUCHPAIN)
            {
              priority = 7;
              st_facecount = ST_TURNCOUNT;
              st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
          else
            {
              priority = 6;
              st_facecount = ST_TURNCOUNT;
              st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
            }

        }

    }

  if (priority < 6)
    {
      // rapid firing
      if (plyr->attackdown)
        {
          if (lastattackdown==-1)
            lastattackdown = ST_RAMPAGEDELAY;
          else if (!--lastattackdown)
            {
              priority = 5;
              st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
              st_facecount = 1;
              lastattackdown = 1;
            }
        }
      else
        lastattackdown = -1;

    }

  if (priority < 5)
    {
      // invulnerability
      if ((plyr->cheats & CF_GODMODE)
          || plyr->powers[pw_invulnerability])
        {
          priority = 4;

          st_faceindex = ST_GODFACE;
          st_facecount = 1;

        }

    }

  // look left or look right if the facecount has timed out
  if (!st_facecount)
    {
      st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
      st_facecount = ST_STRAIGHTFACECOUNT;
      priority = 0;
    }

  st_facecount--;

}

int sts_traditional_keys; // killough 2/28/98: traditional status bar keys
int hud_blink_keys; // [crispy] blinking key or skull in the status bar

void ST_BlinkKeys(player_t* player, int blue, int yellow, int red)
{
  int i;
  int keys[3] = { blue, yellow, red };

  player->keyblinktics = KEYBLINKTICS;

  for (i = 0; i < 3; i++)
  {
    if (   ((keys[i] == KEYBLINK_EITHER) && !(player->cards[i] || player->cards[i+3]))
        || ((keys[i] == KEYBLINK_CARD)   && !(player->cards[i]))
        || ((keys[i] == KEYBLINK_SKULL)  && !(player->cards[i+3]))
        || ((keys[i] == KEYBLINK_BOTH)   && !(player->cards[i] && player->cards[i+3])))
    {
      player->keyblinkkeys[i] = keys[i];
    }
    else
    {
      player->keyblinkkeys[i] = KEYBLINK_NONE;
    }
  }
}

void ST_updateWidgets(void)
{
  static int  largeammo = 1994; // means "n/a"
  int         i;

  // must redirect the pointer if the ready weapon has changed.
  //  if (w_ready.data != plyr->readyweapon)
  //  {
  if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
    w_ready.num = &largeammo;
  else
    w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
  //{
  // static int tic=0;
  // static int dir=-1;
  // if (!(tic&15))
  //   plyr->ammo[weaponinfo[plyr->readyweapon].ammo]+=dir;
  // if (plyr->ammo[weaponinfo[plyr->readyweapon].ammo] == -100)
  //   dir = 1;
  // tic++;
  // }
  w_ready.data = plyr->readyweapon;

  // if (*w_ready.on)
  //  STlib_updateNum(&w_ready, true);
  // refresh weapon change
  //  }

  // update keycard multiple widgets
  for (i=0;i<3;i++)
    {
      keyboxes[i] = plyr->cards[i] ? i : -1;

      //jff 2/24/98 select double key
      //killough 2/28/98: preserve traditional keys by config option

      if (plyr->cards[i+3])
        keyboxes[i] = keyboxes[i]==-1 || sts_traditional_keys ? i+3 : i+6;
    }

  // [crispy] blinking key or skull in the status bar
  if (plyr->keyblinktics)
  {
    if (!hud_blink_keys ||
        !(st_classicstatusbar || (hud_displayed && hud_active > 0)))
    {
      plyr->keyblinktics = 0;
    }
    else
    {
      if (!(plyr->keyblinktics & (2*KEYBLINKMASK - 1)))
        S_StartSound(NULL, sfx_itemup);

      plyr->keyblinktics--;

      for (i = 0; i < 3; i++)
      {
        if (!plyr->keyblinkkeys[i])
          continue;

        if (plyr->keyblinktics & KEYBLINKMASK)
        {
          switch (plyr->keyblinkkeys[i])
          {
            case KEYBLINK_EITHER:
              if ( (plyr->keyblinktics & (2*KEYBLINKMASK)) &&
                  !(plyr->keyblinktics & (4*KEYBLINKMASK)))
                keyboxes[i] = i + 3;
              else
                keyboxes[i] = i;
              break;

            case KEYBLINK_CARD:
              keyboxes[i] = i;
              break;

            case KEYBLINK_SKULL:
              keyboxes[i] = i + 3;
              break;

            case KEYBLINK_BOTH:
              keyboxes[i] = i + 6;
              break;

            default:
              break;
          }
        }
        else
        {
          keyboxes[i] = -1;
        }
      }
    }
  }

  // refresh everything if this is him coming back to life
  ST_updateFaceWidget();

  // used for armbg patch
  st_notdeathmatch = !deathmatch;

  // used by w_arms[] widgets
  st_armson = st_statusbaron && !deathmatch;

  // used by w_frags widget
  st_fragson = deathmatch && st_statusbaron;
  st_fragscount = 0;

  for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (i != displayplayer)            // killough 3/7/98
        st_fragscount += plyr->frags[i];
      else
        st_fragscount -= plyr->frags[i];
    }

}

// [Alaux]
static int SmoothCount(int shownval, int realval)
{
  int step = realval - shownval;

  if (!smooth_counts || !step)
  {
    return realval;
  }
  else
  {
    int sign = step / abs(step);
    step = BETWEEN(1, 7, abs(step) / 20);
    shownval += (step+1)*sign;
  
    if (  (sign > 0 && shownval > realval)
        ||(sign < 0 && shownval < realval))
    {
      shownval = realval;
    }

    return shownval;
  }
}

boolean st_invul;
static void ST_doPaletteStuff(void);

void ST_Ticker(void)
{
  st_health = SmoothCount(st_health, plyr->health);
  st_armor  = SmoothCount(st_armor, plyr->armorpoints);
  
  st_randomnumber = M_Random();
  ST_updateWidgets();
  st_oldhealth = plyr->health;

  st_invul = (plyr->powers[pw_invulnerability] > 4*32 ||
              plyr->powers[pw_invulnerability] & 8) ||
              plyr->cheats & CF_GODMODE;

  if (!nodrawers)
    ST_doPaletteStuff();  // Do red-/gold-shifts from damage/items
}

static int st_palette = 0;
boolean palette_changes = true;

static void ST_doPaletteStuff(void)
{
  int         palette;
  byte*       pal;
  int cnt = plyr->damagecount;

  // killough 7/14/98: beta version did not cause red berserk palette
  if (!beta_emulation)

  if (plyr->powers[pw_strength])
    {
      // slowly fade the berzerk out
      int bzc = 12 - (plyr->powers[pw_strength]>>6);
      if (bzc > cnt)
        cnt = bzc;
    }

  if (STRICTMODE(!palette_changes))
  {
    palette = 0;
  }
  else
  if (cnt)
  {
    // In Chex Quest, the player never sees red. Instead, the radiation suit
    // palette is used to tint the screen green, as though the player is being
    // covered in goo by an attacking flemoid.
    if (gameversion == exe_chex)
    {
      palette = RADIATIONPAL;
    }
    else
    {
      palette = (cnt+7)>>3;
      if (palette >= NUMREDPALS)
        palette = NUMREDPALS-1;
      // [crispy] tune down a bit so the menu remains legible
      if (menuactive || paused)
        palette >>= 1;
      palette += STARTREDPALS;
    }
  }
  else
    if (plyr->bonuscount)
      {
        palette = (plyr->bonuscount+7)>>3;
        if (palette >= NUMBONUSPALS)
          palette = NUMBONUSPALS-1;
        palette += STARTBONUSPALS;
      }
    else
      // killough 7/14/98: beta version did not cause green palette
      if (beta_emulation)
        palette = 0;
      else
      if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
        palette = RADIATIONPAL;
      else
        palette = 0;

  if (palette != st_palette)
    {
      st_palette = palette;
      // haleyjd: must cast to byte *, arith. on void pointer is
      // a GNU C extension
      pal = (byte *)W_CacheLumpNum(lu_palette, PU_CACHE) + palette*768;
      I_SetPalette (pal);
    }
}

void ST_drawWidgets(void)
{
  int i;
  int maxammo = plyr->maxammo[weaponinfo[w_ready.data].ammo];

  // [Alaux] Used to color health and armor counts based on
  // the real values, only ever relevant when using smooth counts
  const int health = plyr->health,  armor = plyr->armorpoints;

  // clear area
  if (!st_crispyhud && st_statusbaron)
  {
    V_CopyRect(video.deltaw, 0, st_backing_screen, ST_WIDTH, ST_HEIGHT,
               video.deltaw, ST_Y);
  }

  // used by w_arms[] widgets
  st_armson = st_statusbaron && !deathmatch;

  // used by w_frags widget
  st_fragson = deathmatch && st_statusbaron;

  // backpack changes thresholds
  if (plyr->backpack && !hud_backpack_thresholds)
    maxammo /= 2;

  //jff 2/16/98 make color of ammo depend on amount
  if (*w_ready.num*100 < ammo_red*maxammo)
    STlib_updateNum(&w_ready, cr_red);
  else
    if (*w_ready.num*100 <
        ammo_yellow*maxammo)
      STlib_updateNum(&w_ready, cr_gold);
    else if (*w_ready.num > maxammo)
      STlib_updateNum(&w_ready, cr_blue2);
    else
      STlib_updateNum(&w_ready, cr_green);

  for (i=0;i<4;i++)
    {
      STlib_updateNum(&w_ammo[i], NULL);   //jff 2/16/98 no xlation
      STlib_updateNum(&w_maxammo[i], NULL);
    }

  // [Alaux] Make color of health gray when invulnerable
  if (st_invul)
    STlib_updatePercent(&w_health, cr_gray);
  else
  //jff 2/16/98 make color of health depend on amount
  if (health<health_red)
    STlib_updatePercent(&w_health, cr_red);
  else if (health<health_yellow)
    STlib_updatePercent(&w_health, cr_gold);
  else if (health<=health_green)
    STlib_updatePercent(&w_health, cr_green);
  else
    STlib_updatePercent(&w_health, cr_blue2); //killough 2/28/98

  // color of armor depends on type
  if (hud_armor_type)
  {
    if (st_invul)
      STlib_updatePercent(&w_armor, cr_gray);
    else if (!plyr->armortype)
      STlib_updatePercent(&w_armor, cr_red);
    else if (plyr->armortype == 1)
      STlib_updatePercent(&w_armor, cr_green);
    else
      STlib_updatePercent(&w_armor, cr_blue2);
  }
  else
  {
  if (st_invul)
    STlib_updatePercent(&w_armor, cr_gray);
  else
  //jff 2/16/98 make color of armor depend on amount
  if (armor<armor_red)
    STlib_updatePercent(&w_armor, cr_red);
  else if (armor<armor_yellow)
    STlib_updatePercent(&w_armor, cr_gold);
  else if (armor<=armor_green)
    STlib_updatePercent(&w_armor, cr_green);
  else
    STlib_updatePercent(&w_armor, cr_blue2); //killough 2/28/98
  }

  for (i=0;i<6;i++)
    STlib_updateMultIcon(&w_arms[i]);

  STlib_updateMultIcon(&w_faces);

  for (i=0;i<3;i++)
    STlib_updateMultIcon(&w_keyboxes[i]);

  STlib_updateNum(&w_frags, NULL);

}

static void ST_MoveHud (void);

void ST_Drawer(boolean fullscreen, boolean refresh)
{
  st_statusbaron = !fullscreen || automap_on;
  // [crispy] immediately redraw status bar after help screens have been shown
  st_firsttime = st_firsttime || refresh || inhelpscreens;

  // [crispy] distinguish classic status bar with background and player face from Crispy HUD
  st_crispyhud = (hud_type == HUD_TYPE_CRISPY) && hud_displayed && automap_off;
  st_classicstatusbar = st_statusbaron && !st_crispyhud;

  ST_MoveHud();

  if (st_firsttime)     // If just after ST_Start(), refresh all
  {
    st_firsttime = false;

    // draw status bar background to off-screen buff
    ST_refreshBackground(false);
  }
  
  ST_drawWidgets();
}

void ST_loadGraphics(void)
{
  int  i, facenum;
  char namebuf[32];

  // Load the numbers, tall and short
  for (i=0;i<10;i++)
    {
      sprintf(namebuf, "STTNUM%d", i);
      tallnum[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
      M_snprintf(namebuf, sizeof(namebuf), "STYSNUM%d", i);
      shortnum[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
    }

  // Load percent key.
  //Note: why not load STMINUS here, too?
  tallpercent = (patch_t *) W_CacheLumpName("STTPRCNT", PU_STATIC);

  // key cards
  for (i=0;i<NUMCARDS+3;i++)  //jff 2/23/98 show both keys too
    {
      sprintf(namebuf, "STKEYS%d", i);
      keys[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
    }

  // arms background
  armsbg = (patch_t *) W_CacheLumpName("STARMS", PU_STATIC);

  // arms ownership widgets
  for (i=0;i<6;i++)
    {
      sprintf(namebuf, "STGNUM%d", i+2);

      // gray #
      arms[i][0] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);

      // yellow #
      arms[i][1] = shortnum[i+2];
    }

  // face backgrounds for different color players
  // killough 3/7/98: add better support for spy mode by loading all
  // player face backgrounds and using displayplayer to choose them:
  for (i=0; i<MAXPLAYERS; i++)
    {
      sprintf(namebuf, "STFB%d", i);
      faceback[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
    }

  // status bar background bits
  if (W_CheckNumForName("STBAR") >= 0)
  {
    sbar  = (patch_t *) W_CacheLumpName("STBAR", PU_STATIC);
    sbarr = NULL;
  }
  else
  {
    sbar  = (patch_t *) W_CacheLumpName("STMBARL", PU_STATIC);
    sbarr = (patch_t *) W_CacheLumpName("STMBARR", PU_STATIC);
  }

  // face states
  facenum = 0;
  for (i=0;i<ST_NUMPAINFACES;i++)
    {
      int j;
      for (j=0;j<ST_NUMSTRAIGHTFACES;j++)
        {
          sprintf(namebuf, "STFST%d%d", i, j);
          faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
        }
      sprintf(namebuf, "STFTR%d0", i);        // turn right
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
      sprintf(namebuf, "STFTL%d0", i);        // turn left
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
      sprintf(namebuf, "STFOUCH%d", i);       // ouch!
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
      sprintf(namebuf, "STFEVL%d", i);        // evil grin ;)
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
      sprintf(namebuf, "STFKILL%d", i);       // pissed off
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
    }
  faces[facenum++] = W_CacheLumpName("STFGOD0", PU_STATIC);
  faces[facenum++] = W_CacheLumpName("STFDEAD0", PU_STATIC);

  // [FG] support face gib animations as in the 3DO/Jaguar/PSX ports
  for (i = 0; i < ST_NUMXDTHFACES; i++)
  {
    sprintf(namebuf, "STFXDTH%d", i);

    if (W_CheckNumForName(namebuf) != -1)
      faces[facenum++] = W_CacheLumpName(namebuf, PU_STATIC);
    else
      break;
  }
  have_xdthfaces = i;
}

void ST_loadData(void)
{
  lu_palette = W_GetNumForName ("PLAYPAL");
  ST_loadGraphics();
}

void ST_initData(void)
{
  int i;

  st_firsttime = true;
  plyr = &players[displayplayer];            // killough 3/7/98

  st_statusbaron = true;

  st_faceindex = 0;
  st_palette = -1;

  st_oldhealth = -1;

  for (i=0;i<NUMWEAPONS;i++)
    oldweaponsowned[i] = plyr->weaponowned[i];

  for (i=0;i<3;i++)
    keyboxes[i] = -1;

  STlib_init();
}

static int distributed_delta = 0;

void ST_createWidgets(void)
{
  int i;

  // ready weapon ammo
  STlib_initNum(&w_ready,
                ST_AMMOX - distributed_delta,
                ST_AMMOY,
                tallnum,
                &plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
                &st_statusbaron,
                ST_AMMOWIDTH );

  // the last weapon type
  w_ready.data = plyr->readyweapon;

  // health percentage
  STlib_initPercent(&w_health,
                    ST_HEALTHX - distributed_delta,
                    ST_HEALTHY,
                    tallnum,
                    &st_health,
                    &st_statusbaron,
                    tallpercent);

  // weapons owned
  for(i=0;i<6;i++)
    {
      STlib_initMultIcon(&w_arms[i],
                         ST_ARMSX+(i%3)*ST_ARMSXSPACE - distributed_delta,
                         ST_ARMSY+(i/3)*ST_ARMSYSPACE,
                         arms[i], (int *) &plyr->weaponowned[i+1],
                         &st_armson);
    }

  // frags sum
  STlib_initNum(&w_frags,
                ST_FRAGSX - distributed_delta,
                ST_FRAGSY,
                tallnum,
                &st_fragscount,
                &st_fragson,
                ST_FRAGSWIDTH);

  // faces
  STlib_initMultIcon(&w_faces,
                     ST_FACESX,
                     ST_FACESY,
                     faces,
                     &st_faceindex,
                     &st_classicstatusbar);

  // armor percentage - should be colored later
  STlib_initPercent(&w_armor,
                    ST_ARMORX + distributed_delta,
                    ST_ARMORY,
                    tallnum,
                    &st_armor,
                    &st_statusbaron, tallpercent);

  // keyboxes 0-2
  STlib_initMultIcon(&w_keyboxes[0],
                     ST_KEY0X + distributed_delta,
                     ST_KEY0Y,
                     keys,
                     &keyboxes[0],
                     &st_statusbaron);

  STlib_initMultIcon(&w_keyboxes[1],
                     ST_KEY1X + distributed_delta,
                     ST_KEY1Y,
                     keys,
                     &keyboxes[1],
                     &st_statusbaron);

  STlib_initMultIcon(&w_keyboxes[2],
                     ST_KEY2X + distributed_delta,
                     ST_KEY2Y,
                     keys,
                     &keyboxes[2],
                     &st_statusbaron);

  // ammo count (all four kinds)
  STlib_initNum(&w_ammo[0],
                ST_AMMO0X + distributed_delta,
                ST_AMMO0Y,
                shortnum,
                &plyr->ammo[0],
                &st_statusbaron,
                ST_AMMO0WIDTH);

  STlib_initNum(&w_ammo[1],
                ST_AMMO1X + distributed_delta,
                ST_AMMO1Y,
                shortnum,
                &plyr->ammo[1],
                &st_statusbaron,
                ST_AMMO1WIDTH);

  STlib_initNum(&w_ammo[2],
                ST_AMMO2X + distributed_delta,
                ST_AMMO2Y,
                shortnum,
                &plyr->ammo[2],
                &st_statusbaron,
                ST_AMMO2WIDTH);

  STlib_initNum(&w_ammo[3],
                ST_AMMO3X + distributed_delta,
                ST_AMMO3Y,
                shortnum,
                &plyr->ammo[3],
                &st_statusbaron,
                ST_AMMO3WIDTH);

  // max ammo count (all four kinds)
  STlib_initNum(&w_maxammo[0],
                ST_MAXAMMO0X + distributed_delta,
                ST_MAXAMMO0Y,
                shortnum,
                &plyr->maxammo[0],
                &st_statusbaron,
                ST_MAXAMMO0WIDTH);

  STlib_initNum(&w_maxammo[1],
                ST_MAXAMMO1X + distributed_delta,
                ST_MAXAMMO1Y,
                shortnum,
                &plyr->maxammo[1],
                &st_statusbaron,
                ST_MAXAMMO1WIDTH);

  STlib_initNum(&w_maxammo[2],
                ST_MAXAMMO2X + distributed_delta,
                ST_MAXAMMO2Y,
                shortnum,
                &plyr->maxammo[2],
                &st_statusbaron,
                ST_MAXAMMO2WIDTH);

  STlib_initNum(&w_maxammo[3],
                ST_MAXAMMO3X + distributed_delta,
                ST_MAXAMMO3Y,
                shortnum,
                &plyr->maxammo[3],
                &st_statusbaron,
                ST_MAXAMMO3WIDTH);
}

static void ST_MoveHud (void)
{
    static int odelta = 0;

    if (st_crispyhud && hud_active == 2)
        distributed_delta = video.deltaw;
    else
        distributed_delta = 0;

    if (distributed_delta != odelta)
    {
      ST_createWidgets();
      odelta = distributed_delta;
    }
}

static boolean st_stopped = true;

void ST_Start(void)
{
  if (!st_stopped)
    ST_Stop();
  ST_initData();
  ST_createWidgets();
  st_stopped = false;
}

void ST_Stop(void)
{
  if (st_stopped)
    return;
  if (!nodrawers)
    I_SetPalette (W_CacheLumpNum (lu_palette, PU_CACHE));
  st_stopped = true;
}

static int StatusBarBufferHeight(void)
{
  int i;
  int st_height = ST_HEIGHT;
  patch_t *const patch = W_CacheLumpName("brdr_b", PU_CACHE);

  if (patch && patch->height > st_height)
    st_height = patch->height;

  if (sbar && sbar->height > st_height)
    st_height = sbar->height;

  if (armsbg && armsbg->height > st_height)
    st_height = armsbg->height;

  for (i = 0; i < MAXPLAYERS; i++)
  {
    if (faceback[i] && faceback[i]->height > st_height)
      st_height = faceback[i]->height;
  }

  return st_height;
}

void ST_Init(void)
{
  ST_loadData();
}

void ST_InitRes(void)
{
  int height = V_ScaleY(StatusBarBufferHeight());

  if (st_backing_screen)
  {
    Z_Free(st_backing_screen);
  }

  // killough 11/98: allocate enough for hires
  st_backing_screen = Z_Malloc(video.pitch * height * sizeof(*st_backing_screen), PU_STATIC, 0);
}

void ST_Warnings(void)
{
  int i;
  patch_t *const patch = W_CacheLumpName("brdr_b", PU_CACHE);

  if (patch && patch->height > ST_HEIGHT)
  {
    I_Printf(VB_WARNING, "ST_Init: Non-standard BRDR_B height of %d. "
                         "Expected <= %d.", patch->height, ST_HEIGHT);
  }

  if (sbar && sbar->height != ST_HEIGHT)
  {
    I_Printf(VB_WARNING, "ST_Init: Non-standard STBAR height of %d. "
                         "Expected %d.", sbar->height, ST_HEIGHT);
  }

  if (armsbg && armsbg->height > ST_HEIGHT)
  {
    I_Printf(VB_WARNING, "ST_Init: Non-standard STARMS height of %d. "
                         "Expected <= %d.", armsbg->height, ST_HEIGHT);
  }

  for (i = 0; i < MAXPLAYERS; i++)
  {
    if (faceback[i] && faceback[i]->height > ST_HEIGHT)
    {
      I_Printf(VB_WARNING, "ST_Init: Non-standard STFB%d height of %d. "
                           "Expected <= %d.", i, faceback[i]->height, ST_HEIGHT);
    }
  }
}

void ST_ResetPalette(void)
{
  st_palette = -1;
  I_SetPalette(W_CacheLumpNum(lu_palette, PU_CACHE));
}

//----------------------------------------------------------------------------
//
// $Log: st_stuff.c,v $
// Revision 1.46  1998/05/06  16:05:40  jim
// formatting and documenting
//
// Revision 1.45  1998/05/03  22:50:58  killough
// beautification, move external declarations, remove cheats
//
// Revision 1.44  1998/04/27  17:30:39  jim
// Fix DM demo/newgame status, remove IDK (again)
//
// Revision 1.43  1998/04/27  02:30:12  killough
// fuck you
//
// Revision 1.42  1998/04/24  23:52:31  thldrmn
// Removed idk cheat
//
// Revision 1.41  1998/04/24  11:39:23  killough
// Fix cheats while demo is played back
//
// Revision 1.40  1998/04/19  01:10:19  killough
// Generalize cheat engine to add deh support
//
// Revision 1.39  1998/04/16  06:26:06  killough
// Prevent cheats from working inside menu
//
// Revision 1.38  1998/04/12  10:58:24  jim
// IDMUSxy for DOOM 1 fix
//
// Revision 1.37  1998/04/12  10:23:52  jim
// IDMUS00 ok in DOOM 1
//
// Revision 1.36  1998/04/12  02:00:39  killough
// Change tranmap to main_tranmap
//
// Revision 1.35  1998/04/12  01:08:51  jim
// Fixed IDMUS00 crash
//
// Revision 1.34  1998/04/11  14:48:11  thldrmn
// Replaced IDK with TNTKA cheat
//
// Revision 1.33  1998/04/10  06:36:45  killough
// Fix -fast parameter bugs
//
// Revision 1.32  1998/03/31  10:37:17  killough
// comment clarification
//
// Revision 1.31  1998/03/28  18:09:19  killough
// Fix deh-cheat self-annihilation bug, make iddt closer to Doom
//
// Revision 1.30  1998/03/28  05:33:02  jim
// Text enabling changes for DEH
//
// Revision 1.29  1998/03/23  15:24:54  phares
// Changed pushers to linedef control
//
// Revision 1.28  1998/03/23  06:43:26  jim
// linedefs reference initial version
//
// Revision 1.27  1998/03/23  03:40:46  killough
// Fix idclip bug, make monster kills message smart
//
// Revision 1.26  1998/03/20  00:30:37  phares
// Changed friction to linedef control
//
// Revision 1.25  1998/03/17  20:44:32  jim
// fixed idmus non-restore, space bug
//
// Revision 1.24  1998/03/12  14:35:01  phares
// New cheat codes
//
// Revision 1.23  1998/03/10  07:14:38  jim
// Initial DEH support added, minus text
//
// Revision 1.22  1998/03/09  07:31:48  killough
// Fix spy mode to display player correctly, add TNTFAST
//
// Revision 1.21  1998/03/06  05:31:02  killough
// PEst control, from the TNT'EM man
//
// Revision 1.20  1998/03/02  15:35:03  jim
// Enabled Lee's status changes, added new types to common.cfg
//
// Revision 1.19  1998/03/02  12:09:18  killough
// blue status bar color, monsters_remember, traditional_keys
//
// Revision 1.18  1998/02/27  11:00:58  phares
// Can't own weapons that don't exist
//
// Revision 1.17  1998/02/26  22:57:45  jim
// Added message review display to HUD
//
// Revision 1.16  1998/02/24  08:46:45  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.15  1998/02/24  04:14:19  jim
// Added double keys to status
//
// Revision 1.14  1998/02/23  04:57:29  killough
// Fix TNTEM cheat again, add new cheats
//
// Revision 1.13  1998/02/20  21:57:07  phares
// Preliminarey sprite translucency
//
// Revision 1.12  1998/02/19  23:15:52  killough
// Add TNTAMMO in addition to TNTAMO
//
// Revision 1.11  1998/02/19  16:55:22  jim
// Optimized HUD and made more configurable
//
// Revision 1.10  1998/02/18  00:59:20  jim
// Addition of HUD
//
// Revision 1.9  1998/02/17  06:15:48  killough
// Add TNTKEYxx, TNTAMOx, TNTWEAPx cheats, and cheat engine support for them.
//
// Revision 1.8  1998/02/15  02:48:01  phares
// User-defined keys
//
// Revision 1.7  1998/02/09  03:19:04  killough
// Rewrite cheat code engine, add IDK and TNTHOM
//
// Revision 1.6  1998/02/02  22:19:01  jim
// Added TNTEM cheat to kill every monster alive
//
// Revision 1.5  1998/01/30  18:48:10  phares
// Changed textspeed and textwait to functions
//
// Revision 1.4  1998/01/30  16:09:03  phares
// Faster end-mission text display
//
// Revision 1.3  1998/01/28  12:23:05  phares
// TNTCOMP cheat code added
//
// Revision 1.2  1998/01/26  19:24:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
