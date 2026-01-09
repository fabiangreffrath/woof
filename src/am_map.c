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
//   the automap code
//
//-----------------------------------------------------------------------------

#include <limits.h>
#include <string.h>

#include "am_map.h"
#include "d_event.h"
#include "d_player.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_video.h"
#include "m_config.h"
#include "m_input.h"
#include "mn_menu.h"
#include "m_misc.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "tables.h"
#include "v_flextran.h"
#include "v_patch.h"
#include "v_video.h"
#include "ws_stuff.h"
#include "z_zone.h"

//jff 1/7/98 default automap colors added
#define MAPCOLOR(x) mapcolor_##x, cur_mapcolor_##x
static int MAPCOLOR(back);    // map background
static int MAPCOLOR(grid);    // grid lines color
static int MAPCOLOR(wall);    // normal 1s wall color
static int MAPCOLOR(fchg);    // line at floor height change color
static int MAPCOLOR(cchg);    // line at ceiling height change color
static int MAPCOLOR(clsd);    // line at sector with floor=ceiling color
static int MAPCOLOR(rkey);    // red key color
static int MAPCOLOR(bkey);    // blue key color
static int MAPCOLOR(ykey);    // yellow key color
static int MAPCOLOR(rdor);    // red door color  (diff from keys to allow option)
static int MAPCOLOR(bdor);    // blue door color (of enabling one but not other )
static int MAPCOLOR(ydor);    // yellow door color
static int MAPCOLOR(tele);    // teleporter line color
static int MAPCOLOR(secr);    // secret sector boundary color
static int MAPCOLOR(revsecr); // revealed secret sector boundary color
static int MAPCOLOR(exit);    // jff 4/23/98 add exit line color
static int MAPCOLOR(unsn);    // computer map unseen line color
static int MAPCOLOR(flat);    // line with no floor/ceiling changes
static int MAPCOLOR(sprt);    // general sprite color
static int MAPCOLOR(hair);    // crosshair color
static int MAPCOLOR(sngl);    // single player arrow color
static int MAPCOLOR(plyr[4]); // colors for player arrows in multiplayer
static int MAPCOLOR(frnd);    // colors for friends of player
static int MAPCOLOR(item);    // item sprite color
static int MAPCOLOR(enemy);   // enemy sprite color
#undef MAPCOLOR

//jff 3/9/98 add option to not show secret sectors until entered
static boolean map_secret_after;

enum {
  MAP_KEYED_DOOR_OFF,
  MAP_KEYED_DOOR_COLOR,
  MAP_KEYED_DOOR_FLASH
};

static int map_keyed_door; // keyed doors are colored or flashing

static boolean map_smooth_lines;

// [Woof!] FRACTOMAPBITS: overflow-safe coordinate system.
// Written by Andrey Budko (entryway), adapted from prboom-plus/src/am_map.*
#define MAPBITS 12
#define MAPUNIT (1<<MAPBITS)
#define FRACTOMAPBITS (FRACBITS-MAPBITS)

// [Woof!] New radius to use with FRACTOMAPBITS, since orginal 
// PLAYERRADIUS macro can't be used in this implementation.
#define MAPPLAYERRADIUS (16*(1<<MAPBITS))

// scale on entry
#define INITSCALEMTOF (int)(.2*FRACUNIT)
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC  4
// [crispy] pan faster by holding run button
#define F2_PANINC 12
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN        ((int) (1.02*FRACUNIT))
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT       ((int) (FRACUNIT/1.02))

// [crispy] zoom faster with the mouse wheel
#define M2_ZOOMIN       ((int) (1.08*FRACUNIT))
#define M2_ZOOMOUT      ((int) (FRACUNIT/1.08))
#define M2_ZOOMINFAST   ((int) (1.5*FRACUNIT))
#define M2_ZOOMOUTFAST  ((int) (FRACUNIT/1.5))

// [crispy] toggleable pan/zoom speed
static int f_paninc = F_PANINC;
static int m_zoomin_kbd = M_ZOOMIN;
static int m_zoomout_kbd = M_ZOOMOUT;
static int m_zoomin_mouse = M2_ZOOMIN;
static int m_zoomout_mouse = M2_ZOOMOUT;
static boolean mousewheelzoom;

// translates between frame-buffer and map distances
// [FG] fix int overflow that causes map and grid lines to disappear
#define FTOM(x) ((((int64_t)(x)<<16)*scale_ftom)>>16)
#define MTOF(x) ((((int64_t)(x)*scale_mtof)>>16)>>16)
// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + MTOF((x)-m_x))
#define CYMTOF(y)  (f_y + (f_h - MTOF((y)-m_y)))

typedef struct
{
    int x, y;
} fpoint_t;

typedef struct
{
    fpoint_t a, b;
} fline_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
#define R ((8*MAPPLAYERRADIUS)/7)
static mline_t player_arrow[] =
{
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/4 } },  // ----->
  { { R, 0 }, { R-R/2, -R/4 } },
  { { -R+R/8, 0 }, { -R-R/8, R/4 } }, // >---->
  { { -R+R/8, 0 }, { -R-R/8, -R/4 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/4 } }, // >>--->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};
#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R ((8*MAPPLAYERRADIUS)/7)
static mline_t cheat_player_arrow[] =
{ // killough 3/22/98: He's alive, Jim :)
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/4 } },  // ----->
  { { R, 0 }, { R-R/2, -R/4 } },
  { { -R+R/8, 0 }, { -R-R/8, R/4 } }, // >---->
  { { -R+R/8, 0 }, { -R-R/8, -R/4 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/4 } }, // >>--->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } },
  { { -R/10-R/6, R/4}, {-R/10-R/6, -R/4} },  // J
  { { -R/10-R/6, -R/4}, {-R/10-R/6-R/8, -R/4} },
  { { -R/10-R/6-R/8, -R/4}, {-R/10-R/6-R/8, -R/8} },
  { { -R/10, R/4}, {-R/10, -R/4}},           // F
  { { -R/10, R/4}, {-R/10+R/8, R/4}},
  { { -R/10+R/4, R/4}, {-R/10+R/4, -R/4}},   // F
  { { -R/10+R/4, R/4}, {-R/10+R/4+R/8, R/4}},
};
#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))

//jff 1/5/98 new symbol for keys on automap
#define R (FRACUNIT)
static mline_t cross_mark[] =
{
  { { -R, 0 }, { R, 0} },
  { { 0, -R }, { 0, R } },
};
#undef R
#define NUMCROSSMARKLINES (sizeof(cross_mark)/sizeof(mline_t))
//jff 1/5/98 end of new symbol

#define R (FRACUNIT)
#define np5R (int)(-.5*R)
#define np7R (int)(-.7*R)
#define p7R  (int)(.7*R)

static mline_t thintriangle_guy[] =
{
  { { np5R, np7R }, { R,       0 } },
  { { R,       0 }, { np5R,  p7R } },
  { { np5R,  p7R }, { np5R, np7R } }
};
#undef R
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))

int ddt_cheating = 0;         // killough 2/7/98: make global, rename to ddt_*

boolean automap_grid = false;

boolean automapactive = false;
static boolean automapfirststart = true;

overlay_t automapoverlay = AM_OVERLAY_OFF;

// location of window on screen
static int  f_x;
static int  f_y;

// size of window on screen
static int  f_w;
static int  f_h;

static mpoint_t m_paninc;    // how far the window pans each tic (map coords)
static fixed_t mtof_zoommul; // how far the window zooms each tic (map coords)
static fixed_t ftom_zoommul; // how far the window zooms each tic (fb coords)

static int64_t m_x, m_y;     // LL x,y window location on the map (map coords)
static int64_t m_x2, m_y2;   // UR x,y window location on the map (map coords)
static int64_t prev_m_x, prev_m_y;

//
// width/height of window on map (map coords)
//
static int64_t  m_w;
static int64_t  m_h;

// based on level size
static fixed_t  min_x;
static fixed_t  min_y; 
static fixed_t  max_x;
static fixed_t  max_y;

static fixed_t  max_w;          // max_x-min_x,
static fixed_t  max_h;          // max_y-min_y

// based on player size
static const fixed_t min_h = 2 * MAPPLAYERRADIUS; // const? never changed?

static fixed_t  min_scale_mtof; // used to tell when to stop zooming out
static fixed_t  max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static int64_t old_m_w, old_m_h;
static int64_t old_m_x, old_m_y;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr;           // the player represented by an arrow

static patch_t *marknums[10];   // numbers used for marking by the automap

// killough 2/22/98: Remove limit on automap marks,
// and make variables external for use in savegames.

mpoint_t *markpoints = NULL;    // where the points are
int markpointnum = 0; // next point to be assigned (also number of points now)
int markpointnum_max = 0;       // killough 2/22/98
boolean followplayer = true; // specifies whether to follow the player around

static boolean stopped = true;

// Forward declare for AM_LevelInit
static void AM_drawFline_Vanilla(fline_t* fl, int color);
static void AM_drawFline_Smooth(fline_t* fl, int color);
void (*AM_drawFline)(fline_t*, int) = AM_drawFline_Vanilla;

// [crispy/Woof!] automap rotate mode and square aspect ratio need these early on
static boolean automaprotate = false;
static boolean automapsquareaspect = false;
#define ADJUST_ASPECT_RATIO (correct_aspect_ratio && automapsquareaspect)
static void AM_rotate(int64_t *x, int64_t *y, angle_t a);
static void AM_transformPoint(mpoint_t *pt);
static mpoint_t mapcenter;
static angle_t mapangle;

enum
{
  PAN_UP,
  PAN_DOWN,
  PAN_LEFT,
  PAN_RIGHT,
  ZOOM_IN,
  ZOOM_OUT,
  STATE_NUM
};

static int buttons_state[STATE_NUM] = { 0 };

//
// AM_activateNewScale()
//
// Changes the map scale after zooming or translating
//
// Passed nothing, returns nothing
//
static void AM_activateNewScale(void)
{
  m_x += m_w/2;
  m_y += m_h/2;
  m_w = FTOM(f_w);
  m_h = FTOM(f_h);
  m_x -= m_w/2;
  m_y -= m_h/2;
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

//
// AM_saveScaleAndLoc()
//
// Saves the current center and zoom
// Affects the variables that remember old scale and loc
//
// Passed nothing, returns nothing
//
static void AM_saveScaleAndLoc(void)
{
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}

//
// AM_restoreScaleAndLoc()
//
// restores the center and zoom from locally saved values
// Affects global variables for location and scale
//
// Passed nothing, returns nothing
//
static void AM_restoreScaleAndLoc(void)
{
  m_w = old_m_w;
  m_h = old_m_h;
  if (!followplayer)
  {
    m_x = old_m_x;
    m_y = old_m_y;
  }
  else
  {
    m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
    m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;
  }
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;

  // Change the scaling multipliers
  scale_mtof = FixedDiv(f_w<<FRACBITS, m_w);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
// AM_addMark()
//
// Adds a marker at the current location
// Affects global variables for marked points
//
// Passed nothing, returns nothing
//
static void AM_addMark(void)
{
  // killough 2/22/98:
  // remove limit on automap marks

  if (markpointnum >= markpointnum_max)
    markpoints = Z_Realloc(markpoints,
                           (markpointnum_max = markpointnum_max ?
                            markpointnum_max*2 : 16) * sizeof(*markpoints),
                           PU_STATIC, 0);

  markpoints[markpointnum].x = m_x + m_w/2;
  markpoints[markpointnum].y = m_y + m_h/2;
  markpointnum++;
}

//
// AM_findMinMaxBoundaries()
//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
// Passed nothing, returns nothing
//

static void AM_setMinMaxScale(void)
{
  const fixed_t a = FixedDiv(f_w << FRACBITS, max_w);
  const fixed_t b = FixedDiv(f_h << FRACBITS, max_h);

  min_scale_mtof = (a < b) ? a : b;
  max_scale_mtof = FixedDiv(f_h << FRACBITS, min_h);

  if (scale_mtof > max_scale_mtof)
    scale_mtof = max_scale_mtof;

  if (scale_mtof < min_scale_mtof)
    scale_mtof = min_scale_mtof;
}

static void AM_findMinMaxBoundaries(void)
{
  int i;

  min_x = min_y =  INT_MAX;
  max_x = max_y = -INT_MAX;

  for (i=0;i<numvertexes;i++)
  {
    if (vertexes[i].x < min_x)
      min_x = vertexes[i].x;
    else if (vertexes[i].x > max_x)
      max_x = vertexes[i].x;

    if (vertexes[i].y < min_y)
      min_y = vertexes[i].y;
    else if (vertexes[i].y > max_y)
      max_y = vertexes[i].y;
  }

  // [FG] cope with huge level dimensions which span the entire INT range
  max_w = (max_x >>= FRACTOMAPBITS) - (min_x >>= FRACTOMAPBITS);
  max_h = (max_y >>= FRACTOMAPBITS) - (min_y >>= FRACTOMAPBITS);

  AM_setMinMaxScale();
}

void AM_SetMapCenter(fixed_t x, fixed_t y)
{
  m_x = (x >> FRACTOMAPBITS) - m_w / 2;
  m_y = (y >> FRACTOMAPBITS) - m_h / 2;
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

//
// AM_changeWindowLoc()
//
// Moves the map window by the global variables m_paninc.x, m_paninc.y
//
// Passed nothing, returns nothing
//
static void AM_changeWindowLoc(void)
{
  int64_t incx, incy;

  if (m_paninc.x || m_paninc.y)
  {
    followplayer = 0;
  }

  if (uncapped && leveltime > oldleveltime)
  {
    incx = FixedMul(m_paninc.x, fractionaltic);
    incy = FixedMul(m_paninc.y, fractionaltic);
  }
  else
  {
    incx = m_paninc.x;
    incy = m_paninc.y;
  }

  if (automaprotate)
  {
    AM_rotate(&incx, &incy, ANGLE_MAX - mapangle);
  }
  m_x = prev_m_x + incx;
  m_y = prev_m_y + incy;

  if (m_x + m_w/2 > max_x)
    m_x = max_x - m_w/2;
  else if (m_x + m_w/2 < min_x)
    m_x = min_x - m_w/2;

  if (m_y + m_h/2 > max_y)
    m_y = max_y - m_h/2;
  else if (m_y + m_h/2 < min_y)
    m_y = min_y - m_h/2;

  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}


//
// AM_initVariables()
//
// Initialize the variables for the automap
//
// Affects the automap global variables
// Status bar is notified that the automap has been entered
// Passed nothing, returns nothing
//
void AM_initVariables(void)
{
  static event_t st_notify = {.type = ev_keyup, .data1.i = AM_MSGENTERED};

  automapactive = true;

  m_paninc.x = m_paninc.y = 0;
  ftom_zoommul = FRACUNIT;
  mtof_zoommul = FRACUNIT;
  mousewheelzoom = false; // [crispy]

  m_w = FTOM(f_w);
  m_h = FTOM(f_h);

  plr = &players[displayplayer];
  // [Alaux] Don't always snap back to player when reopening the Automap
  if (followplayer || automapfirststart)
  {
    m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
    m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;
    automapfirststart = false;
  }
  AM_Ticker(); // initialize variables for interpolation
  AM_changeWindowLoc();

  // for saving & restoring
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;

  // inform the status bar of the change
  ST_Responder(&st_notify);
}

//
// AM_loadPics()
// 
// Load the patches for the mark numbers
//
// Sets the marknums[i] variables to the patches for each digit
// Passed nothing, returns nothing;
//
static void AM_loadPics(void)
{
  int i;
  char namebuf[9];

  for (i=0;i<10;i++)
  {
    M_snprintf(namebuf, sizeof(namebuf), "AMMNUM%d", i);
    marknums[i] = V_CachePatchName(namebuf, PU_STATIC);
  }
}

//
// AM_unloadPics()
//
// Makes the mark patches purgable
//
// Passed nothing, returns nothing
//
static void AM_unloadPics(void)
{
  int i;

  for (i=0;i<10;i++)
    Z_ChangeTag(marknums[i], PU_CACHE);
}

//
// AM_clearMarks()
//
// Sets the number of marks to 0, thereby clearing them from the display
//
// Affects the global variable markpointnum
// Passed nothing, returns nothing
//
void AM_clearMarks(void)
{
  markpointnum = 0;
}

// [Alaux] Clear just the last mark
static void AM_clearLastMark(void)
{
  if (markpointnum)
    markpointnum--;
}

static void AM_EnableSmoothLines(void)
{
  AM_drawFline = map_smooth_lines ? AM_drawFline_Smooth : AM_drawFline_Vanilla;
}

static void AM_initScreenSize(void)
{
  // killough 2/7/98: get rid of finit_ vars
  // to allow runtime setting of width/height
  //
  // killough 11/98: ... finally add hires support :)

  f_w = video.width;
  if (automapoverlay && scaledviewheight == SCREENHEIGHT)
    f_h = video.height;
  else
    f_h = V_ScaleY(SCREENHEIGHT - st_height);
}

void AM_ResetScreenSize(void)
{
  int old_h = f_h;

  AM_initScreenSize();

  if (f_h != old_h)
  {
    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w << FRACBITS, m_w);

    AM_setMinMaxScale();

    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  }

  AM_activateNewScale();
}

//
// AM_LevelInit()
//
// Initialize the automap at the start of a new level
// should be called at the start of every level
//
// Passed nothing, returns nothing
// Affects automap's global variables
//
static void AM_LevelInit(void)
{
  automapfirststart = true;

  f_x = f_y = 0;

  AM_initScreenSize();

  AM_EnableSmoothLines();

  AM_findMinMaxBoundaries();

  // [crispy] initialize zoomlevel on all maps so that a 4096 units
  // square map would just fit in (MAP01 is 3376x3648 units)
  {
    fixed_t a = FixedDiv(f_w, (max_w>>MAPBITS < 2048) ? 2*(max_w>>MAPBITS) : 4096);
    fixed_t b = FixedDiv(f_h, (max_h>>MAPBITS < 2048) ? 2*(max_h>>MAPBITS) : 4096);
    scale_mtof = FixedDiv((a < b ? a : b), (int) (0.7*MAPUNIT));
  }

  if (scale_mtof > max_scale_mtof)
    scale_mtof = max_scale_mtof;

  if (scale_mtof < min_scale_mtof)
    scale_mtof = min_scale_mtof;

  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
// AM_Stop()
//
// Cease automap operations, unload patches, notify status bar
//
// Passed nothing, returns nothing
//
void AM_Stop (void)
{
  static event_t st_notify = {.type = 0, .data1.i = ev_keyup, .data2.i = AM_MSGEXITED};

  memset(buttons_state, 0, sizeof(buttons_state));

  AM_unloadPics();
  automapactive = false;
  ST_Responder(&st_notify);
  stopped = true;
}

//
// AM_Start()
// 
// Start up automap operations, 
//  if a new level, or game start, (re)initialize level variables
//  init map variables
//  load mark patches
//
// Passed nothing, returns nothing
//
void AM_Start()
{
  static int lastlevel = -1, lastepisode = -1;

  if (!stopped)
    AM_Stop();
  stopped = false;
  if (lastlevel != gamemap || lastepisode != gameepisode)
  {
    AM_LevelInit();
    lastlevel = gamemap;
    lastepisode = gameepisode;
  }
  else
  {
    AM_ResetScreenSize();
  }
  AM_initVariables();
  AM_loadPics();
}

//
// AM_minOutWindowScale()
//
// Set the window scale to the maximum size
//
// Passed nothing, returns nothing
//
static void AM_minOutWindowScale()
{
  scale_mtof = min_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

//
// AM_maxOutWindowScale(void)
//
// Set the window scale to the minimum size
//
// Passed nothing, returns nothing
//
static void AM_maxOutWindowScale(void)
{
  scale_mtof = max_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

//
// AM_Responder()
//
// Handle events (user inputs) in automap mode
//
// Passed an input event, returns true if its handled
//
boolean AM_Responder
( event_t*  ev )
{
  int rc;
  static int bigstate=0;

  if (M_InputActivated(input_speed))
  {
    f_paninc = F2_PANINC;
    m_zoomin_kbd = M2_ZOOMIN;
    m_zoomout_kbd = M2_ZOOMOUT;
    m_zoomin_mouse = M2_ZOOMINFAST;
    m_zoomout_mouse = M2_ZOOMOUTFAST;
  }
  else if (M_InputDeactivated(input_speed))
  {
    f_paninc = F_PANINC;
    m_zoomin_kbd = M_ZOOMIN;
    m_zoomout_kbd = M_ZOOMOUT;
    m_zoomin_mouse = M2_ZOOMIN;
    m_zoomout_mouse = M2_ZOOMOUT;
  }

  rc = false;

  if (!automapactive)
  {
    if (M_InputActivated(input_map) && !WS_Override())
    {
      AM_Start ();
      viewactive = false;
      st_refresh_background = true;
      rc = true;
    }
  }
  else if (ev->type == ev_keydown ||
           ev->type == ev_mouseb_down ||
           ev->type == ev_joyb_down)
  {
    rc = true;
                                                                // phares
    if (M_InputActivated(input_map_right))                      //    |
      if (!followplayer && !WS_HoldOverride())                  //    V
        buttons_state[PAN_RIGHT] = 1;
      else
        rc = false;
    else if (M_InputActivated(input_map_left))
      if (!followplayer && !WS_HoldOverride())
        buttons_state[PAN_LEFT] = 1;
      else
        rc = false;
    else if (M_InputActivated(input_map_up))
      if (!followplayer && !WS_HoldOverride())
        buttons_state[PAN_UP] = 1;
      else
        rc = false;
    else if (M_InputActivated(input_map_down))
      if (!followplayer && !WS_HoldOverride())
        buttons_state[PAN_DOWN] = 1;
      else
        rc = false;
    else if (M_InputActivated(input_map_zoomout))
    {
      if (ev->type == ev_mouseb_down && M_IsMouseWheel(ev->data1.i))
      {
        mousewheelzoom = true;
        mtof_zoommul = m_zoomout_mouse;
        ftom_zoommul = m_zoomin_mouse;
      }
      else
        buttons_state[ZOOM_OUT] = 1;
    }
    else if (M_InputActivated(input_map_zoomin))
    {
      if (ev->type == ev_mouseb_down && M_IsMouseWheel(ev->data1.i))
      {
        mousewheelzoom = true;
        mtof_zoommul = m_zoomin_mouse;
        ftom_zoommul = m_zoomout_mouse;
      }
      else
        buttons_state[ZOOM_IN] = 1;
    }
    else if (M_InputActivated(input_map))
    {
      if (!WS_Override())
      {
        bigstate = 0;
        viewactive = true;
        st_refresh_background = true;
        AM_Stop ();
      }
      else
      {
        rc = false;
      }
    }
    else if (M_InputActivated(input_map_gobig))
    {
      bigstate = !bigstate;
      if (bigstate)
      {
        AM_saveScaleAndLoc();
        AM_minOutWindowScale();
      }
      else
        AM_restoreScaleAndLoc();
    }
    else if (M_InputActivated(input_map_follow))
    {
      followplayer = !followplayer;
      memset(buttons_state, 0, sizeof(buttons_state));
      togglemsg(DEH_String(followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF));
    }
    else if (M_InputActivated(input_map_grid))
    {
      automap_grid = !automap_grid;      // killough 2/28/98
      togglemsg(DEH_String(automap_grid ? AMSTR_GRIDON : AMSTR_GRIDOFF));
    }
    else if (M_InputActivated(input_map_mark))
    {
      displaymsg("%s %d", DEH_String(AMSTR_MARKEDSPOT), markpointnum);
      AM_addMark();
    }
    else if (M_InputActivated(input_map_clear))
    {
      // [Alaux] Clear just the last mark
      if (!markpointnum)
        displaymsg(DEH_String(AMSTR_MARKSCLEARED));
      else {
        AM_clearLastMark();
        displaymsg("Cleared spot %d", markpointnum);
      }
    }
    else
    if (M_InputActivated(input_map_overlay))
    {
      if (++automapoverlay > AM_OVERLAY_DARK)
        automapoverlay = AM_OVERLAY_OFF;

      switch (automapoverlay)
      {
        case 2:  togglemsg("Dark Overlay On");        break;
        case 1:  togglemsg(DEH_String(AMSTR_OVERLAYON));  break;
        default: togglemsg(DEH_String(AMSTR_OVERLAYOFF)); break;
      }

      AM_initScreenSize();
      AM_activateNewScale();
      st_refresh_background = true;
    }
    else if (M_InputActivated(input_map_rotate))
    {
      automaprotate = !automaprotate;
      togglemsg(DEH_String(automaprotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF));
    }
    else
    {
      rc = false;
    }
  }
  else if (ev->type == ev_keyup ||
           ev->type == ev_mouseb_up ||
           ev->type == ev_joyb_up)
  {
    rc = false;

    if (M_InputDeactivated(input_map_right))
    {
      if (!followplayer)
        buttons_state[PAN_RIGHT] = 0;
    }
    else if (M_InputDeactivated(input_map_left))
    {
      if (!followplayer)
        buttons_state[PAN_LEFT] = 0;
    }
    else if (M_InputDeactivated(input_map_up))
    {
      if (!followplayer)
        buttons_state[PAN_UP] = 0;
    }
    else if (M_InputDeactivated(input_map_down))
    {
      if (!followplayer)
        buttons_state[PAN_DOWN] = 0;
    }
    else if (M_InputDeactivated(input_map_zoomout))
    {
      buttons_state[ZOOM_OUT] = 0;
    }
    else if (M_InputDeactivated(input_map_zoomin))
    {
      buttons_state[ZOOM_IN] = 0;
    }
  }

  m_paninc.x = 0;
  m_paninc.y = 0;

  if (!followplayer)
  {
    int scaled_f_paninc = (f_paninc * video.xscale) >> FRACBITS;
    if (buttons_state[PAN_RIGHT])
      m_paninc.x += FTOM(scaled_f_paninc);
    if (buttons_state[PAN_LEFT])
      m_paninc.x += -FTOM(scaled_f_paninc);

    scaled_f_paninc = (f_paninc * video.yscale) >> FRACBITS;
    if (buttons_state[PAN_UP])
      m_paninc.y += FTOM(scaled_f_paninc);
    if (buttons_state[PAN_DOWN])
      m_paninc.y += -FTOM(scaled_f_paninc);
  }

  if (!mousewheelzoom)
  {
    mtof_zoommul = FRACUNIT;
    ftom_zoommul = FRACUNIT;

    if (buttons_state[ZOOM_OUT] && !buttons_state[ZOOM_IN])
    {
      mtof_zoommul = m_zoomout_kbd;
      ftom_zoommul = m_zoomin_kbd;
    }
    else if (buttons_state[ZOOM_IN] && !buttons_state[ZOOM_OUT])
    {
      mtof_zoommul = m_zoomin_kbd;
      ftom_zoommul = m_zoomout_kbd;
    }
  }

  return rc;
}

//
// AM_changeWindowScale()
//
// Automap zooming
//
// Passed nothing, returns nothing
//
static void AM_changeWindowScale(void)
{
  // Change the scaling multipliers
  scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

  // [crispy] reset after zooming with the mouse wheel
  if (mousewheelzoom)
  {
    mtof_zoommul = FRACUNIT;
    ftom_zoommul = FRACUNIT;
    mousewheelzoom = false;
  }

  if (scale_mtof < min_scale_mtof)
    AM_minOutWindowScale();
  else if (scale_mtof > max_scale_mtof)
    AM_maxOutWindowScale();
  else
    AM_activateNewScale();
}

//
// AM_doFollowPlayer()
//
// Turn on follow mode - the map scrolls opposite to player motion
//
// Passed nothing, returns nothing
//
static void AM_doFollowPlayer(void)
{
  m_x = (viewx >> FRACTOMAPBITS) - m_w/2;
  m_y = (viewy >> FRACTOMAPBITS) - m_h/2;
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

//
// killough 10/98: return coordinates, to allow use of a non-follow-mode
// pointer. Allows map inspection without moving player to the location.
//

static boolean map_point_coord;

void AM_Coordinates(const mobj_t *mo, fixed_t *x, fixed_t *y, fixed_t *z)
{
  *z = followplayer || !map_point_coord || !automapactive ? *x = mo->x, *y = mo->y, mo->z :
    R_PointInSubsector(*x = (m_x+m_w/2) << FRACTOMAPBITS, *y = (m_y+m_h/2) << FRACTOMAPBITS)->sector->floorheight;
}

//
// AM_Ticker()
//
// Updates on gametic - enter follow mode, zoom, or change map location
//
// Passed nothing, returns nothing
//
void AM_Ticker (void)
{
  if (!automapactive)
    return;

  // Change the zoom if necessary.
  if (ftom_zoommul != FRACUNIT)
  {
    AM_changeWindowScale();
  }

  prev_m_x = m_x;
  prev_m_y = m_y;
}


//
// Clear automap frame buffer.
//
static void AM_clearFB(int color)
{
  memset(I_VideoBuffer, color, f_h * f_w);
}

//
// AM_clipMline()
//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes. If the speed is needed,
// use a hash algorithm to handle the common cases.
//
// Passed the line's coordinates on map and in the frame buffer performs
// clipping on them in the lines frame coordinates.
// Returns true if any part of line was not clipped
//
static boolean AM_clipMline
( mline_t*  ml,
  fline_t*  fl )
{
  enum
  {
    LEFT    =1,
    RIGHT   =2,
    BOTTOM  =4,
    TOP     =8
  };

  register int outcode1 = 0;
  register int outcode2 = 0;
  register int outside;

  fpoint_t  tmp;
  int   dx;
  int   dy;

    
#define DOOUTCODE(oc, mx, my) \
  (oc) = 0; \
  if ((my) < 0) (oc) |= TOP; \
  else if ((my) >= f_h) (oc) |= BOTTOM; \
  if ((mx) < 0) (oc) |= LEFT; \
  else if ((mx) >= f_w) (oc) |= RIGHT;

    
  // do trivial rejects and outcodes
  if (ml->a.y > m_y2)
  outcode1 = TOP;
  else if (ml->a.y < m_y)
  outcode1 = BOTTOM;

  if (ml->b.y > m_y2)
  outcode2 = TOP;
  else if (ml->b.y < m_y)
  outcode2 = BOTTOM;

  if (outcode1 & outcode2)
  return false; // trivially outside

  if (ml->a.x < m_x)
  outcode1 |= LEFT;
  else if (ml->a.x > m_x2)
  outcode1 |= RIGHT;

  if (ml->b.x < m_x)
  outcode2 |= LEFT;
  else if (ml->b.x > m_x2)
  outcode2 |= RIGHT;

  if (outcode1 & outcode2)
  return false; // trivially outside

  // transform to frame-buffer coordinates.
  fl->a.x = CXMTOF(ml->a.x);
  fl->a.y = CYMTOF(ml->a.y);
  fl->b.x = CXMTOF(ml->b.x);
  fl->b.y = CYMTOF(ml->b.y);

  DOOUTCODE(outcode1, fl->a.x, fl->a.y);
  DOOUTCODE(outcode2, fl->b.x, fl->b.y);

  if (outcode1 & outcode2)
  return false;

  while (outcode1 | outcode2)
  {
    // may be partially inside box
    // find an outside point
    if (outcode1)
      outside = outcode1;
    else
      outside = outcode2;

    // clip to each side
    if (outside & TOP)
    {
      dy = fl->a.y - fl->b.y;
      dx = fl->b.x - fl->a.x;
      // [Woof!] 'int64_t' math to avoid overflows on long lines.
      tmp.x = fl->a.x + (fixed_t)(((int64_t)dx*(fl->a.y-f_y))/dy);
      tmp.y = 0;
    }
    else if (outside & BOTTOM)
    {
      dy = fl->a.y - fl->b.y;
      dx = fl->b.x - fl->a.x;
      tmp.x = fl->a.x + (fixed_t)(((int64_t)dx*(fl->a.y-(f_y+f_h)))/dy);
      tmp.y = f_h-1;
    }
    else if (outside & RIGHT)
    {
      dy = fl->b.y - fl->a.y;
      dx = fl->b.x - fl->a.x;
      tmp.y = fl->a.y + (fixed_t)(((int64_t)dy*(f_x+f_w-1 - fl->a.x))/dx);
      tmp.x = f_w-1;
    }
    else if (outside & LEFT)
    {
      dy = fl->b.y - fl->a.y;
      dx = fl->b.x - fl->a.x;
      tmp.y = fl->a.y + (fixed_t)(((int64_t)dy*(f_x-fl->a.x))/dx);
      tmp.x = 0;
    }

    if (outside == outcode1)
    {
      fl->a = tmp;
      DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    }
    else
    {
      fl->b = tmp;
      DOOUTCODE(outcode2, fl->b.x, fl->b.y);
    }

    if (outcode1 & outcode2)
      return false; // trivially outside
  }

  return true;
}
#undef DOOUTCODE

//
// AM_drawFline()
//
// Draw a line in the frame buffer.
// Classic Bresenham w/ whatever optimizations needed for speed
//
// Passed the frame coordinates of line, and the color to be drawn
// Returns nothing
//
static void AM_drawFline_Vanilla(fline_t* fl, int color)
{
  register int x;
  register int y;
  register int dx;
  register int dy;
  register int sx;
  register int sy;
  register int ax;
  register int ay;
  register int d;

#ifdef RANGECHECK         // killough 2/22/98    
  // For debugging only
  if
  (
       fl->a.x < 0 || fl->a.x >= f_w
    || fl->a.y < 0 || fl->a.y >= f_h
    || fl->b.x < 0 || fl->b.x >= f_w
    || fl->b.y < 0 || fl->b.y >= f_h
  )
  {
    return;
  }
#endif

#define PUTDOT(xx,yy,cc) I_VideoBuffer[(yy)*video.width+(xx)]=(cc)

  dx = fl->b.x - fl->a.x;
  ax = 2 * (dx<0 ? -dx : dx);
  sx = dx<0 ? -1 : 1;

  dy = fl->b.y - fl->a.y;
  ay = 2 * (dy<0 ? -dy : dy);
  sy = dy<0 ? -1 : 1;

  x = fl->a.x;
  y = fl->a.y;

  if (ax > ay)
  {
    d = ay - ax/2;
    while (1)
    {
      PUTDOT(x,y,color);
      if (x == fl->b.x) return;
      if (d>=0)
      {
        y += sy;
        d -= ax;
      }
      x += sx;
      d += ay;
    }
  }
  else
  {
    d = ax - ay/2;
    while (1)
    {
      PUTDOT(x, y, color);
      if (y == fl->b.y) return;
      if (d >= 0)
      {
        x += sx;
        d -= ay;
      }
      y += sy;
      d += ax;
    }
  }
}

//
// AM_putWuDot
//
// haleyjd 06/13/09: Pixel plotter for Wu line drawing.
//
static void AM_putWuDot(int x, int y, int color, int weight)
{
   pixel_t *dest = &I_VideoBuffer[y * video.width + x];
   unsigned int *fg2rgb = Col2RGB8[weight];
   unsigned int *bg2rgb = Col2RGB8[64 - weight];
   unsigned int fg, bg;

   fg = fg2rgb[color];
   bg = bg2rgb[*dest];
   fg = (fg + bg) | 0x1f07c1f;
   *dest = RGB32k[0][0][fg & (fg >> 15)];
}


// Given 65536, we need 2048; 65536 / 2048 == 32 == 2^5
// Why 2048? ANG90 == 0x40000000 which >> 19 == 0x800 == 2048.
// The trigonometric correction is based on an angle from 0 to 90.
#define wu_fineshift 5

// Given 64 levels in the Col2RGB8 table, 65536 / 64 == 1024 == 2^10
#define wu_fixedshift 10

//
// AM_drawFlineWu
//
// haleyjd 06/12/09: Wu line drawing for the automap, with trigonometric
// brightness correction by SoM. I call this the Wu-McGranahan line drawing
// algorithm.
//
static void AM_drawFline_Smooth(fline_t *fl, int color)
{
   int dx, dy, xdir = 1;
   int x, y;

   // swap end points if necessary
   if(fl->a.y > fl->b.y)
   {
      fpoint_t tmp = fl->a;

      fl->a = fl->b;
      fl->b = tmp;
   }

   // determine change in x, y and direction of travel
   dx = fl->b.x - fl->a.x;
   dy = fl->b.y - fl->a.y;

   if(dx < 0)
   {
      dx   = -dx;
      xdir = -xdir;
   }

   // detect special cases -- horizontal, vertical, and 45 degrees;
   // revert to Bresenham
   if(dx == 0 || dy == 0 || dx == dy)
   {
      AM_drawFline_Vanilla(fl, color);
      return;
   }

   // draw first pixel
   PUTDOT(fl->a.x, fl->a.y, color);

   x = fl->a.x;
   y = fl->a.y;

   if(dy > dx)
   {
      // line is y-axis major.
      uint16_t erroracc = 0,
         erroradj = (uint16_t)(((uint32_t)dx << 16) / (uint32_t)dy);

      while(--dy)
      {
         uint16_t erroracctmp = erroracc;

         erroracc += erroradj;

         // if error has overflown, advance x coordinate
         if(erroracc <= erroracctmp)
            x += xdir;

         y += 1; // advance y

         // the trick is in the trig!
         AM_putWuDot(x, y, color,
                     finecosine[erroracc >> wu_fineshift] >> wu_fixedshift);
         AM_putWuDot(x + xdir, y, color,
                     finesine[erroracc >> wu_fineshift] >> wu_fixedshift);
      }
   }
   else
   {
      // line is x-axis major.
      uint16_t erroracc = 0,
         erroradj = (uint16_t)(((uint32_t)dy << 16) / (uint32_t)dx);

      while(--dx)
      {
         uint16_t erroracctmp = erroracc;

         erroracc += erroradj;

         // if error has overflown, advance y coordinate
         if(erroracc <= erroracctmp)
            y += 1;

         x += xdir; // advance x

         // the trick is in the trig!
         AM_putWuDot(x, y, color,
                     finecosine[erroracc >> wu_fineshift] >> wu_fixedshift);
         AM_putWuDot(x, y + 1, color,
                     finesine[erroracc >> wu_fineshift] >> wu_fixedshift);
      }
   }

   // draw last pixel
   PUTDOT(fl->b.x, fl->b.y, color);
}

//
// AM_drawMline()
//
// Clip lines, draw visible parts of lines.
//
// Passed the map coordinates of the line, and the color to draw it
// Color -1 is special and prevents drawing. Color 247 is special and
// is translated to black, allowing Color 0 to represent feature disable
// in the defaults file.
// Returns nothing.
//
static void AM_drawMline
( mline_t*  ml,
  int   color )
{
  static fline_t fl;

  if (color==-1)  // jff 4/3/98 allow not drawing any sort of line
    return;       // by setting its color to -1
  if (color==247) // jff 4/3/98 if color is 247 (xparent), use black
    color=0;

  if (AM_clipMline(ml, &fl))
    AM_drawFline(&fl, color); // draws it on frame buffer using fb coords
}

//
// AM_drawGrid()
//
// Draws blockmap aligned grid lines.
//
// Passed the color to draw the grid lines
// Returns nothing
//
static void AM_drawGrid(int color)
{
  int64_t x, y;
  int64_t start, end;
  const fixed_t gridsize = MAPBLOCKUNITS << MAPBITS;
  mline_t ml;

  // Figure out start of vertical gridlines
  start = m_x;
  if (automaprotate)
  {
    start -= m_h / 2;
  }
  // [crispy] fix losing grid lines near the automap boundary
  if ((start-(bmaporgx>>FRACTOMAPBITS))%gridsize)
    start += // (MAPBLOCKUNITS<<FRACBITS)
      - ((start-(bmaporgx>>FRACTOMAPBITS))%gridsize);
  end = m_x + m_w;
  if (automaprotate)
  {
    end += m_h / 2;
  }

  // draw vertical gridlines
  for (x=start; x<end; x+=gridsize)
  {
    ml.a.x = x;
    ml.b.x = x;
    // [crispy] moved here
    ml.a.y = m_y;
    ml.b.y = m_y+m_h;
    if (automaprotate || ADJUST_ASPECT_RATIO)
    {
      ml.a.y -= m_w / 2;
      ml.b.y += m_w / 2;
    }
    AM_transformPoint(&ml.a);
    AM_transformPoint(&ml.b);
    AM_drawMline(&ml, color);
  }

  // Figure out start of horizontal gridlines
  start = m_y;
  if (automaprotate || ADJUST_ASPECT_RATIO)
  {
    start -= m_w / 2;
  }
  // [crispy] fix losing grid lines near the automap boundary
  if ((start-(bmaporgy>>FRACTOMAPBITS))%gridsize)
    start += // (MAPBLOCKUNITS<<FRACBITS)
      - ((start-(bmaporgy>>FRACTOMAPBITS))%gridsize);
  end = m_y + m_h;
  if (automaprotate || ADJUST_ASPECT_RATIO)
  {
    end += m_w / 2;
  }

  // draw horizontal gridlines
  for (y=start; y<end; y+=gridsize)
  {
    ml.a.y = y;
    ml.b.y = y;
    // [crispy] moved here
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    if (automaprotate)
    {
      ml.a.x -= m_h / 2;
      ml.b.x += m_h / 2;
    }
    AM_transformPoint(&ml.a);
    AM_transformPoint(&ml.b);
    AM_drawMline(&ml, color);
  }
}

//
// AM_DoorColor()
//
// Returns the 'color' or key needed for a door linedef type
//
// Passed the type of linedef, returns:
//   -1 if not a keyed door
//    0 if a red key required
//    1 if a blue key required
//    2 if a yellow key required
//    3 if a multiple keys required
//
// jff 4/3/98 add routine to get color of generalized keyed door
//
static int AM_DoorColor(int type)
{
  if (map_keyed_door == MAP_KEYED_DOOR_OFF)
  {
    return -1;
  }

  if (GenLockedBase <= type && type< GenDoorBase)
  {
    type -= GenLockedBase;
    type = (type & LockedKey) >> LockedKeyShift;
    if (!type || type==7)
      return 3;  //any or all keys
    else return (type-1)%3;
  }
  switch (type)  // closed keyed door
  {
    case 26: case 32: case 99: case 133:
      /*bluekey*/
      return 1;
    case 27: case 34: case 136: case 137:
      /*yellowkey*/
      return 2;
    case 28: case 33: case 134: case 135:
      /*redkey*/
      return 0;
    default:
      return -1; //not a keyed door
  }
  return -1;     //not a keyed door
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
// jff 1/5/98 many changes in this routine
// backward compatibility not needed, so just changes, no ifs
// addition of clauses for:
//    doors opening, keyed door id, secret sectors,
//    teleports, exit lines, key things
// ability to suppress any of added features or lines with no height changes
//
// support for gamma correction in automap abandoned
//
// jff 4/3/98 changed mapcolor_xxxx=0 as control to disable feature
// jff 4/3/98 changed mapcolor_xxxx=-1 to disable drawing line completely
//

#define M_ARRAY_INIT_CAPACITY 500
#include "m_array.h"

typedef struct
{
  mline_t l;
  int color;
} am_line_t;

static am_line_t *lines_1S = NULL;

static void AM_drawWalls(void)
{
  int i;
  static mline_t l;

  const boolean keyed_door_flash = (map_keyed_door == MAP_KEYED_DOOR_FLASH) && (leveltime & 16);

  // draw the unclipped visible portions of all lines
  for (i=0;i<numlines;i++)
  {
    l.a.x = lines[i].v1->x >> FRACTOMAPBITS;
    l.a.y = lines[i].v1->y >> FRACTOMAPBITS;
    l.b.x = lines[i].v2->x >> FRACTOMAPBITS;
    l.b.y = lines[i].v2->y >> FRACTOMAPBITS;
    AM_transformPoint(&l.a);
    AM_transformPoint(&l.b);
    // if line has been seen or IDDT has been used
    if (ddt_cheating || (lines[i].flags & ML_MAPPED))
    {
      if ((lines[i].flags & ML_DONTDRAW) && !ddt_cheating)
        continue;
      {
        /* cph - show keyed doors and lines */
        const int amd = AM_DoorColor(lines[i].special);
        if ((cur_mapcolor_bdor || cur_mapcolor_ydor || cur_mapcolor_rdor) &&
            !(lines[i].flags & ML_SECRET) &&    /* non-secret */
            (amd != -1)
        )
        {
            if (keyed_door_flash)
            {
               AM_drawMline(&l, cur_mapcolor_grid);
            }
            else switch (amd) // closed keyed door
            {
              case 1:
                /*bluekey*/
                AM_drawMline(&l,
                  cur_mapcolor_bdor? cur_mapcolor_bdor : cur_mapcolor_cchg);
                break;
              case 2:
                /*yellowkey*/
                AM_drawMline(&l,
                  cur_mapcolor_ydor? cur_mapcolor_ydor : cur_mapcolor_cchg);
                break;
              case 0:
                /*redkey*/
                AM_drawMline(&l,
                  cur_mapcolor_rdor? cur_mapcolor_rdor : cur_mapcolor_cchg);
                break;
              case 3:
                /*any or all*/
                AM_drawMline(&l,
                  cur_mapcolor_clsd? cur_mapcolor_clsd : cur_mapcolor_cchg);
                break;
            }
            continue;
        }
      }
      if //jff 4/23/98 add exit lines to automap
      (
        cur_mapcolor_exit &&
        (
          lines[i].special==11 ||
          lines[i].special==52 ||
          lines[i].special==197 ||
          lines[i].special==51  ||
          lines[i].special==124 ||
          lines[i].special==198
        )
      )
      {
        AM_drawMline(&l, keyed_door_flash ? cur_mapcolor_grid : cur_mapcolor_exit); // exit line
        continue;
      }

      if (!lines[i].backsector)
      {
        if (cur_mapcolor_exit && P_IsDeathExit(lines[i].frontsector))
        {
          array_push(lines_1S, ((am_line_t){l, keyed_door_flash ? cur_mapcolor_grid : cur_mapcolor_exit}));
        }
        // jff 1/10/98 add new color for 1S secret sector boundary
        else if (cur_mapcolor_secr && //jff 4/3/98 0 is disable
            (
             !map_secret_after &&
             P_IsSecret(lines[i].frontsector)
            )
          )
        {
          // line bounding secret sector
          array_push(lines_1S, ((am_line_t){l, cur_mapcolor_secr}));
        }
        else if (cur_mapcolor_revsecr &&
            (
             P_WasSecret(lines[i].frontsector) &&
             !P_IsSecret(lines[i].frontsector)
            )
          )
        {
          // line bounding revealed secret sector
          array_push(lines_1S, ((am_line_t){l, cur_mapcolor_revsecr}));
        }
        else                               //jff 2/16/98 fixed bug
        {
          // special was cleared
          array_push(lines_1S, ((am_line_t){l, cur_mapcolor_wall}));
        }
      }
      else
      {
        // jff 1/10/98 add color change for all teleporter types
        if
        (
            cur_mapcolor_tele && !(lines[i].flags & ML_SECRET) &&
            (lines[i].special == 39 || lines[i].special == 97 /* ||
            lines[i].special == 125 || lines[i].special == 126 */ )
        )
        { // teleporters
          AM_drawMline(&l, cur_mapcolor_tele);
        }
        else if (lines[i].flags & ML_SECRET)    // secret door
        {
          AM_drawMline(&l, cur_mapcolor_wall);      // wall color
        }
        else if
        (
            cur_mapcolor_clsd &&
            !(lines[i].flags & ML_SECRET) &&    // non-secret closed door
            ((lines[i].backsector->floorheight==lines[i].backsector->ceilingheight) ||
            (lines[i].frontsector->floorheight==lines[i].frontsector->ceilingheight))
        )
        {
          AM_drawMline(&l, cur_mapcolor_clsd);      // non-secret closed door
        } //jff 1/6/98 show secret sector 2S lines
        else if (cur_mapcolor_exit &&
            (P_IsDeathExit(lines[i].frontsector) ||
             P_IsDeathExit(lines[i].backsector))
        )
        {
          AM_drawMline(&l, keyed_door_flash ? cur_mapcolor_grid : cur_mapcolor_exit);
        }
        else if
        (
            cur_mapcolor_secr && //jff 2/16/98 fixed bug
            (                    // special was cleared after getting it
              !map_secret_after &&
               (
                P_IsSecret(lines[i].frontsector) ||
                P_IsSecret(lines[i].backsector)
               )
            )
        )
        {
          AM_drawMline(&l, cur_mapcolor_secr); // line bounding secret sector
        } //jff 1/6/98 end secret sector line change
        else if
        (
            cur_mapcolor_revsecr &&
            (
              (P_WasSecret(lines[i].frontsector)
               && !P_IsSecret(lines[i].frontsector)) ||
              (P_WasSecret(lines[i].backsector)
               && !P_IsSecret(lines[i].backsector))
            )
        )
        {
          AM_drawMline(&l, cur_mapcolor_revsecr); // line bounding revealed secret sector
        }
        else if (lines[i].backsector->floorheight !=
                  lines[i].frontsector->floorheight)
        {
          AM_drawMline(&l, cur_mapcolor_fchg); // floor level change
        }
        else if (lines[i].backsector->ceilingheight !=
                  lines[i].frontsector->ceilingheight)
        {
          AM_drawMline(&l, cur_mapcolor_cchg); // ceiling level change
        }
        else if (cur_mapcolor_flat && ddt_cheating)
        { 
          AM_drawMline(&l, cur_mapcolor_flat); //2S lines that appear only in IDDT
        }
      }
    } // now draw the lines only visible because the player has computermap
    else if (plr->powers[pw_allmap]) // computermap visible lines
    {
      if (!(lines[i].flags & ML_DONTDRAW)) // invisible flag lines do not show
      {
        if
        (
          cur_mapcolor_flat
          ||
          !lines[i].backsector
          ||
          lines[i].backsector->floorheight
          != lines[i].frontsector->floorheight
          ||
          lines[i].backsector->ceilingheight
          != lines[i].frontsector->ceilingheight
        )
          AM_drawMline(&l, cur_mapcolor_unsn);
      }
    }
  }

  for (int i = 0; i < array_size(lines_1S); ++i)
  {
    AM_drawMline(&lines_1S[i].l, lines_1S[i].color);
  }
  array_clear(lines_1S);
}

//
// AM_rotate()
//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
// Passed the coordinates of a point, and an angle
// Returns the coordinates rotated by the angle
//
static void AM_rotate
( int64_t*  x,
  int64_t*  y,
  angle_t a )
{
  int64_t tmpx;

  a >>= ANGLETOFINESHIFT;

  tmpx =
    FixedMul(*x,finecosine[a])
      - FixedMul(*y,finesine[a]);

  *y   =
    FixedMul(*x,finesine[a])
      + FixedMul(*y,finecosine[a]);

  *x = tmpx;
}

// [crispy] rotate point around map center
// adapted from prboom-plus/src/am_map.c:898-920
// [Woof!] Also, scale y coordinate of point for square aspect ratio
static void AM_transformPoint(mpoint_t *pt)
{
  if (automaprotate)
  {
    int64_t tmpx;
    // [crispy] smooth automap rotation
    angle_t smoothangle = followplayer ? ANG90 - viewangle : mapangle;

    pt->x -= mapcenter.x;
    pt->y -= mapcenter.y;

    smoothangle >>= ANGLETOFINESHIFT;

    tmpx = (int64_t)FixedMul(pt->x, finecosine[smoothangle])
        - (int64_t)FixedMul(pt->y, finesine[smoothangle])
        + mapcenter.x;

    pt->y = (int64_t)FixedMul(pt->x, finesine[smoothangle])
          + (int64_t)FixedMul(pt->y, finecosine[smoothangle])
          + mapcenter.y;

    pt->x = tmpx;
  }
  if (ADJUST_ASPECT_RATIO)
  {
    int64_t diff = pt->y - mapcenter.y;
    diff = 5 * diff / 6;
    pt->y = mapcenter.y + diff;
  }
}

//
// AM_drawLineCharacter()
//
// Draws a vector graphic according to numerous parameters
//
// Passed the structure defining the vector graphic shape, the number
// of vectors in it, the scale to draw it at, the angle to draw it at,
// the color to draw it with, and the map coordinates to draw it at.
// Returns nothing
//
static void AM_drawLineCharacter
( mline_t*  lineguy,
  int   lineguylines,
  fixed_t scale,
  angle_t angle,
  int   color,
  fixed_t x,
  fixed_t y )
{
  int   i;
  mline_t l;

  if (automaprotate)
  {
    angle += mapangle;
  }

  for (i=0;i<lineguylines;i++)
  {
    l.a.x = lineguy[i].a.x;
    l.a.y = lineguy[i].a.y;

    if (scale)
    {
      l.a.x = FixedMul(scale, l.a.x);
      l.a.y = FixedMul(scale, l.a.y);
    }

    if (angle)
      AM_rotate(&l.a.x, &l.a.y, angle);

    if (ADJUST_ASPECT_RATIO)
      l.a.y = 5 * l.a.y / 6;

    l.a.x += x;
    l.a.y += y;

    l.b.x = lineguy[i].b.x;
    l.b.y = lineguy[i].b.y;

    if (scale)
    {
      l.b.x = FixedMul(scale, l.b.x);
      l.b.y = FixedMul(scale, l.b.y);
    }

    if (angle)
      AM_rotate(&l.b.x, &l.b.y, angle);

    if (ADJUST_ASPECT_RATIO)
      l.b.y = 5 * l.b.y / 6;

    l.b.x += x;
    l.b.y += y;

    AM_drawMline(&l, color);
  }
}

//
// AM_drawPlayers()
//
// Draws the player arrow in single player, 
// or all the player arrows in a netgame.
//
// Passed nothing, returns nothing
//
static void AM_drawPlayers(void)
{
  int   i;
  player_t* p;
  //jff 1/6/98   static int   their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
  int   their_color = -1;
  int   color;
  mpoint_t pt;

  if (!netgame)
  {
    // [crispy] smooth player arrow rotation
    const angle_t smoothangle = automaprotate ? plr->mo->angle : viewangle;

    // interpolate player arrow
    if (uncapped && leveltime > oldleveltime)
    {
        pt.x = viewx >> FRACTOMAPBITS;
        pt.y = viewy >> FRACTOMAPBITS;
    }
    else
    {
        pt.x = plr->mo->x >> FRACTOMAPBITS;
        pt.y = plr->mo->y >> FRACTOMAPBITS;
    }
    AM_transformPoint(&pt);

    if (ddt_cheating)
      AM_drawLineCharacter
      (
        cheat_player_arrow,
        NUMCHEATPLYRLINES,
        0,
        smoothangle,
        cur_mapcolor_sngl,      //jff color
        pt.x,
        pt.y
      ); 
    else
      AM_drawLineCharacter
      (
        player_arrow,
        NUMPLYRLINES,
        0,
        smoothangle,
        cur_mapcolor_sngl,      //jff color
        pt.x,
        pt.y);        
    return;
  }

  for (i=0;i<MAXPLAYERS;i++)
  {
    angle_t smoothangle;

    their_color++;
    p = &players[i];

    // killough 9/29/98: use !demoplayback so internal demos are no different
    if ( (deathmatch && !demoplayback) && p != plr)
      continue;

    if (!playeringame[i])
      continue;

    if (p->powers[pw_invisibility])
      color = 246; // *close* to black
    else
      color = cur_mapcolor_plyr[their_color];   //jff 1/6/98 use default color

    // [crispy] interpolate other player arrows
    if (uncapped && leveltime > oldleveltime && p->mo->interp)
    {
        pt.x = LerpFixed(p->mo->oldx, p->mo->x) >> FRACTOMAPBITS;
        pt.y = LerpFixed(p->mo->oldy, p->mo->y) >> FRACTOMAPBITS;
    }
    else
    {
        pt.x = p->mo->x >> FRACTOMAPBITS;
        pt.y = p->mo->y >> FRACTOMAPBITS;
    }

    AM_transformPoint(&pt);
    if (automaprotate)
    {
      smoothangle = p->mo->angle;
    }
    else
    {
      smoothangle = LerpAngle(p->mo->oldangle, p->mo->angle);
    }

    AM_drawLineCharacter
    (
      player_arrow,
      NUMPLYRLINES,
      0,
      smoothangle,
      color,
      pt.x,
      pt.y
    );
  }
}

//
// AM_drawThings()
//
// Draws the things on the automap in double IDDT cheat mode
//
// Passed colors and colorrange, no longer used
// Returns nothing
//
static void AM_drawThings
( int colors,
  int  colorrange)
{
  int   i;
  mobj_t* t;
  mpoint_t pt;

  // for all sectors
  for (i=0;i<numsectors;i++)
  {
    t = sectors[i].thinglist;
    while (t) // for all things in that sector
    {
      // [crispy] do not draw an extra triangle for the player
      if (t == plr->mo)
      {
          t = t->snext;
          continue;
      }

      // [crispy] interpolate thing triangles movement
      if (uncapped && leveltime > oldleveltime && t->interp)
      {
        pt.x = LerpFixed(t->oldx, t->x) >> FRACTOMAPBITS;
        pt.y = LerpFixed(t->oldy, t->y) >> FRACTOMAPBITS;
      }
      else
      {
        pt.x = t->x >> FRACTOMAPBITS;
        pt.y = t->y >> FRACTOMAPBITS;
      }
      AM_transformPoint(&pt);

      //jff 1/5/98 case over doomednum of thing being drawn
      if (cur_mapcolor_rkey || cur_mapcolor_ykey || cur_mapcolor_bkey)
      {
        switch(t->info->doomednum)
        {
          //jff 1/5/98 treat keys special
          case 38: case 13: //jff  red key
            AM_drawLineCharacter
            (
              cross_mark,
              NUMCROSSMARKLINES,
              16<<MAPBITS,
              t->angle,
              cur_mapcolor_rkey!=-1? cur_mapcolor_rkey : cur_mapcolor_sprt,
              pt.x,
              pt.y
            );
            t = t->snext;
            continue;
          case 39: case 6: //jff yellow key
            AM_drawLineCharacter
            (
              cross_mark,
              NUMCROSSMARKLINES,
              16<<MAPBITS,
              t->angle,
              cur_mapcolor_ykey!=-1? cur_mapcolor_ykey : cur_mapcolor_sprt,
              pt.x,
              pt.y
            );
            t = t->snext;
            continue;
          case 40: case 5: //jff blue key
            AM_drawLineCharacter
            (
              cross_mark,
              NUMCROSSMARKLINES,
              16<<MAPBITS,
              t->angle,
              cur_mapcolor_bkey!=-1? cur_mapcolor_bkey : cur_mapcolor_sprt,
              pt.x,
              pt.y
            );
            t = t->snext;
            continue;
          default:
            break;
        }
      }
      //jff 1/5/98 end added code for keys

      //jff previously entire code
      AM_drawLineCharacter
      (
        thintriangle_guy,
        NUMTHINTRIANGLEGUYLINES,
        t->radius >> FRACTOMAPBITS, // [crispy] triangle size represents actual thing size
        t->angle,
        // killough 8/8/98: mark friends specially
        ((t->flags & MF_FRIEND) && !t->player) ? cur_mapcolor_frnd :
        /* cph 2006/07/30 - Show count-as-kills in red. */
        ((t->flags & (MF_COUNTKILL | MF_CORPSE)) == MF_COUNTKILL) ? cur_mapcolor_enemy :
        /* bbm 2/28/03 Show countable items in yellow. */
        (t->flags & MF_COUNTITEM) ? cur_mapcolor_item :
        cur_mapcolor_sprt,
        pt.x,
        pt.y
      );
      t = t->snext;
    }
  }
}

//
// AM_drawMarks()
//
// Draw the marked locations on the automap
//
// Passed nothing, returns nothing
//
// killough 2/22/98:
// Rewrote AM_drawMarks(). Removed limit on marks.
//
// killough 11/98: added hires support

static void AM_drawMarks(void)
{
  int i;
  mpoint_t pt;

  for (i=0;i<markpointnum;i++) // killough 2/22/98: remove automap mark limit
    if (markpoints[i].x != -1)
      {
	int w = (5 * video.xscale) >> FRACBITS;
	int h = (6 * video.yscale) >> FRACBITS;
	int fx;
	int fy;
	int j = i;

	// [crispy] center marks around player
	pt.x = markpoints[i].x;
	pt.y = markpoints[i].y;
	AM_transformPoint(&pt);
	fx = CXMTOF(pt.x);
	fy = CYMTOF(pt.y);

	do
	  {
	    int d = j % 10;

	    if (d == 1)           // killough 2/22/98: less spacing for '1'
	      fx += (video.xscale >> FRACBITS);

	    if (fx >= f_x && fx < f_w - w && fy >= f_y && fy < f_h - h)
	      V_DrawPatch(((fx << FRACBITS) / video.xscale) - video.deltaw,
                           (fy << FRACBITS) / video.yscale,
                          marknums[d]);

	    fx -= w - (video.yscale >> FRACBITS); // killough 2/22/98: 1 space backwards

	    j /= 10;
	  }
	while (j>0);
      }
}

//
// AM_drawCrosshair()
//
// Draw the single point crosshair representing map center
//
// Passed the color to draw the pixel with
// Returns nothing
//
static void AM_drawCrosshair(int color)
{
  // [crispy] do not draw the useless dot on the player arrow
  if (!followplayer)
  {
    PUTDOT((f_w + 1) / 2, (f_h + 1) / 2, color); // single point for now
  }
}

//
// AM_Drawer()
//
// Draws the entire automap
//
// Passed nothing, returns nothing
//
void AM_Drawer (void)
{
  if (!automapactive) return;

  // move AM_doFollowPlayer and AM_changeWindowLoc from AM_Ticker for
  // interpolation

  if (followplayer)
  {
    AM_doFollowPlayer();
  }

  // Change X and Y location.
  if (m_paninc.x || m_paninc.y)
  {
    AM_changeWindowLoc();
  }

  // [crispy/Woof!] required for AM_transformPoint()
  if (automaprotate || ADJUST_ASPECT_RATIO)
  {
    mapcenter.x = m_x + m_w / 2;
    mapcenter.y = m_y + m_h / 2;
    // [crispy] keep the map static if not following the player
    if (automaprotate && followplayer)
    {
      mapangle = ANG90 - plr->mo->angle;
    }
  }

  if (automapoverlay == AM_OVERLAY_OFF)
    AM_clearFB(cur_mapcolor_back);       //jff 1/5/98 background default color
  // [Alaux] Dark automap overlay
  else if (automapoverlay == AM_OVERLAY_DARK && !MN_MenuIsShaded())
    V_ShadeScreen();

  if (automap_grid)                  // killough 2/28/98: change var name
    AM_drawGrid(cur_mapcolor_grid);      //jff 1/7/98 grid default color
  AM_drawWalls();
  AM_drawPlayers();
  if (ddt_cheating==2)
    AM_drawThings(cur_mapcolor_sprt, 0); //jff 1/5/98 default double IDDT sprite
  AM_drawCrosshair(cur_mapcolor_hair);   //jff 1/7/98 default crosshair color

  AM_drawMarks();
}

typedef enum {
  AM_PRESET_VANILLA,
  AM_PRESET_CRISPY,
  AM_PRESET_BOOM,
  AM_PRESET_ZDOOM,
  NUM_AM_PRESETS
} am_preset_t;

static am_preset_t mapcolor_preset;

#define MAPCOLOR(x) &mapcolor_##x, &cur_mapcolor_##x

static struct
{
    int *var, *cur_var;
    int color[NUM_AM_PRESETS]; // Vanilla Doom, Crispy, Boom, ZDoom
} mapcolors[] =
{                                              // ZDoom CVAR name
    {MAPCOLOR(back),    {  0,   0, 247, 139}}, // am_backcolor
    {MAPCOLOR(grid),    {104, 104, 104,  70}}, // am_gridcolor
    {MAPCOLOR(wall),    {176, 180,  23, 239}}, // am_wallcolor
    {MAPCOLOR(fchg),    { 64,  70,  55, 135}}, // am_fdwallcolor
    {MAPCOLOR(cchg),    {231, 163, 215,  76}}, // am_cdwallcolor
    {MAPCOLOR(clsd),    {  0,   0, 208,   0}},
    {MAPCOLOR(rkey),    {176, 176, 175, 176}}, // P_GetMapColorForLock()
    {MAPCOLOR(bkey),    {200, 200, 204, 200}}, // P_GetMapColorForLock()
    {MAPCOLOR(ykey),    {231, 231, 231, 231}}, // P_GetMapColorForLock()
    {MAPCOLOR(rdor),    {176, 174, 175, 176}}, // P_GetMapColorForLock()
    {MAPCOLOR(bdor),    {200, 200, 204, 200}}, // P_GetMapColorForLock()
    {MAPCOLOR(ydor),    {231, 229, 231, 231}}, // P_GetMapColorForLock()
    {MAPCOLOR(tele),    {  0, 120, 119, 200}}, // am_intralevelcolor
    {MAPCOLOR(secr),    {  0, 251, 252, 251}}, // am_unexploredsecretcolor
    {MAPCOLOR(revsecr), {  0, 112, 112, 251}}, // am_secretsectorcolor
    {MAPCOLOR(exit),    {  0, 209, 208, 176}}, // am_interlevelcolor
    {MAPCOLOR(unsn),    { 99,  99, 104, 100}}, // am_notseencolor
    {MAPCOLOR(flat),    { 96,  96,  88,  95}}, // am_tswallcolor
    {MAPCOLOR(sprt),    {112, 112, 112,   4}}, // am_thingcolor
    {MAPCOLOR(hair),    { 96,  96, 208,  97}}, // am_xhaircolor
    {MAPCOLOR(sngl),    {209, 209, 208, 209}}, // am_yourcolor
    {MAPCOLOR(plyr[0]), {112, 112, 112, 112}},
    {MAPCOLOR(plyr[1]), { 96,  96,  88,  88}},
    {MAPCOLOR(plyr[2]), { 64,  64,  64,  64}},
    {MAPCOLOR(plyr[3]), {176, 176, 176, 176}},
    {MAPCOLOR(frnd),    {252, 252, 252,   4}}, // am_thingcolor_friend
    {MAPCOLOR(enemy),   {112, 176, 177,   4}}, // am_thingcolor_monster
    {MAPCOLOR(item),    {112, 231, 231,   4}}, // am_thingcolor_item

    {&hudcolor_titl, NULL, {CR_NONE, CR_GOLD, CR_GOLD, CR_GRAY}}, // DrawAutomapHUD()
};

#undef MAPCOLOR

void AM_ApplyColors(boolean force)
{
    static boolean first_time = true;

    if (!first_time && !force)
    {
        return;
    }
    first_time = false;

    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
    byte *iwad_playpal = NULL;

    for (int i = 0; i < numlumps; i++)
    {
        if (strcasecmp(lumpinfo[i].name, "PLAYPAL") == 0)
        {
            iwad_playpal = W_CacheLumpNum(i, PU_STATIC);
            break;
        }
    }

    if (iwad_playpal == NULL || playpal == iwad_playpal
        || M_CheckIfDisabled("mapcolor_preset"))
    {
        for (int i = 0; mapcolors[i].cur_var; i++)
        {
            *mapcolors[i].cur_var = *mapcolors[i].var;
        }
    }
    else
    {
        for (int i = 0; mapcolors[i].cur_var; i++)
        {
            const int j = *mapcolors[i].var;
            byte r = iwad_playpal[3 * j + 0],
                 g = iwad_playpal[3 * j + 1],
                 b = iwad_playpal[3 * j + 2];

            *mapcolors[i].cur_var = I_GetNearestColor(playpal, r, g, b);
        }
    }

    Z_ChangeTag(playpal, PU_CACHE);
    Z_ChangeTag(iwad_playpal, PU_CACHE);
}

void AM_ColorPreset(void)
{
    for (int i = 0; i < arrlen(mapcolors); i++)
    {
        *mapcolors[i].var = mapcolors[i].color[mapcolor_preset];
    }

    AM_ApplyColors(true);

    ST_ResetTitle();
}

void AM_BindAutomapVariables(void)
{
  M_BindBool("followplayer", &followplayer, NULL, true, ss_auto, wad_no,
             "Automap follows the player");
  M_BindNum("automapoverlay", &automapoverlay, NULL, AM_OVERLAY_OFF,
            AM_OVERLAY_OFF, AM_OVERLAY_DARK, ss_auto, wad_no,
            "Automap overlay mode (0 = Off; 1 = On; 2 = Dark)");
  M_BindBool("automaprotate", &automaprotate, NULL, false, ss_auto, wad_no,
             "Automap rotation");

  M_BindBool("map_point_coord", &map_point_coord, NULL, true, ss_none, wad_no,
             "Show automap pointer coordinates in non-follow mode");
  M_BindBool("map_secret_after", &map_secret_after, NULL, false, ss_auto, wad_no,
             "Don't highlight secret sectors on the automap before they're revealed");
  M_BindNum("map_keyed_door", &map_keyed_door, NULL,
            MAP_KEYED_DOOR_COLOR, MAP_KEYED_DOOR_OFF, MAP_KEYED_DOOR_FLASH,
            ss_auto, wad_no,
            "Color key-locked doors on the automap (1 = Static; 2 = Flashing)");
  M_BindBool("map_smooth_lines", &map_smooth_lines, NULL, true, ss_none,
             wad_no, "Smooth automap lines");

  M_BindNum("mapcolor_preset", &mapcolor_preset, NULL, AM_PRESET_BOOM,
            AM_PRESET_VANILLA, AM_PRESET_ZDOOM, ss_auto, wad_no,
            "Automap color preset (0 = Vanilla Doom; 1 = Crispy Doom; 2 = Boom; 3 = ZDoom)");

  M_BindBool("automapsquareaspect", &automapsquareaspect, NULL, true, ss_none, wad_no,
             "Use square aspect ratio in automap");

#define BIND_CR(name, v, help) \
  M_BindNum(#name, &name, NULL, (v), 0, 255, ss_none, wad_yes, help)

  BIND_CR(mapcolor_back, 247, "Color used for the automap background");
  BIND_CR(mapcolor_grid, 104, "Color used for grid lines");
  BIND_CR(mapcolor_wall, 23, "Color used for one-sided walls");
  BIND_CR(mapcolor_fchg, 55, "Color used for lines with floor height changes");
  BIND_CR(mapcolor_cchg, 215, "Color used for lines with ceiling height changes");
  BIND_CR(mapcolor_clsd, 208, "Color used for lines denoting closed doors, objects");
  BIND_CR(mapcolor_rkey, 175, "Color used for red-key sprites");
  BIND_CR(mapcolor_bkey, 204, "Color used for blue-key sprites");
  BIND_CR(mapcolor_ykey, 231, "Color used for yellow-key sprites");
  BIND_CR(mapcolor_rdor, 175, "Color used for closed red doors");
  BIND_CR(mapcolor_bdor, 204, "Color used for closed blue doors");
  BIND_CR(mapcolor_ydor, 231, "Color used for closed yellow doors");
  BIND_CR(mapcolor_tele, 119, "Color used for teleporter lines");
  BIND_CR(mapcolor_secr, 252, "Color used for lines around secret sectors");
  BIND_CR(mapcolor_revsecr, 112, "Color used for lines around revealed secret sectors");
  BIND_CR(mapcolor_exit, 0, "Color used for exit lines");
  BIND_CR(mapcolor_unsn, 104, "Color used for lines not seen without computer map");
  BIND_CR(mapcolor_flat, 88, "Color used for lines with no height changes");
  BIND_CR(mapcolor_sprt, 112, "Color used for things");
  BIND_CR(mapcolor_hair, 208, "Color used for the automap pointer/crosshair");
  BIND_CR(mapcolor_sngl, 208, "Color used for the player's arrow (single-player only)");

#define BIND_PLR_CR(num, v, help)                                           \
  M_BindNum("mapcolor_ply"#num, &mapcolor_plyr[(num)-1], NULL, (v), 0, 255, \
            ss_none, wad_yes, help)

  BIND_PLR_CR(1, 112, "Color used for the green player's arrow");
  BIND_PLR_CR(2, 88, "Color used for the gray player's arrow");
  BIND_PLR_CR(3, 64, "Color used for the brown player's arrow");
  BIND_PLR_CR(4, 176, "Color used for the red player's arrow");

  BIND_CR(mapcolor_frnd, 252, "Color used for friends");
  BIND_CR(mapcolor_enemy, 177, "Color used for enemies");
  BIND_CR(mapcolor_item, 231, "Color used for countable items");
}

//----------------------------------------------------------------------------
//
// $Log: am_map.c,v $
// Revision 1.24  1998/05/10  12:05:24  jim
// formatted/documented am_map
//
// Revision 1.23  1998/05/03  22:13:49  killough
// Provide minimal headers at top; no other changes
//
// Revision 1.22  1998/04/23  13:06:53  jim
// Add exit line to automap
//
// Revision 1.21  1998/04/16  16:16:56  jim
// Fixed disappearing marks after new level
//
// Revision 1.20  1998/04/03  14:45:17  jim
// Fixed automap disables at 0, mouse sens unbounded
//
// Revision 1.19  1998/03/28  05:31:40  jim
// Text enabling changes for DEH
//
// Revision 1.18  1998/03/23  03:06:22  killough
// I wonder
//
// Revision 1.17  1998/03/15  14:36:46  jim
// fixed secrets transfer bug in automap
//
// Revision 1.16  1998/03/10  07:06:21  jim
// Added secrets on automap after found only option
//
// Revision 1.15  1998/03/09  18:29:22  phares
// Created separately bound automap and menu keys
//
// Revision 1.14  1998/03/02  11:22:30  killough
// change grid to automap_grid and make external
//
// Revision 1.13  1998/02/23  04:08:11  killough
// Remove limit on automap marks, save them in savegame
//
// Revision 1.12  1998/02/17  22:58:40  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.11  1998/02/15  03:12:42  phares
// Jim's previous comment: Fixed bug in automap from mistaking framebuffer index for mark color
//
// Revision 1.10  1998/02/15  02:47:33  phares
// User-defined keys
//
// Revision 1.8  1998/02/09  02:50:13  killough
// move ddt cheat to st_stuff.c and some cleanup
//
// Revision 1.7  1998/02/02  22:16:31  jim
// Fixed bug in automap that showed secret lines
//
// Revision 1.6  1998/01/26  20:57:54  phares
// Second test of checkin/checkout
//
// Revision 1.5  1998/01/26  20:28:15  phares
// First checkin/checkout script test
//
// Revision 1.4  1998/01/26  19:23:00  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/24  11:21:25  jim
// Changed disables in automap to -1 and -2 (nodraw)
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
