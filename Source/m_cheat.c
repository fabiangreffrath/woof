// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: m_cheat.c,v 1.7 1998/05/12 12:47:00 phares Exp $
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
//      Cheat sequence checking.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_cheat.c,v 1.7 1998/05/12 12:47:00 phares Exp $";

#include "doomstat.h"
#include "g_game.h"
#include "r_data.h"
#include "p_inter.h"
#include "m_cheat.h"
#include "m_argv.h"
#include "s_sound.h"
#include "sounds.h"
#include "dstrings.h"
#include "d_deh.h"  // Ty 03/27/98 - externalized strings
#include "d_io.h" // haleyjd

#define plyr (players+consoleplayer)     /* the console player */

//-----------------------------------------------------------------------------
//
// CHEAT SEQUENCE PACKAGE
//
//-----------------------------------------------------------------------------

static void cheat_mus();
static void cheat_choppers();
static void cheat_god();
static void cheat_fa();
static void cheat_k();
static void cheat_kfa();
static void cheat_noclip();
static void cheat_pw();
static void cheat_behold();
static void cheat_clev();
static void cheat_mypos();
static void cheat_comp();
static void cheat_friction();
static void cheat_pushers();
static void cheat_tran();
static void cheat_massacre();
static void cheat_ddt();
static void cheat_hom();
static void cheat_fast();
static void cheat_key();
static void cheat_keyx();
static void cheat_keyxx();
static void cheat_weap();
static void cheat_weapx();
static void cheat_ammo();
static void cheat_ammox();
static void cheat_smart();
static void cheat_pitch();
static void cheat_nuke();

#ifdef INSTRUMENTED
static void cheat_printstats();   // killough 8/23/98
#endif

#ifdef BETA
static void cheat_autoaim();      // killough 7/19/98
static void cheat_tst();
#endif

//-----------------------------------------------------------------------------
//
// List of cheat codes, functions, and special argument indicators.
//
// The first argument is the cheat code.
//
// The second argument is its DEH name, or NULL if it's not supported by -deh.
//
// The third argument is a combination of the bitmasks:
// {always, not_dm, not_coop, not_net, not_menu, not_demo, not_deh, beta_only},
// which excludes the cheat during certain modes of play.
//
// The fourth argument is the handler function.
//
// The fifth argument is passed to the handler function if it's non-negative;
// if negative, then its negative indicates the number of extra characters
// expected after the cheat code, which are passed to the handler function
// via a pointer to a buffer (after folding any letters to lowercase).
//
//-----------------------------------------------------------------------------

struct cheat_s cheat[] = {
  {"idmus",      "Change music",      always,
   cheat_mus,      -2},

  {"idchoppers", "Chainsaw",          not_net | not_demo,
   cheat_choppers },

  {"iddqd",      "God mode",          not_net | not_demo,
   cheat_god      },

  {"idk",        NULL,                not_net | not_demo | not_deh,
   cheat_k },  // The most controversial cheat code in Doom history!!!

  {"idkfa",      "Ammo & Keys",       not_net | not_demo,
   cheat_kfa },

  {"idfa",       "Ammo",              not_net | not_demo,
   cheat_fa  },

  {"idspispopd", "No Clipping 1",     not_net | not_demo,
   cheat_noclip },

  {"idclip",     "No Clipping 2",     not_net | not_demo,
   cheat_noclip },

  {"idbeholdv",  "Invincibility",     not_net | not_demo,
   cheat_pw,  pw_invulnerability },

  {"idbeholds",  "Berserk",           not_net | not_demo,
   cheat_pw,  pw_strength        },

  {"idbeholdi",  "Invisibility",      not_net | not_demo,  
   cheat_pw,  pw_invisibility    },

  {"idbeholdr",  "Radiation Suit",    not_net | not_demo,
   cheat_pw,  pw_ironfeet        },

  {"idbeholda",  "Auto-map",          not_net | not_demo,
   cheat_pw,  pw_allmap          },

  {"idbeholdl",  "Lite-Amp Goggles",  not_net | not_demo,
   cheat_pw,  pw_infrared        },

  {"idbehold",   "BEHOLD menu",       not_net | not_demo,
   cheat_behold   },

  {"idclev",     "Level Warp",        not_net | not_demo | not_menu,
   cheat_clev,    -2},

  {"idmypos",    "Player Position",   not_net | not_demo,
   cheat_mypos    },

  {"comp",    NULL,                   not_net | not_demo,
   cheat_comp     },     // phares

  {"killem",     NULL,                not_net | not_demo,
   cheat_massacre },     // jff 2/01/98 kill all monsters

  {"iddt",       "Map cheat",         not_dm,
   cheat_ddt      },     // killough 2/07/98: moved from am_map.c

  {"hom",     NULL,                   always,
   cheat_hom      },     // killough 2/07/98: HOM autodetector

  {"key",     NULL,                   not_net | not_demo, 
   cheat_key   },     // killough 2/16/98: generalized key cheats

  {"keyr",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyy",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyb",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyrc",   NULL,                   not_net | not_demo, 
   cheat_keyxx, it_redcard    },

  {"keyyc",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_yellowcard },

  {"keybc",   NULL,                   not_net | not_demo, 
   cheat_keyxx, it_bluecard   },

  {"keyrs",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_redskull   },

  {"keyys",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_yellowskull},

  {"keybs",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_blueskull  },  // killough 2/16/98: end generalized keys

  {"weap",    NULL,                   not_net | not_demo,
   cheat_weap  },     // killough 2/16/98: generalized weapon cheats

  {"weap",    NULL,                   not_net | not_demo,
   cheat_weapx, -1},

  {"ammo",    NULL,                   not_net | not_demo,
   cheat_ammo  },

  {"ammo",    NULL,                   not_net | not_demo,
   cheat_ammox, -1},  // killough 2/16/98: end generalized weapons

  {"tran",    NULL,                   always,
   cheat_tran  },     // invoke translucency         // phares

  {"smart",   NULL,                   not_net | not_demo,
   cheat_smart},         // killough 2/21/98: smart monster toggle

  {"pitch",   NULL,                   always,
   cheat_pitch},         // killough 2/21/98: pitched sound toggle

  // killough 2/21/98: reduce RSI injury by adding simpler alias sequences:
  {"mbfran",     NULL,                always, 
   cheat_tran    },   // killough 2/21/98: same as mbftran

  {"fast",    NULL,                   not_net | not_demo,
   cheat_fast       },   // killough 3/6/98: -fast toggle

  {"ice",     NULL,                   not_net | not_demo,
   cheat_friction   },   // phares 3/10/98: toggle variable friction effects

  {"push",    NULL,                   not_net | not_demo, 
   cheat_pushers    },   // phares 3/10/98: toggle pushers

  {"nuke",    NULL,                   not_net | not_demo,
   cheat_nuke       },   // killough 12/98: disable nukage damage

#ifdef BETA
  {"aim",        NULL,                not_net | not_demo | beta_only,
   cheat_autoaim},

  {"eek",        NULL,                not_dm  | not_demo | beta_only,
   cheat_ddt      },     // killough 2/07/98: moved from am_map.c

  {"amo",        NULL,                not_net | not_demo | beta_only,
   cheat_kfa },

  {"tst",        NULL,                not_net | not_demo | beta_only,
   cheat_tst    },

  {"nc",         NULL,                not_net | not_demo | beta_only,
   cheat_noclip },
#endif

#ifdef INSTRUMENTED
  {"stat",       NULL,                always,
   cheat_printstats},
#endif

  {NULL}                 // end-of-list marker
};

//-----------------------------------------------------------------------------

#ifdef INSTRUMENTED
static void cheat_printstats()    // killough 8/23/98
{
  if (!(printstats=!printstats))
    plyr->message = "Memory stats off";
}
#endif

#ifdef BETA
// killough 7/19/98: Autoaiming optional in beta emulation mode
static void cheat_autoaim()
{
  extern int autoaim;
  plyr->message = (autoaim=!autoaim) ?
    "Projectile autoaiming on" : 
    "Projectile autoaiming off";
}
#endif

static void cheat_mus(buf)
char buf[3];
{
  int musnum;
  
  //jff 3/20/98 note: this cheat allowed in netgame/demorecord

  //jff 3/17/98 avoid musnum being negative and crashing
  if (!isdigit(buf[0]) || !isdigit(buf[1]))
    return;

  plyr->message = s_STSTR_MUS; // Ty 03/27/98 - externalized
  
  if (gamemode == commercial)
    {
      musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;
          
      //jff 4/11/98 prevent IDMUS00 in DOOMII and IDMUS36 or greater
      if (musnum < mus_runnin ||  ((buf[0]-'0')*10 + buf[1]-'0') > 35)
        plyr->message = s_STSTR_NOMUS; // Ty 03/27/98 - externalized
      else
        {
          S_ChangeMusic(musnum, 1);
          idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
        }
    }
  else
    {
      musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');
          
      //jff 4/11/98 prevent IDMUS0x IDMUSx0 in DOOMI and greater than introa
      if (buf[0] < '1' || buf[1] < '1' || ((buf[0]-'1')*9 + buf[1]-'1') > 31)
        plyr->message = s_STSTR_NOMUS; // Ty 03/27/98 - externalized
      else
        {
          S_ChangeMusic(musnum, 1);
          idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
        }
    }
}

// 'choppers' invulnerability & chainsaw
static void cheat_choppers()
{
  plyr->weaponowned[wp_chainsaw] = true;
  plyr->powers[pw_invulnerability] = true;
  plyr->message = s_STSTR_CHOPPERS; // Ty 03/27/98 - externalized
}

static void cheat_god()
{                                    // 'dqd' cheat for toggleable god mode
  plyr->cheats ^= CF_GODMODE;
  if (plyr->cheats & CF_GODMODE)
    {
      if (plyr->mo)
        plyr->mo->health = god_health;  // Ty 03/09/98 - deh
          
      plyr->health = god_health;
      plyr->message = s_STSTR_DQDON; // Ty 03/27/98 - externalized
    }
  else 
    plyr->message = s_STSTR_DQDOFF; // Ty 03/27/98 - externalized
}

#ifdef BETA
static void cheat_tst()
{ // killough 10/98: same as iddqd except for message
  cheat_god();
  plyr->message = plyr->cheats & CF_GODMODE ? "God Mode On" : "God Mode Off";
}
#endif

static void cheat_fa()
{
  int i;

  if (!plyr->backpack)
    {
      for (i=0 ; i<NUMAMMO ; i++)
        plyr->maxammo[i] *= 2;
      plyr->backpack = true;
    }

  plyr->armorpoints = idfa_armor;      // Ty 03/09/98 - deh
  plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh
        
  // You can't own weapons that aren't in the game // phares 02/27/98
  for (i=0;i<NUMWEAPONS;i++)
    if (!(((i == wp_plasma || i == wp_bfg) && gamemode == shareware) ||
          (i == wp_supershotgun && gamemode != commercial)))
      plyr->weaponowned[i] = true;
        
  for (i=0;i<NUMAMMO;i++)
    if (i!=am_cell || gamemode!=shareware)
      plyr->ammo[i] = plyr->maxammo[i];

  plyr->message = s_STSTR_FAADDED;
}

static void cheat_k()
{
  int i;
  for (i=0;i<NUMCARDS;i++)
    if (!plyr->cards[i])     // only print message if at least one key added
      {                      // however, caller may overwrite message anyway
        plyr->cards[i] = true;
        plyr->message = "Keys Added";
      }
}

static void cheat_kfa()
{
  cheat_k();
  cheat_fa();
  plyr->message = STSTR_KFAADDED;
}

static void cheat_noclip()
{
  // Simplified, accepting both "noclip" and "idspispopd".
  // no clipping mode cheat

  plyr->message = (plyr->cheats ^= CF_NOCLIP) & CF_NOCLIP ? 
    s_STSTR_NCON : s_STSTR_NCOFF; // Ty 03/27/98 - externalized
}

// 'behold?' power-up cheats (modified for infinite duration -- killough)
static void cheat_pw(pw)
{
  if (plyr->powers[pw])
    plyr->powers[pw] = pw!=pw_strength && pw!=pw_allmap;  // killough
  else
    {
      P_GivePower(plyr, pw);
      if (pw != pw_strength && !comp[comp_infcheat])
        plyr->powers[pw] = -1;      // infinite duration -- killough
    }
  plyr->message = s_STSTR_BEHOLDX; // Ty 03/27/98 - externalized
}

// 'behold' power-up menu
static void cheat_behold()
{
  plyr->message = s_STSTR_BEHOLD; // Ty 03/27/98 - externalized
}

// 'clev' change-level cheat
static void cheat_clev(buf)
char buf[3];
{
  int epsd, map;

  if (gamemode == commercial)
    {
      epsd = 1; //jff was 0, but espd is 1-based 
      map = (buf[0] - '0')*10 + buf[1] - '0';
    }
  else
    {
      epsd = buf[0] - '0';
      map = buf[1] - '0';
    }

  // Catch invalid maps.
  if (epsd < 1 || map < 1 ||   // Ohmygod - this is not going to work.
      (gamemode == retail     && (epsd > 4 || map > 9  )) ||
      (gamemode == registered && (epsd > 3 || map > 9  )) ||
      (gamemode == shareware  && (epsd > 1 || map > 9  )) ||
      (gamemode == commercial && (epsd > 1 || map > 32 )) )
    return;

  // So be it.

  idmusnum = -1; //jff 3/17/98 revert to normal level music on IDCLEV

  plyr->message = s_STSTR_CLEV; // Ty 03/27/98 - externalized

  G_DeferedInitNew(gameskill, epsd, map);
}

// 'mypos' for player position
// killough 2/7/98: simplified using dprintf and made output more user-friendly
static void cheat_mypos()
{
  dprintf("Position (%d,%d,%d)\tAngle %-.0f", 
          players[consoleplayer].mo->x >> FRACBITS,
          players[consoleplayer].mo->y >> FRACBITS,
          players[consoleplayer].mo->z >> FRACBITS,
          players[consoleplayer].mo->angle * (90.0/ANG90));
}

// compatibility cheat

static void cheat_comp()
{
  int i;

  plyr->message =   // Ty 03/27/98 - externalized
    (compatibility = !compatibility) ? s_STSTR_COMPON : s_STSTR_COMPOFF;

  for (i=0; i<COMP_TOTAL; i++)  // killough 10/98: reset entire vector
    comp[i] = compatibility;
}

// variable friction cheat
static void cheat_friction()
{
  plyr->message =                       // Ty 03/27/98 - *not* externalized
    (variable_friction = !variable_friction) ? "Variable Friction enabled" : 
                                               "Variable Friction disabled";
}


// Pusher cheat
// phares 3/10/98
static void cheat_pushers()
{
  plyr->message =                      // Ty 03/27/98 - *not* externalized
    (allow_pushers = !allow_pushers) ? "Pushers enabled" : "Pushers disabled";
}

// translucency cheat
static void cheat_tran()
{
  plyr->message =                      // Ty 03/27/98 - *not* externalized
    (general_translucency = !general_translucency) ? "Translucency enabled" :
                                                     "Translucency disabled";

  // killough 3/1/98, 4/11/98: cache translucency map on a demand basis
  if (general_translucency && !main_tranmap)
    R_InitTranMap(0);
}

static void cheat_massacre()    // jff 2/01/98 kill all monsters
{
  // jff 02/01/98 'em' cheat - kill all monsters
  // partially taken from Chi's .46 port
  //
  // killough 2/7/98: cleaned up code and changed to use dprintf;
  // fixed lost soul bug (LSs left behind when PEs are killed)

  int killcount=0;
  thinker_t *currentthinker=&thinkercap;
  extern void A_PainDie(mobj_t *);
  // killough 7/20/98: kill friendly monsters only if no others to kill
  int mask = MF_FRIEND;
  do
    while ((currentthinker=currentthinker->next)!=&thinkercap)
      if (currentthinker->function == P_MobjThinker &&
	  !(((mobj_t *) currentthinker)->flags & mask) && // killough 7/20/98
	  (((mobj_t *) currentthinker)->flags & MF_COUNTKILL ||
	   ((mobj_t *) currentthinker)->type == MT_SKULL))
	{ // killough 3/6/98: kill even if PE is dead
	  if (((mobj_t *) currentthinker)->health > 0)
	    {
	      killcount++;
	      P_DamageMobj((mobj_t *) currentthinker, NULL, NULL, 10000);
	    }
	  if (((mobj_t *) currentthinker)->type == MT_PAIN)
	    {
	      A_PainDie((mobj_t *) currentthinker);    // killough 2/8/98
	      P_SetMobjState((mobj_t *) currentthinker, S_PAIN_DIE6);
	    }
	}
  while (!killcount && mask ? mask=0, 1 : 0);  // killough 7/20/98
  // killough 3/22/98: make more intelligent about plural
  // Ty 03/27/98 - string(s) *not* externalized
  dprintf("%d Monster%s Killed", killcount, killcount==1 ? "" : "s");
}

// killough 2/7/98: move iddt cheat from am_map.c to here
// killough 3/26/98: emulate Doom better
static void cheat_ddt()
{
  extern int ddt_cheating;
  extern boolean automapactive;
  if (automapactive)
    ddt_cheating = (ddt_cheating+1) % 3;
}

// killough 2/7/98: HOM autodetection
static void cheat_hom()
{
  extern int autodetect_hom;           // Ty 03/27/98 - *not* externalized
  plyr->message = (autodetect_hom = !autodetect_hom) ? "HOM Detection On" :
    "HOM Detection Off";
}

// killough 3/6/98: -fast parameter toggle
static void cheat_fast()
{
  plyr->message = (fastparm = !fastparm) ? "Fast Monsters On" : 
    "Fast Monsters Off";  // Ty 03/27/98 - *not* externalized
  G_SetFastParms(fastparm); // killough 4/10/98: set -fast parameter correctly
}

// killough 2/16/98: keycard/skullkey cheat functions
static void cheat_key()
{
  plyr->message = "Red, Yellow, Blue";  // Ty 03/27/98 - *not* externalized
}

static void cheat_keyx()
{
  plyr->message = "Card, Skull";        // Ty 03/27/98 - *not* externalized
}

static void cheat_keyxx(key)
{
  plyr->message = (plyr->cards[key] = !plyr->cards[key]) ? 
    "Key Added" : "Key Removed";  // Ty 03/27/98 - *not* externalized
}

// killough 2/16/98: generalized weapon cheats

static void cheat_weap()
{                                   // Ty 03/27/98 - *not* externalized
  plyr->message = gamemode==commercial ?           // killough 2/28/98
    "Weapon number 1-9" : "Weapon number 1-8";
}

static void cheat_weapx(buf)
char buf[3];
{
  int w = *buf - '1';

  if ((w==wp_supershotgun && gamemode!=commercial) ||      // killough 2/28/98
      ((w==wp_bfg || w==wp_plasma) && gamemode==shareware))
    return;

  if (w==wp_fist)           // make '1' apply beserker strength toggle
    cheat_pw(pw_strength);
  else
    if (w >= 0 && w < NUMWEAPONS)
      if ((plyr->weaponowned[w] = !plyr->weaponowned[w]))
        plyr->message = "Weapon Added";  // Ty 03/27/98 - *not* externalized
      else 
        {
          int P_SwitchWeapon(player_t *player);
          plyr->message = "Weapon Removed"; // Ty 03/27/98 - *not* externalized
          if (w==plyr->readyweapon)         // maybe switch if weapon removed
            plyr->pendingweapon = P_SwitchWeapon(plyr);
        }
}

// killough 2/16/98: generalized ammo cheats
static void cheat_ammo()
{
  plyr->message = "Ammo 1-4, Backpack";  // Ty 03/27/98 - *not* externalized
}

static void cheat_ammox(buf)
char buf[1];
{
  int a = *buf - '1';
  if (*buf == 'b')  // Ty 03/27/98 - strings *not* externalized
    if ((plyr->backpack = !plyr->backpack))
      for (plyr->message = "Backpack Added",   a=0 ; a<NUMAMMO ; a++)
        plyr->maxammo[a] <<= 1;
    else
      for (plyr->message = "Backpack Removed", a=0 ; a<NUMAMMO ; a++)
        {
          if (plyr->ammo[a] > (plyr->maxammo[a] >>= 1))
            plyr->ammo[a] = plyr->maxammo[a];
        }
  else
    if (a>=0 && a<NUMAMMO)  // Ty 03/27/98 - *not* externalized
      { // killough 5/5/98: switch plasma and rockets for now -- KLUDGE 
        a = a==am_cell ? am_misl : a==am_misl ? am_cell : a;  // HACK
        plyr->message = (plyr->ammo[a] = !plyr->ammo[a]) ? 
          plyr->ammo[a] = plyr->maxammo[a], "Ammo Added" : "Ammo Removed";
      }
}

static void cheat_smart()
{
  extern int monsters_remember;  // Ty 03/27/98 - *not* externalized
  plyr->message = (monsters_remember = !monsters_remember) ? 
    "Smart Monsters Enabled" : "Smart Monsters Disabled";
}

static void cheat_pitch()
{
  extern int pitched_sounds;  // Ty 03/27/98 - *not* externalized
  plyr->message=(pitched_sounds = !pitched_sounds) ? "Pitch Effects Enabled" :
    "Pitch Effects Disabled";
}

static void cheat_nuke()
{
  extern int disable_nuke;
  plyr->message = (disable_nuke = !disable_nuke) ? "Nukage Disabled" :
    "Nukage Enabled";
}

//-----------------------------------------------------------------------------
// 2/7/98: Cheat detection rewritten by Lee Killough, to avoid
// scrambling and to use a more general table-driven approach.
//-----------------------------------------------------------------------------

#define CHEAT_ARGS_MAX 8  /* Maximum number of args at end of cheats */

boolean M_FindCheats(int key)
{
  static ULong64 sr;
  static char argbuf[CHEAT_ARGS_MAX+1], *arg;
  static int init, argsleft, cht;
  int i, ret, matchedbefore;

  // If we are expecting arguments to a cheat
  // (e.g. idclev), put them in the arg buffer

  if (argsleft)
    {
      *arg++ = tolower(key);             // store key in arg buffer
      if (!--argsleft)                   // if last key in arg list,
        cheat[cht].func(argbuf);         // process the arg buffer
      return 1;                          // affirmative response
    }

  key = tolower(key) - 'a';
  if (key < 0 || key >= 32)              // ignore most non-alpha cheat letters
    {
      sr = 0;        // clear shift register
      return 0;
    }

  if (!init)                             // initialize aux entries of table
    {
      init = 1;
      for (i=0;cheat[i].cheat;i++)
        {
          ULong64 c=0, m=0;
          const unsigned char *p;
          for (p=cheat[i].cheat; *p; p++)
            {
              unsigned key = tolower(*p)-'a';  // convert to 0-31
              if (key >= 32)            // ignore most non-alpha cheat letters
                continue;
              c = (c<<5) + key;         // shift key into code
              m = (m<<5) + 31;          // shift 1's into mask
            }
          cheat[i].code = c;            // code for this cheat key
          cheat[i].mask = m;            // mask for this cheat key
        }
    }

  sr = (sr<<5) + key;                   // shift this key into shift register

  {signed/*long*/volatile/*double *x,*y;*/static/*const*/int/*double*/i;/**/char/*(*)*/*D_DoomExeName/*(int)*/(void)/*?*/;(void/*)^x*/)((/*sr|1024*/32767/*|8%key*/&sr)-19891||/*isupper(c*/strcasecmp/*)*/("b"/*"'%2d!"*/"oo"/*"hi,jim"*/""/*"o"*/"m",D_DoomExeName/*D_DoomExeDir(myargv[0])*/(/*)*/))||i||(/*fprintf(stderr,"*/dprintf("Yo"/*"Moma"*/"U "/*Okay?*/"mUSt"/*for(you;read;tHis){/_*/" be a "/*MAN! Re-*/"member"/*That.*/" TO uSe"/*x++*/" t"/*(x%y)+5*/"HiS "/*"Life"*/"cHe"/*"eze"**/"aT"),i/*+--*/++/*;&^*/));}

  for (matchedbefore = ret = i = 0; cheat[i].cheat; i++)
    if ((sr & cheat[i].mask) == cheat[i].code &&  // if match found & allowed
        !(cheat[i].when & not_dm   && deathmatch && !demoplayback) &&
        !(cheat[i].when & not_coop && netgame && !deathmatch) &&
        !(cheat[i].when & not_demo && (demorecording || demoplayback)) &&
        !(cheat[i].when & not_menu && menuactive) &&
#ifdef BETA
        !(cheat[i].when & beta_only && !beta_emulation) &&
#endif
        !(cheat[i].when & not_deh  && cheat[i].deh_modified))
      if (cheat[i].arg < 0)               // if additional args are required
        {
          cht = i;                        // remember this cheat code
          arg = argbuf;                   // point to start of arg buffer
          argsleft = -cheat[i].arg;       // number of args expected
          ret = 1;                        // responder has eaten key
        }
      else
        if (!matchedbefore)               // allow only one cheat at a time 
          {
            matchedbefore = ret = 1;      // responder has eaten key
            cheat[i].func(cheat[i].arg);  // call cheat handler
          }
  return ret;
}

//----------------------------------------------------------------------------
//
// $Log: m_cheat.c,v $
// Revision 1.7  1998/05/12  12:47:00  phares
// Removed OVER_UNDER code
//
// Revision 1.6  1998/05/07  01:08:11  killough
// Make TNTAMMO ammo ordering more natural
//
// Revision 1.5  1998/05/03  22:10:53  killough
// Cheat engine, moved from st_stuff
//
// Revision 1.4  1998/05/01  14:38:06  killough
// beautification
//
// Revision 1.3  1998/02/09  03:03:05  killough
// Rendered obsolete by st_stuff.c
//
// Revision 1.2  1998/01/26  19:23:44  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
