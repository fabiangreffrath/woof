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
//      Game completion, final screen animation.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <string.h>

#include "d_deh.h" // Ty 03/22/98 - externalizations
#include "d_event.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_umapinfo.h"
#include "info.h"
#include "m_misc.h" // [FG] M_StringDuplicate()
#include "m_swap.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "z_zone.h"

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast

typedef enum
{
    FINALE_STAGE_TEXT,
    FINALE_STAGE_ART,
    FINALE_STAGE_CAST
} finalestage_t;

static finalestage_t finalestage;

static int finalecount;

// defines for the end mission display text                     // phares

#define TEXTSPEED    3     // original value                    // phares
#define TEXTWAIT     250   // original value                    // phares
#define NEWTEXTSPEED 0.01f // new value                         // phares
#define NEWTEXTWAIT  1000  // new value                         // phares

static const char *finaletext;
static const char *finaleflat;

static void F_StartCast(void);
static void F_CastTicker(void);
static boolean F_CastResponder(event_t *ev);
static void F_CastDrawer(void);
static void F_TextWrite(void);
static void F_BunnyScroll(void);
static float Get_TextSpeed(void);

static int midstage;                 // whether we're in "mid-stage"

static boolean mapinfo_finale;

static boolean MapInfo_StartFinale(void)
{
    mapinfo_finale = false;

    if (!gamemapinfo)
    {
        return false;
    }

    if (secretexit)
    {
        if (gamemapinfo->flags & MapInfo_InterTextSecretClear)
        {
            finaletext = NULL;
        }
        else if (gamemapinfo->intertextsecret)
        {
            finaletext = gamemapinfo->intertextsecret;
        }
    }
    else
    {
        if (gamemapinfo->flags & MapInfo_InterTextClear)
        {
            finaletext = NULL;
        }
        else if (gamemapinfo->intertext)
        {
            finaletext = gamemapinfo->intertext;
        }
    }

    if (gamemapinfo->interbackdrop[0])
    {
        finaleflat = gamemapinfo->interbackdrop;
    }

    if (!finaleflat)
    {
        finaleflat = "FLOOR4_8"; // use a single fallback for all maps.
    }

    int lumpnum = W_CheckNumForName(gamemapinfo->intermusic);
    if (lumpnum >= 0)
    {
        S_ChangeMusInfoMusic(lumpnum, true);
    }

    mapinfo_finale = true;

    return lumpnum >= 0;
}

static boolean MapInfo_Ticker()
{
    if (!mapinfo_finale)
    {
        return false;
    }

    boolean next_level = false;

    WI_checkForAccelerate();

    if (!next_level)
    {
        // advance animation
        finalecount++;

        if (finalestage == FINALE_STAGE_CAST)
        {
            F_CastTicker();
            return true;
        }

        if (finalestage == FINALE_STAGE_TEXT)
        {
            int textcount = 0;
            if (finaletext)
            {
                float speed = demo_compatibility ? TEXTSPEED : Get_TextSpeed();
                textcount = strlen(finaletext) * speed
                            + (midstage ? NEWTEXTWAIT : TEXTWAIT);
            }

            if (!textcount || finalecount > textcount
                || (midstage && acceleratestage))
            {
                next_level = true;
            }
        }
    }

    if (next_level)
    {
        if (!secretexit && gamemapinfo->flags & MapInfo_EndGame)
        {
            if (gamemapinfo->flags & MapInfo_EndGameCast)
            {
                F_StartCast();
            }
            else
            {
                finalecount = 0;
                finalestage = FINALE_STAGE_ART;
                wipegamestate = -1; // force a wipe
                if (gamemapinfo->flags & MapInfo_EndGameBunny)
                {
                    S_StartMusic(mus_bunny);
                }
                else if (gamemapinfo->flags & MapInfo_EndGameStandard)
                {
                    mapinfo_finale = false;
                }
            }
        }
        else
        {
            gameaction = ga_worlddone; // next level, e.g. MAP07
        }
    }

    return true;
}

static boolean MapInfo_Drawer(void)
{
    if (!mapinfo_finale)
    {
        return false;
    }

    switch (finalestage)
    {
        case FINALE_STAGE_TEXT:
            if (finaletext)
            {
                F_TextWrite();
            }
            break;
        case FINALE_STAGE_ART:
            if (gamemapinfo->flags & MapInfo_EndGameBunny)
            {
                F_BunnyScroll();
            }
            else if (gamemapinfo->endpic[0])
            {
                V_DrawPatchFullScreen(
                    V_CachePatchName(
                        W_CheckWidescreenPatch(gamemapinfo->endpic), PU_CACHE));
            }
            break;
        case FINALE_STAGE_CAST:
            F_CastDrawer();
            break;
    }

    return true;
}

//
// F_StartFinale
//
void F_StartFinale (void)
{
  musicenum_t music_id = mus_None;

  gameaction = ga_nothing;
  gamestate = GS_FINALE;
  viewactive = false;
  automapactive = false;

  // killough 3/28/98: clear accelerative text flags
  acceleratestage = midstage = 0;

  finaletext = NULL;
  finaleflat = NULL;

  // Okay - IWAD dependend stuff.
  // This has been changed severly, and
  //  some stuff might have changed in the process.
  switch ( gamemode )
  {
    // DOOM 1 - E1, E3 or E4, but each nine missions
    case shareware:
    case registered:
    case retail:
    {
      music_id = mus_victor;
      
      switch (gameepisode)
      {
        case 1:
             finaleflat = bgflatE1; // Ty 03/30/98 - new externalized bg flats
             finaletext = s_E1TEXT; // Ty 03/23/98 - Was e1text variable.
             break;
        case 2:
             finaleflat = bgflatE2;
             finaletext = s_E2TEXT; // Ty 03/23/98 - Same stuff for each 
             break;
        case 3:
             finaleflat = bgflatE3;
             finaletext = s_E3TEXT;
             break;
        case 4:
             finaleflat = bgflatE4;
             finaletext = s_E4TEXT;
             break;
        default:
             // Ouch.
             break;
      }
      break;
    }
    
    // DOOM II and missions packs with E1, M34
    case commercial:
    {
      music_id = mus_read_m;

      // Ty 08/27/98 - added the gamemission logic

      switch (gamemap)      /* This is regular Doom II */
      {
        case 6:
             finaleflat = bgflat06;
             finaletext = gamemission == pack_tnt  ? s_T1TEXT :
	                  gamemission == pack_plut ? s_P1TEXT : s_C1TEXT;
             break;
        case 11:
             finaleflat = bgflat11;
             finaletext = gamemission == pack_tnt  ? s_T2TEXT :
	                  gamemission == pack_plut ? s_P2TEXT : s_C2TEXT;
             break;
        case 20:
             finaleflat = bgflat20;
             finaletext = gamemission == pack_tnt  ? s_T3TEXT :
	                  gamemission == pack_plut ? s_P3TEXT : s_C3TEXT;
             break;
        case 30:
             finaleflat = bgflat30;
             finaletext = gamemission == pack_tnt  ? s_T4TEXT :
	                  gamemission == pack_plut ? s_P4TEXT : s_C4TEXT;
             break;
        case 15:
             finaleflat = bgflat15;
             finaletext = gamemission == pack_tnt  ? s_T5TEXT :
	                  gamemission == pack_plut ? s_P5TEXT : s_C5TEXT;
             break;
        case 31:
             finaleflat = bgflat31;
             finaletext = gamemission == pack_tnt  ? s_T6TEXT :
	                  gamemission == pack_plut ? s_P6TEXT : s_C6TEXT;
             break;
        default:
             // Ouch.
             break;
      }
      // Ty 08/27/98 - end gamemission logic

      break;
    } 

    // Indeterminate.
    default:  // Ty 03/30/98 - not externalized
         music_id = mus_read_m;
         finaleflat = "F_SKY1"; // Not used anywhere else.
         finaletext = s_C1TEXT;  // FIXME - other text, music?
         break;
  }
  
  if (!MapInfo_StartFinale())
  {
      S_ChangeMusic(music_id, true);
  }

  finalestage = FINALE_STAGE_TEXT;
  finalecount = 0;
}

boolean F_Responder (event_t *event)
{
  if (finalestage == FINALE_STAGE_CAST)
    return F_CastResponder(event);
        
  return false;
}

// Get_TextSpeed() returns the value of the text display speed  // phares
// Rewritten to allow user-directed acceleration -- killough 3/28/98

static float Get_TextSpeed(void)
{
  return midstage ? NEWTEXTSPEED : (midstage=acceleratestage) ? 
    acceleratestage=0, NEWTEXTSPEED : TEXTSPEED;
}

//
// F_Ticker
//
// killough 3/28/98: almost totally rewritten, to use
// player-directed acceleration instead of constant delays.
// Now the player can accelerate the text display by using
// the fire/use keys while it is being printed. The delay
// automatically responds to the user, and gives enough
// time to read.
//
// killough 5/10/98: add back v1.9 demo compatibility
//

void F_Ticker(void)
{
  if (MapInfo_Ticker())
  {
      return;
  }

  int i;
  if (!demo_compatibility)
    WI_checkForAccelerate();  // killough 3/28/98: check for acceleration
  else
    if (gamemode == commercial && finalecount > 50) // check for skipping
      for (i=0; i<MAXPLAYERS; i++)
        if (players[i].cmd.buttons)
          goto next_level;      // go on to the next level

  // advance animation
  finalecount++;
 
  if (finalestage == FINALE_STAGE_CAST)
    F_CastTicker();

  if (finalestage == FINALE_STAGE_TEXT)
    {
      float speed = demo_compatibility ? TEXTSPEED : Get_TextSpeed();
      if (finalecount > strlen(finaletext)*speed +  // phares
          (midstage ? NEWTEXTWAIT : TEXTWAIT) ||  // killough 2/28/98:
          (midstage && acceleratestage))       // changed to allow acceleration
      {
        if (gamemode != commercial)       // Doom 1 / Ultimate Doom episode end
          {                               // with enough time, it's automatic
            finalecount = 0;
            finalestage = FINALE_STAGE_ART;
            wipegamestate = -1;         // force a wipe
            if (gameepisode == 3)
              S_StartMusic(mus_bunny);
          }
        else   // you must press a button to continue in Doom 2
          if (!demo_compatibility && midstage)
            {
            next_level:
              if (gamemap == 30)
                F_StartCast();              // cast of Doom 2 characters
              else
                gameaction = ga_worlddone;  // next level, e.g. MAP07
            }
      }
    }
}

//
// F_TextWrite
//
// This program displays the background and text at end-mission     // phares
// text time. It draws both repeatedly so that other displays,      //   |
// like the main menu, can be drawn over it dynamically and         //   V
// erased dynamically. The TEXTSPEED constant is changed into
// the Get_TextSpeed function so that the speed of writing the      //   ^
// text can be increased, and there's still time to read what's     //   |
// written.                                                         // phares

static void F_TextWrite(void)
{
  int         w;         // killough 8/9/98: move variables below
  int         count;
  const char  *ch;
  int         c;
  int         cx;
  int         cy;
  
  // [FG] if interbackdrop does not specify a valid flat, draw it as a patch instead
  if (gamemapinfo && W_CheckNumForName(finaleflat) != -1 &&
      (W_CheckNumForName)(finaleflat, ns_flats) == -1)
  {
    V_DrawPatchFullScreen(V_CachePatchName(finaleflat, PU_LEVEL));
  }
  else if ((W_CheckNumForName)(finaleflat, ns_flats) != -1)
  {
    // erase the entire screen to a tiled background

    // killough 11/98: the background-filling code was already in m_menu.c
    V_DrawBackground(finaleflat);
  }

  // draw some of the text onto the screen
  cx = 10;
  cy = 10;
  ch = finaletext;
      
  count = (int)((finalecount - 10)/Get_TextSpeed());                 // phares
  if (count < 0)
    count = 0;

  for ( ; count ; count-- )
  {
    c = *ch++;
    if (!c)
      break;
    if (c == '\n')
    {
      cx = 10;
      cy += 11;
      continue;
    }
              
    c = M_ToUpper(c) - HU_FONTSTART;
    if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
    {
      cx += 4;
      continue;
    }
              
    w = SHORT (hu_font[c]->width);
    if (cx + w > video.unscaledw - video.deltaw)
    {
      continue;
    }
    // [cispy] prevent text from being drawn off-screen vertically
    if (cy + SHORT(hu_font[c]->height) > SCREENHEIGHT)
      break;
    V_DrawPatch(cx, cy, hu_font[c]);
    cx+=w;
  }
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
  char       *name;
  mobjtype_t  type;
} castinfo_t;

#define MAX_CASTORDER 18 /* Ty - hard coded for now */
castinfo_t      castorder[MAX_CASTORDER]; // Ty 03/22/98 - externalized and init moved into f_startcast()

int             castnum;
int             casttics;
state_t*        caststate;
boolean         castdeath;
int             castframes;
int             castonmelee;
boolean         castattacking;


//
// F_StartCast
//
static void F_StartCast(void)
{
  // Ty 03/23/98 - clumsy but time is of the essence
  castorder[0].name = s_CC_ZOMBIE,  castorder[0].type = MT_POSSESSED;
  castorder[1].name = s_CC_SHOTGUN, castorder[1].type = MT_SHOTGUY;
  castorder[2].name = s_CC_HEAVY,   castorder[2].type = MT_CHAINGUY;
  castorder[3].name = s_CC_IMP,     castorder[3].type = MT_TROOP;
  castorder[4].name = s_CC_DEMON,   castorder[4].type = MT_SERGEANT;
  castorder[5].name = s_CC_LOST,    castorder[5].type = MT_SKULL;
  castorder[6].name = s_CC_CACO,    castorder[6].type = MT_HEAD;
  castorder[7].name = s_CC_HELL,    castorder[7].type = MT_KNIGHT;
  castorder[8].name = s_CC_BARON,   castorder[8].type = MT_BRUISER;
  castorder[9].name = s_CC_ARACH,   castorder[9].type = MT_BABY;
  castorder[10].name = s_CC_PAIN,   castorder[10].type = MT_PAIN;
  castorder[11].name = s_CC_REVEN,  castorder[11].type = MT_UNDEAD;
  castorder[12].name = s_CC_MANCU,  castorder[12].type = MT_FATSO;
  castorder[13].name = s_CC_ARCH,   castorder[13].type = MT_VILE;
  castorder[14].name = s_CC_SPIDER, castorder[14].type = MT_SPIDER;
  castorder[15].name = s_CC_CYBER,  castorder[15].type = MT_CYBORG;
  castorder[16].name = s_CC_HERO,   castorder[16].type = MT_PLAYER;
  castorder[17].name = NULL,        castorder[17].type = 0;

  wipegamestate = -1;         // force a screen wipe
  castnum = 0;
  caststate = &states[mobjinfo[castorder[castnum].type].seestate];
  casttics = caststate->tics;
  castdeath = false;
  finalestage = FINALE_STAGE_CAST;    
  castframes = 0;
  castonmelee = 0;
  castattacking = false;
  S_ChangeMusic(mus_evil, true);
}


//
// F_CastTicker
//
static void F_CastTicker(void)
{
  int st;
  int sfx;
      
  if (--casttics > 0)
    return;                 // not time to change state yet
              
  if (caststate->tics == -1 || caststate->nextstate == S_NULL)
  {
    // switch from deathstate to next monster
    castnum++;
    castdeath = false;
    if (castorder[castnum].name == NULL)
      castnum = 0;
    if (mobjinfo[castorder[castnum].type].seesound)
      S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound);
    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    castframes = 0;
  }
  else
  {
    // just advance to next state in animation
    if (caststate == &states[S_PLAY_ATK1])
      goto stopattack;    // Oh, gross hack!
    st = caststate->nextstate;
    caststate = &states[st];
    castframes++;
      
    // sound hacks....
    switch (st)
    {
      case S_PLAY_ATK1:     sfx = sfx_dshtgn; break;
      case S_POSS_ATK2:     sfx = sfx_pistol; break;
      case S_SPOS_ATK2:     sfx = sfx_shotgn; break;
      case S_VILE_ATK2:     sfx = sfx_vilatk; break;
      case S_SKEL_FIST2:    sfx = sfx_skeswg; break;
      case S_SKEL_FIST4:    sfx = sfx_skepch; break;
      case S_SKEL_MISS2:    sfx = sfx_skeatk; break;
      case S_FATT_ATK8:
      case S_FATT_ATK5:
      case S_FATT_ATK2:     sfx = sfx_firsht; break;
      case S_CPOS_ATK2:
      case S_CPOS_ATK3:
      case S_CPOS_ATK4:     sfx = sfx_shotgn; break;
      case S_TROO_ATK3:     sfx = sfx_claw; break;
      case S_SARG_ATK2:     sfx = sfx_sgtatk; break;
      case S_BOSS_ATK2:
      case S_BOS2_ATK2:
      case S_HEAD_ATK2:     sfx = sfx_firsht; break;
      case S_SKULL_ATK2:    sfx = sfx_sklatk; break;
      case S_SPID_ATK2:
      case S_SPID_ATK3:     sfx = sfx_shotgn; break;
      case S_BSPI_ATK2:     sfx = sfx_plasma; break;
      case S_CYBER_ATK2:
      case S_CYBER_ATK4:
      case S_CYBER_ATK6:    sfx = sfx_rlaunc; break;
      case S_PAIN_ATK3:     sfx = sfx_sklatk; break;
      default: sfx = 0; break;
    }
            
    if (sfx)
      S_StartSound (NULL, sfx);
  }
      
  if (castframes == 12)
  {
    // go into attack frame
    castattacking = true;
    if (castonmelee)
      caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
    else
      caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
    castonmelee ^= 1;
    if (caststate == &states[S_NULL])
    {
      if (castonmelee)
        caststate=
          &states[mobjinfo[castorder[castnum].type].meleestate];
      else
        caststate=
          &states[mobjinfo[castorder[castnum].type].missilestate];
    }
  }
      
  if (castattacking)
  {
    if (castframes == 24
       ||  caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
    {
      stopattack:
      castattacking = false;
      castframes = 0;
      caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    }
  }
      
  casttics = caststate->tics;
  if (casttics == -1)
      casttics = 15;
}


//
// F_CastResponder
//

static boolean F_CastResponder(event_t* ev)
{
  if (ev->type != ev_keydown && ev->type != ev_mouseb_down && ev->type != ev_joyb_down)
    return false;
                
  if (castdeath)
    return true;                    // already in dying frames
                
  // go into death frame
  castdeath = true;
  caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
  casttics = caststate->tics;
  castframes = 0;
  castattacking = false;
  if (mobjinfo[castorder[castnum].type].deathsound)
    S_StartSound (NULL, mobjinfo[castorder[castnum].type].deathsound);
        
  return true;
}


static void F_CastPrint(char* text)
{
  char*       ch;
  int         c;
  int         cx;
  int         w;
  int         width;
  
  // find width
  ch = text;
  width = 0;
      
  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = M_ToUpper(c) - HU_FONTSTART;
    if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
    {
      width += 4;
      continue;
    }
            
    w = SHORT (hu_font[c]->width);
    width += w;
  }
  
  // draw it
  cx = 160-width/2;
  ch = text;
  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = M_ToUpper(c) - HU_FONTSTART;
    if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
    {
      cx += 4;
      continue;
    }
              
    w = SHORT (hu_font[c]->width);
    V_DrawPatch(cx, 180, hu_font[c]);
    cx+=w;
  }
}


//
// F_CastDrawer
//

static void F_CastDrawer(void)
{
  spritedef_t*        sprdef;
  spriteframe_t*      sprframe;
  int                 lump;
  boolean             flip;
  patch_t*            patch;
    
  // erase the entire screen to a background
  // Ty 03/30/98 bg texture extern
  V_DrawPatchFullScreen(
    V_CachePatchName(W_CheckWidescreenPatch(bgcastcall), PU_CACHE));

  F_CastPrint (castorder[castnum].name);
    
  // draw the current frame in the middle of the screen
  sprdef = &sprites[caststate->sprite];
  sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
  lump = sprframe->lump[0];
  flip = (boolean)sprframe->flip[0];
                        
  patch = V_CachePatchNum (lump+firstspritelump, PU_CACHE);
  if (flip)
    V_DrawPatchFlipped (160, 170, patch);
  else
    V_DrawPatch (160, 170, patch);
}

//
// F_BunnyScroll
//
static void F_BunnyScroll(void)
{
  int         scrolled;
  patch_t*    p1;
  patch_t*    p2;
  char        name[16];
  int         stage;
  static int  laststage;

  p1 = V_CachePatchName(W_CheckWidescreenPatch("PFUB1"), PU_LEVEL);
  p2 = V_CachePatchName(W_CheckWidescreenPatch("PFUB2"), PU_LEVEL);

  scrolled = 320 - (finalecount-230)/2;

  int p1offset = DIV_ROUND_CEIL(video.unscaledw - SHORT(p1->width), 2);
  if (SHORT(p1->width) == 320)
  {
      p1offset += (SHORT(p2->width) - 320) / 2;
  }

  int p2offset = DIV_ROUND_CEIL(video.unscaledw - SHORT(p2->width), 2);

  if (scrolled <= 0)
  {
      V_DrawPatch(p2offset - video.deltaw, 0, p2);
  }
  else if (scrolled >= 320)
  {
      V_DrawPatch(p1offset - video.deltaw, 0, p1);
      V_DrawPatch(-320 + p2offset - video.deltaw, 0, p2);
  }
  else
  {
      V_DrawPatch(320 - scrolled + p1offset - video.deltaw, 0, p1);
      V_DrawPatch(-scrolled + p2offset - video.deltaw, 0, p2);
  }

  if (p2offset > 0)
  {
      V_FillRect(0, 0, p2offset, SCREENHEIGHT, v_darkest_color);
      V_FillRect(p2offset + SHORT(p2->width), 0, p2offset, SCREENHEIGHT,
                 v_darkest_color);
  }

  if (finalecount < 1130)
    return;
  if (finalecount < 1180)
  {
    V_DrawPatch ((SCREENWIDTH-13*8)/2,
                 (SCREENHEIGHT-8*8)/2,
                 V_CachePatchName ("END0",PU_CACHE));
    laststage = 0;
    return;
  }
      
  stage = (finalecount-1180) / 5;
  if (stage > 6)
    stage = 6;
  if (stage > laststage)
  {
    S_StartSound (NULL, sfx_pistol);
    laststage = stage;
  }
      
  M_snprintf(name, sizeof(name), "END%i", stage);
  V_DrawPatch ((SCREENWIDTH-13*8)/2,
               (SCREENHEIGHT-8*8)/2,
               V_CachePatchName (name,PU_CACHE));
}


//
// F_Drawer
//
void F_Drawer (void)
{
  if (MapInfo_Drawer())
  {
      return;
  }

  if (finalestage == FINALE_STAGE_CAST)
  {
    F_CastDrawer ();
    return;
  }

  if (finalestage == FINALE_STAGE_TEXT)
    F_TextWrite ();
  else
  {
    switch (gameepisode)
    {
      case 1:
           if ( (gamemode == retail && !pwad_help2) || gamemode == commercial )
             V_DrawPatchFullScreen(
              V_CachePatchName(W_CheckWidescreenPatch("CREDIT"), PU_CACHE));
           else
             V_DrawPatchFullScreen(
              V_CachePatchName(W_CheckWidescreenPatch("HELP2"), PU_CACHE));
           break;
      case 2:
           V_DrawPatchFullScreen(
            V_CachePatchName(W_CheckWidescreenPatch("VICTORY2"), PU_CACHE));
           break;
      case 3:
           F_BunnyScroll();
           break;
      case 4:
           V_DrawPatchFullScreen(
            V_CachePatchName(W_CheckWidescreenPatch("ENDPIC"), PU_CACHE));
           break;
    }
  }
}

//----------------------------------------------------------------------------
//
// $Log: f_finale.c,v $
// Revision 1.16  1998/05/10  23:39:25  killough
// Restore v1.9 demo sync on text intermission screens
//
// Revision 1.15  1998/05/04  21:34:30  thldrmn
// commenting and reformatting
//
// Revision 1.14  1998/05/03  23:25:05  killough
// Fix #includes at the top, nothing else
//
// Revision 1.13  1998/04/19  01:17:18  killough
// Tidy up last fix's code
//
// Revision 1.12  1998/04/17  15:14:10  killough
// Fix showstopper flat bug
//
// Revision 1.11  1998/03/31  16:19:25  killough
// Fix minor merge glitch
//
// Revision 1.10  1998/03/31  11:41:21  jim
// Fix merge glitch in f_finale.c
//
// Revision 1.9  1998/03/31  00:37:56  jim
// Ty's finale.c fixes
//
// Revision 1.8  1998/03/28  17:51:33  killough
// Allow use/fire to accelerate teletype messages
//
// Revision 1.7  1998/02/05  12:15:06  phares
// cleaned up comments
//
// Revision 1.6  1998/02/02  13:43:30  killough
// Relax endgame message speed to demo_compatibility
//
// Revision 1.5  1998/01/31  01:47:39  phares
// Removed textspeed and textwait externs
//
// Revision 1.4  1998/01/30  18:48:18  phares
// Changed textspeed and textwait to functions
//
// Revision 1.3  1998/01/30  16:08:56  phares
// Faster end-mission text display
//
// Revision 1.2  1998/01/26  19:23:14  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
