//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2020 by Ethan Watson
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
//      Rendering main loop and setup functions,
//       utility functions (BSP, geometry, trigonometry).
//      See tables.c, too.
//
//-----------------------------------------------------------------------------

#define _USE_MATH_DEFINES
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "d_loop.h"
#include "d_player.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_video.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_setup.h" // P_SegLengths
#include "r_bsp.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_bmaps.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_swirl.h"
#include "r_things.h"
#include "r_voxel.h"
#include "m_config.h"
#include "st_stuff.h"
#include "v_flextran.h"
#include "v_video.h"
#include "z_zone.h"

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

// killough: viewangleoffset is a legacy from the pre-v1.2 days, when Doom
// had Left/Mid/Right viewing. +/-ANG90 offsets were placed here on each
// node, by d_net.c, to set up a L/M/R session.

int viewangleoffset;
int validcount = 1;         // increment every time a check is made
lighttable_t *fixedcolormap;
int fixedcolormapindex;
int      centerx, centery;
fixed_t  centerxfrac, centeryfrac;
fixed_t  projection;
fixed_t  skyiscale;
fixed_t  viewx, viewy, viewz;
angle_t  viewangle;
localview_t localview;
boolean raw_input;
fixed_t  viewcos, viewsin;
player_t *viewplayer;
fixed_t  viewheightfrac; // [FG] sprite clipping optimizations
int max_project_slope = 4;

static fixed_t focallength, lightfocallength;

//
// precalculated math tables
//

angle_t clipangle;
angle_t vx_clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 

int viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.

angle_t *xtoviewangle = NULL;   // killough 2/8/98

// [FG] linear horizontal sky scrolling
angle_t *linearskyangle = NULL;

int LIGHTLEVELS;
int LIGHTSEGSHIFT;
int LIGHTBRIGHT;
int MAXLIGHTSCALE;
int LIGHTSCALESHIFT;
int MAXLIGHTZ;
int LIGHTZSHIFT;

// killough 3/20/98: Support dynamic colormaps, e.g. deep water
// killough 4/4/98: support dynamic number of them as well

int numcolormaps;
lighttable_t *fullcolormap;
lighttable_t **colormaps;

// updated thanks to Rum-and-Raisin Doom, Ethan Watson
int* scalelightoffset;
int* scalelightindex;
int* zlightoffset;
int* zlightindex;
int* planezlightoffset;
int  planezlightindex;
int* walllightoffset;
int  walllightindex;

// killough 3/20/98, 4/4/98: end dynamic colormaps

int extralight;                           // bumped light from gun blasts

int extra_level_brightness;               // level brightness feature

void (*colfunc)(void);                    // current column draw function

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

// Workaround for optimization bug in clang
// fixes desync in competn/doom/fp2-3655.lmp and in dmnsns.wad dmn01m909.lmp
#if defined(__clang__)
int R_PointOnSide(volatile fixed_t x, volatile fixed_t y, node_t *node)
#else
int R_PointOnSide(fixed_t x, fixed_t y, node_t *node)
#endif
{
  if (!node->dx)
    return x <= node->x ? node->dy > 0 : node->dy < 0;

  if (!node->dy)
    return y <= node->y ? node->dx < 0 : node->dx > 0;
        
  x -= node->x;
  y -= node->y;
  
  // Try to quickly decide by looking at sign bits.
  if ((node->dy ^ node->dx ^ x ^ y) < 0)
    return (node->dy ^ x) < 0;  // (left is negative)
  return FixedMul(y, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, x);
}

// killough 5/2/98: reformatted

int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line)
{
  fixed_t lx = line->v1->x;
  fixed_t ly = line->v1->y;
  fixed_t ldx = line->v2->x - lx;
  fixed_t ldy = line->v2->y - ly;

  if (!ldx)
    return x <= lx ? ldy > 0 : ldy < 0;

  if (!ldy)
    return y <= ly ? ldx < 0 : ldx > 0;
  
  x -= lx;
  y -= ly;
        
  // Try to quickly decide by looking at sign bits.
  if ((ldy ^ ldx ^ x ^ y) < 0)
    return (ldy ^ x) < 0;          // (left is negative)
  return FixedMul(y, ldx>>FRACBITS) >= FixedMul(ldy>>FRACBITS, x);
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{       
  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ? 
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0 
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :               // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
}

angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y)
{       
  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ? 
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0 
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :               // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
}

// [FG] overflow-safe R_PointToAngle() flavor,
// only used in R_CheckBBox(), R_AddLine() and P_SegLengths()

angle_t R_PointToAngleCrispy(fixed_t x, fixed_t y)
{
  // [FG] fix overflows for very long distances
  int64_t y_viewy = (int64_t)y - viewy;
  int64_t x_viewx = (int64_t)x - viewx;

  // [FG] the worst that could happen is e.g. INT_MIN-INT_MAX = 2*INT_MIN
  if (x_viewx < INT_MIN || x_viewx > INT_MAX || y_viewy < INT_MIN || y_viewy > INT_MAX)
  {
    // [FG] preserving the angle by halfing the distance in both directions
    x = (int)(x_viewx / 2 + viewx);
    y = (int)(y_viewy / 2 + viewy);
  }

  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ?
        (x > y) ? tantoangle[SlopeDivCrispy(y,x)] :                      // octant 0
                ANG90-1-tantoangle[SlopeDivCrispy(x,y)] :                // octant 1
        x > (y = -y) ? 0-tantoangle[SlopeDivCrispy(y,x)] :               // octant 8
                       ANG270+tantoangle[SlopeDivCrispy(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDivCrispy(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDivCrispy(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[SlopeDivCrispy(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDivCrispy(x,y)] : // octant 5
    0;
}

// WiggleFix: move R_ScaleFromGlobalAngle to r_segs.c,
// above R_StoreWallRange


// [crispy] in widescreen mode, make sure the same number of horizontal
// pixels shows the same part of the game scene as in regular rendering mode
static int scaledviewwidth_nonwide, viewwidth_nonwide;
static fixed_t centerxfrac_nonwide;

//
// CalcMaxProjectSlope
// Calculate the minimum divider needed to provide at least 45 degrees of FOV
// padding. For fast rejection during sprite/voxel projection.
//

static void CalcMaxProjectSlope(int fov)
{
  max_project_slope = 16;

  for (int i = 1; i < 16; i++)
  {
    if (atan(i) * FINEANGLES / M_PI - fov >= FINEANGLES / 8)
    {
      max_project_slope = i;
      break;
    }
  }
}

//
// R_InitTextureMapping
//
// killough 5/2/98: reformatted

static void R_InitTextureMapping(void)
{
  register int i,x;
  fixed_t slopefrac;
  angle_t fov;
  double linearskyfactor;

  // Use tangent table to generate viewangletox:
  //  viewangletox will give the next greatest x
  //  after the view angle.
  //
  // Calc focallength
  //  so FIELDOFVIEW angles covers SCREENWIDTH.

  if (custom_fov == FOV_DEFAULT && centerxfrac == centerxfrac_nonwide)
  {
    fov = FIELDOFVIEW;
    slopefrac = finetangent[FINEANGLES / 4 + fov / 2];
    focallength = FixedDiv(centerxfrac_nonwide, slopefrac);
    lightfocallength = centerxfrac_nonwide;
    projection = centerxfrac_nonwide;
  }
  else
  {
    const double slope = (tan(custom_fov * M_PI / 360.0) *
                          centerxfrac / centerxfrac_nonwide);

    // For correct light across FOV range. Calculated like R_InitTables().
    const double lightangle = atan(slope) + M_PI / FINEANGLES;
    const double lightslopefrac = tan(lightangle) * FRACUNIT;
    lightfocallength = FixedDiv(centerxfrac, lightslopefrac);

    fov = atan(slope) * FINEANGLES / M_PI;
    slopefrac = finetangent[FINEANGLES / 4 + fov / 2];
    focallength = FixedDiv(centerxfrac, slopefrac);
    projection = centerxfrac / slope;
  }

  for (i=0 ; i<FINEANGLES/2 ; i++)
    {
      int t;
      if (finetangent[i] > slopefrac)
        t = -1;
      else
        if (finetangent[i] < -slopefrac)
          t = viewwidth+1;
      else
        {
          t = FixedMul(finetangent[i], focallength);
          t = (centerxfrac - t + FRACMASK) >> FRACBITS;
          if (t < -1)
            t = -1;
          else
            if (t > viewwidth+1)
              t = viewwidth+1;
        }
      viewangletox[i] = t;
    }
    
  // Scan viewangletox[] to generate xtoviewangle[]:
  //  xtoviewangle will give the smallest view angle
  //  that maps to x.

  linearskyfactor = FixedToDouble(slopefrac) * ANG90;

  for (x=0; x<=viewwidth; x++)
    {
      for (i=0; viewangletox[i] > x; i++)
        ;
      xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
      // [FG] linear horizontal sky scrolling
      int angle = (0.5 - x / (double)viewwidth) * linearskyfactor;
      linearskyangle[x] = (angle >= 0) ? angle : ANGLE_MAX + angle;
    }
    
  // Take out the fencepost cases from viewangletox.
  for (i=0; i<FINEANGLES/2; i++)
    if (viewangletox[i] == -1)
      viewangletox[i] = 0;
    else 
      if (viewangletox[i] == viewwidth+1)
        viewangletox[i] = viewwidth;
        
  clipangle = xtoviewangle[0];

  vx_clipangle = clipangle - ((fov << ANGLETOFINESHIFT) - ANG90);
  CalcMaxProjectSlope(fov);
}

//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//

#define DISTMAP 2

boolean smoothlight;

void R_InitLightTables (void)
{
  if (smoothlight)
  {
      LIGHTLEVELS = 32;
      LIGHTSEGSHIFT = 3;
      LIGHTBRIGHT = 2;
      MAXLIGHTSCALE = 48;
      LIGHTSCALESHIFT = 12;
      MAXLIGHTZ = 1024;
      LIGHTZSHIFT = 17;
  }
  else
  {
      LIGHTLEVELS = 16;
      LIGHTSEGSHIFT = 4;
      LIGHTBRIGHT = 1;
      MAXLIGHTSCALE = 48;
      LIGHTSCALESHIFT = 12;
      MAXLIGHTZ = 128;
      LIGHTZSHIFT = 20;
  }

  // killough 4/4/98: dynamic colormaps
  // ScaleLight calculated below
  int NumZLightEntries = LIGHTLEVELS * MAXLIGHTZ;
  zlightoffset = (int*)Z_Malloc(sizeof(int) * NumZLightEntries, PU_STATIC, NULL);
  zlightindex  = (int*)Z_Malloc(sizeof(int) * NumZLightEntries, PU_STATIC, NULL);

  // Calculate the light levels to use
  //  for each level / distance combination.
  for (int lightlevel = 0; lightlevel < LIGHTLEVELS; lightlevel++)
  {
    int lightz, startmap = ((LIGHTLEVELS-LIGHTBRIGHT-lightlevel)*2)*NUMCOLORMAPS/LIGHTLEVELS;
    for (lightz = 0; lightz < MAXLIGHTZ; lightz++)
    {
      int scale = FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (lightz + 1) << LIGHTZSHIFT);
      int level = startmap - (scale >> LIGHTSCALESHIFT) / DISTMAP;
      level = CLAMP(level, 0, NUMCOLORMAPS - 1);

      // killough 3/20/98: Initialize multiple colormaps
      // killough 4/4/98
      // updated thanks to Rum-and-Raisin Doom
      zlightindex[lightlevel * MAXLIGHTZ + lightz] = level;
      zlightoffset[lightlevel * MAXLIGHTZ + lightz] = level * 256;
    }
  }
}

boolean setsmoothlight;

void R_SmoothLight(void)
{
  setsmoothlight = false;
  // [crispy] re-calculate the zlight[][] array
  R_InitLightTables();
  // [crispy] re-calculate the scalelight[][] array
  // R_ExecuteSetViewSize();
  // [crispy] re-calculate fake contrast
  P_SegLengths(true);
}

int R_GetLightIndex(fixed_t scale)
{
  const int index = ((int64_t)scale * (160 << FRACBITS) / lightfocallength) >> LIGHTSCALESHIFT;
  return clampi(index, 0, MAXLIGHTSCALE - 1);
}

static fixed_t viewpitch;

static void R_SetupFreelook(void)
{
  fixed_t dy;
  int i;

  if (viewpitch)
  {
    dy = FixedMul(projection, -finetangent[(ANG90 - viewpitch) >> ANGLETOFINESHIFT]);
  }
  else
  {
    dy = 0;
  }

  centery = viewheight / 2 + (dy >> FRACBITS);
  centeryfrac = centery << FRACBITS;

  for (i = 0; i < viewheight; i++)
  {
    dy = abs(((i - centery) << FRACBITS) + FRACUNIT / 2);
    yslope[i] = FixedDiv(projection, dy);
  }
}


//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//

boolean setsizeneeded;
int     setblocks;

void R_SetViewSize(int blocks)
{
  setsizeneeded = true;
  setblocks = MIN(blocks, 11);
}

//
// R_ExecuteSetViewSize
//

void R_ExecuteSetViewSize (void)
{
  int i;
  vrect_t view;

  setsizeneeded = false;

  if (setblocks == 11)
    {
      scaledviewwidth_nonwide = NONWIDEWIDTH;
      scaledviewwidth = video.unscaledw;
      scaledviewheight = SCREENHEIGHT;                    // killough 11/98
    }
  // [crispy] hard-code to SCREENWIDTH and SCREENHEIGHT minus status bar height
  else if (setblocks == 10)
    {
      scaledviewwidth_nonwide = NONWIDEWIDTH;
      scaledviewwidth = video.unscaledw;
      scaledviewheight = SCREENHEIGHT - ST_HEIGHT;
    }
  else
    {
      const int st_screen = SCREENHEIGHT - ST_HEIGHT;

      scaledviewwidth_nonwide = setblocks * 32;
      scaledviewheight = (setblocks * st_screen / 10) & ~7; // killough 11/98

      if (video.unscaledw > SCREENWIDTH)
        scaledviewwidth = (scaledviewheight * video.unscaledw / st_screen) & ~7;
      else
        scaledviewwidth = scaledviewwidth_nonwide;
    }

  scaledviewx = (video.unscaledw - scaledviewwidth) / 2;

  if (scaledviewwidth == video.unscaledw)
    scaledviewy = 0;
  else
    scaledviewy = (SCREENHEIGHT - ST_HEIGHT - scaledviewheight) / 2;

  view.x = scaledviewx;
  view.y = scaledviewy;
  view.w = scaledviewwidth;
  view.h = scaledviewheight;

  V_ScaleRect(&view);

  viewwindowx = view.sx;
  viewwindowy = view.sy;
  viewwidth   = view.sw;
  viewheight  = view.sh;

  viewwidth_nonwide = V_ScaleX(scaledviewwidth_nonwide);

  centerxfrac = (viewwidth << FRACBITS) / 2;
  centerx = centerxfrac >> FRACBITS;
  centerxfrac_nonwide = (viewwidth_nonwide << FRACBITS) / 2;

  viewheightfrac = viewheight << (FRACBITS + 1); // [FG] sprite clipping optimizations

  R_InitBuffer();       // killough 11/98

  R_InitTextureMapping();

  R_SetupFreelook();

  // psprite scales
  pspritescale = FixedDiv(viewwidth_nonwide, SCREENWIDTH);       // killough 11/98
  pspriteiscale = FixedDiv(SCREENWIDTH, viewwidth_nonwide);      // killough 11/98

  // [FG] make sure that the product of the weapon sprite scale factor
  //      and its reciprocal is always at least FRACUNIT to
  //      fix garbage lines at the top of weapon sprites
  while (FixedMul(pspriteiscale, pspritescale) < FRACUNIT)
    pspriteiscale++;

  if (custom_fov == FOV_DEFAULT)
  {
    skyiscale = FixedDiv(SCREENWIDTH, viewwidth_nonwide);
  }
  else
  {
    skyiscale = tan(custom_fov * M_PI / 360.0) * SCREENWIDTH / viewwidth_nonwide * FRACUNIT;
  }

  for (i=0 ; i<viewwidth ; i++)
    {
      fixed_t cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
      distscale[i] = FixedDiv(FRACUNIT,cosadj);
      // thing clipping
      screenheightarray[i] = viewheight;
    }

  // Lightz calculated above
  int NumScaleLightEntries = LIGHTLEVELS * MAXLIGHTSCALE;
  scalelightindex  = (int*)Z_Malloc(sizeof(int) * NumScaleLightEntries, PU_STATIC, NULL);
  scalelightoffset = (int*)Z_Malloc(sizeof(int) * NumScaleLightEntries, PU_STATIC, NULL);

  // Calculate the light levels to use
  //  for each level / scale combination.
  for (int lightlevel = 0; lightlevel < LIGHTLEVELS; lightlevel++)
  {
    int startmap = ((LIGHTLEVELS - LIGHTBRIGHT - lightlevel) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
    for (int lightscale = 0; lightscale < MAXLIGHTSCALE; lightscale++)
    {
      // killough 11/98:
      int level = startmap - lightscale / DISTMAP;
      level = CLAMP(level, 0, NUMCOLORMAPS - 1);

      // killough 3/20/98: initialize multiple colormaps
      // killough 4/4/98
      // updated thanks to Rum-and-Raisin Doom
      scalelightindex[lightlevel * MAXLIGHTSCALE + lightscale] = level;
      scalelightoffset[lightlevel * MAXLIGHTSCALE + lightscale] = level * 256;
    }
  }

  st_refresh_background = true;
}

//
// R_Init
//

void R_Init (void)
{
  R_InitData();
  R_SetViewSize(screenblocks);
  R_InitPlanes();
  R_InitLightTables();
  R_InitTranslationTables();
  V_InitFlexTranTable();

  // [FG] spectre drawing mode
  R_SetFuzzColumnMode();

  colfunc = R_DrawColumn;
}

//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
  int nodenum = numnodes-1;

  // [FG] fix crash when loading trivial single subsector maps
  if (!numnodes)
  {
    return subsectors;
  }

  while (!(nodenum & NF_SUBSECTOR))
    nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];
  return &subsectors[nodenum & ~NF_SUBSECTOR];
}

static inline boolean CheckLocalView(const player_t *player)
{
  return (
    // Don't use localview if the player is spying.
    player == &players[consoleplayer] &&
    // Don't use localview if the player is dead.
    player->playerstate != PST_DEAD &&
    // Don't use localview if the player just teleported.
    !player->mo->reactiontime &&
    // Don't use localview if a demo is playing.
    !demoplayback &&
    // Don't use localview during a netgame (single-player or solo-net only).
    (!netgame || solonet)
  );
}

static angle_t CalcViewAngle_RawInput(const player_t *player)
{
  return (player->mo->angle + localview.angle - player->ticangle +
          LerpAngle(player->oldticangle, player->ticangle));
}

static angle_t CalcViewAngle_LerpFakeLongTics(const player_t *player)
{
  return LerpAngle(player->mo->oldangle + localview.oldlerpangle,
                   player->mo->angle + localview.lerpangle);
}

static angle_t (*CalcViewAngle)(const player_t *player);

void R_UpdateViewAngleFunction(void)
{
  if (raw_input)
  {
    CalcViewAngle = CalcViewAngle_RawInput;
  }
  else if (lowres_turn && fake_longtics)
  {
    CalcViewAngle = CalcViewAngle_LerpFakeLongTics;
  }
  else
  {
    CalcViewAngle = NULL;
  }
}

//
// R_SetupFrame
//

void R_SetupFrame (player_t *player)
{
  int cm;
  fixed_t pitch;
  const boolean use_localview = CheckLocalView(player);
  const boolean camera_ready = (
    // Don't interpolate on the first tic of a level,
    // otherwise oldviewz might be garbage.
    leveltime > 1 &&
    // Don't interpolate if the player did something
    // that would necessitate turning it off for a tic.
    player->mo->interp == true &&
    // Don't interpolate during a paused state
    leveltime > oldleveltime
  );

  viewplayer = player;
  // [AM] Interpolate the player camera if the feature is enabled.
  if (uncapped && camera_ready)
  {
    // Interpolate player camera from their old position to their current one.
    viewx = LerpFixed(player->mo->oldx, player->mo->x);
    viewy = LerpFixed(player->mo->oldy, player->mo->y);
    viewz = LerpFixed(player->oldviewz, player->viewz);

    if (use_localview && CalcViewAngle)
    {
      viewangle = CalcViewAngle(player);
    }
    else
    {
      viewangle = LerpAngle(player->mo->oldangle, player->mo->angle);
    }

    if (use_localview && raw_input && !player->centering)
    {
      pitch = player->pitch + localview.pitch;
      pitch = CLAMP(pitch, -max_pitch_angle, max_pitch_angle);
    }
    else
    {
      pitch = LerpFixed(player->oldpitch, player->pitch);
    }

    // [crispy] pitch is actual lookdir and weapon pitch
    pitch += LerpFixed(player->oldrecoilpitch, player->recoilpitch);
  }
  else
  {
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewz = player->viewz; // [FG] moved here
    viewangle = player->mo->angle;
    // [crispy] pitch is actual lookdir and weapon pitch
    pitch = player->pitch + player->recoilpitch;

    if (camera_ready && use_localview && lowres_turn && fake_longtics)
    {
      viewangle += localview.angle;
    }
  }

  if (pitch != viewpitch)
  {
    viewpitch = pitch;
    R_SetupFreelook();
  }

  // 3-screen display mode.
  viewangle += viewangleoffset;

  extralight = player->extralight;
  extralight += STRICTMODE(LIGHTBRIGHT * extra_level_brightness);

  viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
  viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

  // killough 3/20/98, 4/4/98: select colormap based on player status

  if (player->mo->subsector->sector->heightsec != -1)
    {
      const sector_t *s = player->mo->subsector->sector->heightsec + sectors;
      cm = viewz < s->interpfloorheight ? s->bottommap : viewz > s->interpceilingheight ?
        s->topmap : s->midmap;
      if (cm < 0 || cm > numcolormaps)
        cm = 0;
    }
  else
    cm = 0;

  fullcolormap = colormaps[cm];
  fixedcolormapindex = player->fixedcolormap;

  if (fixedcolormapindex)
  {
    // killough 3/20/98: use fullcolormap
    fixedcolormap = fullcolormap + fixedcolormapindex * 256;
  }
  else
  {
    fixedcolormap = 0;
  }

  validcount++;
}

//
// R_ShowStats
//

int rendered_visplanes, rendered_segs, rendered_vissprites, rendered_voxels;

static void R_ClearStats(void)
{
  rendered_visplanes = 0;
  rendered_segs = 0;
  rendered_vissprites = 0;
  rendered_voxels = 0;
}

static boolean flashing_hom;
int autodetect_hom = 0;       // killough 2/7/98: HOM autodetection flag

//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{       
  R_ClearStats();

  R_SetupFrame (player);

  // Clear buffers.
  R_ClearClipSegs ();
  R_ClearDrawSegs ();
  R_ClearPlanes ();
  R_ClearSprites ();
  VX_ClearVoxels ();

  if (autodetect_hom)
    { // killough 2/10/98: add flashing red HOM indicators
      pixel_t c[47*47];
      int i , color = !flashing_hom || (gametic % 20) < 9 ? 0xb0 : 0;
      V_FillRect(scaledviewx, scaledviewy, scaledviewwidth, scaledviewheight, color);
      for (i=0;i<47*47;i++)
        {
          char t =
"/////////////////////////////////////////////////////////////////////////////"
"/////////////////////////////////////////////////////////////////////////////"
"///////jkkkkklk////////////////////////////////////hkllklklkklkj/////////////"
"///////////////////jkkkkklklklkkkll//////////////////////////////kkkkkklklklk"
"lkkkklk//////////////////////////jllkkkkklklklklklkkklk//////////////////////"
"//klkkllklklklkllklklkkklh//////////////////////kkkkkjkjjkkj\3\205\214\3lllkk"
"lkllh////////////////////kllkige\211\210\207\206\205\204\203\203\203\205`\206"
"\234\234\234\234kkllg//////////////////klkkjhfe\210\206\203\203\203\202\202"
"\202\202\202\202\203\205`\207\211eikkk//////////////////kkkk\3g\211\207\206"
"\204\203\202\201\201\200\200\200\200\200\201\201\202\204b\210\211\3lkh///////"
"//////////lklki\213\210b\206\203\201\201\200\200\200\200\200Z\200\200\200\202"
"\203\204\205\210\211jll/////////////////lkkk\3\212\210b\205\202\201\200\200"
"\200XW\200\200\200\200\200\200\202\203\204\206\207eklj////////////////lkkjg"
"\211b\206\204\202\200\200\200YWWX\200Z\200\200\200\202\203\203\205bdjkk//////"
"//////////llkig\211a\205\203\202\200\200\200YXWX\200\200\200\200\200\201\202"
"\203\203\206\207ekk////////////////lkki\3\211\206\204\202\201\200\200XXWWWXX"
"\200\200\200\200\202\202\204\206\207ekk////////////////lkkj\3e\206\206\204\\"
"\200\200XWVVWWWXX\200\200\200\\\203\205\207\231kk////////////////lkkjjgcccfd"
"\207\203WVUVW\200\200\202\202\204\204\205\204\206\210gkk////////////////kkkkj"
"e``\210hjjgb\200W\200\205\206fhghcbdcdfkk////////////////jkkj\3\207ab\211e"
"\213j\3g\204XX\207\213jii\212\207\203\204\210gfkj///////////////j\211lkjf\210"
"\214\3\3kj\213\213\211\205X\200\205\212\210\213\213\213\211\210\203\205gelj//"
"////////////hf\211\213kh\212\212i\212gkh\202\203\210\210\202\201\206\207\206"
"\\kkhf\210aabkk//////////////je\210\210\3g\210\207\210e\210c\205\204\202\210"
"\207\203\202\210\205\203\203fjbe\213\210bbieW/////////////ke\207\206ie\206"
"\203\203\203\205\205\204\203\210\211\207\202\202\206\210\203\204\206\207\210"
"\211\231\206\206`\206\206]/////////////kf\\\202ig\204\203\202\201\\\202\202"
"\205\207\210\207\203\202\206\206\206\205\203\203\203\202\202\203\204b\206\204"
"Z/////////////i\3\\\204j\212\204\202\201\200\202\202\202\203\206\211\210\203"
"\203c\205\202\201\201\201\200\200\201\202\204a\204\201W/////////////j\3\207"
"\210jh\206\202\200\200\200\200\200\202\206\211\205\202\202bb\201\200\200\200"
"\200\200\200\202\203b\\WW/////////////jke\206jic\203\201\200\200\200\200\202"
"\211\211\201\200\200\204\210\201\200\200W\200\200\200\201\204c\\\200]////////"
"//////kd\210\3\3e\205\202\200\200W\200\202\211\210\210\201\202\207\210\203"
"\200WWW\200\200\202\205d\\\202///////////////kkdhigb\203\201\200\200\200\202"
"\206\210\210\205\210\211\206\203\200WWW\200\201\203ce\203\205////////////////"
"ijkig\211\203\201\200\200\202\206\207\207\205\206\207\210\206\203\200\200WW"
"\200\203\206ce\202_//////////////////jig\210\203\202\200\201\206\210\210\205"
"\204\204\205\206\206\204\202\200\200\200\200\203bcd////////////////////hjgc"
"\205\202\201\203\206\210\206\204\204\202\202\204\205\206\204\200\200\200\201"
"\206\207c//////////////////////j\3\207\204\203\202\202\211c\204\201W\200\200"
"\203\205\206\203\200\200\200\203\206b///////////////////////ihd\204\203\202"
"\201\207f\205VTVTW\202\210\206Z\200\200\203aa////////////////////////jg\204"
"\204\203\201\202\210\211\211c\206\205\210d\210\200\200\200\202\204ac/////////"
"///////////////j\3b\203\203\202\202\205\207\206\205\207\207\206\206\202\200"
"\201\202\203ac/////////////////////////iid\206\204\203\202\204\205\377\205"
"\204\205\204\203\201\200\202\203\203bc//////////////////////////ej\207\205"
"\203\201\202\202\203\207\204\203\202\202\201\201\203\203bd///////////////////"
"////////ee\3a\204\201\200\201\202\205\203\201\200\200\201\202\204\205cc//////"
"//////////////////////c\3ec\203\201\200\200\201\202\201\200\200\202\203\206cc"
"//////////////////////////////c\3f\206\203\201\200\200\200\200\200\201\203bdc"
"////////////////////////////////g\3\211\206\202\\\201\200\201\202\203dde/////"
"/////////////////////////////\234\3db\203\203\203\203adec////////////////////"
"/////////////////hffed\211de////////////////////"[i];
          c[i] = t=='/' ? color : t;
        }
      if (gametic-lastshottic < TICRATE*2 && gametic-lastshottic > TICRATE/8)
        V_DrawBlock(scaledviewx +  scaledviewwidth/2 - 24,
                    scaledviewy + scaledviewheight/2 - 24, 47, 47, c);
      R_DrawViewBorder();
    }

  // check for new console commands.
  NetUpdate ();

  // The head node is the last node output.
  R_RenderBSPNode (numnodes-1);

  R_NearbySprites ();

  // [FG] update automap while playing
  if (automap_on)
    return;

  // Check for new console commands.
  NetUpdate ();
    
  R_DrawPlanes ();
    
  // Check for new console commands.
  NetUpdate ();
    
  // [crispy] draw fuzz effect independent of rendering frame rate
  R_SetFuzzPosDraw();
  R_DrawMasked ();

  // Check for new console commands.
  NetUpdate ();
}

void R_InitAnyRes(void)
{
  R_InitSpritesRes();
  R_InitBufferRes();
  R_InitPlanesRes();
}

void R_BindRenderVariables(void)
{
  BIND_NUM_GENERAL(extra_level_brightness, 0, 0, 4, "Level brightness");
  BIND_NUM_GENERAL(fuzzmode, FUZZ_BLOCKY, FUZZ_BLOCKY, FUZZ_ORIGINAL,
    "Partial Invisibility (0 = Blocky; 1 = Refraction; 2 = Shadow, 3 = Original)");
  BIND_BOOL_GENERAL(stretchsky, false, "Stretch short skies");
  BIND_BOOL_GENERAL(linearsky, false, "Linear horizontal scrolling for skies");
  BIND_BOOL_GENERAL(r_swirl, false, "Swirling animated flats");
  BIND_BOOL_GENERAL(smoothlight, false, "Smooth diminishing lighting");
  M_BindBool("voxels_rendering", &default_voxels_rendering, &voxels_rendering,
             true, ss_none, wad_no, "Allow voxel models");
  BIND_BOOL_GENERAL(brightmaps, false,
    "Brightmaps for textures and sprites");
  BIND_NUM_GENERAL(invul_mode, INVUL_MBF, INVUL_VANILLA, INVUL_GRAY,
    "Invulnerability effect (0 = Vanilla; 1 = MBF; 2 = Gray)");
  BIND_BOOL(flashing_hom, true, "Enable flashing of the HOM indicator");
  M_BindNum("screenblocks", &screenblocks, NULL, 10, 3,
            UL, ss_stat, wad_no, "Size of game-world screen");
  BIND_NUM(default_max_pitch_angle, 32, 30, 60, "Maximum view pitch angle");

  M_BindBool("translucency", &translucency, NULL, true, ss_gen, wad_yes,
             "Translucency for some things");
  M_BindNum("tran_filter_pct", &tran_filter_pct, NULL,
            66, 0, 100, ss_none, wad_yes,
            "Percent of foreground/background translucency mix");

  M_BindBool("flipcorpses", &flipcorpses, NULL, false, ss_enem, wad_no,
             "Randomly mirrored death animations");

  BIND_BOOL(draw_nearby_sprites, true,
    "Draw sprites overlapping into visible sectors");
}

//----------------------------------------------------------------------------
//
// $Log: r_main.c,v $
// Revision 1.13  1998/05/07  00:47:52  killough
// beautification
//
// Revision 1.12  1998/05/03  23:00:14  killough
// beautification, fix #includes and declarations
//
// Revision 1.11  1998/04/07  15:24:15  killough
// Remove obsolete HOM detector
//
// Revision 1.10  1998/04/06  04:47:46  killough
// Support dynamic colormaps
//
// Revision 1.9  1998/03/23  03:37:14  killough
// Add support for arbitrary number of colormaps
//
// Revision 1.8  1998/03/16  12:44:12  killough
// Optimize away some function pointers
//
// Revision 1.7  1998/03/09  07:27:19  killough
// Avoid using FP for point/line queries
//
// Revision 1.6  1998/02/17  06:22:45  killough
// Comment out audible HOM alarm for now
//
// Revision 1.5  1998/02/10  06:48:17  killough
// Add flashing red HOM indicator for TNTHOM cheat
//
// Revision 1.4  1998/02/09  03:22:17  killough
// Make TNTHOM control HOM detector, change array decl to MAX_*
//
// Revision 1.3  1998/02/02  13:29:41  killough
// comment out dead code, add HOM detector
//
// Revision 1.2  1998/01/26  19:24:42  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
