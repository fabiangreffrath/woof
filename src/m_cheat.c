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
// DESCRIPTION:
//      Cheat sequence checking.
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "d_deh.h" // Ty 03/27/98 - externalized strings
#include "d_event.h"
#include "d_player.h"
#include "d_think.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "info.h"
#include "m_cheat.h"
#include "m_fixed.h"
#include "m_input.h"
#include "m_misc2.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_spec.h" // SPECHITS
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "tables.h"
#include "u_mapinfo.h"
#include "w_wad.h"

#define plyr (players+consoleplayer)     /* the console player */

//-----------------------------------------------------------------------------
//
// CHEAT SEQUENCE PACKAGE
//
//-----------------------------------------------------------------------------

static void cheat_mus(char *buf);
static void cheat_choppers();
static void cheat_god();
static void cheat_fa();
static void cheat_k();
static void cheat_kfa();
static void cheat_noclip();
static void cheat_pw(int pw);
static void cheat_behold();
static void cheat_clev(char *buf);
static void cheat_clev0();
static void cheat_mypos();
static void cheat_comp(char *buf);
static void cheat_comp0();
static void cheat_skill(char *buf);
static void cheat_skill0();
static void cheat_friction();
static void cheat_pushers();
static void cheat_tran();
static void cheat_massacre();
static void cheat_ddt();
static void cheat_hom();
static void cheat_fast();
static void cheat_key();
static void cheat_keyx();
static void cheat_keyxx(int key);
static void cheat_weap();
static void cheat_weapx(char *buf);
static void cheat_ammo();
static void cheat_ammox(char *buf);
static void cheat_smart();
static void cheat_pitch();
static void cheat_nuke();
static void cheat_rate();
static void cheat_buddha();
static void cheat_spechits();
static void cheat_notarget();
static void cheat_freeze();
static void cheat_health();
static void cheat_megaarmour();
static void cheat_reveal_secret();
static void cheat_reveal_kill();
static void cheat_reveal_item();

static void cheat_autoaim();      // killough 7/19/98
static void cheat_tst();
static void cheat_showfps(); // [FG] FPS counter widget

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
   {cheat_mus}, -2 },

  {"idchoppers", "Chainsaw",          not_net | not_demo,
   {cheat_choppers} },

  {"iddqd",      "God mode",          not_net | not_demo,
   {cheat_god} },

  {"buddha",     "Buddha mode",       not_net | not_demo,
   {cheat_buddha} },

  {"idk",        NULL,                not_net | not_demo | not_deh,
   {cheat_k} }, // The most controversial cheat code in Doom history!!!

  {"idkfa",      "Ammo & Keys",       not_net | not_demo,
   {cheat_kfa} },

  {"idfa",       "Ammo",              not_net | not_demo,
   {cheat_fa} },

  {"idspispopd", "No Clipping 1",     not_net | not_demo,
   {cheat_noclip} },

  {"idclip",     "No Clipping 2",     not_net | not_demo,
   {cheat_noclip} },

  {"idbeholdo",  NULL,                not_net | not_demo | not_deh,
   {cheat_pw}, NUMPOWERS }, // [FG] disable all powerups at once

  {"idbeholdh",  "Health",            not_net | not_demo,
   {cheat_health} },

  {"idbeholdm",  "Mega Armor",        not_net | not_demo,
   {cheat_megaarmour} },

  {"idbeholdv",  "Invincibility",     not_net | not_demo,
   {cheat_pw}, pw_invulnerability },

  {"idbeholds",  "Berserk",           not_net | not_demo,
   {cheat_pw}, pw_strength },

  {"idbeholdi",  "Invisibility",      not_net | not_demo,  
   {cheat_pw}, pw_invisibility },

  {"idbeholdr",  "Radiation Suit",    not_net | not_demo,
   {cheat_pw}, pw_ironfeet },

  {"idbeholda",  "Auto-map",          not_dm,
   {cheat_pw}, pw_allmap },

  {"idbeholdl",  "Lite-Amp Goggles",  not_dm,
   {cheat_pw}, pw_infrared },

  {"idbehold",   "BEHOLD menu",       not_net | not_demo,
   {cheat_behold} },

  {"idclev",     "Level Warp",        not_net | not_demo | not_menu,
   {cheat_clev}, -2 },

  {"idclev",     "Level Warp",        not_net | not_demo | not_menu,
   {cheat_clev0} },

  {"idmypos",    "Player Position",   not_dm, // [FG] not_net | not_demo,
   {cheat_mypos} },

  {"comp",    NULL,                   not_net | not_demo | not_menu,
   {cheat_comp}, -2 },

  {"comp",    NULL,                   not_net | not_demo | not_menu,
   {cheat_comp0} },

  {"skill",    NULL,                  not_net | not_demo | not_menu,
   {cheat_skill}, -1 },

  {"skill",    NULL,                  not_net | not_demo | not_menu,
   {cheat_skill0} },

  {"killem",     NULL,                not_net | not_demo,
   {cheat_massacre} },   // jff 2/01/98 kill all monsters

  {"spechits",     NULL,              not_net | not_demo,
   {cheat_spechits} },

  {"notarget",   "Notarget mode",     not_net | not_demo,
   {cheat_notarget} },

  {"freeze",     "Freeze",            not_net | not_demo,
   {cheat_freeze} },

  {"iddt",       "Map cheat",         not_dm,
   {cheat_ddt} },        // killough 2/07/98: moved from am_map.c

  {"iddst",      NULL,                always,
   {cheat_reveal_secret} },

  {"iddkt",      NULL,                not_dm,
   {cheat_reveal_kill} },

  {"iddit",      NULL,                not_dm,
   {cheat_reveal_item} },

  {"hom",     NULL,                   always,
   {cheat_hom} },        // killough 2/07/98: HOM autodetector

  {"key",     NULL,                   not_net | not_demo, 
   {cheat_key} },     // killough 2/16/98: generalized key cheats

  {"keyr",    NULL,                   not_net | not_demo,
   {cheat_keyx} },

  {"keyy",    NULL,                   not_net | not_demo,
   {cheat_keyx} },

  {"keyb",    NULL,                   not_net | not_demo,
   {cheat_keyx} },

  {"keyrc",   NULL,                   not_net | not_demo, 
   {cheat_keyxx}, it_redcard },

  {"keyyc",   NULL,                   not_net | not_demo,
   {cheat_keyxx}, it_yellowcard },

  {"keybc",   NULL,                   not_net | not_demo, 
   {cheat_keyxx}, it_bluecard },

  {"keyrs",   NULL,                   not_net | not_demo,
   {cheat_keyxx}, it_redskull },

  {"keyys",   NULL,                   not_net | not_demo,
   {cheat_keyxx}, it_yellowskull },

  {"keybs",   NULL,                   not_net | not_demo,
   {cheat_keyxx}, it_blueskull }, // killough 2/16/98: end generalized keys

  {"weap",    NULL,                   not_net | not_demo,
   {cheat_weap} },    // killough 2/16/98: generalized weapon cheats

  {"weap",    NULL,                   not_net | not_demo,
   {cheat_weapx}, -1 },

  {"ammo",    NULL,                   not_net | not_demo,
   {cheat_ammo} },

  {"ammo",    NULL,                   not_net | not_demo,
   {cheat_ammox}, -1 }, // killough 2/16/98: end generalized weapons

  {"tran",    NULL,                   always,
   {cheat_tran} },    // invoke translucency         // phares

  {"smart",   NULL,                   not_net | not_demo,
   {cheat_smart} },      // killough 2/21/98: smart monster toggle

  {"pitch",   NULL,                   always,
   {cheat_pitch} },      // killough 2/21/98: pitched sound toggle

  // killough 2/21/98: reduce RSI injury by adding simpler alias sequences:
  {"mbfran",     NULL,                always, 
   {cheat_tran} },    // killough 2/21/98: same as mbftran

  {"fast",    NULL,                   not_net | not_demo,
   {cheat_fast} },       // killough 3/6/98: -fast toggle

  {"ice",     NULL,                   not_net | not_demo,
   {cheat_friction} },   // phares 3/10/98: toggle variable friction effects

  {"push",    NULL,                   not_net | not_demo, 
   {cheat_pushers} },    // phares 3/10/98: toggle pushers

  {"nuke",    NULL,                   not_net | not_demo,
   {cheat_nuke} },       // killough 12/98: disable nukage damage

  {"rate",    NULL,                   always,
   {cheat_rate} },

  {"aim",        NULL,                not_net | not_demo | beta_only,
   {cheat_autoaim} },

  {"eek",        NULL,                not_dm  | not_demo | beta_only,
   {cheat_ddt} },        // killough 2/07/98: moved from am_map.c

  {"amo",        NULL,                not_net | not_demo | beta_only,
   {cheat_kfa} },

  {"tst",        NULL,                not_net | not_demo | beta_only,
   {cheat_tst} },

  {"nc",         NULL,                not_net | not_demo | beta_only,
   {cheat_noclip} },

// [FG] FPS counter widget
  {"showfps",    NULL,                always,
   {cheat_showfps} },

  {NULL}                 // end-of-list marker
};

//-----------------------------------------------------------------------------

// [FG] FPS counter widget
static void cheat_showfps()
{
  plyr->cheats ^= CF_SHOWFPS;
}

// killough 7/19/98: Autoaiming optional in beta emulation mode
static void cheat_autoaim()
{
  extern int autoaim;
  displaymsg((autoaim=!autoaim) ?
    "Projectile autoaiming on" : 
    "Projectile autoaiming off");
}

static void cheat_mus(char *buf)
{
  int musnum;
  mapentry_t* entry;
  
  //jff 3/20/98 note: this cheat allowed in netgame/demorecord

  //jff 3/17/98 avoid musnum being negative and crashing
  if (!isdigit(buf[0]) || !isdigit(buf[1]))
    return;

  displaymsg("%s", s_STSTR_MUS); // Ty 03/27/98 - externalized
  
  // First check if we have a mapinfo entry for the requested level.
  if (gamemode == commercial)
    entry = G_LookupMapinfo(1, 10*(buf[0]-'0') + (buf[1]-'0'));
  else
    entry = G_LookupMapinfo(buf[0]-'0', buf[1]-'0');

  if (entry && entry->music[0])
  {
     musnum = W_CheckNumForName(entry->music);

     if (musnum == -1)
        displaymsg("%s", s_STSTR_NOMUS);
     else
     {
        S_ChangeMusInfoMusic(musnum, 1);
        idmusnum = -1;
     }
     return;
  }

  if (gamemode == commercial)
    {
      musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;
          
      //jff 4/11/98 prevent IDMUS00 in DOOMII and IDMUS36 or greater
      if (musnum < mus_runnin || musnum >= NUMMUSIC)
        displaymsg("%s", s_STSTR_NOMUS); // Ty 03/27/98 - externalized
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
      if (musnum < mus_e1m1 || musnum >= mus_runnin)
        displaymsg("%s", s_STSTR_NOMUS); // Ty 03/27/98 - externalized
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
  displaymsg("%s", s_STSTR_CHOPPERS); // Ty 03/27/98 - externalized
}

static void cheat_god()
{                                    // 'dqd' cheat for toggleable god mode
  // [crispy] dead players are first respawned at the current position
  if (plyr->playerstate == PST_DEAD)
  {
    signed int an;
    mapthing_t mt = {0};
    extern void P_SpawnPlayer (mapthing_t* mthing);

    P_MapStart();
    mt.x = plyr->mo->x >> FRACBITS;
    mt.y = plyr->mo->y >> FRACBITS;
    mt.angle = (plyr->mo->angle + ANG45/2)*(uint64_t)45/ANG45;
    mt.type = consoleplayer + 1;
    P_SpawnPlayer(&mt);

    // [crispy] spawn a teleport fog
    an = plyr->mo->angle >> ANGLETOFINESHIFT;
    P_SpawnMobj(plyr->mo->x+20*finecosine[an], plyr->mo->y+20*finesine[an], plyr->mo->z, MT_TFOG);
    S_StartSound(plyr->mo, sfx_slop);
    P_MapEnd();

    // Fix reviving as "zombie" if god mode was already enabled
    if (plyr->mo)
      plyr->mo->health = god_health;  // Ty 03/09/98 - deh
    plyr->health = god_health;
  }

  plyr->cheats ^= CF_GODMODE;
  if (plyr->cheats & CF_GODMODE)
    {
      if (plyr->mo)
        plyr->mo->health = god_health;  // Ty 03/09/98 - deh
          
      plyr->health = god_health;
      displaymsg("%s", s_STSTR_DQDON); // Ty 03/27/98 - externalized
    }
  else 
    displaymsg("%s", s_STSTR_DQDOFF); // Ty 03/27/98 - externalized
}

static void cheat_buddha()
{
  plyr->cheats ^= CF_BUDDHA;
  if (plyr->cheats & CF_BUDDHA)
    displaymsg("Buddha Mode ON");
  else
    displaymsg("Buddha Mode OFF");
}

static void cheat_notarget()
{
  plyr->cheats ^= CF_NOTARGET;
  if (plyr->cheats & CF_NOTARGET)
    displaymsg("Notarget ON");
  else
    displaymsg("Notarget OFF");
}

boolean frozen_mode;

static void cheat_freeze()
{
  frozen_mode = !frozen_mode;
  if (frozen_mode)
    displaymsg("Freeze ON");
  else
    displaymsg("Freeze OFF");
}

static void cheat_avj()
{
  void A_VileJump(mobj_t *mo);

  if (plyr->mo)
    A_VileJump(plyr->mo);
}

// CPhipps - new health and armour cheat codes
static void cheat_health()
{
  if (!(plyr->cheats & CF_GODMODE))
  {
    if (plyr->mo)
      plyr->mo->health = mega_health;
    plyr->health = mega_health;
    displaymsg("%s", s_STSTR_BEHOLDX); // Ty 03/27/98 - externalized
  }
}

static void cheat_megaarmour()
{
  plyr->armorpoints = idfa_armor;      // Ty 03/09/98 - deh
  plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh
  displaymsg("%s", s_STSTR_BEHOLDX); // Ty 03/27/98 - externalized
}

static void cheat_tst()
{ // killough 10/98: same as iddqd except for message
  cheat_god();
  displaymsg(plyr->cheats & CF_GODMODE ? "God Mode On" : "God Mode Off");
}

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
          (i == wp_supershotgun && !have_ssg)))
      plyr->weaponowned[i] = true;
        
  for (i=0;i<NUMAMMO;i++)
    if (i!=am_cell || gamemode!=shareware)
      plyr->ammo[i] = plyr->maxammo[i];

  displaymsg("%s", s_STSTR_FAADDED);
}

static void cheat_k()
{
  int i;
  for (i=0;i<NUMCARDS;i++)
    if (!plyr->cards[i])     // only print message if at least one key added
      {                      // however, caller may overwrite message anyway
        plyr->cards[i] = true;
        displaymsg("Keys Added");
      }
}

static void cheat_kfa()
{
  cheat_k();
  cheat_fa();
  displaymsg("%s", s_STSTR_KFAADDED);
}

static void cheat_noclip()
{
  // Simplified, accepting both "noclip" and "idspispopd".
  // no clipping mode cheat

  displaymsg("%s", (plyr->cheats ^= CF_NOCLIP) & CF_NOCLIP ? 
    s_STSTR_NCON : s_STSTR_NCOFF); // Ty 03/27/98 - externalized
}

// 'behold?' power-up cheats (modified for infinite duration -- killough)
static void cheat_pw(int pw)
{
  if (pw == NUMPOWERS)
  {
    memset(plyr->powers, 0, sizeof(plyr->powers));
    plyr->mo->flags &= ~MF_SHADOW; // [crispy] cancel invisibility
  }
  else
  if (plyr->powers[pw])
    plyr->powers[pw] = pw!=pw_strength && pw!=pw_allmap;  // killough
  else
    {
      P_GivePower(plyr, pw);
      if (pw != pw_strength && !comp[comp_infcheat])
        plyr->powers[pw] = -1;      // infinite duration -- killough
    }
  displaymsg("%s", s_STSTR_BEHOLDX); // Ty 03/27/98 - externalized
}

// 'behold' power-up menu
static void cheat_behold()
{
  displaymsg("%s", s_STSTR_BEHOLD); // Ty 03/27/98 - externalized
}

// 'clev' change-level cheat
static void cheat_clev0()
{
  int epsd, map;
  char *cur, *next;
  extern int G_GotoNextLevel(int *e, int *m);

  cur = M_StringDuplicate(MAPNAME(gameepisode, gamemap));

  G_GotoNextLevel(&epsd, &map);
  next = MAPNAME(epsd, map);

  if (W_CheckNumForName(next) != -1)
    displaymsg("Current: %s, Next: %s", cur, next);
  else
    displaymsg("Current: %s", cur);

  free(cur);
}

static void cheat_clev(char *buf)
{
  int epsd, map;
  mapentry_t* entry;

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

  // catch non-numerical input
  if (epsd < 0 || epsd > 9 || map < 0 || map > 99)
    return;

  // First check if we have a mapinfo entry for the requested level.
  // If this is present the remaining checks should be skipped.
  entry = G_LookupMapinfo(epsd, map);
  if (!entry)
  {
    char *next;

    // Chex.exe always warps to episode 1.
    if (gameversion == exe_chex)
    {
      epsd = 1;
    }

    next = MAPNAME(epsd, map);

    if (W_CheckNumForName(next) == -1)
    {
      // [Alaux] Restart map with IDCLEV00
      if ((epsd == 0 && map == 0) || (gamemode == commercial && map == 0))
      {
        epsd = gameepisode;
        map = gamemap;
      }
      else
      {
        displaymsg("IDCLEV target not found: %s", next);
        return;
      }
    }
  }

  // So be it.

  idmusnum = -1; //jff 3/17/98 revert to normal level music on IDCLEV

  displaymsg("%s", s_STSTR_CLEV); // Ty 03/27/98 - externalized

  G_DeferedInitNew(gameskill, epsd, map);
}

// 'mypos' for player position
// killough 2/7/98: simplified using dprintf and made output more user-friendly
static void cheat_mypos()
{
  plyr->cheats ^= CF_MAPCOORDS;
  if ((plyr->cheats & CF_MAPCOORDS) == 0)
    plyr->message = "";
}

void cheat_mypos_print()
{
  displaymsg("X=%.10f Y=%.10f A=%-.0f",
          (double)players[consoleplayer].mo->x / FRACUNIT,
          (double)players[consoleplayer].mo->y / FRACUNIT,
          players[consoleplayer].mo->angle * (90.0/ANG90));
}

// compatibility cheat

static void cheat_comp0()
{
  displaymsg("Complevel: %s", G_GetCurrentComplevelName());
}

static void cheat_comp(char *buf)
{
  int new_demover;

  buf[2] = '\0';

  if (buf[0] == '0')
    buf++;

  new_demover = G_GetNamedComplevel(buf);

  if (new_demover != -1)
  {
    demo_version = new_demover;
    G_ReloadDefaults(true);
    displaymsg("New Complevel: %s", G_GetCurrentComplevelName());
  }
}

// variable friction cheat
static void cheat_friction()
{
  displaymsg(            // Ty 03/27/98 - *not* externalized
    (variable_friction = !variable_friction) ? "Variable Friction enabled" : 
                                               "Variable Friction disabled");
}

extern const char *default_skill_strings[];

static void cheat_skill0()
{
  displaymsg("Skill: %s", default_skill_strings[gameskill + 1]);
}

static void cheat_skill(char *buf)
{
  int skill = buf[0] - '0';

  if (skill >= 1 && skill <= 5)
  {
    gameskill = skill - 1;
    displaymsg("Next Level Skill: %s", default_skill_strings[gameskill + 1]);

    G_SetFastParms(fastparm || gameskill == sk_nightmare);
    respawnmonsters = gameskill == sk_nightmare || respawnparm;
  }
}

// Pusher cheat
// phares 3/10/98
static void cheat_pushers()
{
  displaymsg(           // Ty 03/27/98 - *not* externalized
    (allow_pushers = !allow_pushers) ? "Pushers enabled" : "Pushers disabled");
}

// translucency cheat
static void cheat_tran()
{
  displaymsg(            // Ty 03/27/98 - *not* externalized
    (translucency = !translucency) ? "Translucency enabled" : "Translucency disabled");
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
  P_MapStart();
  do
    while ((currentthinker=currentthinker->next)!=&thinkercap)
      if (currentthinker->function.p1 == (actionf_p1)P_MobjThinker &&
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
  P_MapEnd();
  // killough 3/22/98: make more intelligent about plural
  // Ty 03/27/98 - string(s) *not* externalized
  displaymsg("%d Monster%s Killed", killcount, killcount==1 ? "" : "s");
}

static void cheat_spechits()
{
  int i, speciallines = 0;
  boolean origcards[NUMCARDS];
  line_t dummy;
  boolean trigger_keen = true;

  // [crispy] temporarily give all keys
  for (i = 0; i < NUMCARDS; i++)
  {
    origcards[i] = plyr->cards[i];
    plyr->cards[i] = true;
  }

  for (i = 0; i < numlines; i++)
  {
    if (lines[i].special)
    {
      switch (lines[i].special)
        // [crispy] do not trigger level exit switches/lines
        case 11:
        case 51:
        case 52:
        case 124:
        case 197:
        case 198:
        // [crispy] do not trigger teleporters switches/lines
        case 39:
        case 97:
        case 125:
        case 126:
        case 174:
        case 195:
        {
          continue;
        }

      // [crispy] special without tag --> DR linedef type
      // do not change door direction if it is already moving
      if (lines[i].tag == 0 &&
          lines[i].sidenum[1] != NO_INDEX &&
         (sides[lines[i].sidenum[1]].sector->floordata ||
          sides[lines[i].sidenum[1]].sector->ceilingdata))
      {
        continue;
      }

      P_CrossSpecialLine(&lines[i], 0, plyr->mo, false);
      P_ShootSpecialLine(plyr->mo, &lines[i]);
      P_UseSpecialLine(plyr->mo, &lines[i], 0, false);

      speciallines++;
    }
  }

  for (i = 0; i < NUMCARDS; i++)
  {
    plyr->cards[i] = origcards[i];
  }

  if (gamemapinfo && gamemapinfo->numbossactions > 0)
  {
    thinker_t *th;

    for (th = thinkercap.next ; th != &thinkercap ; th = th->next)
    {
      if (th->function.p1 == (actionf_p1)P_MobjThinker)
      {
        mobj_t *mo = (mobj_t *) th;

        for (i = 0; i < gamemapinfo->numbossactions; i++)
        {
          if (gamemapinfo->bossactions[i].type == mo->type)
          {
            dummy = *lines;
            dummy.special = (short)gamemapinfo->bossactions[i].special;
            dummy.tag = (short)gamemapinfo->bossactions[i].tag;
            // use special semantics for line activation to block problem types.
            if (!P_UseSpecialLine(mo, &dummy, 0, true))
              P_CrossSpecialLine(&dummy, 0, mo, true);

            speciallines++;

            if (dummy.tag == 666)
              trigger_keen = false;
          }
        }
      }
    }
  }
  else
  {
    // [crispy] trigger tag 666/667 events
    if (gamemode == commercial)
    {
      if (gamemap == 7)
      {
        // Mancubi
        dummy.tag = 666;
        speciallines += EV_DoFloor(&dummy, lowerFloorToLowest);
        trigger_keen = false;

        // Arachnotrons
        dummy.tag = 667;
        speciallines += EV_DoFloor(&dummy, raiseToTexture);
      }
    }
    else
    {
      if (gameepisode == 1)
      {
        // Barons of Hell
        dummy.tag = 666;
        speciallines += EV_DoFloor(&dummy, lowerFloorToLowest);
        trigger_keen = false;
      }
      else if (gameepisode == 4)
      {
        if (gamemap == 6)
        {
          // Cyberdemons
          dummy.tag = 666;
          speciallines += EV_DoDoor(&dummy, blazeOpen);
          trigger_keen = false;
        }
        else if (gamemap == 8)
        {
          // Spider Masterminds
          dummy.tag = 666;
          speciallines += EV_DoFloor(&dummy, lowerFloorToLowest);
          trigger_keen = false;
        }
      }
    }
  }

  // Keens (no matter which level they are on)
  if (trigger_keen)
  {
    dummy.tag = 666;
    speciallines += EV_DoDoor(&dummy, doorOpen);
  }

  displaymsg("%d Special Action%s Triggered", speciallines, speciallines == 1 ? "" : "s");
}

// killough 2/7/98: move iddt cheat from am_map.c to here
// killough 3/26/98: emulate Doom better
static void cheat_ddt()
{
  extern int ddt_cheating;
  if (automapactive)
    ddt_cheating = (ddt_cheating+1) % 3;
}

static void cheat_reveal_secret()
{
  static int last_secret = -1;

  if (automapactive)
  {
    int i, start_i;

    i = last_secret + 1;
    if (i >= numsectors)
      i = 0;
    start_i = i;

    do
    {
      sector_t *sec = &sectors[i];

      if (P_IsSecret(sec))
      {
        followplayer = false;

        // This is probably not necessary
        if (sec->lines && sec->lines[0] && sec->lines[0]->v1)
        {
          AM_SetMapCenter(sec->lines[0]->v1->x, sec->lines[0]->v1->y);
          last_secret = i;
          break;
        }
      }

      i++;
      if (i >= numsectors)
        i = 0;
    } while (i != start_i);
  }
}

static void cheat_cycle_mobj(mobj_t **last_mobj, int *last_count,
                             int flags, int alive)
{
  extern int init_thinkers_count;
  thinker_t *th, *start_th;

  // If the thinkers have been wiped, addresses are invalid
  if (*last_count != init_thinkers_count)
  {
    *last_count = init_thinkers_count;
    *last_mobj = NULL;
  }

  if (*last_mobj)
    th = &(*last_mobj)->thinker;
  else
    th = &thinkercap;

  start_th = th;

  do
  {
    th = th->next;
    if (th->function.p1 == (actionf_p1)P_MobjThinker)
    {
      mobj_t *mobj;

      mobj = (mobj_t *) th;

      if ((!alive || mobj->health > 0) && mobj->flags & flags)
      {
        followplayer = false;
        AM_SetMapCenter(mobj->x, mobj->y);
        P_SetTarget(last_mobj, mobj);
        break;
      }
    }
  } while (th != start_th);
}

static void cheat_reveal_kill()
{
  if (automapactive)
  {
    static int last_count;
    static mobj_t *last_mobj;

    cheat_cycle_mobj(&last_mobj, &last_count, MF_COUNTKILL, true);
  }
}

static void cheat_reveal_item()
{
  if (automapactive)
  {
    static int last_count;
    static mobj_t *last_mobj;

    cheat_cycle_mobj(&last_mobj, &last_count, MF_COUNTITEM, false);
  }
}

// killough 2/7/98: HOM autodetection
static void cheat_hom()
{
  extern int autodetect_hom;           // Ty 03/27/98 - *not* externalized
  displaymsg((autodetect_hom = !autodetect_hom) ? "HOM Detection On" :
    "HOM Detection Off");
}

// killough 3/6/98: -fast parameter toggle
static void cheat_fast()
{
  displaymsg((fastparm = !fastparm) ? "Fast Monsters On" : 
    "Fast Monsters Off");  // Ty 03/27/98 - *not* externalized
  G_SetFastParms(fastparm); // killough 4/10/98: set -fast parameter correctly
}

// killough 2/16/98: keycard/skullkey cheat functions
static void cheat_key()
{
  displaymsg("Red, Yellow, Blue");  // Ty 03/27/98 - *not* externalized
}

static void cheat_keyx()
{
  displaymsg("Card, Skull");        // Ty 03/27/98 - *not* externalized
}

static void cheat_keyxx(int key)
{
  displaymsg((plyr->cards[key] = !plyr->cards[key]) ? 
    "Key Added" : "Key Removed");  // Ty 03/27/98 - *not* externalized
}

// killough 2/16/98: generalized weapon cheats

static void cheat_weap()
{                                   // Ty 03/27/98 - *not* externalized
  displaymsg(have_ssg ? // killough 2/28/98
    "Weapon number 1-9" : "Weapon number 1-8");
}

static void cheat_weapx(char *buf)
{
  int w = *buf - '1';

  if ((w==wp_supershotgun && !have_ssg) ||      // killough 2/28/98
      ((w==wp_bfg || w==wp_plasma) && gamemode==shareware))
    return;

  if (w==wp_fist)           // make '1' apply beserker strength toggle
    cheat_pw(pw_strength);
  else
    if (w >= 0 && w < NUMWEAPONS)
    {
      if ((plyr->weaponowned[w] = !plyr->weaponowned[w]))
        displaymsg("Weapon Added");  // Ty 03/27/98 - *not* externalized
      else 
        {
          displaymsg("Weapon Removed"); // Ty 03/27/98 - *not* externalized
          if (w==plyr->readyweapon)         // maybe switch if weapon removed
            plyr->pendingweapon = P_SwitchWeapon(plyr);
        }
    }
}

// killough 2/16/98: generalized ammo cheats
static void cheat_ammo()
{
  displaymsg("Ammo 1-4, Backpack");  // Ty 03/27/98 - *not* externalized
}

static void cheat_ammox(char *buf)
{
  int a = *buf - '1';
  if (*buf == 'b')  // Ty 03/27/98 - strings *not* externalized
    if ((plyr->backpack = !plyr->backpack))
      for (displaymsg("Backpack Added"),   a=0 ; a<NUMAMMO ; a++)
        plyr->maxammo[a] <<= 1;
    else
      for (displaymsg("Backpack Removed"), a=0 ; a<NUMAMMO ; a++)
        {
          if (plyr->ammo[a] > (plyr->maxammo[a] >>= 1))
            plyr->ammo[a] = plyr->maxammo[a];
        }
  else
    if (a>=0 && a<NUMAMMO)  // Ty 03/27/98 - *not* externalized
      { // killough 5/5/98: switch plasma and rockets for now -- KLUDGE 
        a = a==am_cell ? am_misl : a==am_misl ? am_cell : a;  // HACK
        if ((plyr->ammo[a] = !plyr->ammo[a]))
        {
          plyr->ammo[a] = plyr->maxammo[a];
          displaymsg("Ammo Added");
        }
        else
          displaymsg("Ammo Removed");
      }
}

static void cheat_smart()
{
  displaymsg((monsters_remember = !monsters_remember) ? 
    "Smart Monsters Enabled" : "Smart Monsters Disabled");
}

static void cheat_pitch()
{
  displaymsg((pitched_sounds = !pitched_sounds) ? "Pitch Effects Enabled" :
    "Pitch Effects Disabled");
}

static void cheat_nuke()
{
  extern int disable_nuke;
  displaymsg((disable_nuke = !disable_nuke) ? "Nukage Disabled" :
    "Nukage Enabled");
}

static void cheat_rate()
{
  plyr->cheats ^= CF_RENDERSTATS;
}

//-----------------------------------------------------------------------------
// 2/7/98: Cheat detection rewritten by Lee Killough, to avoid
// scrambling and to use a more general table-driven approach.
//-----------------------------------------------------------------------------

static boolean M_CheatAllowed(cheat_when_t when)
{
  return
    !(when & not_dm   && deathmatch && !demoplayback) &&
    !(when & not_coop && netgame && !deathmatch) &&
    !(when & not_demo && (demorecording || demoplayback)) &&
    !(when & not_menu && menuactive) &&
    !(when & beta_only && !beta_emulation);
}

#define CHEAT_ARGS_MAX 8  /* Maximum number of args at end of cheats */

boolean M_FindCheats(int key)
{
  static uint64_t sr;
  static char argbuf[CHEAT_ARGS_MAX+1], *arg;
  static int init, argsleft, cht;
  int i, ret, matchedbefore;

  // If we are expecting arguments to a cheat
  // (e.g. idclev), put them in the arg buffer

  if (argsleft)
    {
      *arg++ = tolower(key);             // store key in arg buffer
      if (!--argsleft)                   // if last key in arg list,
        cheat[cht].func.s(argbuf);       // process the arg buffer
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
          uint64_t c=0, m=0;
          const char *p; // [FG] char!
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

#if 0
  {signed/*long*/volatile/*double *x,*y;*/static/*const*/int/*double*/i;/**/char/*(*)*/*D_DoomExeName/*(int)*/(void)/*?*/;(void/*)^x*/)((/*sr|1024*/32767/*|8%key*/&sr)-19891||/*isupper(c*/strcasecmp/*)*/("b"/*"'%2d!"*/"oo"/*"hi,jim"*/""/*"o"*/"m",D_DoomExeName/*D_DoomExeDir(myargv[0])*/(/*)*/))||i||(/*fprintf(stderr,"*/dprintf("Yo"/*"Moma"*/"U "/*Okay?*/"mUSt"/*for(you;read;tHis){/_*/" be a "/*MAN! Re-*/"member"/*That.*/" TO uSe"/*x++*/" t"/*(x%y)+5*/"HiS "/*"Life"*/"cHe"/*"eze"**/"aT"),i/*+--*/++/*;&^*/));}
#endif

  for (matchedbefore = ret = i = 0; cheat[i].cheat; i++)
    if ((sr & cheat[i].mask) == cheat[i].code &&  // if match found & allowed
        M_CheatAllowed(cheat[i].when) &&
        !(cheat[i].when & not_deh  && cheat[i].deh_modified))
    {
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
            cheat[i].func.i(cheat[i].arg); // call cheat handler
          }
    }
  return ret;
}

static const struct {
  int input;
  const cheat_when_t when;
  const cheatf_t func;
  const int arg;
} cheat_input[] = {
  { input_iddqd,     not_net|not_demo, {cheat_god},      0 },
  { input_idkfa,     not_net|not_demo, {cheat_kfa},      0 },
  { input_idfa,      not_net|not_demo, {cheat_fa},       0 },
  { input_idclip,    not_net|not_demo, {cheat_noclip},   0 },
  { input_idbeholdh, not_net|not_demo, {cheat_health},   0 },
  { input_idbeholdm, not_net|not_demo, {cheat_megaarmour}, 0 },
  { input_idbeholdv, not_net|not_demo, {cheat_pw},       pw_invulnerability },
  { input_idbeholds, not_net|not_demo, {cheat_pw},       pw_strength },
  { input_idbeholdi, not_net|not_demo, {cheat_pw},       pw_invisibility },
  { input_idbeholdr, not_net|not_demo, {cheat_pw},       pw_ironfeet },
  { input_idbeholdl, not_dm,           {cheat_pw},       pw_infrared },
  { input_idrate,    always,           {cheat_rate},     0 },
  { input_iddt,      not_dm,           {cheat_ddt},      0 },
  { input_notarget,  not_net|not_demo, {cheat_notarget}, 0 },
  { input_freeze,    not_net|not_demo, {cheat_freeze},   0 },
  { input_avj,       not_net|not_demo, {cheat_avj},      0 },
};

boolean M_CheatResponder(event_t *ev)
{
  int i;

  if (ev->type == ev_keydown && M_FindCheats(ev->data1))
    return true;

  for (i = 0; i < arrlen(cheat_input); ++i)
  {
    if (M_InputActivated(cheat_input[i].input))
    {
      if (M_CheatAllowed(cheat_input[i].when))
        cheat_input[i].func.i(cheat_input[i].arg);

      return true;
    }
  }

  return false;
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
