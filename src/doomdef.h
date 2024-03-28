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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF__
#define __DOOMDEF__

// Game mode handling - identify IWAD version
//  to handle IWAD dependend animations etc.
typedef enum {
  shareware,    // DOOM 1 shareware, E1, M9
  registered,   // DOOM 1 registered, E3, M27
  commercial,   // DOOM 2 retail, E1 M34  (DOOM 2 german edition not handled)
  retail,       // DOOM 1 retail, E4, M36
  indetermined  // Well, no IWAD found.
} GameMode_t;

// Mission packs - might be useful for TC stuff?
typedef enum {
  doom,         // DOOM 1
  doom2,        // DOOM 2
  pack_tnt,     // TNT mission pack
  pack_plut,    // Plutonia pack
  pack_chex,    // Chex Quest
  pack_hacx,    // Hacx
  pack_rekkr,   // Rekkr
  none
} GameMission_t;

// Identify language to use, software localization.
typedef enum {
  english,
  french,
  german,
  unknown
} Language_t;

// [FG] emulate a specific version of Doom

typedef enum
{
    exe_indetermined = -1,
    exe_doom_1_9,    // Doom 1.9: for shareware, registered and commercial
    exe_ultimate,    // Ultimate Doom (retail)
    exe_final,       // Final Doom
    exe_chex,        // Chex Quest
} GameVersion_t;

// [FG] flashing disk icon
#define DISK_ICON_THRESHOLD (20 * 1024)

#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#define NONWIDEWIDTH SCREENWIDTH // [crispy] non-widescreen SCREENWIDTH
#define ACTUALHEIGHT 240

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS       4

// phares 5/14/98:
// DOOM Editor Numbers (aka doomednum in mobj_t)

#define DEN_PLAYER5 4001
#define DEN_PLAYER6 4002
#define DEN_PLAYER7 4003
#define DEN_PLAYER8 4004

// State updates, number of tics / second.
#define TICRATE          35

// The current state of the game: whether we are playing, gazing
// at the intermission screen, the game final animation, or a demo.

typedef enum {
  GS_LEVEL,
  GS_INTERMISSION,
  GS_FINALE,
  GS_DEMOSCREEN
} gamestate_t;

//
// Difficulty/skill settings/filters.
//
// These are Thing flags

// Skill flags.
#define MTF_EASY                1
#define MTF_NORMAL              2
#define MTF_HARD                4
// Deaf monsters/do not react to sound.
#define MTF_AMBUSH              8

// killough 11/98
#define MTF_NOTSINGLE          16
#define MTF_NOTDM              32
#define MTF_NOTCOOP            64
#define MTF_FRIEND            128
#define MTF_RESERVED          256

typedef enum {
  sk_default=-2,
  sk_none=-1, //jff 3/24/98 create unpicked skill setting
  sk_baby=0,
  sk_easy,
  sk_medium,
  sk_hard,
  sk_nightmare
} skill_t;

//
// Key cards.
//

typedef enum {
  it_bluecard,
  it_yellowcard,
  it_redcard,
  it_blueskull,
  it_yellowskull,
  it_redskull,
  NUMCARDS
} card_t;

// The defined weapons, including a marker
// indicating user has not changed weapon.
typedef enum {
  wp_fist,
  wp_pistol,
  wp_shotgun,
  wp_chaingun,
  wp_missile,
  wp_plasma,
  wp_bfg,
  wp_chainsaw,
  wp_supershotgun,

  NUMWEAPONS,
  wp_nochange              // No pending weapon change.
} weapontype_t;

// Ammunition types defined.
typedef enum {
  am_clip,    // Pistol / chaingun ammo.
  am_shell,   // Shotgun / double barreled shotgun.
  am_cell,    // Plasma rifle, BFG.
  am_misl,    // Missile launcher.
  NUMAMMO,
  am_noammo   // Unlimited for chainsaw / fist.
} ammotype_t;

// Power up artifacts.
typedef enum {
  pw_invulnerability,
  pw_strength,
  pw_invisibility,
  pw_ironfeet,
  pw_allmap,
  pw_infrared,
  NUMPOWERS,
} powertype_t;

// Power up durations (how many seconds till expiration).
typedef enum {
  INVULNTICS  = (30*TICRATE),
  INVISTICS   = (60*TICRATE),
  INFRATICS   = (120*TICRATE),
  IRONTICS    = (60*TICRATE)
} powerduration_t;

// phares 4/19/98:
// Defines Setup Screen groups that config variables appear in.
// Used when resetting the defaults for every item in a Setup group.

typedef enum {
  ss_none = -1,
  ss_keys,
  ss_weap,
  ss_stat,
  ss_auto,
  ss_enem,
  ss_gen,       // killough 10/98
  ss_comp,      // killough 10/98
  ss_max
} ss_types;

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_MOMENTUM 15000       // mud factor based on momentum
#define ORIG_FRICTION          0xE800      // original value
#define ORIG_FRICTION_FACTOR   2048        // original value

#endif          // __DOOMDEF__

//----------------------------------------------------------------------------
//
// $Log: doomdef.h,v $
// Revision 1.23  1998/05/14  08:02:00  phares
// Added Player Starts 5-8 (4001-4004)
//
// Revision 1.22  1998/05/05  15:34:48  phares
// Documentation and Reformatting changes
//
// Revision 1.21  1998/05/03  22:39:56  killough
// beautification
//
// Revision 1.20  1998/04/27  01:50:51  killough
// Make gcc's __attribute__ mean nothing on other compilers
//
// Revision 1.19  1998/04/22  13:45:23  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.18  1998/03/24  15:59:13  jim
// Added default_skill parameter to config file
//
// Revision 1.17  1998/03/23  15:23:34  phares
// Changed pushers to linedef control
//
// Revision 1.16  1998/03/20  00:29:34  phares
// Changed friction to linedef control
//
// Revision 1.15  1998/03/12  14:28:36  phares
// friction and IDCLIP changes
//
// Revision 1.14  1998/03/09  18:27:16  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.13  1998/03/09  07:08:30  killough
// Add numlock key scancode
//
// Revision 1.12  1998/03/04  21:26:27  phares
// Repaired syntax error (left-over conflict marker)
//
// Revision 1.11  1998/03/04  21:02:16  phares
// Dynamic HELP screen
//
// Revision 1.10  1998/03/02  11:25:52  killough
// Remove now-dead monster_ai mask idea
//
// Revision 1.9  1998/02/24  08:45:32  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.8  1998/02/23  04:15:50  killough
// New monster AI option mask enums
//
// Revision 1.7  1998/02/15  02:48:06  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  02:52:01  killough
// Make SCREENWIDTH/HEIGHT more flexible
//
// Revision 1.5  1998/02/02  13:22:47  killough
// user new version files
//
// Revision 1.4  1998/01/30  18:48:07  phares
// Changed textspeed and textwait to functions
//
// Revision 1.3  1998/01/30  16:09:06  phares
// Faster end-mission text display
//
// Revision 1.2  1998/01/26  19:26:39  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
