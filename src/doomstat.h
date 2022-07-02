// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: doomstat.h,v 1.13 1998/05/12 12:47:28 phares Exp $
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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//
//-----------------------------------------------------------------------------

#ifndef __D_STATE__
#define __D_STATE__

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"
#include "d_loop.h"

// We need the playr data structure as well.
#include "d_player.h"

// and mapinfo information
#include "u_mapinfo.h"

// ------------------------
// Command line parameters.
//

extern  boolean nomonsters; // checkparm of -nomonsters
extern  boolean respawnparm;  // checkparm of -respawn
extern  boolean fastparm; // checkparm of -fast
extern  boolean devparm;  // DEBUG: launched with -devparm

extern  int screenblocks;     // killough 11/98

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//

extern GameMode_t gamemode;
extern GameMission_t  gamemission;

// [FG] emulate a specific version of Doom
extern GameVersion_t gameversion;

extern char *MAPNAME(int e, int m);

// Set if homebrew PWAD stuff has been added.
extern  boolean modifiedgame;

// compatibility with old engines (monster behavior, metrics, etc.)
extern int compatibility, default_compatibility;          // killough 1/31/98

// [FG] overflow emulation
enum {
  emu_spechits,
  emu_reject,
  emu_intercepts,
  emu_missedbackside,

  EMU_TOTAL
};

typedef struct {
  boolean enabled;
  boolean triggered;
  const char *str;
} overflow_t;

extern overflow_t overflow[EMU_TOTAL];

extern int demo_version;           // killough 7/19/98: Version of demo

// Only true when playing back an old demo -- used only in "corner cases"
// which break playback but are otherwise unnoticable or are just desirable:

#define demo_compatibility (demo_version < 200) /* killough 11/98: macroized */

#define mbf21 (demo_version == 221)

// killough 7/19/98: whether monsters should fight against each other
extern int monster_infighting, default_monster_infighting;

extern int monkeys, default_monkeys;

// v1.1-like pitched sounds
extern int pitched_sounds;

extern int general_translucency;

enum {
  TRANSLUCENCY_OFF,
  TRANSLUCENCY_WALLS,
  TRANSLUCENCY_THINGS,
  TRANSLUCENCY_ALL
};

extern int demo_insurance;      // killough 4/5/98

// -------------------------------------------
// killough 10/98: compatibility vector

enum {
  comp_telefrag,
  comp_dropoff,
  comp_vile,
  comp_pain,
  comp_skull,
  comp_blazing,
  comp_doorlight,
  comp_model,
  comp_god,
  comp_falloff,
  comp_floors,
  comp_skymap,
  comp_pursuit,
  comp_doorstuck,
  comp_staylift,
  comp_zombie,
  comp_stairs,
  comp_infcheat,
  comp_zerotags,

  // from PrBoom+/Eternity Engine (part of mbf21 spec)
  comp_respawn,
  comp_soul,

  // mbf21
  comp_ledgeblock,
  comp_friendlyspawn,
  comp_voodooscroller,
  comp_reservedlineflag,

  MBF21_COMP_TOTAL,

  COMP_TOTAL=32  // Some extra room for additional variables
};

extern int comp[COMP_TOTAL], default_comp[COMP_TOTAL];

// -------------------------------------------
// Language.
extern  Language_t   language;

// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern  skill_t   startskill;
extern  int             startepisode;
extern  int   startmap;

// the -loadgame option.  If this has not been provided, this is -1.
extern  int       startloadgame;

extern  boolean   autostart;

// Selected by user.
extern  skill_t         gameskill;
extern  int   gameepisode;
extern  int   gamemap;
extern  mapentry_t*     gamemapinfo;

// If non-zero, exit the level after this number of minutes
extern  int             timelimit;

// Nightmare mode flag, single player.
extern  boolean         respawnmonsters;

// Netgame? Only true if >1 player.
extern  boolean netgame;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern  boolean deathmatch;

extern boolean coop_spawns;

// ------------------------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//  but are not (yet) supported with Linux
//  (e.g. no sound volume adjustment with menu.

// These are not used, but should be (menu).
// From m_menu.c:
//  Sound FX volume has default, 0 - 15
//  Music volume has default, 0 - 15
// These are multiplied by 8.
extern int snd_SfxVolume;      // maximum volume for sound
extern int snd_MusicVolume;    // maximum volume for music

// Current music/sfx card - index useless
//  w/o a reference LUT in a sound module.
// Ideally, this would use indices found
//  in: /usr/include/linux/soundcard.h
extern int snd_MusicDevice;
extern int snd_SfxDevice;
// Config file? Same disclaimer as above.
extern int snd_DesiredMusicDevice;
extern int snd_DesiredSfxDevice;


// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern  boolean statusbaractive;

extern  boolean automapactive; // In AutoMap mode?
extern  boolean automapoverlay;
extern  boolean automaprotate;
extern  boolean menuactive;    // Menu overlayed?
extern  boolean paused;        // Game Pause?
extern  boolean viewactive;
extern  boolean nodrawers;
extern  boolean noblit;
extern  boolean nosfxparm;
extern  boolean nomusicparm;

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern  int viewangleoffset;

// Player taking events, and displaying.
extern  int consoleplayer;
extern  int displayplayer;

// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern  int totalkills;
extern  int extrakills; // [crispy] count spawned monsters
extern  int totalitems;
extern  int totalsecret;

// Timer, for scores.
extern  int levelstarttic;  // gametic at level start
extern  int basetic;    // killough 9/29/98: levelstarttic, adjusted
extern  int leveltime;  // tics in game play for par
extern  int oldleveltime;
extern  int totalleveltimes; // [FG] total time for all completed levels
// --------------------------------------
// DEMO playback/recording related stuff.

extern  boolean usergame;
extern  boolean demoplayback;
extern  boolean demorecording;

// Round angleturn in ticcmds to the nearest 256.  This is used when
// recording Vanilla demos in netgames.
extern  boolean lowres_turn;

// cph's doom 1.91 longtics hack
extern  boolean longtics;

// Quit after playing a demo from cmdline.
extern  boolean   singledemo;
// Print timing information after quitting.  killough
extern  boolean   timingdemo;
// Run tick clock at fastest speed possible while playing demo.  killough
extern  boolean   fastdemo;
// [FG] fast-forward demo to the desired map
extern  int       demowarp;
// fast-forward demo to the next map
extern  boolean   demonext;
// skipping demo
extern  int       demoskip_tics;

#define DEMOSKIP (demowarp >= 0 || demoskip_tics > 0 || demonext)

extern  boolean   strictmode, default_strictmode;

#define STRICTMODE(x) (strictmode ? 0 : x)

#define STRICTMODE_COMP(x) (strictmode ? comp[x] : default_comp[x])

#define STRICTMODE_VANILLA(x) (strictmode && demo_compatibility ? 0 : x)

extern  int       savegameslot;

extern  gamestate_t  gamestate;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

extern  int   gametic;


// Bookkeeping on players - state.
extern  player_t  players[MAXPLAYERS];

// Alive? Disconnected?
extern  boolean    playeringame[];

extern  mapthing_t *deathmatchstarts;     // killough
extern  size_t     num_deathmatchstarts; // killough

extern  mapthing_t *deathmatch_p;

// Player spawn spots.
extern  mapthing_t playerstarts[];

// Intermission stats.
// Parameters for world map / intermission.
extern wbstartstruct_t wminfo;

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern  char   *basedefault;

// if true, load all graphics at level load
extern  boolean precache;

// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern  gamestate_t     wipegamestate;

extern  int             mouseSensitivity_horiz; // killough
extern  int             mouseSensitivity_vert;

// debug flag to cancel adaptiveness
extern  boolean         singletics;

extern  int             bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.

extern int    skyflatnum;

extern  int        rndindex;

extern  int        maketic;

extern  ticcmd_t   *netcmds;
extern  int        ticdup;

//-----------------------------------------------------------------------------

extern int allow_pushers;         // MT_PUSH Things    // phares 3/10/98
extern int default_allow_pushers;

extern int variable_friction;  // ice & mud            // phares 3/10/98
extern int default_variable_friction;

extern int monsters_remember;                          // killough 3/1/98
extern int default_monsters_remember;

extern int weapon_recoil;          // weapon recoil    // phares
extern int default_weapon_recoil;

extern int player_bobbing;  // whether player bobs or not   // phares 2/25/98
extern int default_player_bobbing;  // killough 3/1/98: make local to each game

// killough 7/19/98: Classic Pre-Beta BFG
extern int classic_bfg, default_classic_bfg;

// killough 7/24/98: Emulation of Press Release version of Doom
extern int beta_emulation;

extern int dogs, default_dogs;     // killough 7/19/98: Marine's best friend :)
extern int dog_jumping, default_dog_jumping;   // killough 10/98

// killough 8/8/98: distance friendly monsters tend to stay from player
extern int distfriend, default_distfriend;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
extern int monster_backing, default_monster_backing;

// killough 9/9/98: whether monsters intelligently avoid hazards
extern int monster_avoid_hazards, default_monster_avoid_hazards;

// killough 10/98: whether monsters are affected by friction
extern int monster_friction, default_monster_friction;

// killough 9/9/98: whether monsters help friends
extern int help_friends, default_help_friends;

extern int flashing_hom; // killough 10/98

extern int doom_weapon_toggles;   // killough 10/98

// [FG] centered weapon sprite
extern int center_weapon;

extern boolean cosmetic_bobbing;

#endif

//----------------------------------------------------------------------------
//
// $Log: doomstat.h,v $
// Revision 1.13  1998/05/12  12:47:28  phares
// Removed OVER_UNDER code
//
// Revision 1.12  1998/05/06  16:05:34  jim
// formatting and documenting
//
// Revision 1.11  1998/05/05  16:28:51  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.10  1998/05/03  23:12:52  killough
// beautify, move most global switch variable decls here
//
// Revision 1.9  1998/04/06  04:54:55  killough
// Add demo_insurance
//
// Revision 1.8  1998/03/02  11:26:25  killough
// Remove now-dead monster_ai mask idea
//
// Revision 1.7  1998/02/23  04:17:38  killough
// fix bad translucency flag
//
// Revision 1.5  1998/02/20  21:56:29  phares
// Preliminarey sprite translucency
//
// Revision 1.4  1998/02/19  16:55:30  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/18  00:58:54  jim
// Addition of HUD
//
// Revision 1.2  1998/01/26  19:26:41  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
