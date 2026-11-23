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
#include <stdlib.h>
#include <string.h>

#include "d_event.h"
#include "d_loop.h"
#include "d_player.h"
#include "d_rollback.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_input.h"
#include "i_exit.h"
#include "i_system.h"
#include "i_thread.h"
#include "i_timer.h"
#include "i_video.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_tick.h"
#include "r_bsp.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_common.h"
#include "r_plane.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_swirl.h"
#include "r_things.h"
#include "r_voxel.h"
#include "m_config.h"
#include "m_profiler.h"
#include "st_stuff.h"
#include "v_palette.h"
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
const lighttable_t *fixedcolormap;
int fixedcolormapoffset;
int      centerx, centery;
fixed_t  centerxfrac, centeryfrac;
fixed_t  projection, yprojection;
fixed_t  skyiscale;
fixed_t  viewx, viewy, viewz;
angle_t  viewangle;
localview_t localview;
boolean raw_input;
fixed_t  viewcos, viewsin;
player_t *viewplayer;
fixed_t  bottomydeltafrac;
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

// killough 3/20/98: Support dynamic colormaps, e.g. deep water
// killough 4/4/98: support dynamic number of them as well

int numcolormaps;
lighttable_t *fullcolormap;
lighttable_t **colormaps;

int       ** scalelightoffset;
int       ** zlightoffset;

// [RNR] per-column distance correction for the flat rasteriser.
fixed_t *distscale = NULL;
static int distscale_size = 0;
// [MT] both point into the tables above but are selected while drawing,
// so they are thread-local like the rest of the drawer state.
THREADLOCAL int const *planezlightoffset;
THREADLOCAL int const *walllightoffset;

// killough 3/20/98, 4/4/98: end dynamic colormaps

int extralight;                           // bumped light from gun blasts

int extra_level_brightness;               // level brightness feature

THREADLOCAL void (*colfunc)(void);        // current column draw function

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//
int (*R_PointOnSide)(fixed_t x, fixed_t y, struct node_s *node) = R_PointOnSide_Classic;

int R_PointOnSide_Classic(fixed_t x, fixed_t y, node_t *node)
{
    if (!node->dx)
    {
        return x <= node->x ? node->dy > 0 : node->dy < 0;
    }

    if (!node->dy)
    {
        return y <= node->y ? node->dx < 0 : node->dx > 0;
    }

    // Workaround for optimization bug in clang
    // fixes desync in competn/doom/fp2-3655.lmp and in dmnsns.wad dmn01m909.lmp
    x = FixedSub(x, node->x);
    y = FixedSub(y, node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ x ^ y) < 0)
    {
        return (node->dy ^ x) < 0; // (left is negative)
    }
    return FixedMul(y, node->dx >> FRACBITS)
           >= FixedMul(node->dy >> FRACBITS, x);
}

int R_PointOnSide_Precise(fixed_t x, fixed_t y, node_t *node)
{
    if (!node->dx)
    {
        return x <= node->x ? node->dy > 0 : node->dy < 0;
    }

    if (!node->dy)
    {
        return y <= node->y ? node->dx < 0 : node->dx > 0;
    }

    x = FixedSub(x, node->x);
    y = FixedSub(y, node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ x ^ y) < 0)
    {
        return (node->dy ^ x) < 0; // (left is negative)
    }
    return (int64_t)y * node->dx >= (int64_t)node->dy * x;
}

// killough 5/2/98: reformatted
// [Woof!] rewritten to use only higher precision version
int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line)
{
  fixed_t lx = line->v1->x;
  fixed_t ly = line->v1->y;
  const fixed_t ldx = FixedSub(line->v2->x, lx);
  const fixed_t ldy = FixedSub(line->v2->y, ly);

  if (!ldx)
    return x <= lx ? ldy > 0 : ldy < 0;

  if (!ldy)
    return y <= ly ? ldx < 0 : ldx > 0;

  x = FixedSub(x, lx);
  y = FixedSub(y, ly);

  // Try to quickly decide by looking at sign bits.
  if ((ldy ^ ldx ^ x ^ y) < 0)
    return (ldy ^ x) < 0;          // (left is negative)
  return (int64_t) y * ldx >= (int64_t) x * ldy;
}

// Fold the view-relative vector into the first octant and look up its angle.
static inline angle_t PointToAngle(fixed_t x, fixed_t y,
                                   int (*slope_div)(unsigned, unsigned))
{
    if (x == 0 && y == 0)
    {
        return 0;
    }

    if (x >= 0)
    {
        if (y >= 0)
        {
            if (x > y)
            {
                return tantoangle[slope_div(y, x)]; // octant 0
            }
            return ANG90 - 1 - tantoangle[slope_div(x, y)]; // octant 1
        }

        y = FixedNeg(y);
        if (x > y)
        {
            return 0 - tantoangle[slope_div(y, x)]; // octant 8
        }
        return ANG270 + tantoangle[slope_div(x, y)]; // octant 7
    }

    x = FixedNeg(x);
    if (y >= 0)
    {
        if (x > y)
        {
            return ANG180 - 1 - tantoangle[slope_div(y, x)]; // octant 3
        }
        return ANG90 + tantoangle[slope_div(x, y)]; // octant 2
    }

    y = FixedNeg(y);
    if (x > y)
    {
        return ANG180 + tantoangle[slope_div(y, x)]; // octant 4
    }
    return ANG270 - 1 - tantoangle[slope_div(x, y)]; // octant 5
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
    return R_PointToAngle2(viewx, viewy, x, y);
}

angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y)
{
    return PointToAngle(FixedSub(x, viewx), FixedSub(y, viewy), SlopeDiv);
}

// [FG] overflow-safe R_PointToAngle() flavor,
// only used in R_CheckBBox(), R_AddLine() and P_SegLengths()

angle_t R_PointToAngleCrispy(fixed_t x, fixed_t y)
{
    // [FG] fix overflows for very long distances
    int64_t y_viewy = (int64_t)y - viewy;
    int64_t x_viewx = (int64_t)x - viewx;

    // [FG] the worst that could happen is e.g. INT_MIN-INT_MAX = 2*INT_MIN
    if (x_viewx <= INT32_MIN || x_viewx > INT32_MAX || y_viewy <= INT32_MIN
        || y_viewy > INT32_MAX)
    {
        // [FG] preserving the angle by halfing the distance in both directions
        x_viewx /= 2;
        y_viewy /= 2;
    }

    return PointToAngle((fixed_t)x_viewx, (fixed_t)y_viewy, SlopeDivCrispy);
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

  if (adjust_aspect)
  {
    yprojection = FixedMul(projection, video.aspect_v_factor);
    lightfocallength = FixedMul(lightfocallength, video.aspect_v_factor);
  }
  else
  {
    yprojection = projection;
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

  // [RNR] distscale table for the per-column flat rasteriser.

  if (distscale_size != viewwidth)
  {
    free(distscale);
    distscale = malloc(viewwidth * sizeof(*distscale));
    distscale_size = viewwidth;
  }

  for (i = 0; i < viewwidth; i++)
  {
    fixed_t cosadj = finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT];

    if (cosadj < 0)
      cosadj = -cosadj;
    else if (!cosadj) // extreme FOV guard; RNR has the same division
      cosadj = 1;

    distscale[i] = FixedDiv(FRACUNIT, cosadj);
  }

  if (custom_fov == FOV_DEFAULT)
  {
    skyiscale = FixedDiv(SCREENWIDTH, viewwidth_nonwide);
  }
  else
  {
    skyiscale = tan(custom_fov * M_PI / 360.0) * SCREENWIDTH / viewwidth_nonwide * FRACUNIT;
  }

  if (adjust_aspect)
  {
    skyiscale = FixedDiv(skyiscale, video.aspect_v_factor);
  }
}

//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//

#define DISTMAP 2

void R_InitLightTables (void)
{
  // killough 4/4/98: dynamic colormaps

  zlightoffset = Z_Malloc(sizeof(*zlightoffset) * LIGHTLEVELS, PU_STATIC, NULL);

  int *const all_zlightoffsets =
    Z_Malloc(sizeof(**zlightoffset) * LIGHTLEVELS * MAXLIGHTZ, PU_STATIC, NULL);

  // Calculate the light levels to use
  //  for each level / distance combination.
  for (int lightlevel = 0; lightlevel < LIGHTLEVELS; lightlevel++)
  {
    zlightoffset[lightlevel] = all_zlightoffsets + MAXLIGHTZ * lightlevel;

    const int startmap =
      ((LIGHTLEVELS - 1 - lightlevel) * 2) * NUMCOLORMAPS / LIGHTLEVELS;

    for (int lightz = 0; lightz < MAXLIGHTZ; lightz++)
    {
      const int scale =
        FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (lightz + 1) << LIGHTZSHIFT);

      int level = startmap - (scale >> LIGHTSCALESHIFT) / DISTMAP;
      level = CLAMP(level, 0, NUMCOLORMAPS - 1);

      zlightoffset[lightlevel][lightz] = level * 256;
    }
  }

  // [Woof!] scalelight has been made independent of view size,
  // so we initialize it here

  scalelightoffset = Z_Malloc(sizeof(*scalelightoffset) * LIGHTLEVELS, PU_STATIC, NULL);

  int *const all_scalelightoffsets =
    Z_Malloc(sizeof(**scalelightoffset) * LIGHTLEVELS * MAXLIGHTSCALE, PU_STATIC, NULL);

  // Calculate the light levels to use
  //  for each level / scale combination.
  for (int lightlevel = 0; lightlevel < LIGHTLEVELS; lightlevel++)
  {
    scalelightoffset[lightlevel] = all_scalelightoffsets + MAXLIGHTSCALE * lightlevel;

    const int startmap =
      ((LIGHTLEVELS - 1 - lightlevel) * 2) * NUMCOLORMAPS / LIGHTLEVELS;

    for (int lightscale = 0; lightscale < MAXLIGHTSCALE; lightscale++)
    {
      int level = startmap - lightscale / DISTMAP;
      level = CLAMP(level, 0, NUMCOLORMAPS - 1);

      scalelightoffset[lightlevel][lightscale] = level * 256;
    }
  }
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
    dy = FixedMul(yprojection, -finetangent[(ANG90 - viewpitch) >> ANGLETOFINESHIFT]);
  }
  else
  {
    dy = 0;
  }

  centery = viewheight / 2 + (dy >> FRACBITS);
  centeryfrac = centery << FRACBITS;

  bottomydeltafrac = (viewheight - centery) << FRACBITS;

  for (i = 0; i < viewheight; i++)
  {
    dy = abs(IntToFixed(i - centery));
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

  if (setblocks >= 10)
    {
      ST_UpdateStatusBar(); // let the new statusbar take effect
      ST_SetSTHeight();

      scaledviewwidth_nonwide = NONWIDEWIDTH;
      scaledviewwidth = video.unscaledw;
      scaledviewheight = SCREENHEIGHT - st_height; // killough 11/98
    }
  else
    {
      st_height = st_height_screenblocks10;

      const int st_screen = SCREENHEIGHT - st_height;

      scaledviewwidth_nonwide = setblocks * 32;
      scaledviewheight = (setblocks * st_screen / 10) & ~7; // killough 11/98

      if (!scaledviewheight)
        return;

      if (video.unscaledw > SCREENWIDTH)
        scaledviewwidth = (scaledviewheight * video.unscaledw / st_screen) & ~7;
      else
        scaledviewwidth = scaledviewwidth_nonwide;
    }

  scaledviewx = (video.unscaledw - scaledviewwidth) / 2;

  if (scaledviewwidth == video.unscaledw)
    scaledviewy = 0;
  else
    scaledviewy = (SCREENHEIGHT - st_height - scaledviewheight) / 2;

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

  if (adjust_aspect)
  {
    pspriteyscale = FixedMul(pspritescale, video.aspect_v_factor);
    pspriteyiscale = FixedDiv(pspriteiscale, video.aspect_v_factor);
  }
  else
  {
    pspriteyscale = pspritescale;
    pspriteyiscale = pspriteiscale;
  }

  for (i=0 ; i<viewwidth ; i++)
    {
      // thing clipping
      screenheightarray[i] = viewheight;
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

  // [MT] pre-create the missing-flat dummy on the main thread (PU_STATIC),
  // so render workers never allocate it from the zone.
  R_MissingFlat();

  // [FG] spectre drawing mode
  R_SetFuzzColumnMode();

  colfunc = R_DrawColumn;

  // [MT] worker pool for parallel render contexts; context 0 renders on this
  // (main) thread. Worker threads inherit nothing - their thread-local drawer
  // state is re-established at the start of every R_RenderViewContext().
  R_InitRenderThreads();
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
    // Don't use localview during a netgame, except when rollback can catch
    // the simulation up with the input that is shown ahead of it. A
    // transition stops the simulation short of the built tics, so fall
    // back to interpolation while one is pending.
    (!netgame || solonet
     || (RB_RawCameraAllowed() && gameaction == ga_nothing))
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
  const boolean raw_camera = use_localview && G_RawInputActive();
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
  // The tic clock passes a tic even when the simulation cannot run one
  // (confirmed input is missing). fractionaltic then wraps without gametic
  // advancing, and interpolating old -> new again would oscillate.
  static int last_gametic;
  static fixed_t last_fractionaltic;
  static boolean tic_stalled;

  if (gametic != last_gametic)
  {
    tic_stalled = false;
  }
  else if (fractionaltic < last_fractionaltic)
  {
    tic_stalled = true;
  }

  last_gametic = gametic;
  last_fractionaltic = fractionaltic;

  // Nothing to interpolate: stay where the simulation is, but keep showing
  // the local input that is built and waiting.
  const boolean raw_stall = (
    uncapped && raw_camera && tic_stalled &&
    // The input is not applied while the game is held.
    !paused && !menuactive &&
    leveltime > 1 && player->mo->interp == true
  );
  angle_t pending_angle = 0;
  fixed_t pending_pitch = 0;

  if (raw_camera && rollback_enabled)
  {
    D_GetUnrunLocalInput(&pending_angle, &pending_pitch);
  }

  viewplayer = player;
  // [AM] Interpolate the player camera if the feature is enabled.
  if (uncapped && camera_ready && !raw_stall)
  {
    // Interpolate player camera from their old position to their current one.
    viewx = LerpFixed(player->mo->oldx, player->mo->x);
    viewy = LerpFixed(player->mo->oldy, player->mo->y);
    viewz = LerpFixed(player->oldviewz, player->viewz);

    if (use_localview && CalcViewAngle)
    {
      viewangle = CalcViewAngle(player) + pending_angle;
    }
    else
    {
      viewangle = LerpAngle(player->mo->oldangle, player->mo->angle);
    }

    if (raw_camera && !player->centering)
    {
      pitch = player->pitch + localview.pitch + pending_pitch;
      pitch = CLAMP(pitch, -max_pitch_angle, max_pitch_angle);
    }
    else
    {
      pitch = LerpFixed(player->oldpitch, player->pitch);
    }

    // [crispy] pitch is actual lookdir and weapon pitch
    pitch += LerpFixed(player->oldrecoilpitch, player->recoilpitch);
  }
  else if (uncapped && raw_stall)
  {
    // fractionaltic keeps moving while no tic runs, so neither the position
    // nor oldticangle -> ticangle can be interpolated here: both would
    // oscillate. Show the last completed tic plus the input that is built.

    viewx = player->mo->x;
    viewy = player->mo->y;
    viewz = player->viewz;
    viewangle = player->mo->angle + localview.angle + pending_angle;

    if (!player->centering)
    {
      pitch = player->pitch + localview.pitch + pending_pitch;
      pitch = CLAMP(pitch, -max_pitch_angle, max_pitch_angle);
    }
    else
    {
      pitch = player->pitch;
    }

    // [crispy] pitch is actual lookdir and weapon pitch
    pitch += player->recoilpitch;
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
  extralight += STRICTMODE(extra_level_brightness);

  viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
  viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

  // killough 3/20/98, 4/4/98: select colormap based on player status
  const sector_t * sec = player->mo->subsector->sector;

  if (sec->colormap)
  {
    cm = sec->colormap;
  }
  else if (sec->heightsec != -1)
  {
    const sector_t * const s = &sectors[sec->heightsec];
    cm = viewz < s->interpfloorheight   ? s->bottommap
       : viewz > s->interpceilingheight ? s->topmap
                                        : s->midmap;
  }
  else
  {
    cm = 0;
  }

  if (cm < 0 || cm > numcolormaps)
  {
    cm = 0;
  }

  fullcolormap = colormaps[cm];
  fixedcolormapoffset = player->fixedcolormap * PLAYPAL_SIZE;

  if (fixedcolormapoffset)
  {
    // killough 3/20/98: use fullcolormap
    fixedcolormap = fullcolormap + fixedcolormapoffset;
  }
  else
  {
    fixedcolormap = NULL;
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
// Port of the Rum and Raisin Doom multithreaded renderer design:
//
//  - The view is split into vertical column slabs, one per render context.
//  - Context 0 renders on the calling thread, contexts 1..N-1 are dispatched
//    to the persistent worker pool in i_thread.c.
//  - All renderer state mutated during a frame is THREADLOCAL.
//  - Columns outside a context's slab are marked solid in R_ClearClipSegs(),
//    which makes the BSP traversal cull everything that cannot affect the
//    slab. Sprites/voxels/psprites are clipped to the slab in r_things.c and
//    r_voxel.c; border sprites are projected once per context and each
//    context draws its own slice.
//

static int num_render_contexts = 0;      // 0 = auto: one per logical CPU core
static boolean render_loadbalancing = true;

THREADLOCAL rendercontext_t *r_context = NULL;

// [MT] scratch allocator index for this thread: 0 = main thread,
// 1..numjobthreads = worker threads. Workers set this in R_RenderContextJob.
THREADLOCAL int r_worker_scratch_index = 0;

static rendercontext_t rendercontexts[MAX_RENDER_CONTEXTS];

#define MIN_SLAB_WIDTH 32

// number of contexts usable this frame (pool size is fixed at init)
static int ActiveContexts(void)
{
    // Disable when resolution is low
    if (video.height < SCREENHEIGHT * 2)
    {
        return 1;
    }

    const int contexts = CLAMP(num_render_contexts, 1, R_JobsNumThreads() + 1);

    if (viewwidth > 0)
    {
        return MIN(contexts, MAX(1, viewwidth / MIN_SLAB_WIDTH));
    }

    return contexts;
}

boolean R_IsMultiThreaded(void)
{
    return ActiveContexts() > 1;
}

//
// R_RenderLoadBalance
//
// Each context's last frame time becomes a share of the view width; contexts
// that took longer than their fair share shrink, faster ones grow.
//
// The plan is applied at most once per LOADBALANCE_PERIOD frames, and only
// when the imbalance is worth the move: shifting a boundary changes which
// segments, planes and sprites a context touches, so a split that is good
// enough is better left alone.
//

#define LOADBALANCE_PERIOD  1     // frames between two plans
#define LOADBALANCE_BUDGET  0.25  // share of the ideal slab shifted per plan
#define LOADBALANCE_TRIGGER 0.10  // imbalance below this is ignored

static int loadbalance_frames;

static void R_EvenContextSplit(int contexts)
{
    int start = 0;

    for (int i = 0; i < contexts; i++)
    {
        const int end = ((i + 1) * viewwidth) / contexts;

        rendercontexts[i].startcol = start;
        rendercontexts[i].endcol = (i == contexts - 1) ? viewwidth : end;
        start = rendercontexts[i].endcol;
    }
}

static void R_RenderLoadBalance(int contexts)
{
    const double idealpercentage = 1.0 / contexts;
    double total_smoothed_time = 0;
    double timepercentages[MAX_RENDER_CONTEXTS];
    double loadpercentages[MAX_RENDER_CONTEXTS];
    double widthpercentages[MAX_RENDER_CONTEXTS];
    int overbudgetcount = 0;
    int underbudgetcount = 0;

    // Update exponentially smoothed time: 90% old + 10% new
    // This prevents slab boundaries from jumping every frame.
    for (int i = 0; i < contexts; i++)
    {
        rendercontexts[i].smoothed_time =
            0.9 * rendercontexts[i].smoothed_time
            + 0.1 * (double)rendercontexts[i].timetaken;
        total_smoothed_time += rendercontexts[i].smoothed_time;
    }

    if (total_smoothed_time <= 0)
    {
        R_EvenContextSplit(contexts);
        return;
    }

    // The percentages below are relative to the widths we already have, so
    // the first frame and a resize have to start from an even split.

    int covered = 0;
    for (int i = 0; i < contexts; i++)
    {
        covered += rendercontexts[i].endcol - rendercontexts[i].startcol;
    }

    if (covered != viewwidth)
    {
        R_EvenContextSplit(contexts);
        return;
    }

    // Keep the current split in between plans.

    if (++loadbalance_frames < LOADBALANCE_PERIOD)
    {
        return;
    }

    loadbalance_frames = 0;

    for (int i = 0; i < contexts; i++)
    {
        timepercentages[i] =
            rendercontexts[i].smoothed_time / total_smoothed_time;

        loadpercentages[i] = timepercentages[i];

        if (loadpercentages[i] > idealpercentage)
        {
            overbudgetcount++;
        }
        else
        {
            underbudgetcount++;
        }
    }

    // Give up on this plan if nothing is off by more than the trigger.

    double worstpercentage = 0;
    for (int i = 0; i < contexts; i++)
    {
        worstpercentage = MAX(worstpercentage, loadpercentages[i]);
    }

    if (worstpercentage <= idealpercentage * (1.0 + LOADBALANCE_TRIGGER))
    {
        return;
    }

    // shift up to LOADBALANCE_BUDGET of the ideal share from slow to fast
    const double totalshuffle = idealpercentage * LOADBALANCE_BUDGET;
    double removeamount = totalshuffle / 2;
    double addamount = totalshuffle - removeamount;

    if (overbudgetcount)
    {
        removeamount /= overbudgetcount;
    }
    if (underbudgetcount)
    {
        addamount /= underbudgetcount;
    }

    for (int i = 0; i < contexts; i++)
    {
        const double oldwidthpercentage =
            (double)(rendercontexts[i].endcol - rendercontexts[i].startcol)
            / viewwidth;

        const double growamount =
            loadpercentages[i] > idealpercentage ? -removeamount : addamount;

        // Hysteresis: only adjust if imbalance exceeds the trigger. This
        // prevents slab boundaries from jittering due to timing noise.
        const double imbalance = fabs(loadpercentages[i] - idealpercentage);
        if (imbalance > idealpercentage * LOADBALANCE_TRIGGER)
        {
            widthpercentages[i] = oldwidthpercentage + growamount;
        }
        else
        {
            widthpercentages[i] = oldwidthpercentage;
        }
    }

    double totalpercentage = 0.0;
    for (int i = 0; i < contexts; i++)
    {
        totalpercentage += widthpercentages[i];
    }

    if (totalpercentage <= 0.0)
    {
        R_EvenContextSplit(contexts);
        return;
    }

    for (int i = 0; i < contexts; i++)
    {
        widthpercentages[i] /= totalpercentage;
    }

    int start = 0;
    double cumulative = 0.0;

    for (int i = 0; i < contexts - 1; i++)
    {
        cumulative += widthpercentages[i];

        int end = (int)lround(viewwidth * cumulative);

        // At least one column for this context, and one left for every
        // context that follows.

        end = CLAMP(end, start + 1, viewwidth - (contexts - 1 - i));

        rendercontexts[i].startcol = start;
        rendercontexts[i].endcol = end;
        start = end;
    }

    rendercontexts[contexts - 1].startcol = start;
    rendercontexts[contexts - 1].endcol = viewwidth;
}

//
// R_RenderViewContext
//
// Renders one column slab end to end. This is the worker body: what used to
// be the single-threaded R_RenderPlayerView() loop, with every piece of
// state either thread-local or clipped to the context's column range.
//

void R_RenderViewContext(rendercontext_t *context)
{
    PROF_ZONE_BEGIN(ctx);

    const uint64_t starttime = I_GetTimeUS();

    r_context = context;

    // per-frame stats (rendered_vissprites/rendered_voxels are set by the
    // Clear functions below, matching the original R_ClearStats semantics)
    context->rendered_visplanes = 0;
    context->rendered_segs = 0;

    // Thread-local storage starts zero-initialized; restore the base drawer
    // invariant that R_Init() establishes for the main thread.
    colfunc = R_DrawColumn;

    // [crispy] draw fuzz effect independent of rendering frame rate. With
    // multiple contexts every thread restarts from the shared tic seed and
    // accumulates locally; single-threaded mode keeps vanilla progression.
    R_SetFuzzPosDraw();

    // Clear buffers.
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();
    VX_ClearVoxels();

    // The head node is the last node output.
    R_RenderBSPNode(numnodes - 1);

    R_NearbySprites();

    R_ClearTrackedRendered();
    R_ClearVisitedSectors();

    // [FG] update automap while playing: planes and sprites are skipped.
    if (automap_on)
    {
        context->timetaken = I_GetTimeUS() - starttime;
        return;
    }

    R_DrawPlanes();

    R_DrawMasked();

    context->timetaken = I_GetTimeUS() - starttime;

    PROF_ZONE_END(ctx);
}

// Job wrapper: userdata is the owning render context.
static void R_RenderContextJob(void *userdata)
{
    rendercontext_t *context = (rendercontext_t *)userdata;
    // If the job system ever runs this inline on the calling thread, the
    // scratch index must be restored afterwards.
    const int saved_scratch_index = r_worker_scratch_index;

    r_worker_scratch_index = context->index;
    R_FrameScratchReset();
    R_RenderViewContext(context);

    r_worker_scratch_index = saved_scratch_index;
}

static void R_AggregateStats(int contexts)
{
    for (int i = 0; i < contexts; i++)
    {
        rendered_visplanes += rendercontexts[i].rendered_visplanes;
        rendered_segs += rendercontexts[i].rendered_segs;
        rendered_vissprites += rendercontexts[i].rendered_vissprites;
        rendered_voxels += rendercontexts[i].rendered_voxels;
    }
}

// [MT] Size the per-context render stamps before the contexts are dispatched:
// the sector array is exactly numsectors (reallocated and cleared whenever the
// level changes), the mobj array is grow-only because the thinkers arena grows
// with every spawn. Called from the main thread.

static void R_EnsureContextStamps(int contexts)
{
    const int sector_size = numsectors;
    const int mobj_size = R_MobjStampSize();

    for (int i = 0; i < contexts; i++)
    {
        rendercontext_t *context = &rendercontexts[i];

        if (context->sector_stamp_size != sector_size)
        {
            context->sector_stamp =
                I_Realloc(context->sector_stamp, sector_size);
            memset(context->sector_stamp, 0, sector_size);
            context->sector_stamp_size = sector_size;
        }

        if (context->mobj_stamp_size < mobj_size)
        {
            const int grow = MAX(mobj_size, context->mobj_stamp_size * 2);

            context->mobj_stamp = I_Realloc(context->mobj_stamp, grow);
            memset(context->mobj_stamp + context->mobj_stamp_size, 0,
                   grow - context->mobj_stamp_size);
            context->mobj_stamp_size = grow;
        }
    }
}

//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{
  R_FrameScratchReset();

  R_ClearStats();

  R_SetupFrame (player);

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
  // [MT] kept before the parallel render starts; the NetUpdate() calls that
  // used to sit between the render phases are gone.
  NetUpdate ();

  const int contexts = ActiveContexts();

  R_EnsureContextStamps(contexts);

  if (contexts > 1)
  {
    if (render_loadbalancing)
      R_RenderLoadBalance(contexts);
    else
      R_EvenContextSplit(contexts);

    // Contexts 1..N-1 go to the worker threads, context 0 renders inline on
    // the calling thread, then everything is flushed.
    for (int i = 1; i < contexts; i++)
      R_JobsAdd(R_RenderContextJob, &rendercontexts[i]);

    R_RenderViewContext(&rendercontexts[0]);
    R_JobsFlush();
  }
  else
  {
    rendercontexts[0].startcol = 0;
    rendercontexts[0].endcol = viewwidth;
    R_RenderViewContext(&rendercontexts[0]);
  }

  R_AggregateStats(contexts);

  // Check for new console commands.
  NetUpdate ();
}

//
// Render thread pool lifecycle
//

void R_InitRenderThreads(void)
{
    if (num_render_contexts <= 0 || num_render_contexts > MAX_RENDER_CONTEXTS)
    {
        num_render_contexts =
            CLAMP(I_ThreadGetHardwareCount(), 1, MAX_RENDER_CONTEXTS);
    }

    for (int i = 0; i < MAX_RENDER_CONTEXTS; i++)
    {
        rendercontexts[i].index = i;
        rendercontexts[i].timetaken = 1;
        rendercontexts[i].smoothed_time = 1.0;
    }

    // Context 0 always renders on the calling thread, so the pool only needs
    // num_render_contexts - 1 workers.
    R_JobsInit(num_render_contexts - 1);

    // Frame scratch allocator: per-thread bump arena for small fixed-size
    // per-frame allocations.
    // The only consumer is clipbot for now, 2 * viewwidth * sizeof(int) bytes

    R_FrameScratchInit(MAX(64 * 1024, 2 * video.width * (int)sizeof(int)));
    I_AtExit(R_ShutdownRenderThreads, true);
}

void R_ShutdownRenderThreads(void)
{
    // Join the workers before the per-thread scratch goes away: a worker
    // still running a job would allocate from a freed arena.

    R_JobsShutdown();
    R_FrameScratchShutdown();

    for (int i = 0; i < MAX_RENDER_CONTEXTS; i++)
    {
        free(rendercontexts[i].sector_stamp);
        free(rendercontexts[i].mobj_stamp);
        rendercontexts[i].sector_stamp = NULL;
        rendercontexts[i].mobj_stamp = NULL;
        rendercontexts[i].sector_stamp_size = 0;
        rendercontexts[i].mobj_stamp_size = 0;
    }
}

void R_InitAnyRes(void)
{
    R_InitSpritesRes();
    R_InitBufferRes();
    R_InitPlanesRes();
}

void R_BindRenderVariables(void)
{
  BIND_NUM(extra_level_brightness, 0, 0, 4, "Level brightness");
  BIND_NUM(fuzzmode, FUZZ_BLOCKY, FUZZ_BLOCKY, FUZZ_ORIGINAL,
    "Partial Invisibility (0 = Blocky; 1 = Refraction; 2 = Shadow, 3 = Original)");
  BIND_BOOL(stretchsky, false, "Stretch short skies");
  BIND_NUM(sky_projection, SKYPROJ_VANILLA, SKYPROJ_VANILLA, NUM_SKYPROJS-1,
    "Sky projection (0 = Vanilla; 1 = Linear; 2 = Cylindrical)");
  BIND_BOOL(r_swirl, false, "Swirling animated flats");
  M_BindBool("voxels_rendering", &default_voxels_rendering, &voxels_rendering,
             true, wad_no, "Allow voxel models");
  BIND_NUM(invul_mode, INVUL_MBF, INVUL_VANILLA, INVUL_GRAY,
    "Invulnerability effect (0 = Vanilla; 1 = MBF; 2 = Gray)");
  BIND_BOOL(flashing_hom, true, "Enable flashing of the HOM indicator");
  BIND_NUM(screenblocks, 10, 3, UL, "Size of game-world screen");
  BIND_NUM(default_max_pitch_angle, 32, 30, 60, "Maximum view pitch angle");

  M_BindBool("translucency", &translucency, NULL, true, wad_yes,
             "Translucency for some things");
  BIND_BOOL(flipcorpses, false, "Randomly mirrored death animations");

  // [MT] multithreaded rendering
  M_BindNum("render_contexts", &num_render_contexts, NULL, 0, 0,
            MAX_RENDER_CONTEXTS, wad_no,
            "Parallel render contexts (0 = one per CPU core, 1 = single-threaded)");
  BIND_BOOL(render_loadbalancing, true,
            "Load-balance render contexts by previous frame times");
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
