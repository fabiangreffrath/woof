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


#include <string.h>

#include "d_event.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "f_finale.h"
#include "g_game.h"
#include "g_umapinfo.h"
#include "i_printf.h"
#include "info.h"
#include "m_misc.h" // [FG] M_StringDuplicate()
#include "m_swap.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "v_patch.h"
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
static boolean F_CastTicker(void);
static boolean F_CastResponder(event_t *ev);
static void F_CastDrawer(void);
static void F_TextWrite(void);
static void F_BunnyScroll(void);
static float Get_TextSpeed(void);

static int midstage;                 // whether we're in "mid-stage"

static boolean mapinfo_finale;

//
// ID24 EndFinale extensions
//

#define M_ARRAY_MALLOC(size) Z_Malloc(size, PU_LEVEL, NULL)
#define M_ARRAY_REALLOC(ptr, size) Z_Realloc(ptr, size, PU_LEVEL, NULL)
#define M_ARRAY_FREE(ptr) Z_Free(ptr)
#include "m_array.h"

#include "m_json.h"

// Custom EndFinale type
typedef enum end_type_e
{
    END_ART,    // Plain graphic, e.g CREDIT, VICTORY2, ENDPIC
    END_SCROLL, // Custom "bunny" scroller
    END_CAST,   // Custom cast call
} end_type_t;

// Custom cast call, per-callee frame
typedef struct cast_frame_s
{
    char        frame_lump[9];
    char        tran_lump[9];
    char        xlat_lump[9];
    const byte *tranmap;
    const byte *xlat;
    boolean     flipped;
    int         duration;
    int         sound;
} cast_frame_t;

// Custom cast call, callee
typedef struct cast_entry_s
{
    const char   *name;             // BEX [STRINGS] mnemonic
    int           alertsound;
    cast_frame_t *aliveframes;
    cast_frame_t *deathframes;
    int           aliveframescount; // Book-keeping
    int           deathframescount; // Book-keeping
} cast_anim_t;


// ID24 EndFinale
typedef struct end_finale_s
{
    end_type_t   type;
    char         music[9];      // e.g. `D_EVIL` or `D_BUNNY` or `D_VICTOR`
    char         background[9]; // e.g. `BOSSBACK` or `PFUB1` or `ENDPIC`
    boolean      musicloops;
    boolean      donextmap;
    char         bunny_stitchimage[9];
    int          bunny_overlay;
    int          bunny_overlaycount;
    int          bunny_overlaysound;
    int          bunny_overlayx;
    int          bunny_overlayy;
    int          cast_animscount;
    cast_anim_t *cast_anims;
} end_finale_t;

static end_finale_t *endfinale;

static void ParseEndFinale_CastFrame(json_t *js_frame, cast_frame_t **frames,
                                     int *framecount, const char *lump)
{
    cast_frame_t cast_frame = {0};
    const char *frame_lump = JS_GetStringValue(js_frame, "lump");
    const char *tran_lump = JS_GetStringValue(js_frame, "tranmap");
    const char *xlat_lump = JS_GetStringValue(js_frame, "translation");

    if (frame_lump == NULL || frame_lump[0] == '\0')
    {
        I_Error("EndFinale: invalid cast anim lump field on lump '%s'", lump);
    }
    else
    {
        M_CopyLumpName(cast_frame.frame_lump, frame_lump);
    }

    M_CopyLumpName(cast_frame.tran_lump, (tran_lump ? tran_lump : "\0"));
    M_CopyLumpName(cast_frame.xlat_lump, (xlat_lump ? xlat_lump : "\0"));
    cast_frame.flipped = JS_GetBooleanValue(js_frame, "flipped");
    cast_frame.duration = MAX(1, JS_GetNumberValue(js_frame, "duration") * TICRATE);
    cast_frame.sound = JS_GetIntegerValue(js_frame, "sound");

    array_push(*frames, cast_frame);
    (*framecount)++;
}

static cast_anim_t ParseEndFinale_CastAnims(json_t *js_castanim_entry,
                                            const char *lump)
{
    cast_anim_t out = {0};
    out.name = DEH_StringForMnemonic(JS_GetStringValue(js_castanim_entry, "name"));
    out.alertsound = JS_GetIntegerValue(js_castanim_entry, "alertsound");

    json_t *js_alive_frame_list = JS_GetObject(js_castanim_entry, "aliveframes");
    json_t *js_alive_frame = NULL;
    JS_ArrayForEach(js_alive_frame, js_alive_frame_list)
    {
        ParseEndFinale_CastFrame(js_alive_frame, &out.aliveframes,
                                 &out.aliveframescount, lump);
    }

    json_t *js_death_frame = NULL;
    json_t *js_death_frame_list = JS_GetObject(js_castanim_entry, "deathframes");
    JS_ArrayForEach(js_death_frame, js_death_frame_list)
    {
        ParseEndFinale_CastFrame(js_death_frame, &out.deathframes,
                                 &out.deathframescount, lump);
    }

    return out;
}

static void ParseEndFinale_CastRollCall(json_t *js_castrollcall,
                                        end_finale_t *out, const char *lump)
{
    json_t *js_castanim_list = JS_GetObject(js_castrollcall, "castanims");

    json_t *js_castanim_entry = NULL;
    JS_ArrayForEach(js_castanim_entry, js_castanim_list)
    {
        cast_anim_t castanim_entry =
            ParseEndFinale_CastAnims(js_castanim_entry, lump);
        out->cast_animscount++;
        array_push(out->cast_anims, castanim_entry);
    }
}

static void ParseEndFinale_Bunny(json_t *js_bunny, end_finale_t *out,
                                 const char *lump)
{
    out->bunny_overlay = JS_GetIntegerValue(js_bunny, "overlay");
    out->bunny_overlaycount = JS_GetIntegerValue(js_bunny, "overlaycount");
    out->bunny_overlaysound = JS_GetIntegerValue(js_bunny, "overlaysound");
    out->bunny_overlayx = JS_GetIntegerValue(js_bunny, "overlayx");
    out->bunny_overlayy = JS_GetIntegerValue(js_bunny, "overlayy");
    const char *bunny_lump = JS_GetStringValue(js_bunny, "stitchimage");
    if (bunny_lump == NULL || (W_CheckNumForName(bunny_lump) < 0))
    {
        I_Printf(VB_WARNING,
                 "EndFinale: invalid bunny stitchimage field on lump '%s'",
                 lump);
    }
    else
    {
        M_CopyLumpName(out->bunny_stitchimage, bunny_lump);
    }
}

static end_finale_t *F_ParseEndFinale(const char *lump)
{
    // Does the JSON lump even exist?
    json_t *json = JS_Open(lump, "finale", (version_t){1, 0, 0});
    if (json == NULL)
    {
        I_Printf(VB_WARNING, "EndFinale: invalid lump '%s'", lump);
        return NULL;
    }

    // Does the lump actually have any data?
    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Printf(VB_WARNING, "EndFinale: data object undefined on lump '%s'", lump);
        JS_Close(lump);
        return NULL;
    }

    // Now, actually parse it
    end_finale_t *out = Z_Calloc(1, sizeof(end_finale_t), PU_LEVEL, NULL);
    out->type = JS_GetIntegerValue(data, "type");
    out->donextmap = JS_GetBooleanValue(data, "donextmap");
    out->musicloops = JS_GetBooleanValue(data, "musicloops");

    const char *music = JS_GetStringValue(data, "music");
    const char *background = JS_GetStringValue(data, "background");
    if (music == NULL || background == NULL)
    {
        I_Printf(VB_WARNING,
                 "EndFinale: invalid music or background fields on lump '%s'",
                 lump);
        Z_Free(out);
        JS_Close(lump);
        return NULL;
    }

    M_CopyLumpName(out->music, music);
    M_CopyLumpName(out->background, background);

    switch (out->type)
    {
        case END_CAST:
            ParseEndFinale_CastRollCall(JS_GetObject(data, "castrollcall"), out, lump);
            break;

        case END_SCROLL:
            ParseEndFinale_Bunny(JS_GetObject(data, "bunny"), out, lump);
            break;

        case END_ART:
            break;

        default:
            I_Printf(
                VB_WARNING,
                "EndFinale: unknown entry of type '%d' on lump %s, skipping",
                out->type, lump);
            Z_Free(out);
            JS_Close(lump);
            return NULL;
    }

    JS_Close(lump);
    return out;
}

//
// UMAPINFO
//

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

    if (W_CheckNumForName(gamemapinfo->endfinale) >= 0)
    {
        endfinale = F_ParseEndFinale(gamemapinfo->endfinale);
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

    // advance animation
    finalecount++;

    if (finalestage == FINALE_STAGE_CAST)
    {
        if (F_CastTicker())
        {
            gameaction = ga_worlddone;
        }
    }
    else if (finalestage == FINALE_STAGE_TEXT)
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

    if (next_level)
    {
        if (!secretexit && gamemapinfo->flags & MapInfo_EndGame)
        {
            if (gamemapinfo->flags & MapInfo_EndGameCustomFinale)
            {
                if (endfinale->type == END_CAST)
                {
                    F_StartCast();
                }
                else
                {
                    finalecount = 0;
                    finalestage = FINALE_STAGE_ART;
                    wipegamestate = -1; // force a wipe
                    S_ChangeMusInfoMusic(W_GetNumForName(endfinale->music), 
                                         endfinale->musicloops);
                    if (endfinale->type == END_ART)
                    {
                        mapinfo_finale = false;
                    }
                }
            }
            else if (gamemapinfo->flags & MapInfo_EndGameCast)
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
          finaleflat = DEH_String(BGFLATE1);
          finaletext = DEH_String(E1TEXT);
          break;
        case 2:
          finaleflat = DEH_String(BGFLATE2);
          finaletext = DEH_String(E2TEXT);
          break;
        case 3:
          finaleflat = DEH_String(BGFLATE3);
          finaletext = DEH_String(E3TEXT);
          break;
        case 4:
          finaleflat = DEH_String(BGFLATE4);
          finaletext = DEH_String(E4TEXT);
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
          finaleflat = DEH_String(BGFLAT06);
          finaletext = gamemission == pack_tnt  ? DEH_String(T1TEXT) :
                       gamemission == pack_plut ? DEH_String(P1TEXT) :
                                                  DEH_String(C1TEXT);
          break;
        case 11:
          finaleflat = DEH_String(BGFLAT11);
          finaletext = gamemission == pack_tnt  ? DEH_String(T2TEXT) :
                       gamemission == pack_plut ? DEH_String(P2TEXT) :
                                                  DEH_String(C2TEXT);
          break;
        case 20:
          finaleflat = DEH_String(BGFLAT20);
          finaletext = gamemission == pack_tnt  ? DEH_String(T3TEXT) :
                       gamemission == pack_plut ? DEH_String(P3TEXT) :
                                                  DEH_String(C3TEXT);
          break;
        case 30:
          finaleflat = DEH_String(BGFLAT30);
          finaletext = gamemission == pack_tnt  ? DEH_String(T4TEXT) :
                       gamemission == pack_plut ? DEH_String(P4TEXT) :
                                                  DEH_String(C4TEXT);
          break;
        case 15:
          finaleflat = DEH_String(BGFLAT15);
          finaletext = gamemission == pack_tnt  ? DEH_String(T5TEXT) :
                       gamemission == pack_plut ? DEH_String(P5TEXT) :
                                                  DEH_String(C5TEXT);
          break;
        case 31:
          finaleflat = DEH_String(BGFLAT31);
          finaletext = gamemission == pack_tnt  ? DEH_String(T6TEXT) :
                       gamemission == pack_plut ? DEH_String(P6TEXT) :
                                                  DEH_String(C6TEXT);
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
      finaletext = DEH_String(C1TEXT);  // FIXME - other text, music?
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

// ID24 EndFinale
static int ef_callee_count = 0;
static int ef_callee_point = 0;
static cast_anim_t *ef_current_callee = NULL;
static cast_frame_t *ef_current_frame = NULL;
static boolean ef_current_alive = false;
static int ef_current_duration = 0;

static void EndFinaleCast_Frame(cast_frame_t *frame)
{
    ef_current_frame = frame;
    ef_current_duration = ef_current_frame->duration;

    if (ef_current_frame->sound)
    {
        S_StartSound(NULL, ef_current_frame->sound);
    }
}

static void EndFinaleCast_CalleeAlive(cast_anim_t *callee)
{
    ef_current_callee = callee;
    ef_current_alive = true;
    if (ef_current_callee)
    {
        S_StartSound(NULL, ef_current_callee->alertsound);
    }
    EndFinaleCast_Frame(ef_current_callee->aliveframes);
}

static void EndFinaleCast_CalleeDead(void)
{
    ef_current_alive = false;
    EndFinaleCast_Frame(ef_current_callee->deathframes);
}

static void EndFinaleCast_SetupCall(void)
{
    S_ChangeMusInfoMusic(W_GetNumForName(endfinale->music), endfinale->musicloops);
    W_CacheLumpName(endfinale->background, PU_LEVEL);
    ef_callee_count = endfinale->cast_animscount;

    cast_anim_t *callee;
    array_foreach(callee, endfinale->cast_anims)
    {
        cast_frame_t *frame;
        array_foreach(frame, callee->aliveframes)
        {
            W_CacheSpriteName(frame->frame_lump, PU_LEVEL);
            frame->tranmap = (W_CheckNumForName(frame->tran_lump) >= 0)
                           ? W_CacheLumpName(frame->tran_lump, PU_LEVEL)
                           : NULL;
            frame->xlat = (W_CheckNumForName(frame->xlat_lump) >= 0)
                        ? W_CacheLumpName(frame->xlat_lump, PU_LEVEL)
                        : NULL;
        }
        array_foreach(frame, callee->deathframes)
        {
            W_CacheSpriteName(frame->frame_lump, PU_LEVEL);
            frame->tranmap = (W_CheckNumForName(frame->tran_lump) >= 0)
                           ? W_CacheLumpName(frame->tran_lump, PU_LEVEL)
                           : NULL;
            frame->xlat = (W_CheckNumForName(frame->xlat_lump) >= 0)
                        ? W_CacheLumpName(frame->xlat_lump, PU_LEVEL)
                        : NULL;
        }
    }

    EndFinaleCast_CalleeAlive(&endfinale->cast_anims[0]);
}

static boolean EndFinaleCast_Ticker(void)
{
    boolean loop_finished = false;

    if (--ef_current_duration <= 0)
    {
        cast_frame_t *start = ef_current_alive ? ef_current_callee->aliveframes : ef_current_callee->deathframes;
        cast_frame_t *end = start + (ef_current_alive ? ef_current_callee->aliveframescount : ef_current_callee->deathframescount);
        cast_frame_t *next = ef_current_frame + 1;

        if (ef_current_alive || (next != end))
        {
            EndFinaleCast_Frame(next != end ? next : start);
        }
        else
        {
            ef_callee_point = (ef_callee_point + 1) % ef_callee_count;
            cast_anim_t *next_callee = &endfinale->cast_anims[ef_callee_point];

            // When out of bounds
            if (next_callee == ef_current_callee + ef_callee_count)
            {
                // If possible, go to next map, else start again
                loop_finished = true;
                next_callee = endfinale->donextmap || (wminfo.nextmapinfo != NULL)
                            ? NULL
                            : ef_current_callee;
            }

            if (next_callee)
            {
                EndFinaleCast_CalleeAlive(next_callee);
            }
        }
    }

    return loop_finished;
}

static boolean EndFinaleCast_Responder(event_t *ev)
{
    if (!(ev->type == ev_keydown || ev->type == ev_mouseb_down
          || ev->type == ev_joyb_down))
    {
        return false;
    }

    if (ef_current_alive)
    {
        EndFinaleCast_CalleeDead();
    }

    return true;
}

static void F_CastPrint(const char *text);

void EndFinaleCast_Drawer(void)
{
    V_DrawPatchFullScreen(W_CacheLumpName(endfinale->background, PU_LEVEL));
    F_CastPrint(ef_current_callee->name);
    patch_t *frame = W_CacheSpriteName(ef_current_frame->frame_lump, PU_LEVEL);
    const byte *tranmap = ef_current_frame->tranmap;
    const byte *xlat = ef_current_frame->xlat;
    boolean flip = ef_current_frame->flipped;
    V_DrawPatchCastCall(frame, tranmap, xlat, flip);
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
  const char *name;
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
  wipegamestate = -1; // force a screen wipe
  finalestage = FINALE_STAGE_CAST;

  if (gamemapinfo->flags & MapInfo_EndGameCustomFinale)
  {
    EndFinaleCast_SetupCall();
    return;
  }

  // Ty 03/23/98 - clumsy but time is of the essence
  castorder[0].name = DEH_String(CC_ZOMBIE),  castorder[0].type = MT_POSSESSED;
  castorder[1].name = DEH_String(CC_SHOTGUN), castorder[1].type = MT_SHOTGUY;
  castorder[2].name = DEH_String(CC_HEAVY),   castorder[2].type = MT_CHAINGUY;
  castorder[3].name = DEH_String(CC_IMP),     castorder[3].type = MT_TROOP;
  castorder[4].name = DEH_String(CC_DEMON),   castorder[4].type = MT_SERGEANT;
  castorder[5].name = DEH_String(CC_LOST),    castorder[5].type = MT_SKULL;
  castorder[6].name = DEH_String(CC_CACO),    castorder[6].type = MT_HEAD;
  castorder[7].name = DEH_String(CC_HELL),    castorder[7].type = MT_KNIGHT;
  castorder[8].name = DEH_String(CC_BARON),   castorder[8].type = MT_BRUISER;
  castorder[9].name = DEH_String(CC_ARACH),   castorder[9].type = MT_BABY;
  castorder[10].name = DEH_String(CC_PAIN),   castorder[10].type = MT_PAIN;
  castorder[11].name = DEH_String(CC_REVEN),  castorder[11].type = MT_UNDEAD;
  castorder[12].name = DEH_String(CC_MANCU),  castorder[12].type = MT_FATSO;
  castorder[13].name = DEH_String(CC_ARCH),   castorder[13].type = MT_VILE;
  castorder[14].name = DEH_String(CC_SPIDER), castorder[14].type = MT_SPIDER;
  castorder[15].name = DEH_String(CC_CYBER),  castorder[15].type = MT_CYBORG;
  castorder[16].name = DEH_String(CC_HERO),   castorder[16].type = MT_PLAYER;
  castorder[17].name = NULL,                  castorder[17].type = 0;

  castnum = 0;
  caststate = &states[mobjinfo[castorder[castnum].type].seestate];
  casttics = caststate->tics;
  castdeath = false;
  castframes = 0;
  castonmelee = 0;
  castattacking = false;
  S_ChangeMusic(mus_evil, true);
}


//
// F_CastTicker
//
static boolean F_CastTicker(void)
{
  int st;
  int sfx;

  if (gamemapinfo->flags & MapInfo_EndGameCustomFinale)
    return EndFinaleCast_Ticker();

  if (--casttics > 0)
    return false; // not time to change state yet

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
  return false;
}


//
// F_CastResponder
//

static boolean F_CastResponder(event_t* ev)
{
  if (gamemapinfo->flags & MapInfo_EndGameCustomFinale)
    return EndFinaleCast_Responder(ev);

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


static void F_CastPrint(const char* text)
{
  const char* ch;
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
  if (gamemapinfo->flags & MapInfo_EndGameCustomFinale)
  {
      EndFinaleCast_Drawer();
      return;
  }

  spritedef_t*        sprdef;
  spriteframe_t*      sprframe;
  int                 lump;
  boolean             flip;
  patch_t*            patch;
    
  // erase the entire screen to a background
  // Ty 03/30/98 bg texture extern
  V_DrawPatchFullScreen(
    V_CachePatchName(W_CheckWidescreenPatch(DEH_String(BGCASTCALL)), PU_CACHE));

  F_CastPrint (castorder[castnum].name);
    
  // draw the current frame in the middle of the screen
  sprdef = &sprites[caststate->sprite];
  sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
  lump = sprframe->lump[0];
  flip = (boolean)sprframe->flip[0];
                        
  patch = V_CachePatchNum (lump+firstspritelump, PU_CACHE);
  V_DrawPatchCastCall(patch, NULL, NULL, flip);
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
