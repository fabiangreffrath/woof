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
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

#include <time.h>
#include <stdarg.h>
#include <errno.h>

#include "m_io.h" // haleyjd

#include "doomstat.h"
#include "i_printf.h"
#include "doomkeys.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "p_map.h"
#include "s_sound.h"
#include "s_musinfo.h"
#include "dstrings.h"
#include "sounds.h"
#include "r_data.h"
#include "r_sky.h"
#include "d_deh.h"              // Ty 3/27/98 deh declarations
#include "p_inter.h"
#include "g_game.h"
#include "i_video.h" // [FG] MAX_JSB, MAX_MB
#include "statdump.h" // [FG] StatCopy()
#include "m_misc2.h"
#include "u_mapinfo.h"
#include "m_input.h"
#include "memio.h"
#include "m_snapshot.h"
#include "m_swap.h" // [FG] LONG
#include "i_input.h"

#define SAVEGAMESIZE  0x20000
#define SAVESTRINGSIZE  24

static size_t   savegamesize = SAVEGAMESIZE; // killough
static char     *demoname = NULL;
// [crispy] the name originally chosen for the demo, i.e. without "-00000"
static char     *orig_demoname = NULL;
static boolean  netdemo;
static byte     *demobuffer;   // made some static -- killough
static size_t   maxdemosize;
static byte     *demo_p;
static byte     consistancy[MAXPLAYERS][BACKUPTICS];

static int G_GameOptionSize(void);

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
boolean         respawnmonsters;
int             gameepisode;
int             gamemap;
mapentry_t*     gamemapinfo;

// If non-zero, exit the level after this number of minutes.
int             timelimit;

int             paused;
boolean         sendpause;     // send a pause event next tic
boolean         sendsave;      // send a save event next tic
boolean         sendreload;    // send a reload level event next tic
boolean         sendjoin;      // send a join demo event next tic
boolean         usergame;      // ok to save / end game
boolean         timingdemo;    // if true, exit with report on completion
boolean         fastdemo;      // if true, run at full speed -- killough
boolean         nodrawers;     // for comparative timing purposes
boolean         noblit;        // for comparative timing purposes
int             starttime;     // for comparative timing purposes
boolean         viewactive;
int             deathmatch;    // only if started as net death
boolean         netgame;       // only true if packets are broadcast
boolean         solonet;
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
int             consoleplayer; // player taking events and displaying
int             displayplayer; // view being displayed
int             gametic;
int             levelstarttic; // gametic at level start
int             basetic;       // killough 9/29/98: for demo sync
int             totalkills, totalitems, totalsecret;    // for intermission
int             extrakills;    // [crispy] count spawned monsters
int             totalleveltimes; // [FG] total time for all completed levels
boolean         demorecording;
boolean         longtics;             // cph's doom 1.91 longtics hack
boolean         lowres_turn;          // low resolution turning for longtics
boolean         demoplayback;
boolean         singledemo;           // quit after playing a demo from cmdline
boolean         precache = true;      // if true, load all graphics at start
wbstartstruct_t wminfo;               // parms for world map / intermission
boolean         haswolflevels = false;// jff 4/18/98 wolf levels present
byte            *savebuffer;
int             autorun = false;      // always running?          // phares
int             novert = false;
boolean         mouselook = false;
boolean         padlook = false;
// killough 4/13/98: Make clock rate adjustable by scale factor
int             realtic_clock_rate = 100;

int             default_complevel;

boolean         pistolstart, default_pistolstart;

boolean         strictmode, default_strictmode;
boolean         critical;

// [crispy] store last cmd to track joins
static ticcmd_t* last_cmd = NULL;

//
// controls (have defaults)
//
int     key_escape = KEY_ESCAPE;                           // phares 4/13/98
int     key_help = KEY_F1;                                 // phares 4/13/98
// [FG] double click acts as "use"
int     dclick_use;
// [FG] invert vertical axis
int     mouse_y_invert;

#define MAXPLMOVE   (forwardmove[1])
#define TURBOTHRESHOLD  0x32
#define SLOWTURNTICS  6
#define QUICKREVERSE 32768 // 180 degree reverse                    // phares
#define NUMKEYS   256

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2]    = {0x18, 0x28};
fixed_t angleturn[3]   = {640, 1280, 320};  // + slow turn

static fixed_t lookspeed[] = {160, 320};

boolean gamekeydown[NUMKEYS];
int     turnheld;       // for accelerative turning

boolean mousearray[MAX_MB+1]; // [FG] support more mouse buttons
boolean *mousebuttons = &mousearray[1];    // allow [-1]

// mouse values are used once
static int mousex;
static int mousey;
boolean dclick;

// Skip mouse if using a controller and recording in strict mode (DSDA rule).
static boolean skip_mouse = true;

boolean joyarray[MAX_JSB+1]; // [FG] support more joystick buttons
boolean *joybuttons = &joyarray[1];    // allow [-1]

int axis_forward;
int axis_strafe;
int axis_turn;
int axis_look;
int axis_turn_sens;
int axis_move_sens;
int axis_look_sens;
static const int direction[] = { 1, -1 };
boolean invert_turn;
boolean invert_forward;
boolean invert_strafe;
boolean invert_look;
boolean analog_controls;
int controller_axes[NUM_AXES];

int   savegameslot = -1;
char  savedescription[32];

//jff 3/24/98 declare startskill external, define defaultskill here
int defaultskill;               //note 1-based

// killough 2/8/98: make corpse queue variable in size
int    bodyqueslot, bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98

// [FG] prev/next weapon handling from Chocolate Doom

static int next_weapon = 0;

static const struct
{
    weapontype_t weapon;
    weapontype_t weapon_num;
} weapon_order_table[] = {
    { wp_fist,            wp_fist },
    { wp_chainsaw,        wp_fist },
    { wp_pistol,          wp_pistol },
    { wp_shotgun,         wp_shotgun },
    { wp_supershotgun,    wp_shotgun },
    { wp_chaingun,        wp_chaingun },
    { wp_missile,         wp_missile },
    { wp_plasma,          wp_plasma },
    { wp_bfg,             wp_bfg }
};

static boolean WeaponSelectable(weapontype_t weapon)
{
    // Can't select the super shotgun in Doom 1.

    if (weapon == wp_supershotgun && !have_ssg)
    {
        return false;
    }

    // These weapons aren't available in shareware.

    if ((weapon == wp_plasma || weapon == wp_bfg)
     && gamemission == doom && gamemode == shareware)
    {
        return false;
    }

    // Can't select a weapon if we don't own it.

    if (!players[consoleplayer].weaponowned[weapon])
    {
        return false;
    }

    // Can't select the fist if we have the chainsaw, unless
    // we also have the berserk pack.

    if (weapon == wp_fist
     && players[consoleplayer].weaponowned[wp_chainsaw]
     && !players[consoleplayer].powers[pw_strength])
    {
        return false;
    }

    return true;
}

static int G_NextWeapon(int direction)
{
    weapontype_t weapon;
    int start_i, i;

    // Find index in the table.

    if (players[consoleplayer].pendingweapon == wp_nochange)
    {
        weapon = players[consoleplayer].readyweapon;
    }
    else
    {
        weapon = players[consoleplayer].pendingweapon;
    }

    for (i=0; i<arrlen(weapon_order_table); ++i)
    {
        if (weapon_order_table[i].weapon == weapon)
        {
            break;
        }
    }

    if (i == arrlen(weapon_order_table))
    {
        return wp_nochange;
    }

    // Switch weapon. Don't loop forever.
    start_i = i;
    do
    {
        i += direction;
        i = (i + arrlen(weapon_order_table)) % arrlen(weapon_order_table);
    } while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

    return weapon_order_table[i].weapon_num;
}

// [FG] toggle demo warp mode
void G_EnableWarp(boolean warp)
{
  static boolean nodrawers_old;
  static boolean nomusicparm_old, nosfxparm_old;

  if (warp)
  {
    nodrawers_old = nodrawers;
    nomusicparm_old = nomusicparm;
    nosfxparm_old = nosfxparm;

    I_SetFastdemoTimer(true);
    nodrawers = true;
    nomusicparm = true;
    nosfxparm = true;
  }
  else
  {
    I_SetFastdemoTimer(false);
    nodrawers = nodrawers_old;
    nomusicparm = nomusicparm_old;
    nosfxparm = nosfxparm_old;
  }
}

static int playback_levelstarttic;
int playback_skiptics = 0;

static void G_DemoSkipTics(void)
{
  static boolean warp = false;

  if (!playback_skiptics || !playback_totaltics)
    return;

  if (playback_warp >= 0)
    warp = true;

  if (playback_warp == -1)
  {
    int curtic;

    if (playback_skiptics < 0)
    {
      playback_skiptics = playback_totaltics + playback_skiptics;
      warp = false; // ignore -warp
    }

    curtic = (warp ? playback_tic - playback_levelstarttic : playback_tic);

    if (playback_skiptics < curtic)
    {
      playback_skiptics = 0;
      G_EnableWarp(false);
      S_RestartMusic();
    }
  }
}

void G_MouseMovementResponder(const event_t *ev)
{
  if (strictmode && demorecording && skip_mouse)
    return;

  mousex += ev->data2;
  mousey += ev->data3;

  if (!M_InputGameActive(input_strafe) && mouseSensitivity_horiz)
  {
    localview.angle = (I_AccelerateMouse(mousex) *
                       (mouseSensitivity_horiz + 5) * 8 / 10);
  }

  if (mouselook && mouseSensitivity_vert_look)
  {
    localview.pitch = (I_AccelerateMouse(mouse_y_invert ? -mousey : mousey) *
                       (mouseSensitivity_vert_look + 5) / 10);
  }
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//

void G_BuildTiccmd(ticcmd_t* cmd)
{
  boolean strafe;
  int speed;
  int tspeed;
  int forward;
  int side;
  int newweapon;                                          // phares
  ticcmd_t *base;

  extern boolean boom_weapon_state_injection;
  static boolean done_autoswitch = false;

  localview.useangle = true;
  localview.usepitch = true;

  G_DemoSkipTics();

  base = I_BaseTiccmd();   // empty, or external driver
  memcpy(cmd, base, sizeof *cmd);

  cmd->consistancy = consistancy[consoleplayer][maketic%BACKUPTICS];

  strafe = M_InputGameActive(input_strafe);
  // [FG] speed key inverts autorun
  speed = autorun ^ M_InputGameActive(input_speed); // phares

  forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
  if (M_InputGameActive(input_turnleft) ||
      M_InputGameActive(input_turnright))
    turnheld += ticdup;
  else
    turnheld = 0;

  if (turnheld < SLOWTURNTICS)
    tspeed = 2;             // slow turn
  else
    tspeed = speed;

  // turn 180 degrees in one keystroke?                           // phares
                                                                  //    |
  if (STRICTMODE(M_InputGameActive(input_reverse)))               //    V
    {
      cmd->angleturn += (short)QUICKREVERSE;                      //    ^
      localview.useangle = false;
      M_InputGameDeactivate(input_reverse);                       //    |
    }                                                             // phares

  // let movement keys cancel each other out

  if (strafe)
    {
      if (M_InputGameActive(input_turnright))
        side += sidemove[speed];
      if (M_InputGameActive(input_turnleft))
        side -= sidemove[speed];

      if (analog_controls && controller_axes[axis_turn])
      {
        fixed_t x = axis_move_sens * controller_axes[axis_turn] / 10;
        x = direction[invert_turn] * x;
        side += FixedMul(sidemove[speed], x);
      }
    }
  else
    {
      if (M_InputGameActive(input_turnright))
      {
        cmd->angleturn -= angleturn[tspeed];
        localview.useangle = false;
      }
      if (M_InputGameActive(input_turnleft))
      {
        cmd->angleturn += angleturn[tspeed];
        localview.useangle = false;
      }

      if (analog_controls && controller_axes[axis_turn])
      {
        fixed_t x = controller_axes[axis_turn];

        // response curve to compensate for lack of near-centered accuracy
        x = FixedMul(FixedMul(x, x), x);

        x = direction[invert_turn] * axis_turn_sens * x / 10;
        cmd->angleturn -= FixedMul(angleturn[1], x);
        localview.useangle = false;
      }
    }

  if (M_InputGameActive(input_forward))
    forward += forwardmove[speed];
  if (M_InputGameActive(input_backward))
    forward -= forwardmove[speed];
  if (M_InputGameActive(input_straferight))
    side += sidemove[speed];
  if (M_InputGameActive(input_strafeleft))
    side -= sidemove[speed];

  if (analog_controls)
  {
    if (controller_axes[axis_forward])
    {
      fixed_t y = axis_move_sens * controller_axes[axis_forward] / 10;
      y = direction[invert_forward] * y;
      forward -= FixedMul(forwardmove[speed], y);
    }
    if (controller_axes[axis_strafe])
    {
      fixed_t x = axis_move_sens * controller_axes[axis_strafe] / 10;
      x = direction[invert_strafe] * x;
      side += FixedMul(sidemove[speed], x);
    }

    if (padlook && controller_axes[axis_look])
    {
      fixed_t y = controller_axes[axis_look];

      // response curve to compensate for lack of near-centered accuracy
      y = FixedMul(FixedMul(y, y), y);

      y = direction[invert_look] * axis_look_sens * y / 10;
      cmd->lookdir -= FixedMul(lookspeed[0], y);
      localview.usepitch = false;
    }
  }

    // buttons
  cmd->chatchar = HU_dequeueChatChar();

  if (M_InputGameActive(input_fire))
    cmd->buttons |= BT_ATTACK;

  if (M_InputGameActive(input_use)) // [FG] mouse button for "use"
    {
      cmd->buttons |= BT_USE;
      // clear double clicks if hit use button
      dclick = false;
    }

  // Toggle between the top 2 favorite weapons.                   // phares
  // If not currently aiming one of these, switch to              // phares
  // the favorite. Only switch if you possess the weapon.         // phares

  // killough 3/22/98:
  //
  // Perform automatic weapons switch here rather than in p_pspr.c,
  // except in demo_compatibility mode.
  //
  // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left
 
  if (!players[consoleplayer].attackdown)
  {
    done_autoswitch = false;
  }

  if ((!demo_compatibility && players[consoleplayer].attackdown &&
       !P_CheckAmmo(&players[consoleplayer]) &&
       ((boom_weapon_state_injection && !done_autoswitch) ||
       (cmd->buttons & BT_ATTACK && players[consoleplayer].pendingweapon == wp_nochange))) ||
       M_InputGameActive(input_weapontoggle))
  {
    done_autoswitch = true;
    boom_weapon_state_injection = false;
    newweapon = P_SwitchWeapon(&players[consoleplayer]);           // phares
  }
  else
    {                                 // phares 02/26/98: Added gamemode checks
      // [FG] prev/next weapon keys and buttons
      if (gamestate == GS_LEVEL && next_weapon != 0)
        newweapon = G_NextWeapon(next_weapon);
      else
      newweapon =
        M_InputGameActive(input_weapon1) ? wp_fist :    // killough 5/2/98: reformatted
        M_InputGameActive(input_weapon2) ? wp_pistol :
        M_InputGameActive(input_weapon3) ? wp_shotgun :
        M_InputGameActive(input_weapon4) ? wp_chaingun :
        M_InputGameActive(input_weapon5) ? wp_missile :
        M_InputGameActive(input_weapon6) && gamemode != shareware ? wp_plasma :
        M_InputGameActive(input_weapon7) && gamemode != shareware ? wp_bfg :
        M_InputGameActive(input_weapon8) ? wp_chainsaw :
        M_InputGameActive(input_weapon9) && have_ssg ? wp_supershotgun :
        wp_nochange;

      // killough 3/22/98: For network and demo consistency with the
      // new weapons preferences, we must do the weapons switches here
      // instead of in p_user.c. But for old demos we must do it in
      // p_user.c according to the old rules. Therefore demo_compatibility
      // determines where the weapons switch is made.

      // killough 2/8/98:
      // Allow user to switch to fist even if they have chainsaw.
      // Switch to fist or chainsaw based on preferences.
      // Switch to shotgun or SSG based on preferences.
      //
      // killough 10/98: make SG/SSG and Fist/Chainsaw
      // weapon toggles optional
      
      if (!demo_compatibility && doom_weapon_toggles)
        {
          const player_t *player = &players[consoleplayer];

          // only select chainsaw from '1' if it's owned, it's
          // not already in use, and the player prefers it or
          // the fist is already in use, or the player does not
          // have the berserker strength.

          if (newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
              player->readyweapon!=wp_chainsaw &&
              (player->readyweapon==wp_fist ||
               !player->powers[pw_strength] ||
               P_WeaponPreferred(wp_chainsaw, wp_fist)))
            newweapon = wp_chainsaw;

          // Select SSG from '3' only if it's owned and the player
          // does not have a shotgun, or if the shotgun is already
          // in use, or if the SSG is not already in use and the
          // player prefers it.

          if (newweapon == wp_shotgun && have_ssg &&
              player->weaponowned[wp_supershotgun] &&
              (!player->weaponowned[wp_shotgun] ||
               player->readyweapon == wp_shotgun ||
               (player->readyweapon != wp_supershotgun &&
                P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
            newweapon = wp_supershotgun;
        }
      // killough 2/8/98, 3/22/98 -- end of weapon selection changes
    }

  if (newweapon != wp_nochange)
    {
      cmd->buttons |= BT_CHANGE;
      cmd->buttons |= newweapon<<BT_WEAPONSHIFT;
    }

    // [FG] prev/next weapon keys and buttons
    next_weapon = 0;

  // [FG] double click acts as "use"
  if (dclick)
  {
    dclick = false;
    cmd->buttons |= BT_USE;
  }

  if (strafe)
  {
    if (mouseSensitivity_horiz_strafe)
    {
      side += (I_AccelerateMouse(mousex) *
               (mouseSensitivity_horiz_strafe + 5) * 2 / 10);
    }
  }
  else if (mouseSensitivity_horiz)
  {
    cmd->angleturn -= localview.angle;
  }

  if (mouselook)
  {
    if (mouseSensitivity_vert_look)
      cmd->lookdir += localview.pitch;
  }
  else if (!novert && mouseSensitivity_vert)
  {
    forward += I_AccelerateMouse(mousey) * (mouseSensitivity_vert + 5) / 10;
  }

  mousex = mousey = 0;
  localview.angle = 0.0f;
  localview.pitch = 0.0f;

  if (forward > MAXPLMOVE)
    forward = MAXPLMOVE;
  else if (forward < -MAXPLMOVE)
    forward = -MAXPLMOVE;
  if (side > MAXPLMOVE)
    side = MAXPLMOVE;
  else if (side < -MAXPLMOVE)
    side = -MAXPLMOVE;
  
  cmd->forwardmove += forward;
  cmd->sidemove += side;

  // special buttons
  if (sendpause)
    {
      sendpause = false;
      cmd->buttons = BT_SPECIAL | (BTS_PAUSE & BT_SPECIALMASK);
    }

  // killough 10/6/98: suppress savegames in demos
  if (sendsave && !demoplayback)
    {
      sendsave = false;
      cmd->buttons = BT_SPECIAL | (BTS_SAVEGAME & BT_SPECIALMASK) | (savegameslot<<BTS_SAVESHIFT);
    }

  if (sendreload)
  {
    sendreload = false;
    cmd->buttons = BT_SPECIAL | (BTS_RELOAD & BT_SPECIALMASK);
  }

  if (sendjoin)
  {
    sendjoin = false;
    cmd->buttons |= BT_JOIN;
  }

  // low-res turning

  if (lowres_turn)
  {
    static signed short carry = 0;
    signed short desired_angleturn;

    desired_angleturn = cmd->angleturn + carry;

    // round angleturn to the nearest 256 unit boundary
    // for recording demos with single byte values for turn

    cmd->angleturn = (desired_angleturn + 128) & 0xff00;

    // Carry forward the error from the reduced resolution to the
    // next tic, so that successive small movements can accumulate.

    carry = desired_angleturn - cmd->angleturn;
  }
}

//
// G_DoLoadLevel
//

static void G_DoLoadLevel(void)
{
  int i;

  // Set the sky map.
  // First thing, we have a dummy sky texture name,
  //  a flat. The data is in the WAD only because
  //  we look for an actual index, instead of simply
  //  setting one.

  skyflatnum = R_FlatNumForName ( SKYFLATNAME );

  if (gamemapinfo && gamemapinfo->skytexture[0])
  {
    skytexture = R_TextureNumForName(gamemapinfo->skytexture);
  }
  else
  // DOOM determines the sky texture to be used
  // depending on the current episode, and the game version.
  if (gamemode == commercial)
    // || gamemode == pack_tnt   //jff 3/27/98 sorry guys pack_tnt,pack_plut
    // || gamemode == pack_plut) //aren't gamemodes, this was matching retail
    {
      skytexture = R_TextureNumForName ("SKY3");
      if (gamemap < 12)
        skytexture = R_TextureNumForName ("SKY1");
      else
        if (gamemap < 21)
          skytexture = R_TextureNumForName ("SKY2");
    }
  else //jff 3/27/98 and lets not forget about DOOM and Ultimate DOOM huh?
    switch (gameepisode)
      {
      default:
      case 1:
        skytexture = R_TextureNumForName ("SKY1");
        break;
      case 2:
	// killough 10/98: beta version had different sky orderings
        skytexture = R_TextureNumForName (beta_emulation ? "SKY1" : "SKY2");
        break;
      case 3:
        skytexture = R_TextureNumForName ("SKY3");
        break;
      case 4: // Special Edition sky
        skytexture = R_TextureNumForName ("SKY4");
        break;
      }//jff 3/27/98 end sky setting fix

  R_InitSkyMap(); // [FG] stretch short skies

  levelstarttic = gametic;        // for time calculation

  playback_levelstarttic = playback_tic;

  if (!demo_compatibility && demo_version < 203)   // killough 9/29/98
    basetic = gametic;

  if (wipegamestate == GS_LEVEL)
    wipegamestate = -1;             // force a wipe

  gamestate = GS_LEVEL;

  for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (playeringame[i] && players[i].playerstate == PST_DEAD)
        players[i].playerstate = PST_REBORN;
      memset (players[i].frags,0,sizeof(players[i].frags));
    }

  // initialize the msecnode_t freelist.                     phares 3/25/98
  // any nodes in the freelist are gone by now, cleared
  // by Z_FreeTags() when the previous level ended or player
  // died.

   {
    extern msecnode_t *headsecnode; // phares 3/25/98
    headsecnode = NULL;
   }

  critical = (gameaction == ga_playdemo || demorecording || demoplayback || D_CheckNetConnect());

  P_UpdateDirectVerticalAiming();

  // [crispy] pistol start
  if (CRITICAL(pistolstart))
  {
    G_PlayerReborn(0);
  }

  P_SetupLevel (gameepisode, gamemap, 0, gameskill);
  displayplayer = consoleplayer;    // view the guy you are playing
  // [Alaux] Update smooth count values
  st_health = players[displayplayer].health;
  st_armor  = players[displayplayer].armorpoints;
  gameaction = ga_nothing;

  // Set the initial listener parameters using the player's initial state.
  S_InitListener(players[displayplayer].mo);

  // clear cmd building stuff
  memset (gamekeydown, 0, sizeof(gamekeydown));
  mousex = mousey = 0;
  memset(&localview, 0, sizeof(localview));
  sendpause = sendsave = paused = false;
  // [FG] array size!
  memset (mousearray, 0, sizeof(mousearray));
  memset (joyarray, 0, sizeof(joyarray));
  memset (controller_axes, 0, sizeof(controller_axes));

  //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
  // killough 5/13/98: in case netdemo has consoleplayer other than green
  ST_Start();
  HU_Start();

  // killough: make -timedemo work on multilevel demos
  // Move to end of function to minimize noise -- killough 2/22/98:

  if (timingdemo)
    {
      static int first=1;
      if (first)
        {
          starttime = I_GetTime_RealTime();
          first=0;
        }
    }
}

extern int ddt_cheating;

static void G_ReloadLevel(void)
{
  if (demorecording || netgame)
  {
    gamemap = startmap;
    gameepisode = startepisode;
  }

  basetic = gametic;
  rngseed += gametic;

  if (demorecording)
  {
    ddt_cheating = 0;
    G_CheckDemoStatus();
    G_RecordDemo(orig_demoname);
  }

  G_InitNew(gameskill, gameepisode, gamemap);
  gameaction = ga_nothing;

  if (demorecording)
    G_BeginRecording();
}

static boolean G_StrictModeSkipEvent(event_t *ev)
{
  static boolean enable_mouse = false;
  static boolean enable_controller = false;
  static boolean first_event = true;

  if (!strictmode || !demorecording)
    return false;

  switch (ev->type)
  {
    case ev_mouseb_down:
    case ev_mouseb_up:
    case ev_mouse:
        if (first_event)
        {
          first_event = false;
          enable_mouse = true;
          skip_mouse = false;
        }
        return !enable_mouse;

    case ev_joyb_down:
    case ev_joyb_up:
    case ev_joystick:
        if (first_event && (ev->data1 || ev->data2 || ev->data3 || ev->data4))
        {
          first_event = false;
          enable_controller = true;
        }
        return !enable_controller;

    default:
        break;
  }

  return false;
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//

boolean G_Responder(event_t* ev)
{
  // allow spy mode changes even during the demo
  // killough 2/22/98: even during DM demo
  //
  // killough 11/98: don't autorepeat spy mode switch

  if (M_InputActivated(input_spy) && netgame && (demoplayback || !deathmatch) &&
      gamestate == GS_LEVEL)
    {
	  do                                          // spy mode
	    if (++displayplayer >= MAXPLAYERS)
	      displayplayer = 0;
	  while (!playeringame[displayplayer] && displayplayer!=consoleplayer);

	  ST_Start();    // killough 3/7/98: switch status bar views too
	  HU_Start();
	  S_UpdateSounds(players[displayplayer].mo);
	  // [crispy] re-init automap variables for correct player arrow angle
	  if (automapactive)
	    AM_initVariables();
      return true;
    }

  if (M_InputActivated(input_menu_reloadlevel) &&
      (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION) &&
      !menuactive)
  {
    sendreload = true;
    return true;
  }

  // killough 9/29/98: reformatted
  if (gamestate == GS_LEVEL && (HU_Responder(ev) ||  // chat ate the event
				ST_Responder(ev) ||  // status window ate it
				AM_Responder(ev)))   // automap ate it
    return true;

  // any other key pops up menu if in demos
  //
  // killough 8/2/98: enable automap in -timedemo demos
  //
  // killough 9/29/98: make any key pop up menu regardless of
  // which kind of demo, and allow other events during playback

  if (gameaction == ga_nothing && (demoplayback || gamestate == GS_DEMOSCREEN))
    {
      // killough 9/29/98: allow user to pause demos during playback
	if (M_InputActivated(input_pause))
	{
	  if (paused ^= 2)
	    S_PauseSound();
	  else
	    S_ResumeSound();
	  return true;
	}

      if (M_InputActivated(input_demo_join) && !PLAYBACK_SKIP && !fastdemo)
      {
        sendjoin = true;
        return true;
      }

      // killough 10/98:
      // Don't pop up menu, if paused in middle
      // of demo playback, or if automap active.
      // Don't suck up keys, which may be cheats

      return gamestate == GS_DEMOSCREEN &&
	!(paused & 2) && !automapactive &&
	((ev->type == ev_keydown) ||
	 (ev->type == ev_mouseb_down) ||
	 (ev->type == ev_joyb_down)) ?
	(!menuactive ? S_StartSound(NULL,sfx_swtchn) : true),
	M_StartControlPanel(), true : false;
    }

  if (gamestate == GS_FINALE && F_Responder(ev))
  {
    return true;  // finale ate the event
  }

    // [FG] prev/next weapon handling from Chocolate Doom

  if (M_InputActivated(input_prevweapon))
  {
      next_weapon = -1;
  }
  else if (M_InputActivated(input_nextweapon))
  {
      next_weapon = 1;
  }

  if (dclick_use && ev->type == ev_mouseb_down &&
       (
         M_InputMatchMouseB(input_strafe, ev->data1) ||
         M_InputMatchMouseB(input_forward, ev->data1)
       ) &&
       ev->data2 >= 2 && (ev->data2 % 2) == 0)
  {
    dclick = true;
  }

  if (M_InputActivated(input_pause))
  {
    sendpause = true;
    return true;
  }

  if (G_StrictModeSkipEvent(ev))
    return true; // eat events

  switch (ev->type)
    {
    case ev_keydown:
	if (ev->data1 <NUMKEYS)
	  gamekeydown[ev->data1] = true;
      return true;    // eat key down events

    case ev_keyup:
      if (ev->data1 <NUMKEYS)
        gamekeydown[ev->data1] = false;
      return false;   // always let key up events filter down

    case ev_mouseb_down:
      if (ev->data1 < MAX_MB)
        mousebuttons[ev->data1] = true;
      return true;

    case ev_mouseb_up:
      if (ev->data1 < MAX_MB)
        mousebuttons[ev->data1] = false;
      return true;

    case ev_mouse:
      G_MouseMovementResponder(ev);
      return true;    // eat events

    case ev_joyb_down:
      if (ev->data1 < MAX_JSB)
        joybuttons[ev->data1] = true;
      return true;

    case ev_joyb_up:
      if (ev->data1 < MAX_JSB)
        joybuttons[ev->data1] = false;
      return true;

    case ev_joystick:
      controller_axes[AXIS_LEFTX]  = ev->data1 * 2;
      controller_axes[AXIS_LEFTY]  = ev->data2 * 2;
      controller_axes[AXIS_RIGHTX] = ev->data3 * 2;
      controller_axes[AXIS_RIGHTY] = ev->data4 * 2;
      return true;    // eat events

    default:
      break;
    }
  return false;
}

int D_GetPlayersInNetGame(void);

static void CheckPlayersInNetGame(void)
{
  int i;
  int playerscount = 0;
  for (i = 0; i < MAXPLAYERS; ++i)
  {
    if (playeringame[i])
      ++playerscount;
  }

  if (playerscount != D_GetPlayersInNetGame())
    I_Error("Not enough players to continue the demo.");
}

//
// DEMO RECORDING
//

int playback_tic = 0, playback_totaltics = 0;

static char *defdemoname;

#define DEMOMARKER    0x80

// Stay in the game, hand over controls to the player and continue recording the
// demo under a different name
static void G_JoinDemo(void)
{
  if (netgame)
    CheckPlayersInNetGame();

  if (!orig_demoname)
  {
    byte *actualbuffer = demobuffer;
    size_t actualsize = maxdemosize;

    // [crispy] find a new name for the continued demo
    G_RecordDemo(defdemoname);

    // [crispy] discard the newly allocated demo buffer
    Z_Free(demobuffer);
    demobuffer = actualbuffer;
    maxdemosize = actualsize;
  }

  // [crispy] continue recording
  demoplayback = false;

  // clear progress demo bar
  ST_Start();

  displaymsg("Demo recording: %s", demoname);
}

static void G_ReadDemoTiccmd(ticcmd_t *cmd)
{
  if (*demo_p == DEMOMARKER)
  {
    last_cmd = cmd; // [crispy] remember last cmd to track joins
    G_CheckDemoStatus();      // end of demo data stream
  }
  else
    {
      cmd->forwardmove = ((signed char)*demo_p++);
      cmd->sidemove = ((signed char)*demo_p++);
      if (!longtics)
      {
      cmd->angleturn = ((unsigned char)*demo_p++)<<8;
      }
      else
      {
        cmd->angleturn = *demo_p++;
        cmd->angleturn |= (*demo_p++) << 8;
      }
      cmd->buttons = (unsigned char)*demo_p++;

      // killough 3/26/98, 10/98: Ignore savegames in demos 
      if (demoplayback && 
	  cmd->buttons & BT_SPECIAL &&
	  cmd->buttons & BT_SPECIALMASK &&
	  cmd->buttons & BTS_SAVEGAME)
	{
	  cmd->buttons &= ~BT_SPECIALMASK;
	  displaymsg("Game Saved (Suppressed)");
	}
    }
}

static void CheckDemoBuffer(size_t size)
{
  ptrdiff_t position = demo_p - demobuffer;
  if (position + size > maxdemosize)
  {
    maxdemosize += size + 128 * 1024; // add another 128K
    demobuffer = Z_Realloc(demobuffer, maxdemosize, PU_STATIC, 0);
    demo_p = position + demobuffer;
  }
}

// Demo limits removed -- killough

static void G_WriteDemoTiccmd(ticcmd_t* cmd)
{
  if (M_InputGameActive(input_demo_quit)) // press to end demo recording
    G_CheckDemoStatus();

  demo_p[0] = cmd->forwardmove;
  demo_p[1] = cmd->sidemove;
  if (!longtics)
  {
  demo_p[2] = (cmd->angleturn+128)>>8;
  demo_p[3] = cmd->buttons;
  }
  else
  {
    demo_p[2] = (cmd->angleturn & 0xff);
    demo_p[3] = (cmd->angleturn >> 8) & 0xff;
    demo_p[4] = cmd->buttons;
  }

  CheckDemoBuffer(16);   // killough 8/23/98

  G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same
}

boolean secretexit;

void G_ExitLevel(void)
{
  secretexit = false;
  gameaction = ga_completed;
}

// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!

void G_SecretExitLevel(void)
{
  secretexit = gamemode != commercial || haswolflevels;
  gameaction = ga_completed;
}

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//

static void G_PlayerFinishLevel(int player)
{
  player_t *p = &players[player];
  memset(p->powers, 0, sizeof p->powers);
  memset(p->cards, 0, sizeof p->cards);
  p->mo->flags &= ~MF_SHADOW;   // cancel invisibility
  p->extralight = 0;      // cancel gun flashes
  p->fixedcolormap = 0;   // cancel ir gogles
  p->damagecount = 0;     // no palette changes
  p->bonuscount = 0;
  // [crispy] reset additional player properties
  p->oldlookdir = p->lookdir = 0;
  p->centering = false;
  p->slope = 0;
  p->recoilpitch = p->oldrecoilpitch = 0;
}

// [crispy] format time for level statistics
#define TIMESTRSIZE 16
static void G_FormatLevelStatTime(char *str, int tics, boolean total)
{
    int exitHours, exitMinutes;
    float exitTime, exitSeconds;

    exitTime = (float) tics / TICRATE;
    exitHours = exitTime / 3600;
    exitTime -= exitHours * 3600;
    exitMinutes = exitTime / 60;
    exitTime -= exitMinutes * 60;
    exitSeconds = exitTime;

    if (total)
    {
        M_snprintf(str, TIMESTRSIZE, "%d:%02d",
                tics / TICRATE / 60, (tics % (60 * TICRATE)) / TICRATE);
    }
    else if (exitHours)
    {
        M_snprintf(str, TIMESTRSIZE, "%d:%02d:%05.2f",
                    exitHours, exitMinutes, exitSeconds);
    }
    else
    {
        M_snprintf(str, TIMESTRSIZE, "%01d:%05.2f", exitMinutes, exitSeconds);
    }
}

// [crispy] Write level statistics upon exit
static void G_WriteLevelStat(void)
{
    static FILE *fstream = NULL;

    int i, playerKills = 0, playerItems = 0, playerSecrets = 0;

    char levelString[8];
    char levelTimeString[TIMESTRSIZE];
    char totalTimeString[TIMESTRSIZE];

    if (fstream == NULL)
    {
        fstream = M_fopen("levelstat.txt", "w");

        if (fstream == NULL)
        {
            I_Printf(VB_ERROR, "G_WriteLevelStat: Unable to open levelstat.txt for writing!");
            return;
        }
    }

    strcpy(levelString, MAPNAME(gameepisode, gamemap));

    G_FormatLevelStatTime(levelTimeString, leveltime, false);
    G_FormatLevelStatTime(totalTimeString, totalleveltimes + leveltime, true);

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            playerKills += players[i].killcount;
            playerItems += players[i].itemcount;
            playerSecrets += players[i].secretcount;
        }
    }

    if (playerKills - extrakills >= 0)
        playerKills -= extrakills;
    else
        playerKills = 0;

    fprintf(fstream, "%s%s - %s (%s)  K: %d/%d  I: %d/%d  S: %d/%d\n",
            levelString, (secretexit ? "s" : ""),
            levelTimeString, totalTimeString, playerKills, totalkills,
            playerItems, totalitems, playerSecrets, totalsecret);
}

//
// G_DoCompleted
//

boolean um_pars = false;

static void G_DoCompleted(void)
{
  int i;

  //!
  // @category demo
  // @help
  //
  // Write level statistics upon exit to levelstat.txt
  //

  if (M_CheckParm("-levelstat"))
  {
      G_WriteLevelStat();
  }

  gameaction = ga_nothing;

  for (i=0; i<MAXPLAYERS; i++)
    if (playeringame[i])
      G_PlayerFinishLevel(i);        // take away cards and stuff

  if (automapactive)
    AM_Stop();

  wminfo.nextep = wminfo.epsd = gameepisode -1;
  wminfo.last = gamemap -1;

  wminfo.lastmapinfo = gamemapinfo;
  wminfo.nextmapinfo = NULL;
  um_pars = false;
  if (gamemapinfo)
  {
    const char *next = NULL;
    boolean intermission = false;

    if (U_CheckField(gamemapinfo->endpic))
    {
      if (gamemapinfo->nointermission)
      {
        gameaction = ga_victory;
        return;
      }
      else
      {
        intermission = true;
      }
    }

    if (secretexit && gamemapinfo->nextsecret[0])
      next = gamemapinfo->nextsecret;
    else if (gamemapinfo->nextmap[0])
      next = gamemapinfo->nextmap;

    if (next)
    {
      G_ValidateMapName(next, &wminfo.nextep, &wminfo.next);
      wminfo.nextep--;
      wminfo.next--;
      // episode change
      if (wminfo.nextep != wminfo.epsd)
      {
        for (i = 0; i < MAXPLAYERS; i++)
          players[i].didsecret = false;
      }
    }

    if (next || intermission)
    {
      wminfo.didsecret = players[consoleplayer].didsecret;
      wminfo.partime = gamemapinfo->partime * TICRATE;
      if (wminfo.partime > 0)
        um_pars = true;
      goto frommapinfo;	// skip past the default setup.
    }
  }

  if (gamemode != commercial) // kilough 2/7/98
    switch(gamemap)
      {
      case 8:
        gameaction = ga_victory;
        return;
      case 9:
        for (i=0 ; i<MAXPLAYERS ; i++)
          players[i].didsecret = true;
        break;
      }

  wminfo.didsecret = players[consoleplayer].didsecret;

  // wminfo.next is 0 biased, unlike gamemap
  if (gamemode == commercial)
    {
      if (secretexit)
        switch(gamemap)
          {
          case 15:
            wminfo.next = 30; break;
          case 31:
            wminfo.next = 31; break;
          }
      else
        switch(gamemap)
          {
          case 31:
          case 32:
            wminfo.next = 15; break;
          default:
            wminfo.next = gamemap;
          }
    }
  else
    {
      if (secretexit)
        wminfo.next = 8;  // go to secret level
      else
        if (gamemap == 9)
          {
            // returning from secret level
            switch (gameepisode)
              {
              case 1:
                wminfo.next = 3;
                break;
              case 2:
                wminfo.next = 5;
                break;
              case 3:
                wminfo.next = 6;
                break;
              case 4:
                wminfo.next = 2;
                break;
              }
          }
        else
          wminfo.next = gamemap;          // go to next level
    }

  if (gamemode == commercial)
  {
    // MAP33 reads its par time from beyond the cpars[] array.
    if (demo_compatibility && gamemap == 33)
    {
      int cpars32;

      memcpy(&cpars32, s_GAMMALVL0, sizeof(int));
      wminfo.partime = TICRATE*LONG(cpars32);
    }
    else if (gamemap >= 1 && gamemap <= 34)
    {
      wminfo.partime = TICRATE*cpars[gamemap-1];
    }
  }
  else
  {
    // Doom Episode 4 doesn't have a par time, so this overflows into the cpars[] array.
    if (demo_compatibility && gameepisode == 4 && gamemap >= 1 && gamemap <= 9)
    {
      wminfo.partime = TICRATE*cpars[gamemap];
    }
    else if (gameepisode >= 1 && gameepisode <= 3 && gamemap >= 1 && gamemap <= 9)
    {
      wminfo.partime = TICRATE*pars[gameepisode][gamemap];
    }
  }

frommapinfo:
  
  wminfo.nextmapinfo = G_LookupMapinfo(wminfo.nextep+1, wminfo.next+1);

  wminfo.maxkills = totalkills;
  wminfo.maxitems = totalitems;
  wminfo.maxsecret = totalsecret;
  wminfo.maxfrags = 0;

  wminfo.pnum = consoleplayer;

  for (i=0 ; i<MAXPLAYERS ; i++)
    {
      wminfo.plyr[i].in = playeringame[i];
      wminfo.plyr[i].skills = players[i].killcount;
      wminfo.plyr[i].sitems = players[i].itemcount;
      wminfo.plyr[i].ssecret = players[i].secretcount;
      wminfo.plyr[i].stime = leveltime;
      memcpy (wminfo.plyr[i].frags, players[i].frags,
              sizeof(wminfo.plyr[i].frags));
    }

  // [FG] total time for all completed levels
  wminfo.totaltimes = (totalleveltimes += (leveltime - leveltime % TICRATE));

  gamestate = GS_INTERMISSION;
  viewactive = false;
  automapactive = false;

  // [FG] -statdump implementation from Chocolate Doom
  if (gamemode == commercial || gamemap != 8)
  {
    StatCopy(&wminfo);
  }

  WI_Start (&wminfo);
}

static void G_DoWorldDone(void)
{
  idmusnum = -1;             //jff 3/17/98 allow new level's music to be loaded
  musinfo.from_savegame = false;
  gamestate = GS_LEVEL;
  gameepisode = wminfo.nextep + 1;
  gamemap = wminfo.next+1;
  gamemapinfo = G_LookupMapinfo(gameepisode, gamemap);
  G_DoLoadLevel();
  gameaction = ga_nothing;
  viewactive = true;
  AM_clearMarks();           //jff 4/12/98 clear any marks on the automap
}

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 32

#define INVALID_DEMO(a, b) \
   do \
   { \
     I_Printf(VB_WARNING, "G_DoPlayDemo: " a, b); \
     gameaction = ga_nothing; \
     demoplayback = true; \
     G_CheckDemoStatus(); \
     return; \
   } while(0)

static void G_DoPlayDemo(void)
{
  skill_t skill;
  int i, episode, map;
  char basename[9];
  int demover;
  byte *option_p = NULL;      // killough 11/98
  int lumpnum, lumplength;

  if (gameaction != ga_loadgame)      // killough 12/98: support -loadgame
    basetic = gametic;  // killough 9/29/98

  // [crispy] in demo continue mode free the obsolete demo buffer
  // of size 'maxdemosize' previously allocated in G_RecordDemo()
  if (demorecording)
  {
      Z_Free(demobuffer);
  }

  ExtractFileBase(defdemoname,basename);           // killough

  lumpnum = W_GetNumForName(basename);
  lumplength = W_LumpLength(lumpnum);

  demobuffer = demo_p = W_CacheLumpNum(lumpnum, PU_STATIC);  // killough

  // [FG] ignore too short demo lumps
  if (lumplength < 0xd)
  {
    INVALID_DEMO("Short demo lump %s.", basename);
  }

  demover = *demo_p++;

  // skip UMAPINFO demo header
  if (demover == 255)
  {
    // we check for the PR+UM signature.
    // Eternity Engine also uses 255 demover, with other signatures.
    if (strncmp((const char *)demo_p, "PR+UM", 5) != 0)
    {
      INVALID_DEMO("Extended demo format %d found, but \"PR+UM\" string not found.", demover);
    }

    demo_p += 6;

    if (*demo_p++ != 1)
    {
      I_Error("G_DoPlayDemo: Unknown demo format.");
    }

    // the defunct format had only one extension (in two bytes)
    if (*demo_p++ != 1 || *demo_p++ != 0)
    {
      I_Error("G_DoPlayDemo: Unknown demo format.");
    }

    if (*demo_p++ != 8)
    {
      I_Error("G_DoPlayDemo: Unknown demo format.");
    }

    if (strncmp((const char *)demo_p, "UMAPINFO", 8))
    {
      I_Error("G_DoPlayDemo: Unknown demo format.");
    }

    demo_p += 8;

    // skip map name
    demo_p += 8;

    // "real" demover
    demover = *demo_p++;
  }

  // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
  // Old demos turn on demo_compatibility => compatibility; new demos load
  // compatibility flag, and other flags as well, as a part of the demo.

  demo_version = demover;     // killough 7/19/98: use the version id stored in demo

  // [FG] PrBoom's own demo format starts with demo version 210
  if (demover >= 210 && !mbf21)
  {
    INVALID_DEMO("Unknown demo format %d.", demover);
  }

  longtics = false;

  if (demover < 200)     // Autodetect old demos
    {
      if (demover == 111)
      {
        longtics = true;
      }
      compatibility = true;
      memset(comp, 0xff, sizeof comp);  // killough 10/98: a vector now

      // killough 3/2/98: force these variables to be 0 in demo_compatibility

      variable_friction = 0;

      weapon_recoil = 0;

      allow_pushers = 0;

      monster_infighting = 1;           // killough 7/19/98

      classic_bfg = 0;                  // killough 7/19/98
      beta_emulation = 0;               // killough 7/24/98
      
      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98

      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98

      monster_friction = 0;             // killough 10/98
      help_friends = 0;                 // killough 9/9/98
      monkeys = 0;

      // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
      // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

      if ((skill=demover) >= 100)         // For demos from versions >= 1.4
        {
          skill = *demo_p++;
          episode = *demo_p++;
          map = *demo_p++;
          deathmatch = *demo_p++;
          respawnparm = *demo_p++;
          fastparm = *demo_p++;
          nomonsters = *demo_p++;
          consoleplayer = *demo_p++;
        }
      else
        {
          episode = *demo_p++;
          map = *demo_p++;
          deathmatch = respawnparm = fastparm =
            nomonsters = consoleplayer = 0;
        }
    }
  else    // new versions of demos
    {
      demo_p += 6;               // skip signature;

      if (mbf21)
      {
        longtics = true;
        compatibility = 0;
      }
      else
      {
      compatibility = *demo_p++;       // load old compatibility flag
      }
      skill = *demo_p++;
      episode = *demo_p++;
      map = *demo_p++;
      deathmatch = *demo_p++;
      consoleplayer = *demo_p++;

      // killough 11/98: save option pointer for below
      if (demover >= 203)
	option_p = demo_p;

      // killough 3/1/98: Read game options
      if (mbf21)
        demo_p = G_ReadOptionsMBF21(demo_p);
      else
        demo_p = G_ReadOptions(demo_p);

      if (demover == 200)        // killough 6/3/98: partially fix v2.00 demos
        demo_p += 256-G_GameOptionSize();
    }

  if (demo_compatibility)  // only 4 players can exist in old demos
    {
      for (i=0; i<4; i++)  // intentionally hard-coded 4 -- killough
        playeringame[i] = *demo_p++;
      for (;i < MAXPLAYERS; i++)
        playeringame[i] = 0;
    }
  else
    {
      for (i=0 ; i < MAXPLAYERS; i++)
        playeringame[i] = *demo_p++;
      demo_p += MIN_MAXPLAYERS - MAXPLAYERS;
    }

  if (playeringame[1])
    netgame = netdemo = true;

  // don't spend a lot of time in loadlevel

  if (gameaction != ga_loadgame)      // killough 12/98: support -loadgame
    {
      // killough 2/22/98:
      // Do it anyway for timing demos, to reduce timing noise
      precache = timingdemo;
  
      G_InitNew(skill, episode, map);

      // killough 11/98: If OPTIONS were loaded from the wad in G_InitNew(),
      // reload any demo sync-critical ones from the demo itself, to be exactly
      // the same as during recording.
      
      if (option_p)
      {
        if (mbf21)
          G_ReadOptionsMBF21(option_p);
        else
          G_ReadOptions(option_p);
      }
    }

  precache = true;
  usergame = false;
  demoplayback = true;

  for (i=0; i<MAXPLAYERS;i++)         // killough 4/24/98
    players[i].cheats = 0;

  gameaction = ga_nothing;

  maxdemosize = lumplength;

  // [crispy] demo progress bar
  {
    int i;
    int playerscount = 0;
    byte *demo_ptr = demo_p;

    playback_totaltics = playback_tic = 0;

    for (i = 0; i < MAXPLAYERS; ++i)
    {
      if (playeringame[i])
        ++playerscount;
    }

    while (*demo_ptr != DEMOMARKER && (demo_ptr - demobuffer) < lumplength)
    {
      demo_ptr += playerscount * (longtics ? 5 : 4);
      ++playback_totaltics;
    }
  }

  // [FG] report compatibility mode
  I_Printf(VB_INFO, "G_DoPlayDemo: Playing demo with %s (%d) compatibility.",
    G_GetCurrentComplevelName(), demover);
}

#define VERSIONSIZE   16

// killough 2/22/98: version id string format for savegames
#define VERSIONID "MBF %d"

#define CURRENT_SAVE_VERSION "Woof 6.0.0"

static char *savename = NULL;

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

static boolean forced_loadgame = false;
static boolean command_loadgame = false;

void G_ForcedLoadGame(void)
{
  gameaction = ga_loadgame;
  forced_loadgame = true;
}

// killough 3/16/98: add slot info
// killough 5/15/98: add command-line

void G_LoadGame(char *name, int slot, boolean command)
{
  if (savename) free(savename);
  savename = M_StringDuplicate(name);
  savegameslot = slot;
  gameaction = ga_loadgame;
  forced_loadgame = false;
  command_loadgame = command;
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(const char *msg)
{
  Z_Free(savebuffer);                // Free the savegame buffer
  M_ForcedLoadGame(msg);             // Print message asking for 'Y' to force
  if (command_loadgame)              // If this was a command-line -loadgame
    {
      G_CheckDemoStatus();           // If there was also a -record
      D_StartTitle();                // Start the title screen
      gamestate = GS_DEMOSCREEN;     // And set the game state accordingly
    }
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, char *description)
{
  savegameslot = slot;
  strcpy(savedescription, description);
  sendsave = true;
}

// Check for overrun and realloc if necessary -- Lee Killough 1/22/98
void CheckSaveGame(size_t size)
{
  size_t pos = save_p - savebuffer;
  size += 1024;  // breathing room
  if (pos+size > savegamesize)
    save_p = (savebuffer = Z_Realloc(savebuffer,
           savegamesize += (size+1023) & ~1023, PU_STATIC, 0)) + pos;
}

// killough 3/22/98: form savegame name in one location
// (previously code was scattered around in multiple places)

// [FG] support up to 8 pages of savegames
extern int savepage;

char* G_SaveGameName(int slot)
{
  // Ty 05/04/98 - use savegamename variable (see d_deh.c)
  // killough 12/98: add .7 to truncate savegamename
  char buf[16] = {0};
  sprintf(buf, "%.7s%d.dsg", savegamename, 10*savepage+slot);

  return M_StringJoin(basesavegame, DIR_SEPARATOR_S, buf, NULL);
}

char* G_MBFSaveGameName(int slot)
{
   char buf[16] = {0};
   sprintf(buf, "MBFSAV%d.dsg", 10*savepage+slot);
   return M_StringJoin(basesavegame, DIR_SEPARATOR_S, buf, NULL);
}

// killough 12/98:
// This function returns a signature for the current wad.
// It is used to distinguish between wads, for the purposes
// of savegame compatibility warnings, and options lookups.
//
// killough 12/98: use faster algorithm which has less IO

static uint64_t G_Signature(int sig_epi, int sig_map)
{
  uint64_t s = 0;
  int lump, i;
  char name[9];
  
  strcpy(name, MAPNAME(sig_epi, sig_map));

  lump = W_CheckNumForName(name);

  if (lump != -1 && (i = lump+10) < numlumps)
    do
      s = s*2+W_LumpLength(i);
    while (--i > lump);

  return s;
}

static void G_DoSaveGame(void)
{
  char *name = NULL;
  char name2[VERSIONSIZE];
  char *description;
  int  length, i;

  name = G_SaveGameName(savegameslot);

  description = savedescription;

  save_p = savebuffer = Z_Malloc(savegamesize, PU_STATIC, 0);

  CheckSaveGame(SAVESTRINGSIZE+VERSIONSIZE+sizeof(uint64_t));
  memcpy (save_p, description, SAVESTRINGSIZE);
  save_p += SAVESTRINGSIZE;
  memset (name2,0,sizeof(name2));

  // killough 2/22/98: "proprietary" version string :-)
  strcpy(name2, CURRENT_SAVE_VERSION);
  saveg_compat = saveg_current;

  memcpy (save_p, name2, VERSIONSIZE);
  save_p += VERSIONSIZE;

  *save_p++ = demo_version;

  // killough 2/14/98: save old compatibility flag:
  *save_p++ = compatibility;

  *save_p++ = gameskill;
  *save_p++ = gameepisode;
  *save_p++ = gamemap;

  {  // killough 3/16/98, 12/98: store lump name checksum
    uint64_t checksum = G_Signature(gameepisode, gamemap);
    saveg_write64(checksum);
  }

  // killough 3/16/98: store pwad filenames in savegame
  {
    char **w = wadfiles;
    for (*save_p = 0; *w; w++)
      {
        const char *basename = M_BaseName(*w);
        CheckSaveGame(strlen(basename)+2);
        strcat(strcat((char *) save_p, basename), "\n");
      }
    save_p += strlen((char *) save_p)+1;
  }

  CheckSaveGame(G_GameOptionSize()+MIN_MAXPLAYERS+10);

  for (i=0 ; i<MAXPLAYERS ; i++)
    *save_p++ = playeringame[i];

  for (;i<MIN_MAXPLAYERS;i++)         // killough 2/28/98
    *save_p++ = 0;

  *save_p++ = idmusnum;               // jff 3/17/98 save idmus state

  save_p = G_WriteOptions(save_p);    // killough 3/1/98: save game options

  // [FG] fix copy size and pointer progression
  saveg_write32(leveltime); //killough 11/98: save entire word

  // killough 11/98: save revenant tracer state
  *save_p++ = (gametic-basetic) & 255;

  P_ArchivePlayers();
  P_ArchiveWorld();
  P_ArchiveThinkers();
  P_ArchiveSpecials();
  P_ArchiveRNG();    // killough 1/18/98: save RNG information
  P_ArchiveMap();    // killough 1/22/98: save automap information

  *save_p++ = 0xe6;   // consistancy marker

  // [FG] save total time for all completed levels
  CheckSaveGame(sizeof totalleveltimes);
  saveg_write32(totalleveltimes);

  // save lump name for current MUSINFO item
  CheckSaveGame(8);
  if (musinfo.current_item > 0)
    memcpy(save_p, lumpinfo[musinfo.current_item].name, 8);
  else
    memset(save_p, 0, 8);
  save_p += 8;

  // save extrakills
  CheckSaveGame(sizeof extrakills);
  saveg_write32(extrakills);

  // [FG] save snapshot
  CheckSaveGame(M_SnapshotDataSize());
  M_WriteSnapshot(save_p);
  save_p += M_SnapshotDataSize();

  length = save_p - savebuffer;

  if (!M_WriteFile(name, savebuffer, length))
    displaymsg("%s", errno ? strerror(errno) : "Could not save game: Error unknown");
  else
    displaymsg("%s", s_GGSAVED);  // Ty 03/27/98 - externalized

  Z_Free(savebuffer);  // killough
  savebuffer = save_p = NULL;

  gameaction = ga_nothing;
  savedescription[0] = 0;

  if (name) free(name);

  M_SetQuickSaveSlot(savegameslot);
}

static void G_DoLoadGame(void)
{
  int  length, i;
  char vcheck[VERSIONSIZE];
  uint64_t checksum;
  int tmp_compat, tmp_skill, tmp_epi, tmp_map;

  I_SetFastdemoTimer(false);

  // [crispy] loaded game must always be single player.
  // Needed for ability to use a further game loading, as well as
  // cheat codes and other single player only specifics.
  if (!command_loadgame)
  {
    netdemo = false;
    netgame = false;
    solonet = false;
    deathmatch = false;
  }

  gameaction = ga_nothing;

  length = M_ReadFile(savename, &savebuffer);
  save_p = savebuffer + SAVESTRINGSIZE;

  // skip the description field

  // killough 2/22/98: "proprietary" version string :-)
  sprintf (vcheck,VERSIONID,MBFVERSION);

  if (strncmp((char *) save_p, CURRENT_SAVE_VERSION, strlen(CURRENT_SAVE_VERSION)) == 0)
  {
    saveg_compat = saveg_current;
  }

  // killough 2/22/98: Friendly savegame version difference message
  if (!forced_loadgame && strncmp((char *) save_p, vcheck, VERSIONSIZE) &&
                          saveg_compat != saveg_current)
    {
      G_LoadGameErr("Different Savegame Version!!!\n\nAre you sure?");
      return;
    }

  save_p += VERSIONSIZE;

  if (saveg_compat > saveg_woof510)
  {
    demo_version = *save_p++;
  }
  else
  {
    demo_version = 203;
  }

  // killough 2/14/98: load compatibility mode
  tmp_compat = *save_p++;

  tmp_skill = *save_p++;
  tmp_epi = *save_p++;
  tmp_map = *save_p++;

  checksum = saveg_read64();

  if (!forced_loadgame)
   {  // killough 3/16/98, 12/98: check lump name checksum
     if (checksum != G_Signature(tmp_epi, tmp_map))
       {
	 char *msg = malloc(strlen((char *) save_p) + 128);
	 strcpy(msg,"Incompatible Savegame!!!\n");
	 if (save_p[sizeof checksum])
	   strcat(strcat(msg,"Wads expected:\n\n"), (char *) save_p);
	 strcat(msg, "\nAre you sure?");
	 G_LoadGameErr(msg);
	 free(msg);
	 return;
       }
   }

  while (*save_p++);

  compatibility = tmp_compat;
  gameskill = tmp_skill;
  gameepisode = tmp_epi;
  gamemap = tmp_map;
  gamemapinfo = G_LookupMapinfo(gameepisode, gamemap);

  for (i=0 ; i<MAXPLAYERS ; i++)
    playeringame[i] = *save_p++;
  save_p += MIN_MAXPLAYERS-MAXPLAYERS;         // killough 2/28/98

  // jff 3/17/98 restore idmus music
  // jff 3/18/98 account for unsigned byte
  // killough 11/98: simplify
  idmusnum = *(signed char *) save_p++;

  /* cph 2001/05/23 - Must read options before we set up the level */
  if (mbf21)
    G_ReadOptionsMBF21(save_p);
  else
    G_ReadOptions(save_p);

  // load a base level
  G_InitNew(gameskill, gameepisode, gamemap);

  // killough 3/1/98: Read game options
  // killough 11/98: move down to here
  /* cph - MBF needs to reread the savegame options because G_InitNew
   * rereads the WAD options. The demo playback code does this too. */
  if (mbf21)
    save_p = G_ReadOptionsMBF21(save_p);
  else
    save_p = G_ReadOptions(save_p);

  // get the times
  // killough 11/98: save entire word
  // [FG] fix copy size and pointer progression
  leveltime = saveg_read32();

  // killough 11/98: load revenant tracer state
  basetic = gametic - (int) *save_p++;

  // dearchive all the modifications
  P_MapStart();
  P_UnArchivePlayers();
  P_UnArchiveWorld();
  P_UnArchiveThinkers();
  P_UnArchiveSpecials();
  P_UnArchiveRNG();    // killough 1/18/98: load RNG information
  P_UnArchiveMap();    // killough 1/22/98: load automap information
  P_MapEnd();

  if (*save_p != 0xe6)
    I_Error ("Bad savegame");

  // [FG] restore total time for all completed levels
  if (save_p++ - savebuffer < length - sizeof totalleveltimes)
  {
    totalleveltimes = saveg_read32();
  }

  // restore MUSINFO music
  if (save_p - savebuffer <= length - 8)
  {
    char lump[9] = {0};
    int i;

    memcpy(lump, save_p, 8);

    i = W_CheckNumForName(lump);

    if (lump[0] && i > 0)
    {
      memset(&musinfo, 0, sizeof(musinfo));
      musinfo.current_item = i;
      musinfo.from_savegame = true;
      S_ChangeMusInfoMusic(i, true);
    }

    save_p += 8;
  }

  // restore extrakills
  if (save_p - savebuffer <= length - sizeof extrakills)
  {
    extrakills = saveg_read32();
  }

  // done
  Z_Free(savebuffer);

  // [Alaux] Update smooth count values;
  // the same procedure is done in G_LoadLevel, but we have to repeat it here
  st_health = players[displayplayer].health;
  st_armor  = players[displayplayer].armorpoints;

  if (setsizeneeded)
    R_ExecuteSetViewSize();

  // draw the pattern into the back screen
  R_FillBackScreen();

  // killough 12/98: support -recordfrom and -loadgame -playdemo
  if (!command_loadgame)
    singledemo = false;         // Clear singledemo flag if loading from menu
  else
    if (singledemo)
      {
	gameaction = ga_loadgame; // Mark that we're loading a game before demo
	G_DoPlayDemo();           // This will detect it and won't reinit level
      }
    else       // Loading games from menu isn't allowed during demo recordings,
      if (demorecording) // So this can only possibly be a -recordfrom command.
	G_BeginRecording();// Start the -recordfrom, since the game was loaded.

  // [FG] log game loading
  {
    char *maplump = MAPNAME(gameepisode, gamemap);
    int maplumpnum = W_CheckNumForName(maplump);

    I_Printf(VB_INFO, "G_DoLoadGame: Slot %d, %.8s (%s)",
      10*savepage+savegameslot, maplump, W_WadNameForLump(maplumpnum));
  }

  M_SetQuickSaveSlot(savegameslot);
}

boolean clean_screenshot;

void G_CleanScreenshot(void)
{
  int old_screenblocks;
  boolean old_hide_weapon;
  extern void ST_ResetPalette(void);

  ST_ResetPalette();

  old_screenblocks = screenblocks;
  old_hide_weapon = hide_weapon;
  hide_weapon = true;
  R_SetViewSize(11);
  R_ExecuteSetViewSize();
  R_RenderPlayerView(&players[displayplayer]);
  R_SetViewSize(old_screenblocks);
  hide_weapon = old_hide_weapon;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//

void G_Ticker(void)
{
  int i;

  // do player reborns if needed
  P_MapStart();
  for (i=0 ; i<MAXPLAYERS ; i++)
    if (playeringame[i] && players[i].playerstate == PST_REBORN)
      G_DoReborn (i);
  P_MapEnd();

  // do things to change the game state
  while (gameaction != ga_nothing)
    switch (gameaction)
      {
      case ga_loadlevel:
	G_DoLoadLevel();
	break;
      case ga_newgame:
	G_DoNewGame();
	break;
      case ga_loadgame:
	G_DoLoadGame();
	break;
      case ga_savegame:
	G_DoSaveGame();
	break;
      case ga_playdemo:
	G_DoPlayDemo();
	break;
      case ga_completed:
	G_DoCompleted();
	break;
      case ga_victory:
	F_StartFinale();
	break;
      case ga_worlddone:
	G_DoWorldDone();
	break;
      case ga_screenshot:
	if (clean_screenshot)
	{
	  G_CleanScreenshot();
	  clean_screenshot = false;
	}
	M_ScreenShot();
	gameaction = ga_nothing;
	break;
      case ga_reloadlevel:
	G_ReloadLevel();
	break;
      default:  // killough 9/29/98
	gameaction = ga_nothing;
	break;
    }

  // killough 10/6/98: allow games to be saved during demo
  // playback, by the playback user (not by demo itself)

  if (demoplayback && sendsave)
    {
      sendsave = false;
      gameaction = ga_savegame;
    }

  // killough 9/29/98: Skip some commands while pausing during demo
  // playback, or while menu is active.
  //
  // We increment basetic and skip processing if a demo being played
  // back is paused or if the menu is active while a non-net game is
  // being played, to maintain sync while allowing pauses.
  //
  // P_Ticker() does not stop netgames if a menu is activated, so
  // we do not need to stop if a menu is pulled up during netgames.

  if (paused & 2 || (!demoplayback && menuactive && !netgame))
    basetic++;  // For revenant tracers and RNG -- we must maintain sync
  else
    {
      // get commands, check consistancy, and build new consistancy check
      int buf = (gametic/ticdup)%BACKUPTICS;

      for (i=0 ; i<MAXPLAYERS ; i++)
	{
	  if (playeringame[i])
	    {
	      ticcmd_t *cmd = &players[i].cmd;

	      memcpy(cmd, &netcmds[i], sizeof *cmd);

	      // catch BT_JOIN before G_ReadDemoTiccmd overwrites it
	      if (demoplayback && cmd->buttons & BT_JOIN)
		G_JoinDemo();

	      // catch BTS_RELOAD for demo playback restart
	      if (demoplayback &&
	          cmd->buttons & BT_SPECIAL && cmd->buttons & BT_SPECIALMASK &&
	          cmd->buttons & BTS_RELOAD)
	      {
	        playback_tic = 0;
	        gameaction = ga_playdemo;
	      }

	      if (demoplayback)
		G_ReadDemoTiccmd(cmd);

	      // [crispy] do not record tics while still playing back in demo
	      // continue mode
	      if (demorecording && !demoplayback)
		G_WriteDemoTiccmd(cmd);

	      // check for turbo cheats
	      // killough 2/14/98, 2/20/98 -- only warn in netgames and demos

	      if ((netgame || demoplayback) && 
		  cmd->forwardmove > TURBOTHRESHOLD &&
		  !(gametic&31) && ((gametic>>5)&3) == i )
		{
		  extern char **player_names[];
		  displaymsg("%s is turbo!", *player_names[i]); // killough 9/29/98
		}

	      if (netgame && !netdemo && !(gametic%ticdup) )
		{
		  if (gametic > BACKUPTICS
		      && consistancy[i][buf] != cmd->consistancy)
		    I_Error ("consistency failure (%i should be %i)",
			     cmd->consistancy, consistancy[i][buf]);
		  if (players[i].mo)
		    consistancy[i][buf] = players[i].mo->x;
		  else
		    consistancy[i][buf] = 0; // killough 2/14/98
		}
	    }
	}

      if (demoplayback)
        ++playback_tic;

      // check for special buttons
      for (i=0; i<MAXPLAYERS; i++)
	if (playeringame[i] && players[i].cmd.buttons & BT_SPECIAL)
	  switch (players[i].cmd.buttons & BT_SPECIALMASK)
	  {
	    case BTS_RELOAD:
	      if (!demoplayback) // ignore in demos
	      {
	        gameaction = ga_reloadlevel;
	      }
	      break;

	    case BTS_PAUSE:
	      if ((paused ^= 1))
		S_PauseSound();
	      else
		S_ResumeSound();
	      break;
	
	    case BTS_SAVEGAME:
		if (!savedescription[0])
		  strcpy(savedescription, "NET GAME");
		savegameslot =
		  (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
		gameaction = ga_savegame;
	      break;

	    default:
	      break;
	  }
    }

  oldleveltime = leveltime;

  // do main actions

  // killough 9/29/98: split up switch statement
  // into pauseable and unpauseable parts.

  gamestate == GS_LEVEL ? P_Ticker(), ST_Ticker(), AM_Ticker(), HU_Ticker() :
    paused & 2 ? (void) 0 :
      gamestate == GS_INTERMISSION ? WI_Ticker() :
	gamestate == GS_FINALE ? F_Ticker() :
	  gamestate == GS_DEMOSCREEN ? D_PageTicker() : (void) 0;
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//

void G_PlayerReborn(int player)
{
  player_t *p;
  int i;
  int frags[MAXPLAYERS];
  int killcount;
  int itemcount;
  int secretcount;

  memcpy (frags, players[player].frags, sizeof frags);
  killcount = players[player].killcount;
  itemcount = players[player].itemcount;
  secretcount = players[player].secretcount;

  p = &players[player];

  // killough 3/10/98,3/21/98: preserve cheats across idclev
  {
    int cheats = p->cheats;
    memset (p, 0, sizeof(*p));
    p->cheats = cheats;
  }

  memcpy(players[player].frags, frags, sizeof(players[player].frags));
  players[player].killcount = killcount;
  players[player].itemcount = itemcount;
  players[player].secretcount = secretcount;

  p->usedown = p->attackdown = true;  // don't do anything immediately
  p->playerstate = PST_LIVE;
  p->health = initial_health;  // Ty 03/12/98 - use dehacked values
  p->readyweapon = p->pendingweapon = wp_pistol;
  p->weaponowned[wp_fist] = true;
  p->weaponowned[wp_pistol] = true;
  p->ammo[am_clip] = initial_bullets; // Ty 03/12/98 - use dehacked values

  for (i=0 ; i<NUMAMMO ; i++)
    p->maxammo[i] = maxammo[i];
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//

void P_SpawnPlayer(mapthing_t *mthing);

static boolean G_CheckSpot(int playernum, mapthing_t *mthing)
{
  fixed_t     x,y;
  subsector_t *ss;
  mobj_t      *mo;
  int         i;

  if (!players[playernum].mo)
    {
      // first spawn of level, before corpses
      for (i=0 ; i<playernum ; i++)
        if (players[i].mo->x == mthing->x << FRACBITS
            && players[i].mo->y == mthing->y << FRACBITS)
          return false;
      return true;
    }

  x = mthing->x << FRACBITS;
  y = mthing->y << FRACBITS;

  // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
  // corpse to detect collisions with other players in DM starts
  //
  // Old code:
  // if (!P_CheckPosition (players[playernum].mo, x, y))
  //    return false;

  players[playernum].mo->flags |=  MF_SOLID;
  i = P_CheckPosition(players[playernum].mo, x, y);
  players[playernum].mo->flags &= ~MF_SOLID;
  if (!i)
    return false;

  // flush an old corpse if needed
  // killough 2/8/98: make corpse queue have an adjustable limit
  // killough 8/1/98: Fix bugs causing strange crashes

  if (bodyquesize > 0)
    {
      static mobj_t **bodyque;
      static size_t queuesize;
      if (queuesize < bodyquesize)
	{
	  bodyque = Z_Realloc(bodyque, bodyquesize*sizeof*bodyque, PU_STATIC, 0);
	  memset(bodyque+queuesize, 0, 
		 (bodyquesize-queuesize)*sizeof*bodyque);
	  queuesize = bodyquesize;
	}
      if (bodyqueslot >= bodyquesize) 
	P_RemoveMobj(bodyque[bodyqueslot % bodyquesize]); 
      bodyque[bodyqueslot++ % bodyquesize] = players[playernum].mo; 
    }
  else
    if (!bodyquesize)
      P_RemoveMobj(players[playernum].mo);

  // spawn a teleport fog
  ss = R_PointInSubsector (x,y);

  // The code in the released source looks like this:
  //
  //    an = ( ANG45 * (((unsigned int) mthing->angle)/45) )
  //         >> ANGLETOFINESHIFT;
  //    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
  //                     , ss->sector->floorheight
  //                     , MT_TFOG);
  //
  // But 'an' can be a signed value in the DOS version. This means that
  // we get a negative index and the lookups into finecosine/finesine
  // end up dereferencing values in finetangent[].
  // A player spawning on a deathmatch start facing directly west spawns
  // "silently" with no spawn fog. Emulate this.
  //
  // This code is imported from PrBoom+.

  {
    fixed_t xa, ya;
    signed int an;

    // This calculation overflows in Vanilla Doom, but here we deliberately
    // avoid integer overflow as it is undefined behavior, so the value of
    // 'an' will always be positive.
    an = (ANG45 >> ANGLETOFINESHIFT) * ((signed int) mthing->angle / 45);

    if (demo_compatibility)
      switch (an)
      {
        case 4096:  // -4096:
            xa = finetangent[2048];    // finecosine[-4096]
            ya = finetangent[0];       // finesine[-4096]
            break;
        case 5120:  // -3072:
            xa = finetangent[3072];    // finecosine[-3072]
            ya = finetangent[1024];    // finesine[-3072]
            break;
        case 6144:  // -2048:
            xa = finesine[0];          // finecosine[-2048]
            ya = finetangent[2048];    // finesine[-2048]
            break;
        case 7168:  // -1024:
            xa = finesine[1024];       // finecosine[-1024]
            ya = finetangent[3072];    // finesine[-1024]
            break;
        case 0:
        case 1024:
        case 2048:
        case 3072:
            xa = finecosine[an];
            ya = finesine[an];
            break;
        default:
            I_Error("G_CheckSpot: unexpected angle %d\n", an);
            xa = ya = 0;
            break;
      }
    else
    {
      xa = finecosine[an];
      ya = finesine[an];
    }
    mo = P_SpawnMobj(x + 20 * xa, y + 20 * ya,
                     ss->sector->floorheight, MT_TFOG);
  }

  if (players[consoleplayer].viewz != 1)
    S_StartSound(mo, sfx_telept);  // don't start sound on first frame

  return true;
}

//
// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//

void G_DeathMatchSpawnPlayer(int playernum)
{
  int j, selections = deathmatch_p - deathmatchstarts;

  if (selections < MAXPLAYERS)
    I_Error("Only %i deathmatch spots, %d required", selections, MAXPLAYERS);

  for (j=0 ; j<20 ; j++)
    {
      int i = P_Random(pr_dmspawn) % selections;
      if (G_CheckSpot(playernum, &deathmatchstarts[i]) )
        {
          deathmatchstarts[i].type = playernum+1;
          P_SpawnPlayer (&deathmatchstarts[i]);
          return;
        }
    }

  // no good spot, so the player will probably get stuck
  P_SpawnPlayer (&playerstarts[playernum]);
}

//
// G_DoReborn
//

void G_DoReborn(int playernum)
{
  if (!netgame)
  {
    if (gameaction != ga_reloadlevel)
      gameaction = ga_loadlevel;      // reload the level from scratch
  }
  else
    {                               // respawn at the start
      int i;

      // first dissasociate the corpse
      players[playernum].mo->player = NULL;

      // spawn at random spot if in death match
      if (deathmatch)
        {
          G_DeathMatchSpawnPlayer (playernum);
          return;
        }

      if (G_CheckSpot (playernum, &playerstarts[playernum]) )
        {
          P_SpawnPlayer (&playerstarts[playernum]);
          return;
        }

      // try to spawn at one of the other players spots
      for (i=0 ; i<MAXPLAYERS ; i++)
        {
          if (G_CheckSpot (playernum, &playerstarts[i]) )
            {
              playerstarts[i].type = playernum+1; // fake as other player
              P_SpawnPlayer (&playerstarts[i]);
              playerstarts[i].type = i+1;   // restore
              return;
            }
          // he's going to be inside something.  Too bad.
        }
      P_SpawnPlayer (&playerstarts[playernum]);
    }
}

void G_ScreenShot(void)
{
  gameaction = ga_screenshot;
}

// DOOM Par Times
int pars[4][10] = {
  {0},
  {0,30,75,120,90,165,180,180,30,165},
  {0,90,90,90,120,90,360,240,30,170},
  {0,90,45,90,150,90,90,165,30,135}
};

// DOOM II Par Times
int cpars[34] = {
  30,90,120,120,90,150,120,120,270,90,  //  1-10
  210,150,150,150,210,150,420,150,210,150,  // 11-20
  240,150,180,150,150,300,330,420,300,180,  // 21-30
  120,30,30,30          // 31-34
};

//
// G_WorldDone
//

void G_WorldDone(void)
{
  gameaction = ga_worlddone;

  if (secretexit)
    players[consoleplayer].didsecret = true;

  if (gamemapinfo)
  {
    if (gamemapinfo->intertextsecret && secretexit)
    {
      if (U_CheckField(gamemapinfo->intertextsecret)) // if the intermission was not cleared
        F_StartFinale();
      return;
    }
    else if (gamemapinfo->intertext && !secretexit)
    {
      if (U_CheckField(gamemapinfo->intertext)) // if the intermission was not cleared
        F_StartFinale();
      return;
    }
    else if (U_CheckField(gamemapinfo->endpic) && !secretexit)
    {
      // game ends without a status screen.
      gameaction = ga_victory;
      return;
    }
    // if nothing applied, use the defaults.
  }

  if (gamemode == commercial)
    {
      switch (gamemap)
        {
        case 15:
        case 31:
          if (!secretexit)
            break;
        case 6:
        case 11:
        case 20:
        case 30:
          F_StartFinale();
          break;
        }
    }
}

static skill_t d_skill;
static int     d_episode;
static int     d_map;

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
  d_skill = skill;
  d_episode = episode;
  d_map = map;
  gameaction = ga_newgame;
  musinfo.from_savegame = false;

  if (demorecording)
  {
    ddt_cheating = 0;
    G_CheckDemoStatus();
    G_RecordDemo(orig_demoname);
  }
}

// killough 7/19/98: Marine's best friend :)
static int G_GetHelpers(void)
{
  //!
  // @category game
  //
  // Enables a single helper dog.
  //

  int j = M_CheckParm ("-dog");

  if (!j)

    //!
    // @arg <n>
    // @category game
    //
    // Overrides the current number of helper dogs, setting it to n.
    //

    j = M_CheckParm ("-dogs");
  return j ? (j+1 < myargc && myargv[j+1][0] != '-') ? M_ParmArgToInt(j) : 1 : default_dogs;
}

// [FG] support named complevels on the command line, e.g. "-complevel boom",

int G_GetNamedComplevel (const char *arg)
{
  int i;

  const struct {
    int level;
    const char *const name;
    int exe;
  } named_complevel[] = {
    {109, "vanilla", -1},
    {109, "doom2",   exe_doom_1_9},
    {109, "1.9",     exe_doom_1_9},
    {109, "2",       exe_doom_1_9},
    {109, "ultimate",exe_ultimate},
    {109, "3",       exe_ultimate},
    {109, "final",   exe_final},
    {109, "tnt",     exe_final},
    {109, "plutonia",exe_final},
    {109, "4",       exe_final},
    {202, "boom",    -1},
    {202, "9",       -1},
    {203, "mbf",     -1},
    {203, "11",      -1},
    {221, "mbf21",   -1},
    {221, "21",      -1},
  };

  for (i = 0; i < sizeof(named_complevel)/sizeof(*named_complevel); i++)
  {
    if (!strcasecmp(arg, named_complevel[i].name))
    {
      if (named_complevel[i].exe >= 0)
        gameversion = named_complevel[i].exe;

      return named_complevel[i].level;
    }
  }

  return -1;
}

static int G_GetDefaultComplevel()
{
  switch (default_complevel)
  {
    case 0:
      return 109;
    case 1:
      return 202;
    case 2:
      return 203;
    default:
      return 221;
  }
}

const char *G_GetCurrentComplevelName(void)
{
  switch (demo_version)
  {
    case 109:
      return gameversions[gameversion].description;
    case 202:
      return "Boom";
    case 203:
      return "MBF";
    case 221:
      return "MBF21";
    default:
      return "Unknown";
  }
}

static int G_GetWadComplevel(void)
{
  int lumpnum;

  lumpnum = W_CheckNumForName("COMPLVL");

  if (lumpnum >= 0)
  {
    int length;
    char *data = NULL;

    length = W_LumpLength(lumpnum);
    data = W_CacheLumpNum(lumpnum, PU_CACHE);

    if (length == 7 && !strncasecmp("vanilla", data, 7))
      return 109;
    else if (length == 4 && !strncasecmp("boom", data, 4))
      return 202;
    else if (length == 3 && !strncasecmp("mbf", data, 3))
      return 203;
    else if (length == 5 && !strncasecmp("mbf21", data, 5))
      return 221;
  }

  return -1;
}

static void G_MBFDefaults(void)
{
  weapon_recoil = 0;
  monsters_remember = 1;
  monster_infighting = 1;
  monster_backing = 0;
  monster_avoid_hazards = 1;
  monkeys = 0;
  monster_friction = 1;
  help_friends = 0;
  dogs = 0;
  distfriend = 128;
  dog_jumping = 1;

  memset(comp, 0, sizeof comp);

  comp[comp_zombie] = 1;
}

static void G_MBF21Defaults(void)
{
  G_MBFDefaults();

  comp[comp_pursuit] = 1;

  comp[comp_respawn] = 0;
  comp[comp_soul] = 0;
  comp[comp_ledgeblock] = 1;
  comp[comp_friendlyspawn] = 1;
  comp[comp_voodooscroller] = 0;
  comp[comp_reservedlineflag] = 1;
}

static void G_MBFComp()
{
  comp[comp_respawn] = 1;
  comp[comp_soul] = 1;
  comp[comp_ledgeblock] = 0;
  comp[comp_friendlyspawn] = 1;
  comp[comp_voodooscroller] = 1;
  comp[comp_reservedlineflag] = 0;
}

static void G_BoomComp()
{
  comp[comp_telefrag] = 1;
  comp[comp_dropoff]  = 0;
  comp[comp_falloff]  = 1;
  comp[comp_skymap]   = 1;
  comp[comp_pursuit]  = 1;
  comp[comp_staylift] = 1;
  comp[comp_zombie]   = 1;
  comp[comp_infcheat] = 1;

  comp[comp_respawn] = 1;
  comp[comp_soul] = 1;
  comp[comp_ledgeblock] = 0;
  comp[comp_friendlyspawn] = 1;
  comp[comp_voodooscroller] = 0;
  comp[comp_reservedlineflag] = 0;
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(boolean keep_demover)
{
  // killough 3/1/98: Initialize options based on config file
  // (allows functions above to load different values for demos
  // and savegames without messing up defaults).

  weapon_recoil = default_weapon_recoil;    // weapon recoil

  player_bobbing = default_player_bobbing;  // whether player bobs or not

  variable_friction = allow_pushers = true;

  monsters_remember = default_monsters_remember;   // remember former enemies

  monster_infighting = default_monster_infighting; // killough 7/19/98

  dogs = netgame ? 0 : G_GetHelpers();             // killough 7/19/98
  dog_jumping = default_dog_jumping;

  distfriend = default_distfriend;                 // killough 8/8/98

  monster_backing = default_monster_backing;     // killough 9/8/98

  monster_avoid_hazards = default_monster_avoid_hazards; // killough 9/9/98

  monster_friction = default_monster_friction;     // killough 10/98

  help_friends = default_help_friends;             // killough 9/9/98

  monkeys = default_monkeys;

  classic_bfg = default_classic_bfg;               // killough 7/19/98
  beta_emulation = !!M_CheckParm("-beta");         // killough 7/24/98

  // jff 1/24/98 reset play mode to command line spec'd version
  // killough 3/1/98: moved to here
  respawnparm = clrespawnparm;
  fastparm = clfastparm;
  nomonsters = clnomonsters;

  //jff 3/24/98 set startskill from defaultskill in config file, unless
  // it has already been set by a -skill parameter
  if (startskill==sk_default)
    startskill = (skill_t)(defaultskill-1);

  demoplayback = false;
  singledemo = false;            // killough 9/29/98: don't stop after 1 demo
  netdemo = false;

  // killough 2/21/98:
  memset(playeringame+1, 0, sizeof(*playeringame)*(MAXPLAYERS-1));

  consoleplayer = 0;

  compatibility = false;     // killough 10/98: replaced by comp[] vector
  memcpy(comp, default_comp, sizeof comp);

  if (!keep_demover)
  {
    int i;

    demo_version = G_GetWadComplevel();

    //!
    // @arg <version>
    // @category compat
    // @help
    //
    // Emulate a specific version of Doom/Boom/MBF. Valid values are
    // "vanilla", "boom", "mbf", "mbf21".
    //

    i = M_CheckParmWithArgs("-complevel", 1);

    if (i > 0)
    {
      int l = G_GetNamedComplevel(myargv[i+1]);
      if (l > -1)
        demo_version = l;
      else
        I_Error("Invalid parameter '%s' for -complevel, "
                "valid values are vanilla, boom, mbf, mbf21.",
                myargv[i+1]);
    }

    if (demo_version == -1)
      demo_version = G_GetDefaultComplevel();
  }

  strictmode = default_strictmode;

  //!
  // @category demo
  // @help
  //
  // Sets compatibility and cosmetic settings according to DSDA rules.
  //

  if (M_CheckParm("-strict"))
    strictmode = true;

  P_UpdateDirectVerticalAiming();

  pistolstart = default_pistolstart;

  //!
  // @category game
  // @help
  //
  // Enables automatic pistol starts on each level.
  //

  if (M_CheckParm("-pistolstart"))
    pistolstart = true;

  // Reset MBF compatibility options in strict mode
  if (strictmode)
  {
    if (demo_version == 203)
      G_MBFDefaults();
    else if (mbf21)
      G_MBF21Defaults();
  }

  D_SetMaxHealth();

  D_SetBloodColor();

  D_SetPredefinedTranslucency();

  if (!mbf21)
  {
    // Set new compatibility options
    G_MBFComp();
  }

  // killough 3/31/98, 4/5/98: demo sync insurance
  demo_insurance = 0;

  // haleyjd
  rngseed = time(NULL);

  if (beta_emulation && demo_version != 203)
    I_Error("G_ReloadDefaults: Beta emulation requires complevel MBF.");

  if ((M_CheckParm("-dog") || M_CheckParm("-dogs")) && demo_version < 203)
    I_Error("G_ReloadDefaults: Helper dogs require complevel MBF or MBF21.");

  if (demo_version < 203)
  {
    monster_infighting = 1;
    monster_backing = 0;
    monster_avoid_hazards = 0;
    monster_friction = 0;
    help_friends = 0;

    dogs = 0;
    dog_jumping = 0;

    monkeys = 0;

    if (demo_version == 109)
    {
      compatibility = true;
      memset(comp, 0xff, sizeof comp);
    }
    else if (demo_version == 202)
    {
      memset(comp, 0, sizeof comp);
      G_BoomComp();
    }
  }
  else if (mbf21)
  {
    // These are not configurable
    variable_friction = 1;
    allow_pushers = 1;
    classic_bfg = 0;
  }

  M_ResetSetupMenu();
}

void G_DoNewGame (void)
{
  I_SetFastdemoTimer(false);
  G_ReloadDefaults(false); // killough 3/1/98
  netgame = false;               // killough 3/29/98
  solonet = false;
  deathmatch = false;
  basetic = gametic;             // killough 9/29/98

  G_InitNew(d_skill, d_episode, d_map);
  gameaction = ga_nothing;

  if (demorecording)
    G_BeginRecording();
}

// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.

void G_SetFastParms(int fast_pending)
{
  static int fast = 0;            // remembers fast state
  int i;

  if (fast != fast_pending)       // only change if necessary
  {
    for (i = 0; i < num_mobj_types; ++i)
      if (mobjinfo[i].altspeed != NO_ALTSPEED)
      {
        int swap = mobjinfo[i].speed;
        mobjinfo[i].speed = mobjinfo[i].altspeed;
        mobjinfo[i].altspeed = swap;
      }

    if ((fast = fast_pending))
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST && (states[i].tics != 1 || demo_compatibility))
          states[i].tics >>= 1;  // don't change 1->0 since it causes cycles
    }
    else
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST)
          states[i].tics <<= 1;
    }
  }
}

mapentry_t *G_LookupMapinfo(int episode, int map)
{
  int i;
  char lumpname[9];

  strcpy(lumpname, MAPNAME(episode, map));

  for (i = 0; i < U_mapinfo.mapcount; i++)
  {
    if (!strcasecmp(lumpname, U_mapinfo.maps[i].mapname))
      return &U_mapinfo.maps[i];
  }

  for (i = 0; i < default_mapinfo.mapcount; i++)
  {
    if (!strcasecmp(lumpname, default_mapinfo.maps[i].mapname))
      return &default_mapinfo.maps[i];
  }

  return NULL;
}

// Check if the given map name can be expressed as a gameepisode/gamemap pair
// and be reconstructed from it.
int G_ValidateMapName(const char *mapname, int *pEpi, int *pMap)
{
  char lumpname[9], mapuname[9];
  int epi = -1, map = -1;

  if (strlen(mapname) > 8)
    return 0;
  strncpy(mapuname, mapname, 8);
  mapuname[8] = 0;
  M_ForceUppercase(mapuname);

  if (gamemode != commercial)
  {
    if (sscanf(mapuname, "E%dM%d", &epi, &map) != 2)
      return 0;
    strcpy(lumpname, MAPNAME(epi, map));
  }
  else
  {
    if (sscanf(mapuname, "MAP%d", &map) != 1)
      return 0;
    strcpy(lumpname, MAPNAME(epi = 1, map));
  }

  if (epi > 4)
    EpiCustom = true;

  if (pEpi)
    *pEpi = epi;
  if (pMap)
    *pMap = map;

  return !strcmp(mapuname, lumpname);
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.

void G_InitNew(skill_t skill, int episode, int map)
{
  int i;

  if (paused)
    {
      paused = false;
      S_ResumeSound();
    }

  if (skill > sk_nightmare)
    skill = sk_nightmare;

  if (episode < 1)
    episode = 1;

  // Disable all sanity checks if there are custom episode definitions. They do not make sense in this case.
  if (!EpiCustom && W_CheckNumForName(MAPNAME(episode, map)) == -1)
  {

  if (gamemode == retail)
    {
      if (episode > 4)
        episode = 4;
    }
  else
    if (gamemode == shareware)
      {
        if (episode > 1)
          episode = 1; // only start episode 1 on shareware
      }
    else
      if (episode > 3)
        episode = 3;

  if (map < 1)
    map = 1;
  if (map > 9 && gamemode != commercial)
    map = 9;
  }

  G_SetFastParms(fastparm || skill == sk_nightmare);  // killough 4/10/98

  M_ClearRandom();

  respawnmonsters = skill == sk_nightmare || respawnparm;

  // force players to be initialized upon first level load
  for (i=0 ; i<MAXPLAYERS ; i++)
    players[i].playerstate = PST_REBORN;

  usergame = true;                // will be set false if a demo
  paused = false;
  demoplayback = false;
  automapactive = false;
  viewactive = true;
  gameepisode = episode;
  gamemap = map;
  gameskill = skill;
  gamemapinfo = G_LookupMapinfo(gameepisode, gamemap);

  // [FG] total time for all completed levels
  totalleveltimes = 0;
  playback_tic = 0;

  //jff 4/16/98 force marks on automap cleared every new level start
  AM_clearMarks();

  M_LoadOptions();     // killough 11/98: read OPTIONS lump from wad

  if (demo_version == 203)
    G_MBFComp();

  G_DoLoadLevel();
}

//
// G_RecordDemo
//

void G_RecordDemo(char *name)
{
  int i;
  size_t demoname_size;
  // [crispy] demo file name suffix counter
  static int j = 0;

  // [crispy] the name originally chosen for the demo, i.e. without "-00000"
  if (!orig_demoname)
  {
    orig_demoname = name;
  }

  demo_insurance = 0;
      
  usergame = false;
  demoname_size = strlen(name) + 5 + 6; // [crispy] + 6 for "-00000"
  demoname = I_Realloc(demoname, demoname_size);
  AddDefaultExtension(strcpy(demoname, name), ".lmp");  // 1/18/98 killough

  for(; j <= 99999 && !M_access(demoname, F_OK); ++j)
  {
    char *str = M_StringDuplicate(name);
    if (M_StringCaseEndsWith(str, ".lmp"))
      str[strlen(str) - 4] = '\0';
    M_snprintf(demoname, demoname_size, "%s-%05d.lmp", str, j);
    free(str);
  }

  //!
  // @arg <size>
  // @category demo
  // @vanilla
  //
  // Sets the initial size of the demo recording buffer (KiB). This is no longer a
  // hard limit, and the buffer will expand if the given limit is exceeded.
  //

  i = M_CheckParm ("-maxdemo");
  if (i && i<myargc-1)
    maxdemosize = M_ParmArgToInt(i) * 1024;
  if (maxdemosize < 0x20000)  // killough
    maxdemosize = 0x20000;
  demobuffer = Z_Malloc(maxdemosize, PU_STATIC, 0); // killough
  demorecording = true;
}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

static int G_GameOptionSize(void) {
  return mbf21 ? MBF21_GAME_OPTION_SIZE : GAME_OPTION_SIZE;
}

static byte* G_WriteOptionsMBF21(byte* demo_p)
{
  int i;
  byte *target = demo_p + MBF21_GAME_OPTION_SIZE;

  *demo_p++ = monsters_remember;
  *demo_p++ = weapon_recoil;
  *demo_p++ = player_bobbing;

  *demo_p++ = respawnparm;
  *demo_p++ = fastparm;
  *demo_p++ = nomonsters;

  *demo_p++ = (byte)((rngseed >> 24) & 0xff);
  *demo_p++ = (byte)((rngseed >> 16) & 0xff);
  *demo_p++ = (byte)((rngseed >>  8) & 0xff);
  *demo_p++ = (byte)( rngseed        & 0xff);

  *demo_p++ = monster_infighting;
  *demo_p++ = dogs;

  *demo_p++ = (distfriend >> 8) & 0xff;
  *demo_p++ =  distfriend       & 0xff;

  *demo_p++ = monster_backing;
  *demo_p++ = monster_avoid_hazards;
  *demo_p++ = monster_friction;
  *demo_p++ = help_friends;
  *demo_p++ = dog_jumping;
  *demo_p++ = monkeys;

  *demo_p++ = MBF21_COMP_TOTAL;

  for (i = 0; i < MBF21_COMP_TOTAL; i++)
    *demo_p++ = comp[i] != 0;

  if (demo_p != target)
    I_Error("mbf21_WriteOptions: MBF21_GAME_OPTION_SIZE is too small");

  return demo_p;
}

byte *G_WriteOptions(byte *demo_p)
{
  byte *target = demo_p + GAME_OPTION_SIZE;

  if (mbf21)
  {
    return G_WriteOptionsMBF21(demo_p);
  }

  *demo_p++ = monsters_remember;  // part of monster AI

  *demo_p++ = variable_friction;  // ice & mud

  *demo_p++ = weapon_recoil;      // weapon recoil

  *demo_p++ = allow_pushers;      // MT_PUSH Things

  *demo_p++ = 0;

  *demo_p++ = player_bobbing;  // whether player bobs or not

  // killough 3/6/98: add parameters to savegame, move around some in demos
  *demo_p++ = respawnparm;
  *demo_p++ = fastparm;
  *demo_p++ = nomonsters;

  *demo_p++ = demo_insurance;        // killough 3/31/98

  // killough 3/26/98: Added rngseed. 3/31/98: moved here
  *demo_p++ = (byte)((rngseed >> 24) & 0xff);
  *demo_p++ = (byte)((rngseed >> 16) & 0xff);
  *demo_p++ = (byte)((rngseed >>  8) & 0xff);
  *demo_p++ = (byte) (rngseed        & 0xff);

  // Options new to v2.03 begin here

  *demo_p++ = monster_infighting;   // killough 7/19/98

  *demo_p++ = dogs;                 // killough 7/19/98

  *demo_p++ = classic_bfg;          // killough 7/19/98
  *demo_p++ = beta_emulation;       // killough 7/24/98

  *demo_p++ = (distfriend >> 8) & 0xff;  // killough 8/8/98  
  *demo_p++ =  distfriend       & 0xff;  // killough 8/8/98  

  *demo_p++ = monster_backing;         // killough 9/8/98

  *demo_p++ = monster_avoid_hazards;    // killough 9/9/98

  *demo_p++ = monster_friction;         // killough 10/98

  *demo_p++ = help_friends;             // killough 9/9/98

  *demo_p++ = dog_jumping;

  *demo_p++ = monkeys;

  {   // killough 10/98: a compatibility vector now
    int i;
    for (i=0; i < COMP_TOTAL; i++)
      *demo_p++ = comp[i] != 0;
  }

  //----------------
  // Padding at end
  //----------------
  while (demo_p < target)
    *demo_p++ = 0;

  if (demo_p != target)
    I_Error("G_WriteOptions: GAME_OPTION_SIZE is too small");

  return target;
}

// Same, but read instead of write

byte *G_ReadOptionsMBF21(byte *demo_p)
{
  int i, count;

  // not configurable in mbf21
  variable_friction = 1;
  allow_pushers = 1;
  demo_insurance = 0;
  classic_bfg = 0;
  beta_emulation = 0;

  monsters_remember = *demo_p++;
  weapon_recoil = *demo_p++;
  player_bobbing = *demo_p++;

  respawnparm = *demo_p++;
  fastparm = *demo_p++;
  nomonsters = *demo_p++;

  rngseed  = *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;

  monster_infighting = *demo_p++;
  dogs = *demo_p++;

  distfriend  = *demo_p++ << 8;
  distfriend += *demo_p++;

  monster_backing = *demo_p++;
  monster_avoid_hazards = *demo_p++;
  monster_friction = *demo_p++;
  help_friends = *demo_p++;
  dog_jumping = *demo_p++;
  monkeys = *demo_p++;

  count = *demo_p++;

  if (count > MBF21_COMP_TOTAL)
    I_Error("Encountered unknown mbf21 compatibility options!");

  for (i = 0; i < count; i++)
    comp[i] = *demo_p++;

  // comp_voodooscroller
  if (count < MBF21_COMP_TOTAL - 2)
    comp[comp_voodooscroller] = 1;

  // comp_reservedlineflag
  if (count < MBF21_COMP_TOTAL - 1)
    comp[comp_reservedlineflag] = 0;

  return demo_p;
}

byte *G_ReadOptions(byte *demo_p)
{
  byte *target = demo_p + GAME_OPTION_SIZE;

  monsters_remember = *demo_p++;

  variable_friction = *demo_p;  // ice & mud
  demo_p++;

  weapon_recoil = *demo_p;       // weapon recoil
  demo_p++;

  allow_pushers = *demo_p;      // MT_PUSH Things
  demo_p++;

  demo_p++;

  player_bobbing = *demo_p;     // whether player bobs or not
  demo_p++;

  // killough 3/6/98: add parameters to savegame, move from demo
  respawnparm = *demo_p++;
  fastparm = *demo_p++;
  nomonsters = *demo_p++;

  demo_insurance = *demo_p++;              // killough 3/31/98

  // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

  rngseed  = *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;

  // Options new to v2.03
  if (demo_version >= 203)
    {
      monster_infighting = *demo_p++;   // killough 7/19/98

      dogs = *demo_p++;                 // killough 7/19/98

      classic_bfg = *demo_p++;          // killough 7/19/98
      beta_emulation = *demo_p++;       // killough 7/24/98
      
      if (beta_emulation && !M_CheckParm("-beta"))
	I_Error("The -beta option is required to play "
		"back beta emulation demos");

      distfriend = *demo_p++ << 8;      // killough 8/8/98
      distfriend+= *demo_p++;

      monster_backing = *demo_p++;     // killough 9/8/98

      monster_avoid_hazards = *demo_p++; // killough 9/9/98

      monster_friction = *demo_p++;      // killough 10/98

      help_friends = *demo_p++;          // killough 9/9/98

      dog_jumping = *demo_p++;           // killough 10/98

      monkeys = *demo_p++;

      {   // killough 10/98: a compatibility vector now
	int i;
	for (i=0; i < COMP_TOTAL; i++)
	  comp[i] = *demo_p++;
      }

      G_MBFComp();

      // Options new to v2.04, etc.
      if (demo_version >= 204)
	;
    }
  else  // defaults for versions < 2.02
    {
      int i;  // killough 10/98: a compatibility vector now
      for (i=0; i < COMP_TOTAL; i++)
	comp[i] = compatibility;

      if (demo_version == 202)
        G_BoomComp();

      monster_infighting = 1;           // killough 7/19/98

      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98

      monster_friction = 0;             // killough 10/98

      help_friends = 0;                 // killough 9/9/98

      classic_bfg = 0;                  // killough 7/19/98
      beta_emulation = 0;               // killough 7/24/98

      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98
      monkeys = 0;
    }

  return target;
}

void G_BeginRecording(void)
{
  int i;

  demo_p = demobuffer;

  if (demo_version == 203 || mbf21)
  {
  *demo_p++ = demo_version;

  // signature
  *demo_p++ = 0x1d;
  *demo_p++ = 'M';
  *demo_p++ = 'B';
  *demo_p++ = 'F';
  *demo_p++ = 0xe6;
  *demo_p++ = '\0';

  if (!mbf21)
  {
  // killough 2/22/98: save compatibility flag in new demos
  *demo_p++ = compatibility;       // killough 2/22/98
  }

  *demo_p++ = gameskill;
  *demo_p++ = gameepisode;
  *demo_p++ = gamemap;
  *demo_p++ = deathmatch;
  *demo_p++ = consoleplayer;

  demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options

  for (i=0 ; i<MAXPLAYERS ; i++)
    *demo_p++ = playeringame[i];

  // killough 2/28/98:
  // We always store at least MIN_MAXPLAYERS bytes in demo, to
  // support enhancements later w/o losing demo compatibility

  for (; i<MIN_MAXPLAYERS; i++)
    *demo_p++ = 0;
  }
  else if (demo_version == 202)
  {
    *demo_p++ = demo_version;

    // signature
    *demo_p++ = 0x1d;
    *demo_p++ = 'B';
    *demo_p++ = 'o';
    *demo_p++ = 'o';
    *demo_p++ = 'm';
    *demo_p++ = 0xe6;

    /* CPhipps - save compatibility level in demos */
    *demo_p++ = 0;

    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = consoleplayer;

    demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options

    for (i=0 ; i<MAXPLAYERS ; i++)
      *demo_p++ = playeringame[i];

    // killough 2/28/98:
    // We always store at least MIN_MAXPLAYERS bytes in demo, to
    // support enhancements later w/o losing demo compatibility

    for (; i<MIN_MAXPLAYERS; i++)
      *demo_p++ = 0;
  }
  else if (demo_version == 109)
  {
    if (longtics)
    {
      *demo_p++ = 111;
    }
    else
    {
      *demo_p++ = demo_version;
    }
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;
    for (i=0; i<4; i++)  // intentionally hard-coded 4 -- killough
      *demo_p++ = playeringame[i];
  }

  displaymsg("Demo Recording: %s", M_BaseName(demoname));
}

//
// G_PlayDemo
//

void D_CheckNetPlaybackSkip(void);

void G_DeferedPlayDemo(char* name)
{
  // [FG] avoid demo lump name collisions
  W_DemoLumpNameCollision(&name);

  defdemoname = name;
  gameaction = ga_playdemo;

  D_CheckNetPlaybackSkip();

  // [FG] fast-forward demo to the desired map
  if (playback_warp >= 0 || playback_skiptics)
  {
    G_EnableWarp(true);
  }
}

#define DEMO_FOOTER_SEPARATOR "\n"
#define NUM_DEMO_FOOTER_LUMPS 4
extern char **dehfiles;

static size_t WriteCmdLineLump(MEMFILE *stream)
{
  int i;
  char *tmp;
  boolean has_files = false;

  long pos = mem_ftell(stream);

  tmp = M_StringJoin("-iwad \"", M_BaseName(wadfiles[0]), "\"", NULL);
  mem_fputs(tmp, stream);
  free(tmp);

  for (i = 1; wadfiles[i]; i++)
  {
    const char *basename = M_BaseName(wadfiles[i]);

    if (!strcasecmp("brghtmps.lmp", basename))
      continue;

    if (!has_files)
    {
      mem_fputs(" -file", stream);
      has_files = true;
    }

    tmp = M_StringJoin(" \"", basename, "\"", NULL);
    mem_fputs(tmp, stream);
    free(tmp);
  }

  if (dehfiles)
  {
    mem_fputs(" -deh", stream);
    for (i = 0; dehfiles[i]; ++i)
    {
      tmp = M_StringJoin(" \"", M_BaseName(dehfiles[i]), "\"", NULL);
      mem_fputs(tmp, stream);
      free(tmp);
    }
  }

  if (demo_compatibility)
  {
    if (gameversion == exe_doom_1_9)
      mem_fputs(" -complevel 2", stream);
    else if (gameversion == exe_ultimate)
      mem_fputs(" -complevel 3", stream);
    else if (gameversion == exe_final)
      mem_fputs(" -complevel 4", stream);
  }

  if (coop_spawns)
  {
    mem_fputs(" -coop_spawns", stream);
  }

  if (M_CheckParm("-solo-net"))
  {
    mem_fputs(" -solo-net", stream);
  }

  for (i = 0; i < EMU_TOTAL; ++i)
  {
     if (overflow[i].triggered)
     {
        mem_fputs(" -", stream);
        mem_fputs(overflow[i].str, stream);
     }
  }

  return mem_ftell(stream) - pos;
}

static void WriteFileInfo(const char *name, size_t size, MEMFILE *stream)
{
  filelump_t fileinfo = { 0 };
  static long filepos = sizeof(wadinfo_t);

  fileinfo.filepos = LONG(filepos);
  fileinfo.size = LONG(size);

  if (name)
    M_CopyLumpName(fileinfo.name, name);

  mem_fwrite(&fileinfo, 1, sizeof(fileinfo), stream);

  filepos += size;
}

static void G_AddDemoFooter(void)
{
  byte *data;
  size_t size;

  MEMFILE *stream = mem_fopen_write();

  wadinfo_t header = { "PWAD" };
  header.numlumps = LONG(NUM_DEMO_FOOTER_LUMPS);
  mem_fwrite(&header, 1, sizeof(header), stream);

  mem_fputs(PROJECT_STRING, stream);
  mem_fputs(DEMO_FOOTER_SEPARATOR, stream);
  size = WriteCmdLineLump(stream);
  mem_fputs(DEMO_FOOTER_SEPARATOR, stream);

  header.infotableofs = LONG(mem_ftell(stream));
  mem_fseek(stream, 0, MEM_SEEK_SET);
  mem_fwrite(&header, 1, sizeof(header), stream);
  mem_fseek(stream, 0, MEM_SEEK_END);

  WriteFileInfo("PORTNAME", strlen(PROJECT_STRING), stream);
  WriteFileInfo(NULL, strlen(DEMO_FOOTER_SEPARATOR), stream);
  WriteFileInfo("CMDLINE", size, stream);
  WriteFileInfo(NULL, strlen(DEMO_FOOTER_SEPARATOR), stream);

  mem_get_buf(stream, (void **)&data, &size);

  CheckDemoBuffer(size);

  memcpy(demo_p, data, size);
  demo_p += size;

  mem_fclose(stream);
}

//===================
//=
//= G_CheckDemoStatus
//=
//= Called after a death or level completion to allow demos to be cleaned up
//= Returns true if a new demo loop action will take place
//===================

boolean G_CheckDemoStatus(void)
{
  // [crispy] catch the last cmd to track joins
  ticcmd_t* cmd = last_cmd;
  last_cmd = NULL;

  if (timingdemo)
    {
      int endtime = I_GetTime_RealTime();
      // killough -- added fps information and made it work for longer demos:
      unsigned realtics = endtime-starttime;
      I_Success("Timed %u gametics in %u realtics = %-.1f frames per second",
               (unsigned) gametic,realtics,
               (unsigned) gametic * (double) TICRATE / realtics);
    }

  if (demoplayback)
    {
      if (demorecording)
      {
        if (netgame)
          CheckPlayersInNetGame();

        demoplayback = false;

        // clear progress demo bar
        ST_Start();

        // [crispy] record demo join
        if (cmd != NULL)
        {
          cmd->buttons |= BT_JOIN;
        }

        displaymsg("Demo Recording: %s", M_BaseName(demoname));

        return true;
      }

      if (singledemo)
        I_SafeExit(0);  // killough

      // [FG] ignore empty demo lumps
      if (demobuffer)
      {
        Z_ChangeTag(demobuffer, PU_CACHE);
      }

      G_ReloadDefaults(false); // killough 3/1/98
      netgame = false;       // killough 3/29/98
      solonet = false;
      deathmatch = false;
      D_AdvanceDemo();
      return true;
    }

  if (demorecording)
    {
      demorecording = false;

      if (!demo_p)
        return false;

      *demo_p++ = DEMOMARKER;

      G_AddDemoFooter();

      if (!M_WriteFile(demoname, demobuffer, demo_p - demobuffer))
	I_Error("Error recording demo %s: %s", demoname,  // killough 11/98
		errno ? strerror(errno) : "(Unknown Error)");

      Z_Free(demobuffer);
      demobuffer = NULL;  // killough
      I_Printf(VB_ALWAYS, "Demo %s recorded", demoname);
      // [crispy] if a new game is started during demo recording, start a new demo
      if (gameaction != ga_newgame && gameaction != ga_reloadlevel)
      {
        I_SafeExit(0);
      }
      return false;  // killough
    }

  return false;
}

// killough 1/22/98: this is a "Doom printf" for messages. I've gotten
// tired of using players->message=... and so I've added this dprintf.
//
// killough 3/6/98: Made limit static to allow z_zone functions to call
// this function, without calling realloc(), which seems to cause problems.

#define MAX_MESSAGE_SIZE 1024

extern int show_toggle_messages, show_pickup_messages;

void doomprintf(player_t *player, msg_category_t category, const char *s, ...)
{
  static char msg[MAX_MESSAGE_SIZE];
  va_list v;

  if ((category == MESSAGES_TOGGLE && !show_toggle_messages) ||
      (category == MESSAGES_PICKUP && !show_pickup_messages) ||
      (category == MESSAGES_OBITUARY && !show_obituary_messages))
    return;

  va_start(v,s);
  vsprintf(msg,s,v);                  // print message in buffer
  va_end(v);

  if (player)
    player->message = msg;
  else
    players[displayplayer].message = msg;  // set new message
}

//----------------------------------------------------------------------------
//
// $Log: g_game.c,v $
// Revision 1.59  1998/06/03  20:23:10  killough
// fix v2.00 demos
//
// Revision 1.58  1998/05/16  09:16:57  killough
// Make loadgame checksum friendlier
//
// Revision 1.57  1998/05/15  00:32:28  killough
// Remove unnecessary crash hack
//
// Revision 1.56  1998/05/13  22:59:23  killough
// Restore Doom bug compatibility for demos, fix multiplayer status bar
//
// Revision 1.55  1998/05/12  12:46:16  phares
// Removed OVER_UNDER code
//
// Revision 1.54  1998/05/07  00:48:51  killough
// Avoid displaying uncalled for precision
//
// Revision 1.53  1998/05/06  15:32:24  jim
// document g_game.c, change externals
//
// Revision 1.52  1998/05/05  16:29:06  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.51  1998/05/04  22:00:33  thldrmn
// savegamename globalization
//
// Revision 1.50  1998/05/03  22:15:19  killough
// beautification, decls & headers, net consistency fix
//
// Revision 1.49  1998/04/27  17:30:12  jim
// Fix DM demo/newgame status, remove IDK (again)
//
// Revision 1.48  1998/04/25  12:03:44  jim
// Fix secret level fix
//
// Revision 1.46  1998/04/24  12:09:01  killough
// Clear player cheats before demo starts
//
// Revision 1.45  1998/04/19  19:24:19  jim
// Improved IWAD search
//
// Revision 1.44  1998/04/16  16:17:09  jim
// Fixed disappearing marks after new level
//
// Revision 1.43  1998/04/14  10:55:13  phares
// Recoil, Bobbing, Monsters Remember changes in Setup now take effect immediately
//
// Revision 1.42  1998/04/13  21:36:12  phares
// Cemented ESC and F1 in place
//
// Revision 1.41  1998/04/13  10:40:58  stan
// Now synch up all items identified by Lee Killough as essential to
// game synch (including bobbing, recoil, rngseed).  Commented out
// code in g_game.c so rndseed is always set even in netgame.
//
// Revision 1.40  1998/04/13  00:39:29  jim
// Fix automap marks carrying over thru levels
//
// Revision 1.39  1998/04/10  06:33:00  killough
// Fix -fast parameter bugs
//
// Revision 1.38  1998/04/06  04:51:32  killough
// Allow demo_insurance=2
//
// Revision 1.37  1998/04/05  00:50:48  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.36  1998/04/02  16:15:24  killough
// Fix weapons switch
//
// Revision 1.35  1998/04/02  04:04:27  killough
// Fix DM respawn sticking problem
//
// Revision 1.34  1998/04/02  00:47:19  killough
// Fix net consistency errors
//
// Revision 1.33  1998/03/31  10:36:41  killough
// Fix crash caused by last change, add new RNG options
//
// Revision 1.32  1998/03/28  19:15:48  killough
// fix DM spawn bug (Stan's fix)
//
// Revision 1.31  1998/03/28  17:55:06  killough
// Fix weapons switch bug, improve RNG while maintaining sync
//
// Revision 1.30  1998/03/28  15:49:47  jim
// Fixed merge glitches in d_main.c and g_game.c
//
// Revision 1.29  1998/03/28  05:32:00  jim
// Text enabling changes for DEH
//
// Revision 1.28  1998/03/27  21:27:00  jim
// Fixed sky bug for Ultimate DOOM
//
// Revision 1.27  1998/03/27  16:11:43  stan
// (SG) Commented out lines in G_ReloadDefaults that reset netgame and
//      deathmatch to zero.
//
// Revision 1.26  1998/03/25  22:51:25  phares
// Fixed headsecnode bug trashing memory
//
// Revision 1.25  1998/03/24  15:59:17  jim
// Added default_skill parameter to config file
//
// Revision 1.24  1998/03/23  15:23:39  phares
// Changed pushers to linedef control
//
// Revision 1.23  1998/03/23  03:14:27  killough
// Fix savegame checksum, net/demo consistency w.r.t. weapon switch
//
// Revision 1.22  1998/03/20  00:29:39  phares
// Changed friction to linedef control
//
// Revision 1.21  1998/03/18  16:16:47  jim
// Fix to idmusnum handling
//
// Revision 1.20  1998/03/17  20:44:14  jim
// fixed idmus non-restore, space bug
//
// Revision 1.19  1998/03/16  12:29:14  killough
// Add savegame checksum test
//
// Revision 1.18  1998/03/14  17:17:24  jim
// Fixes to deh
//
// Revision 1.17  1998/03/11  17:48:01  phares
// New cheats, clean help code, friction fix
//
// Revision 1.16  1998/03/09  18:29:17  phares
// Created separately bound automap and menu keys
//
// Revision 1.15  1998/03/09  07:09:20  killough
// Avoid realloc() in dprintf(), fix savegame -nomonsters bug
//
// Revision 1.14  1998/03/02  11:27:45  killough
// Forward and backward demo sync compatibility
//
// Revision 1.13  1998/02/27  08:09:22  phares
// Added gamemode checks to weapon selection
//
// Revision 1.12  1998/02/24  08:45:35  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.11  1998/02/23  04:19:35  killough
// Fix Internal and v1.9 Demo sync problems
//
// Revision 1.10  1998/02/20  22:50:51  killough
// Fix dprintf for multiplayer games
//
// Revision 1.9  1998/02/20  06:15:08  killough
// Turn turbo messages on in demo playbacks
//
// Revision 1.8  1998/02/17  05:53:41  killough
// Suppress "green is turbo" in non-net games
// Remove dependence on RNG for net consistency (intereferes with RNG)
// Use new RNG calling method, with keys assigned to blocks
// Friendlier savegame version difference message (instead of nothing)
// Remove futile attempt to make Boom v1.9-savegame-compatibile
//
// Revision 1.7  1998/02/15  02:47:41  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  02:57:08  killough
// Make player corpse limit user-configurable
// Fix ExM8 level endings
// Stop 'q' from ending demo recordings
//
// Revision 1.5  1998/02/02  13:44:45  killough
// Fix dprintf and CheckSaveGame realloc bugs
//
// Revision 1.4  1998/01/26  19:23:18  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/24  21:03:07  jim
// Fixed disappearence of nomonsters, respawn, or fast mode after demo play or IDCLEV
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
