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
//  DOOM main program (D_DoomMain) and game loop, plus functions to
//  determine game mode (shareware, registered), parse command line
//  parameters, configure game parameters (turbo), and call the startup
//  functions.
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "config.h"
#include "d_deh.h"  // Ty 04/08/98 - Externalizations
#include "d_demoloop.h"
#include "d_event.h"
#include "d_iwad.h"
#include "d_loop.h"
#include "d_main.h"
#include "d_player.h"
#include "d_quit.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dsdhacked.h"
#include "dstrings.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "g_compatibility.h"
#include "g_game.h"
#include "i_endoom.h"
#include "i_glob.h"
#include "i_input.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "info.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_config.h"
#include "m_fixed.h"
#include "m_input.h"
#include "m_io.h"
#include "mn_menu.h"
#include "m_misc.h"
#include "m_swap.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "p_ambient.h"
#include "p_inter.h" // maxhealthbonus
#include "p_map.h"   // MELEERANGE
#include "p_mobj.h"
#include "p_setup.h"
#include "r_bmaps.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "r_voxel.h"
#include "s_sound.h"
#include "sounds.h"
#include "s_trakinfo.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "statdump.h"
#include "g_umapinfo.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "ws_stuff.h"
#include "z_zone.h"

// DEHacked support - Ty 03/09/97
// killough 10/98:
// Add lump number as third argument, for use when filename==NULL
void ProcessDehFile(const char *filename, char *outfilename, int lump);

// mbf21
void PostProcessDeh(void);

// killough 10/98: support -dehout filename
static char *D_dehout(void)
{
  static char *s;      // cache results over multiple calls
  if (!s)
    {
      //!
      // @category mod
      // @arg <filename>
      //
      // Enables verbose dehacked parser logging.
      //

      int p = M_CheckParm("-dehout");
      if (!p)

        //!
        // @category mod
        // @arg <filename>
        //
        // Alias to -dehout.
        //

        p = M_CheckParm("-bexout");
      s = p && ++p < myargc ? myargv[p] : "";
    }
  return s;
}

static void ProcessDehLump(int lumpnum)
{
  ProcessDehFile(NULL, D_dehout(), lumpnum);
}

boolean devparm;        // started game with -devparm

// jff 1/24/98 add new versions of these variables to remember command line
boolean clnomonsters;   // checkparm of -nomonsters
boolean clrespawnparm;  // checkparm of -respawn
boolean clfastparm;     // checkparm of -fast
boolean clpistolstart;  // checkparm of -pistolstart
boolean clcoopspawns;   // checkparm of -coop_spawns
// jff 1/24/98 end definition of command line version of play mode switches

// custom skill options
boolean cshalfplayerdamage = false;
boolean csdoubleammo = false;
boolean csaggromonsters = false;

boolean nomonsters;     // working -nomonsters
boolean respawnparm;    // working -respawn
boolean fastparm;       // working -fast
boolean pistolstart;    // working -pistolstart
boolean coopspawns;     // working -coop_spawns

boolean halfplayerdamage;
boolean doubleammo;
boolean aggromonsters;

boolean singletics = false; // debug flag to cancel adaptiveness

//jff 1/22/98 parms for disabling music and sound
boolean nosfxparm;
boolean nomusicparm;

skill_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
int     startloadgame;

boolean advancedemo;

char    *basedefault = NULL;   // default file
char    *basesavegame = NULL;  // killough 2/16/98: savegame directory
char    *screenshotdir = NULL; // [FG] screenshot directory

int organize_savefiles;

static boolean demobar;

// [FG] colored blood and gibs
static boolean colored_blood;

void D_ConnectNetGame (void);
void D_CheckNetGame (void);
void D_ProcessEvents (void);
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo (void);

//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//

event_t events[MAXEVENTS];
int eventhead, eventtail;

//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent(event_t *ev)
{
  switch (ev->type)
  {
    case ev_mouse:
      if (uncapped && raw_input)
      {
        G_MovementResponder(ev);
        G_PrepMouseTiccmd();
        return;
      }
      break;

    case ev_joystick:
      if (uncapped && raw_input)
      {
        G_MovementResponder(ev);
        G_PrepGamepadTiccmd();
        return;
      }
      break;

    case ev_gyro:
      if (uncapped && raw_input)
      {
        G_MovementResponder(ev);
        G_PrepGyroTiccmd();
        return;
      }
      break;

    default:
      break;
  }

  events[eventhead++] = *ev;
  eventhead &= MAXEVENTS - 1;
}

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//

void D_ProcessEvents (void)
{
    for (; eventtail != eventhead; eventtail = (eventtail+1) & (MAXEVENTS-1))
    {
      M_InputTrackEvent(events+eventtail);
      if (!M_Responder(events+eventtail))
        G_Responder(events+eventtail);
    }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t    wipegamestate = GS_DEMOSCREEN;
static int     screen_melt = wipe_Melt;

void D_Display (void)
{
  static boolean viewactivestate = false;
  static boolean menuactivestate = false;
  static gamestate_t oldgamestate = GS_NONE;
  static boolean borderdrawcount;
  int wipestart;
  boolean done, wipe;

  if (demobar && PLAYBACK_SKIP)
  {
    if (ST_DemoProgressBar(false))
    {
      I_FinishUpdate();
      return;
    }
  }

  if (nodrawers)                    // for comparative timing / profiling
    return;

  if (uncapped)
  {
    // [AM] Figure out how far into the current tic we're in as a fixed_t.
    fractionaltic = I_GetFracTime();

    if (!menuactive && gamestate == GS_LEVEL && !paused && raw_input)
    {
      I_StartDisplay();
    }
  }

  wipe = false;

  // save the current screen if about to wipe
  if (gamestate != wipegamestate && (strictmode || screen_melt))
    {
      wipe = true;
      wipe_StartScreen(0, 0, video.width, video.height);
    }

  if (!wipe)
    {
      if (resetneeded)
        I_ResetScreen();
      else if (gamestate == GS_LEVEL)
        I_DynamicResolution();
    }

  if (setsmoothlight)
    R_SmoothLight();

  if (setsizeneeded)                // change the view size if needed
    {
      R_ExecuteSetViewSize();
      oldgamestate = GS_NONE;            // force background redraw
      borderdrawcount = true;
    }

  if (gamestate == GS_LEVEL && gametic)
    ST_Erase();

  switch (gamestate)                // do buffered drawing
    {
    case GS_LEVEL:
      if (automapactive && !automapoverlay && gametic)
        {
          // [FG] update automap while playing
          R_RenderPlayerView(&players[displayplayer]);
          AM_Drawer();
        }
      break;
    case GS_INTERMISSION:
      WI_Drawer();
      break;
    case GS_FINALE:
      F_Drawer();
      break;
    case GS_DEMOSCREEN:
      D_PageDrawer();
      break;
    case GS_NONE:
      break;
    }

  // draw the view directly
  if (gamestate == GS_LEVEL && automap_off && gametic)
    R_RenderPlayerView(&players[displayplayer]);

  // clean up border stuff
  if (gamestate != oldgamestate && gamestate != GS_LEVEL)
    I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));

  // see if the border needs to be initially drawn
  if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
      viewactivestate = false;        // view was not active
      R_FillBackScreen ();    // draw the pattern into the back screen
    }

  // see if the border needs to be updated to the screen
  if (gamestate == GS_LEVEL && automap_off && scaledviewwidth != video.unscaledw)
    {
      if (menuactive || menuactivestate || !viewactivestate)
        borderdrawcount = true;
      if (borderdrawcount)
        {
          R_DrawViewBorder();    // erase old menu stuff
          borderdrawcount = false;
        }
    }

  menuactivestate = menuactive;
  viewactivestate = viewactive;
  oldgamestate = wipegamestate = gamestate;

  if (gamestate == GS_LEVEL && automapactive && automapoverlay)
    {
      AM_Drawer();
      // [crispy] force redraw of border
      viewactivestate = false;
    }

  if (gamestate == GS_LEVEL && gametic)
    ST_Drawer();

  // draw pause pic
  if (paused)
    {
      int x = scaledviewx;
      int y = 4;
      patch_t *patch = V_CachePatchName("M_PAUSE", PU_CACHE);

      x += (scaledviewwidth - SHORT(patch->width)) / 2 - video.deltaw;

      if (!automapactive)
        y += scaledviewy;

      V_DrawPatch(x, y, patch);
    }

  // menus go directly to the screen
  M_Drawer();          // menu is drawn even on top of everything
  NetUpdate();         // send out any new accumulation

  if (demobar && demoplayback)
    ST_DemoProgressBar(true);

  // normal update
  if (!wipe)
    {
      I_FinishUpdate ();              // page flip or blit buffer
      return;
    }

  // wipe update
  wipe_EndScreen(0, 0, video.width, video.height);

  wipestart = I_GetTime () - 1;

  do
    {
      int nowtime = I_GetTime();
      int tics = nowtime - wipestart;

      fractionaltic = I_GetFracTime();

      done = wipe_ScreenWipe(strictmode ? wipe_Melt : screen_melt,
                             0, 0, video.width, video.height, tics);
      wipestart = nowtime;
      M_Drawer();                   // menu is drawn even on top of wipes
      I_FinishUpdate();             // page flip or blit buffer
    }
  while (!done);
}

//
//  DEMO LOOP
//

static int demosequence;         // killough 5/2/98: made static
static int pagetic;
static const char *pagename;
static demoloop_t demoloop_point;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void)
{
  if (menuactive && !netgame && menu_pause_demos)
  {
    return;
  }

  // killough 12/98: don't advance internal demos if a single one is 
  // being played. The only time this matters is when using -loadgame with
  // -fastdemo, -playdemo, or -timedemo, and a consistency error occurs.

  if (!singledemo && --pagetic < 0)
    D_AdvanceDemo();
}

//
// D_PageDrawer
//
// killough 11/98: add credits screen
//

void D_PageDrawer(void)
{
  V_DrawPatchFullScreen(
    V_CachePatchName(W_CheckWidescreenPatch(pagename), PU_CACHE));
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//

void D_AdvanceDemo(void)
{
  advancedemo = true;
}

// This cycles through the demo sequences.
void D_AdvanceDemoLoop(void)
{
  demosequence = (demosequence + 1) % demoloop_count;
  demoloop_point = &demoloop[demosequence];
}

void D_DoAdvanceDemo(void)
{
    S_StopAmbientSounds();

    players[consoleplayer].playerstate = PST_LIVE; // not reborn
    advancedemo = false;
    usergame = false; // no save / end game here
    paused = false;
    gameaction = ga_nothing;
    gamestate = GS_DEMOSCREEN;

    D_AdvanceDemoLoop();
    switch (demoloop_point->type)
    {
        case TYPE_ART:
            if (W_CheckNumForName(demoloop_point->primary_lump) >= 0)
            {
                pagename = demoloop_point->primary_lump;
                pagetic = demoloop_point->duration;
                int music = W_CheckNumForName(demoloop_point->secondary_lump);
                if (music >= 0)
                {
                    S_ChangeMusInfoMusic(music, false);
                }
            }
            break;

        case TYPE_DEMO:
            if (W_CheckNumForName(demoloop_point->primary_lump) >= 0)
            {
                G_DeferedPlayDemo(demoloop_point->primary_lump);
            }
            break;

        default:
            I_Printf(VB_DEBUG, "D_DoAdvanceDemo: unhandled demoloop type");
            break;
    }
}

//
// D_StartTitle
//
void D_StartTitle (void)
{
  gameaction = ga_nothing;
  demosequence = -1;
  demoplayback = false;
  D_AdvanceDemo();
}

//
// D_AddFile
//
// Rewritten by Lee Killough
//
// killough 11/98: remove limit on number of files
//

void D_AddFile(const char *file)
{
  // [FG] search for PWADs by their filename
  char *path = D_TryFindWADByName(file);

  if (!W_AddPath(path))
  {
    I_Error("Failed to load %s", file);
  }
}

// killough 10/98: return the name of the program the exe was invoked as
const char *D_DoomExeName(void)
{
  return PROJECT_SHORTNAME;
}

// Calculate the path to the directory for autoloaded WADs/DEHs.
// Creates the directory as necessary.

typedef struct {
    const char *dir;
    char *(*func)(void);
    boolean createdir;
} basedir_t;

static basedir_t basedirs[] = {
#if !defined(_WIN32)
    {"../share/" PROJECT_SHORTNAME, D_DoomExeDir, false},
#endif
    {NULL, D_DoomPrefDir, true},
#if !defined(_WIN32) || defined(_WIN32_WCE)
    {NULL, D_DoomExeDir, false},
#endif
};

static void LoadBaseFile(void)
{
    for (int i = 0; i < arrlen(basedirs); ++i)
    {
        basedir_t d = basedirs[i];
        boolean result = false;

        if (d.dir && d.func)
        {
            char *s = M_StringJoin(d.func(), DIR_SEPARATOR_S, d.dir);
            result = W_InitBaseFile(s);
            free(s);
        }
        else if (d.dir)
        {
            result = W_InitBaseFile(d.dir);
        }
        else if (d.func)
        {
            result = W_InitBaseFile(d.func());
        }

        if (result)
        {
            return;
        }
    }

    I_Error(PROJECT_SHORTNAME ".pk3 not found");
}

static char **autoload_paths = NULL;

static char *GetAutoloadDir(const char *base, const char *iwadname, boolean createdir)
{
    char *result;
    char *lower;

    lower = M_StringDuplicate(iwadname);
    M_StringToLower(lower);
    result = M_StringJoin(base, DIR_SEPARATOR_S, lower);
    free(lower);

    if (createdir)
    {
        M_MakeDirectory(result);
    }

    return result;
}

static void PrepareAutoloadPaths(void)
{
    //!
    // @category mod
    //
    // Disable auto-loading of .wad and .deh files.
    //

    if (M_CheckParm("-noautoload"))
    {
        return;
    }

    for (int i = 0; i < arrlen(basedirs); i++)
    {
        basedir_t d = basedirs[i];

        if (d.dir && d.func)
        {
            array_push(autoload_paths,
                       M_StringJoin(d.func(), DIR_SEPARATOR_S, d.dir,
                                    DIR_SEPARATOR_S, "autoload"));
        }
        else if (d.dir)
        {
            array_push(autoload_paths,
                       M_StringJoin(d.dir, DIR_SEPARATOR_S, "autoload"));
        }
        else if (d.func)
        {
            array_push(autoload_paths, M_StringJoin(d.func(), DIR_SEPARATOR_S,
                                                    "autoload"));
        }

        if (d.createdir)
        {
            M_MakeDirectory(autoload_paths[i]);
        }
    }
}

//
// CheckIWAD
//

static void CheckIWAD(void)
{
    for (int i = 0; i < numlumps; ++i)
    {
        if (!strncasecmp(lumpinfo[i].name, "MAP01", 8))
        {
            gamemission = doom2;
            break;
        }
        else if (!strncasecmp(lumpinfo[i].name, "E1M1", 8))
        {
            gamemode = shareware;
            gamemission = doom;
            break;
        }
    }

    if (gamemission == doom2)
    {
        gamemode = commercial;
    }
    else
    {
        for (int i = 0; i < numlumps; ++i)
        {
            if (!strncasecmp(lumpinfo[i].name, "E4M1", 8))
            {
                gamemode = retail;
                break;
            }
            else if (!strncasecmp(lumpinfo[i].name, "E3M1", 8))
            {
                gamemode = registered;
            }
        }
    }

    if (gamemode == indetermined)
    {
        I_Error("Unknown or invalid IWAD file.");
    }
}

static boolean CheckMapLump(const char *lumpname, const char *filename)
{
    int lumpnum = W_CheckNumForName(lumpname);
    if (lumpnum >= 0 && lumpinfo[lumpnum].wad_file == filename)
    {
        return true;
    }
    return false;
}

static boolean FileContainsMaps(const char *filename)
{
    for (int i = 0; i < array_size(umapinfo); ++i)
    {
        if (CheckMapLump(umapinfo[i].mapname, filename))
        {
            return true;
        }
    }

    if (gamemode == commercial)
    {
        for (int m = 1; m < 35; ++m)
        {
            if (CheckMapLump(MapName(1, m), filename))
            {
                return true;
            }
        }
    }
    else
    {
        for (int e = 1; e < 5; ++e)
        {
            for (int m = 1; m < 10; ++m)
            {
                if (CheckMapLump(MapName(e, m), filename))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

//
// IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm

const char *gamedescription = NULL;

void IdentifyVersion(void)
{
    // get config file from same directory as executable
    // killough 10/98

    basedefault = M_StringJoin(D_DoomPrefDir(), DIR_SEPARATOR_S,
                               D_DoomExeName(), ".cfg");

    // locate the IWAD and determine game mode from it

    char *iwadfile = D_FindIWADFile();

    if (!iwadfile)
    {
        I_Error("IWAD not found!\n"
                "\n"
                "Place an IWAD file (e.g. doom.wad, doom2.wad)\n"
                "in one of the IWAD search directories, e.g.\n"
                "%s", D_DoomPrefDir());
    }

    D_AddFile(iwadfile);

    D_GetModeAndMissionByIWADName(M_BaseName(wadfiles[0]), &gamemode, &gamemission);

    if (gamemode == indetermined)
    {
        CheckIWAD();
    }

    gamedescription = D_GetIWADDescription(M_BaseName(wadfiles[0]), gamemode, gamemission);
    I_Printf(VB_INFO, " - \"%s\" version", gamedescription);
}

// [FG] emulate a specific version of Doom

static void InitGameVersion(void)
{
    int i, p;

    //!
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom. Valid values are "1.9",
    // "ultimate", "final", "chex". Implies -complevel vanilla.
    //

    p = M_CheckParm("-gameversion");

    if (p && p < myargc-1)
    {
        for (i=0; gameversions[i].description != NULL; ++i)
        {
            if (!strcmp(myargv[p+1], gameversions[i].cmdline))
            {
                gameversion = gameversions[i].version;
                break;
            }
        }

        if (gameversions[i].description == NULL)
        {
            I_Printf(VB_ERROR, "Supported game versions:");

            for (i=0; gameversions[i].description != NULL; ++i)
            {
                I_Printf(VB_ERROR, "\t%s (%s)", gameversions[i].cmdline,
                         gameversions[i].description);
            }

            I_Error("Unknown game version '%s'", myargv[p+1]);
        }
    }
    else
    {
        // Determine automatically

        if (gamemode == shareware || gamemode == registered ||
            (gamemode == commercial && gamemission != pack_tnt && gamemission != pack_plut))
        {
            // original
            gameversion = exe_doom_1_9;
        }
        else if (gamemode == retail)
        {
            if (gamemission == pack_chex)
            {
                gameversion = exe_chex;
            }
            else
            {
                gameversion = exe_ultimate;
            }
        }
        else if (gamemode == commercial)
        {
            // Final Doom: tnt or plutonia
            // Defaults to emulating the first Final Doom executable,
            // which has the crash in the demo loop; however, having
            // this as the default should mean that it plays back
            // most demos correctly.

            gameversion = exe_final;
        }
    }
}

// killough 5/3/98: old code removed

//
// Find a Response File
//

#define MAXARGVS 100

void FindResponseFile (void)
{
  int i;

  for (i = 1;i < myargc;i++)
    if (myargv[i][0] == '@')
      {
        FILE *handle;
        int  size;
        int  k;
        int  index;
        int  indexinfile;
        char *infile;
        char *file;
        char *moreargs[MAXARGVS];
        char *firstargv;

        // READ THE RESPONSE FILE INTO MEMORY

        // killough 10/98: add default .rsp extension
        char *filename = AddDefaultExtension(&myargv[i][1],".rsp");

        handle = M_fopen(filename,"rb");
        if (!handle)
          I_Error("No such response file!");          // killough 10/98

        I_Printf(VB_INFO, "Found response file %s!",filename);
        free(filename);

        fseek(handle,0,SEEK_END);
        size = ftell(handle);
        fseek(handle,0,SEEK_SET);
        file = malloc(size);
        // [FG] check return value
        if (!fread(file,size,1,handle))
        {
          fclose(handle);
          free(file);
          return;
        }
        fclose(handle);

        // KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
        for (index = 0,k = i+1; k < myargc; k++)
          moreargs[index++] = myargv[k];

        firstargv = myargv[0];
        myargv = calloc(sizeof(char *),MAXARGVS);
        myargv[0] = firstargv;

        infile = file;
        indexinfile = k = 0;
        indexinfile++;  // SKIP PAST ARGV[0] (KEEP IT)
        do
          {
            boolean quote = false;
            if (infile[k] == '"')
            {
                quote = true;
                k++;
            }
            myargv[indexinfile++] = infile+k;
            while(k < size &&
                  ((*(infile+k)>= ' ') && (*(infile+k)<='z')))
            {
              if ((!quote && infile[k] == ' ') ||
                  (quote && infile[k] == '"'))
              {
                break;
              }
              k++;
            }
            *(infile+k) = 0;
            while(k < size &&
                  ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
              k++;
          }
        while(k < size);

        for (k = 0;k < index;k++)
          myargv[indexinfile++] = moreargs[k];
        myargc = indexinfile;

        // DISPLAY ARGS
        I_Printf(VB_INFO, "%d command-line args:",myargc-1); // killough 10/98
        for (k=1;k<myargc;k++)
          I_Printf(VB_INFO, "%s",myargv[k]);
        break;
      }
}

// [FG] compose a proper command line from loose file paths passed as arguments
// to allow for loading WADs and DEHACKED patches by drag-and-drop

enum
{
    FILETYPE_UNKNOWN = 0x0,
    FILETYPE_IWAD =    0x2,
    FILETYPE_PWAD =    0x4,
    FILETYPE_DEH =     0x8,
    FILETYPE_DEMO =    0x10,
};

static boolean FileIsDemoLump(const char *filename)
{
    FILE *handle;
    int count, ver;
    byte buf[32], *p = buf;

    handle = M_fopen(filename, "rb");

    if (handle == NULL)
    {
        return false;
    }

    count = fread(buf, 1, sizeof(buf), handle);
    fclose(handle);

    if (count != sizeof(buf))
    {
        return false;
    }

    ver = *p++;

    if (ver == 255) // skip UMAPINFO demo header
    {
        p += 26;
        ver = *p++;
    }

    if (ver >= 0 && ver <= 4) // v1.0/v1.1/v1.2
    {
        p--;
    }
    else
    {
        switch (ver)
        {
            case 104: // v1.4
            case 105: // v1.5
            case 106: // v1.6/v1.666
            case 107: // v1.7/v1.7a
            case 108: // v1.8
            case 109: // v1.9
            case 111: // v1.91 hack
                break;
            case 200: // Boom
            case 201:
            case 202:
            case 203: // MBF
                p += 7; // skip signature and compatibility flag
                break;
            case 221: // MBF21
                p += 6; // skip signature
                break;
            default:
                return false;
                break;
        }
    }

    if (*p++ > 5) // skill
    {
        return false;
    }
    if (*p++ > 9) // episode
    {
        return false;
    }
    if (*p++ > 99) // map
    {
        return false;
    }

    return true;
}

static int GuessFileType(const char *name)
{
    int ret = FILETYPE_UNKNOWN;
    const char *base;
    char *lower;
    static boolean iwad_found = false, demo_found = false;

    base = M_BaseName(name);
    lower = M_StringDuplicate(base);
    M_StringToLower(lower);

    // only ever add one argument to the -iwad parameter

    if (iwad_found == false && D_IsIWADName(lower))
    {
        ret = FILETYPE_IWAD;
        iwad_found = true;
    }
    else if (M_StringEndsWith(lower, ".wad") ||
             M_StringEndsWith(lower, ".zip"))
    {
        ret = FILETYPE_PWAD;
    }
    else if (M_StringEndsWith(lower, ".lmp"))
    {
        // only ever add one argument to the -playdemo parameter

        if (demo_found == false && FileIsDemoLump(name))
        {
            ret = FILETYPE_DEMO;
            demo_found = true;
        }
        else
        {
            ret = FILETYPE_PWAD;
        }
    }
    else if (M_StringEndsWith(lower, ".deh") ||
             M_StringEndsWith(lower, ".bex"))
    {
        ret = FILETYPE_DEH;
    }

    free(lower);

    return ret;
}

typedef struct
{
    char *str;
    int type, stable;
} argument_t;

static int CompareByFileType(const void *a, const void *b)
{
    const argument_t *arg_a = (const argument_t *) a;
    const argument_t *arg_b = (const argument_t *) b;

    const int ret = arg_a->type - arg_b->type;

    return ret ? ret : (arg_a->stable - arg_b->stable);
}

static void M_AddLooseFiles(void)
{
    int i, types = 0;
    char **newargv;
    argument_t *arguments;

    if (myargc < 2)
    {
        return;
    }

    // allocate space for up to four additional regular parameters
    // (-iwad, -merge, -deh, -playdemo)

    arguments = malloc((myargc + 4) * sizeof(*arguments));
    memset(arguments, 0, (myargc + 4) * sizeof(*arguments));

    // check the command line and make sure it does not already
    // contain any regular parameters or response files
    // but only fully-qualified LFS or UNC file paths

    for (i = 1; i < myargc; i++)
    {
        char *arg = myargv[i];
        int type;

        if (strlen(arg) < 3 ||
            arg[0] == '-' ||
            arg[0] == '@' ||
#if defined (_WIN32)
            ((!isalpha(arg[0]) || arg[1] != ':' || arg[2] != '\\') &&
            (arg[0] != '\\' || arg[1] != '\\'))
#else
            (arg[0] != '/' && arg[0] != '.')
#endif
          )
        {
            free(arguments);
            return;
        }

        type = GuessFileType(arg);
        arguments[i].str = arg;
        arguments[i].type = type;
        arguments[i].stable = i;
        types |= type;
    }

    // add space for one additional regular parameter
    // for each discovered file type in the new argument list
    // and sort parameters right before their corresponding file paths

    if (types & FILETYPE_IWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-iwad");
        arguments[myargc].type = FILETYPE_IWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_PWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-file");
        arguments[myargc].type = FILETYPE_PWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_DEH)
    {
        arguments[myargc].str = M_StringDuplicate("-deh");
        arguments[myargc].type = FILETYPE_DEH - 1;
        myargc++;
    }
    if (types & FILETYPE_DEMO)
    {
        arguments[myargc].str = M_StringDuplicate("-playdemo");
        arguments[myargc].type = FILETYPE_DEMO - 1;
        myargc++;
    }

    newargv = malloc(myargc * sizeof(*newargv));

    // sort the argument list by file type, except for the zeroth argument
    // which is the executable invocation itself

    qsort(arguments + 1, myargc - 1, sizeof(*arguments), CompareByFileType);

    newargv[0] = myargv[0];

    for (i = 1; i < myargc; i++)
    {
        newargv[i] = arguments[i].str;
    }

    free(arguments);

    myargv = newargv;
}

// killough 10/98: moved code to separate function

static void D_ProcessDehCommandLine(void)
{
  // ty 03/09/98 do dehacked stuff
  // Note: do this before any other since it is expected by
  // the deh patch author that this is actually part of the EXE itself
  // Using -deh in BOOM, others use -dehacked.
  // Ty 03/18/98 also allow .bex extension.  .bex overrides if both exist.
  // killough 11/98: also allow -bex

  //!
  // @arg <files>
  // @category mod
  // @help
  //
  // Load the given dehacked/bex patch(es).
  //

  int p = M_CheckParm ("-deh");

  //!
  // @arg <files>
  // @category mod
  //
  // Alias to -deh.
  //

  if (p || (p = M_CheckParm("-bex")))
    {
      // the parms after p are deh/bex file names,
      // until end of parms or another - preceded parm
      // Ty 04/11/98 - Allow multiple -deh files in a row
      // killough 11/98: allow multiple -deh parameters

      boolean deh = true;
      while (++p < myargc)
        if (*myargv[p] == '-')
          deh = !strcasecmp(myargv[p],"-deh") || !strcasecmp(myargv[p],"-bex");
        else
          if (deh)
            {
              char *probe;
              char *file = AddDefaultExtension(myargv[p], ".bex");
              probe = D_TryFindWADByName(file);
              free(file);
              if (M_access(probe, F_OK))  // nope
                {
                  free(probe);
                  file = AddDefaultExtension(myargv[p], ".deh");
                  probe = D_TryFindWADByName(file);
                  free(file);
                  if (M_access(probe, F_OK))  // still nope
                  {
                    free(probe);
                    I_Error("Cannot find .deh or .bex file named %s",
                            myargv[p]);
                  }
                }
              // during the beta we have debug output to dehout.txt
              // (apparently, this was never removed after Boom beta-killough)
              ProcessDehFile(probe, D_dehout(), 0);  // killough 10/98
              free(probe);
            }
    }
  // ty 03/09/98 end of do dehacked stuff
}

// Load all WAD files from the given directory.

static void AutoLoadWADs(const char *path)
{
    glob_t * glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                                     "*.wad", "*.zip", "*.pk3");
    for (;;)
    {
        const char *filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }

        if (!W_AddPath(filename))
        {
            I_Error("Failed to load %s", filename);
        }
    }
    I_EndGlob(glob);

    W_AddPath(path);
}

static int firstpwad = 1;

static void LoadIWadBase(void)
{
    GameMode_t local_gamemode;
    GameMission_t local_gamemission;
    D_GetModeAndMissionByIWADName(M_BaseName(wadfiles[0]), &local_gamemode,
                                  &local_gamemission);

    if (local_gamemission == none || local_gamemode == indetermined)
    {
        return;
    }

    if (local_gamemission < pack_chex)
    {
        W_AddBaseDir("doom-all");
    }
    if (local_gamemission == pack_chex || local_gamemission == pack_chex3v)
    {
        W_AddBaseDir("chex-all");
    }
    if (local_gamemission == doom)
    {
        W_AddBaseDir("doom1-all");
    }
    else if (local_gamemission >= doom2 && local_gamemission <= pack_plut)
    {
        W_AddBaseDir("doom2-all");
    }
    else if (local_gamemission == pack_freedoom)
    {
        W_AddBaseDir("freedoom-all");
        if (local_gamemode == commercial)
        {
            W_AddBaseDir("freedoom2-all");
        }
        else
        {
            W_AddBaseDir("freedoom1-all");
        }
    }
    else if (local_gamemission == pack_rekkr)
    {
        W_AddBaseDir("rekkr-all");
    }
    for (int i = 0; i < firstpwad; i++)
    {
        W_AddBaseDir(M_BaseName(wadfiles[i]));
    }
}

static void AutoloadIWadDir(void (*AutoLoadFunc)(const char *path))
{
    GameMode_t local_gamemode;
    GameMission_t local_gamemission;
    D_GetModeAndMissionByIWADName(M_BaseName(wadfiles[0]), &local_gamemode, &local_gamemission);

    for (int i = 0; i < firstpwad; i++)
    {
        for (int j = 0; j < array_size(autoload_paths); ++j)
        {
            char *dir = GetAutoloadDir(autoload_paths[j], "all-all", true);
            AutoLoadFunc(dir);
            free(dir);

            // common auto-loaded files for all Doom flavors
            if (local_gamemission != none)
            {
                if (local_gamemission < pack_chex)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "doom-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                }
                else if (local_gamemission == pack_chex || local_gamemission == pack_chex3v)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "chex-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                }

                if (local_gamemission == doom)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "doom1-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                }
                else if (local_gamemission >= doom2
                         && local_gamemission <= pack_plut)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "doom2-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                }
                else if (local_gamemission == pack_freedoom)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "freedoom-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                    if (local_gamemode == commercial)
                    {
                        dir = GetAutoloadDir(autoload_paths[j], "freedoom2-all", true);
                        AutoLoadFunc(dir);
                        free(dir);
                    }
                    else
                    {
                        dir = GetAutoloadDir(autoload_paths[j], "freedoom1-all", true);
                        AutoLoadFunc(dir);
                        free(dir);
                    }
                }
                else if (local_gamemission == pack_rekkr)
                {
                    dir = GetAutoloadDir(autoload_paths[j], "rekkr-all", true);
                    AutoLoadFunc(dir);
                    free(dir);
                }
            }

            // auto-loaded files per IWAD
            dir = GetAutoloadDir(autoload_paths[j], M_BaseName(wadfiles[i]), true);
            AutoLoadFunc(dir);
            free(dir);
        }
    }
}

static void LoadPWadBase(void)
{
    for (int i = firstpwad; i < array_size(wadfiles); ++i)
    {
        W_AddBaseDir(wadfiles[i]);
    }
}

static void AutoloadPWadDir(void (*AutoLoadFunc)(const char *path))
{
    for (int i = firstpwad; i < array_size(wadfiles); ++i)
    {
        for (int j = 0; j < array_size(autoload_paths); ++j)
        {
            char *dir = GetAutoloadDir(autoload_paths[j],
                                       M_BaseName(wadfiles[i]), false);
            AutoLoadFunc(dir);
            free(dir);
        }
    }
}

// Load all dehacked patches from the given directory.

static void AutoLoadPatches(const char *path)
{
    const char *filename;
    glob_t *glob;

    glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                            "*.deh", "*.bex");
    for (;;)
    {
        filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }
        ProcessDehFile(filename, D_dehout(), 0);
    }

    I_EndGlob(glob);
}

// mbf21: don't want to reorganize info.c structure for a few tweaks...

static void D_InitTables(void)
{
  int i;
  for (i = 0; i < num_mobj_types; ++i)
  {
    mobjinfo[i].flags2           = 0;
    mobjinfo[i].infighting_group = IG_DEFAULT;
    mobjinfo[i].projectile_group = PG_DEFAULT;
    mobjinfo[i].splash_group     = SG_DEFAULT;
    mobjinfo[i].ripsound         = sfx_None;
    mobjinfo[i].altspeed         = NO_ALTSPEED;
    mobjinfo[i].meleerange       = MELEERANGE;
    // [Woof!]
    mobjinfo[i].bloodcolor       = 0; // Normal
    // DEHEXTRA
    mobjinfo[i].droppeditem      = MT_NULL;
  }

  mobjinfo[MT_VILE].flags2    = MF2_SHORTMRANGE | MF2_DMGIGNORED | MF2_NOTHRESHOLD;
  mobjinfo[MT_CYBORG].flags2  = MF2_NORADIUSDMG | MF2_HIGHERMPROB | MF2_RANGEHALF |
                                MF2_FULLVOLSOUNDS | MF2_E2M8BOSS | MF2_E4M6BOSS;
  mobjinfo[MT_SPIDER].flags2  = MF2_NORADIUSDMG | MF2_RANGEHALF | MF2_FULLVOLSOUNDS |
                                MF2_E3M8BOSS | MF2_E4M8BOSS;
  mobjinfo[MT_SKULL].flags2   = MF2_RANGEHALF;
  mobjinfo[MT_FATSO].flags2   = MF2_MAP07BOSS1;
  mobjinfo[MT_BABY].flags2    = MF2_MAP07BOSS2;
  mobjinfo[MT_BRUISER].flags2 = MF2_E1M8BOSS;
  mobjinfo[MT_UNDEAD].flags2  = MF2_LONGMELEE | MF2_RANGEHALF;

  mobjinfo[MT_BRUISER].projectile_group = PG_BARON;
  mobjinfo[MT_KNIGHT].projectile_group = PG_BARON;

  mobjinfo[MT_BRUISERSHOT].altspeed = 20 * FRACUNIT;
  mobjinfo[MT_HEADSHOT].altspeed = 20 * FRACUNIT;
  mobjinfo[MT_TROOPSHOT].altspeed = 20 * FRACUNIT;

  // DEHEXTRA
  mobjinfo[MT_WOLFSS].droppeditem = MT_CLIP;
  mobjinfo[MT_POSSESSED].droppeditem = MT_CLIP;
  mobjinfo[MT_SHOTGUY].droppeditem = MT_SHOTGUN;
  mobjinfo[MT_CHAINGUY].droppeditem = MT_CHAINGUN;

  // [crispy] randomly mirrored death animations
  for (i = MT_PLAYER; i <= MT_KEEN; ++i)
  {
    switch (i)
    {
      case MT_FIRE:
      case MT_TRACER:
      case MT_SMOKE:
      case MT_FATSHOT:
      case MT_BRUISERSHOT:
      case MT_CYBORG:
        continue;
    }
    mobjinfo[i].flags_extra |= MFX_MIRROREDCORPSE;
  }

  mobjinfo[MT_PUFF].flags_extra |= MFX_MIRROREDCORPSE;
  mobjinfo[MT_BLOOD].flags_extra |= MFX_MIRROREDCORPSE;

  for (i = MT_MISC61; i <= MT_MISC69; ++i)
     mobjinfo[i].flags_extra |= MFX_MIRROREDCORPSE;

  mobjinfo[MT_DOGS].flags_extra |= MFX_MIRROREDCORPSE;

  for (i = S_SARG_RUN1; i <= S_SARG_PAIN2; ++i)
    states[i].flags |= STATEF_SKILL5FAST;

  P_InitAmbientSoundMobjInfo();
}

void D_SetMaxHealth(void)
{
  if (demo_compatibility)
  {
    maxhealth = 100;
    maxhealthbonus = deh_set_maxhealth ? deh_maxhealth : 200;
  }
  else
  {
    maxhealth = deh_set_maxhealth ? deh_maxhealth : 100;
    maxhealthbonus = maxhealth * 2;
  }
}

void D_SetBloodColor(void)
{
  if (deh_set_blood_color)
    return;

  if (STRICTMODE(colored_blood))
  {
    mobjinfo[MT_HEAD].bloodcolor = 3; // Blue
    mobjinfo[MT_BRUISER].bloodcolor = 2; // Green
    mobjinfo[MT_KNIGHT].bloodcolor = 2; // Green
  }
  else
  {
    mobjinfo[MT_HEAD].bloodcolor = 0;
    mobjinfo[MT_BRUISER].bloodcolor = 0;
    mobjinfo[MT_KNIGHT].bloodcolor = 0;
  }
}

// killough 2/22/98: Add support for ENDBOOM, which is PC-specific
// killough 8/1/98: change back to ENDOOM

typedef enum {
  ENDOOM_OFF,
  ENDOOM_PWAD_ONLY,
  ENDOOM_ALWAYS
} endoom_t;

boolean quit_prompt;
boolean quit_sound;
static endoom_t show_endoom;

static void D_ShowEndDoom(void)
{
  int lumpnum = W_CheckNumForName("ENDOOM");
  byte *endoom = W_CacheLumpNum(lumpnum, PU_STATIC);

  I_Endoom(endoom);
}

boolean fast_exit = false;

boolean D_AllowEndDoom(void)
{
  if (fast_exit)
  {
    return false; // Alt-F4 or pressed the close button.
  }

  if (show_endoom == ENDOOM_OFF)
  {
    return false; // ENDOOM disabled.
  }

  if (W_IsIWADLump(W_CheckNumForName("ENDOOM"))
      && show_endoom == ENDOOM_PWAD_ONLY)
  {
    return false; // User prefers PWAD ENDOOM only.
  }

  return true;
}

static void D_EndDoom(void)
{
  if (D_AllowEndDoom())
  {
    D_ShowEndDoom();
  }
}

// [FG] fast-forward demo to the desired map
int playback_warp = -1;

static int mainwadfile;

#define SET_DIR(a, b) \
    if ((a)) \
    { \
        free((a)); \
    } \
    (a) = (b);

void D_SetSavegameDirectory(void)
{
    // set save path to -save parm or current dir

    SET_DIR(screenshotdir, M_StringDuplicate("."));

    SET_DIR(basesavegame, M_StringDuplicate(D_DoomPrefDir()));

    //!
    // @arg <directory>
    //
    // Specify a path from which to load and save games. If the directory
    // does not exist then it will automatically be created.
    //

    int p = M_CheckParmWithArgs("-save", 1);
    if (p > 0)
    {
        SET_DIR(basesavegame, M_StringDuplicate(myargv[p + 1]));

        M_MakeDirectory(basesavegame);

        // [FG] fall back to -save parm
        SET_DIR(screenshotdir, M_StringDuplicate(basesavegame));
    }
    else
    {
        if (organize_savefiles == -1)
        {
            // [FG] check for at least one savegame in the old location
            glob_t *glob = I_StartMultiGlob(
            basesavegame, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.dsg");

            organize_savefiles = (I_NextGlob(glob) == NULL);

            I_EndGlob(glob);
        }

        if (organize_savefiles)
        {
            const char *wadname = wadfiles[0];

            for (int i = mainwadfile; i < array_size(wadfiles); i++)
            {
                if (FileContainsMaps(wadfiles[i]))
                {
                    wadname = wadfiles[i];
                    break;
                }
            }

            char *oldsavegame = basesavegame;
            basesavegame =
                M_StringJoin(oldsavegame, DIR_SEPARATOR_S, "savegames");
            free(oldsavegame);

            M_MakeDirectory(basesavegame);

            oldsavegame = basesavegame;
            basesavegame = M_StringJoin(oldsavegame, DIR_SEPARATOR_S,
                M_BaseName(wadname));
            free(oldsavegame);
        }
    }

    //!
    // @arg <directory>
    //
    // Specify a path to save screenshots. If the directory does not
    // exist then it will automatically be created.
    //

    p = M_CheckParmWithArgs("-shotdir", 1);
    if (p > 0)
    {
        SET_DIR(screenshotdir, M_StringDuplicate(myargv[p + 1]));

        M_MakeDirectory(screenshotdir);
    }

    I_Printf(VB_INFO, "Savegame directory: %s", basesavegame);
}

#undef SET_DIR

//
// D_DoomMain
//

void D_DoomMain(void)
{
  int p;

  setbuf(stdout,NULL);

  I_AtExitPrio(I_QuitFirst, true,  "I_QuitFirst", exit_priority_first);
  I_AtExitPrio(I_QuitLast,  false, "I_QuitLast",  exit_priority_last);
  I_AtExitPrio(I_Quit,      true,  "I_Quit",      exit_priority_last);

  I_AtExitPrio(I_ErrorMsg,  true,  "I_ErrorMsg",  exit_priority_verylast);

  I_UpdatePriority(true);

  // [FG] compose a proper command line from loose file paths passed as
  // arguments to allow for loading WADs and DEHACKED patches by drag-and-drop
  M_AddLooseFiles();

  //!
  //
  // Print command line help.
  //

  if (M_ParmExists("-help") || M_ParmExists("--help"))
  {
    M_PrintHelpString();
    I_SafeExit(0);
  }

  // [FG] initialize logging verbosity early to decide
  //      if the following lines will get printed or not

  I_InitPrintf();

  // Don't check undocumented options if -devparm is set
  if (!M_ParmExists("-devparm"))
  {
    M_CheckCommandLine();
  }

  FindResponseFile();         // Append response file arguments to command-line

  //!
  // @category net
  //
  // Start a dedicated server, routing packets but not participating
  // in the game itself.
  //

  if (M_CheckParm("-dedicated") > 0)
  {
      I_Printf(VB_INFO, "Dedicated server mode.");
      I_InitTimer();
      NET_DedicatedServer();

      // Never returns
  }

  // killough 10/98: set default savename based on executable's name
  sprintf(savegamename = malloc(16), "%.4ssav", D_DoomExeName());

  I_Printf(VB_INFO, "W_Init: Init WADfiles.");

  LoadBaseFile();

  IdentifyVersion();

  //!
  // @category mod
  //
  // Disable auto-loading of extars.wad file.
  //

  if (gamemission < pack_chex && !M_ParmExists("-noextras"))
  {
      char *path = D_FindWADByName("extras.wad");
      if (path)
      {
          D_AddFile(path);
          firstpwad = array_size(wadfiles);
      }
  }

  // [FG] emulate a specific version of Doom
  InitGameVersion();

  dsdh_InitTables();

  D_InitTables();

  modifiedgame = false;

  // killough 7/19/98: beta emulation option

  //!
  // @category game
  //
  // Press beta emulation mode (complevel mbf only).
  //

  beta_emulation = !!M_CheckParm("-beta");

  if (beta_emulation)
    { // killough 10/98: beta lost soul has different behavior frames
      mobjinfo[MT_SKULL].spawnstate   = S_BSKUL_STND;
      mobjinfo[MT_SKULL].seestate     = S_BSKUL_RUN1;
      mobjinfo[MT_SKULL].painstate    = S_BSKUL_PAIN1;
      mobjinfo[MT_SKULL].missilestate = S_BSKUL_ATK1;
      mobjinfo[MT_SKULL].deathstate   = S_BSKUL_DIE1;
      mobjinfo[MT_SKULL].damage       = 1;
    }
#ifdef MBF_STRICT
  // This code causes MT_SCEPTRE and MT_BIBLE to not spawn on the map,
  // which causes desync in Eviternity.wad demos.
  else
    mobjinfo[MT_SCEPTRE].doomednum = mobjinfo[MT_BIBLE].doomednum = -1;
#endif

  // jff 1/24/98 set both working and command line value of play parms

  //!
  // @category game
  // @vanilla
  // @help
  //
  // Disable monsters.
  //

  p = M_CheckParm("-nomonsters");

  if (!p)
  {
  //!
  // @category game
  // @help
  //
  // Alias to -nomonsters.
  //
    p = M_CheckParm("-nomo");
  }

  nomonsters = clnomonsters = p;

  //!
  // @category game
  // @vanilla
  //
  // Monsters respawn after being killed.
  //

  respawnparm = clrespawnparm = M_CheckParm ("-respawn");

  //!
  // @category game
  // @vanilla
  //
  // Monsters move faster.
  //

  fastparm = clfastparm = M_CheckParm ("-fast");
  // jff 1/24/98 end of set to both working and command line value

  //!
  // @category game
  // @help
  //
  // Enables automatic pistol starts on each level.
  //

  pistolstart = clpistolstart = M_CheckParm("-pistolstart");

  //!
  // @category game
  //
  // Start single player game with items spawns as in cooperative netgame.
  //

  coopspawns = clcoopspawns = M_CheckParm("-coop_spawns");

  //!
  // @vanilla
  //
  // Developer mode.
  //

  devparm = M_CheckParm ("-devparm");

  //!
  // @category net
  // @vanilla
  //
  // Start a deathmatch game.
  //

  if (M_CheckParm ("-deathmatch"))
    deathmatch = 1;

  //!
  // @category net
  // @vanilla
  //
  // Start a deathmatch 2.0 game. Weapons do not stay in place and
  // all items respawn after 30 seconds.
  //

  if (M_CheckParm ("-altdeath"))
    deathmatch = 2;

  //!
  // @category net
  // @vanilla
  //
  // Start a deathmatch 3.0 game.  Weapons stay in place and
  // all items respawn after 30 seconds.
  //

  if (M_CheckParm ("-dm3"))
    deathmatch = 3;

  if (devparm)
    I_Printf(VB_INFO, "%s", D_DEVSTR);

  //!
  // @category game
  // @arg <x>
  // @vanilla
  //
  // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
  // x defaults to 200.  Values are rounded up to 10 and down to 400.
  //

  if ((p=M_CheckParm ("-turbo")))
    {
      int scale = 200;

      if (p < myargc - 1 && myargv[p + 1][0] != '-')
        scale = M_ParmArgToInt(p);
      if (scale < 10)
        scale = 10;
      if (scale > 400)
        scale = 400;
      I_Printf(VB_INFO, "turbo scale: %i%%",scale);
      forwardmove[0] = forwardmove[0]*scale/100;
      forwardmove[1] = forwardmove[1]*scale/100;
      sidemove[0] = sidemove[0]*scale/100;
      sidemove[1] = sidemove[1]*scale/100;

      // Prevent ticcmd overflow.
      forwardmove[0] = MIN(forwardmove[0], SCHAR_MAX);
      forwardmove[1] = MIN(forwardmove[1], SCHAR_MAX);
      sidemove[0] = MIN(sidemove[0], SCHAR_MAX);
      sidemove[1] = MIN(sidemove[1], SCHAR_MAX);
    }

  if (beta_emulation)
    {
      char *path = D_FindWADByName("betagrph.wad");
      if (path == NULL)
      {
        I_Error("'BETAGRPH.WAD' is required for beta emulation! "
                "You can find it in the 'mbf.zip' archive at "
                "https://www.doomworld.com/idgames/source/mbf");
      }
      D_AddFile("betagrph.wad");
    }

  // add wad files from autoload IWAD directories before wads from -file parameter

  LoadIWadBase();
  PrepareAutoloadPaths();
  AutoloadIWadDir(AutoLoadWADs);

  // add any files specified on the command line with -file wadfile
  // to the wad list

  // killough 1/31/98, 5/2/98: reload hack removed, -wart same as -warp now.

  //!
  // @arg <files>
  // @vanilla
  // @help
  //
  // Load the specified PWAD files.
  //

  if ((p = M_CheckParm ("-file")))
    {
      // the parms after p are wadfile/lump names,
      // until end of parms or another - preceded parm
      // killough 11/98: allow multiple -file parameters

      boolean file = modifiedgame = true;            // homebrew levels
      mainwadfile = array_size(wadfiles);
      while (++p < myargc)
        if (*myargv[p] == '-')
          file = !strcasecmp(myargv[p],"-file");
        else
          if (file)
            D_AddFile(myargv[p]);
    }

  // add wad files from autoload PWAD directories

  LoadPWadBase();
  AutoloadPWadDir(AutoLoadWADs);

  // get skill / episode / map from parms

  startskill = sk_default; // jff 3/24/98 was sk_medium, just note not picked
  startepisode = 1;
  startmap = 1;
  autostart = false;

  //!
  // @category game
  // @arg <skill>
  // @vanilla
  // @help
  //
  // Set the game skill, 1-5 (1: easiest, 5: hardest). A skill of 0 disables all
  // monsters only in -complevel vanilla.
  //

  if ((p = M_CheckParm ("-skill")) && p < myargc-1)
   {
     startskill = M_ParmArgToInt(p);
     startskill--;
     if (startskill >= sk_none && startskill <= sk_nightmare)
      {
        autostart = true;
      }
     else
      {
        I_Error("Invalid parameter '%s' for -skill, valid values are 1-5 "
                "(1: easiest, 5: hardest).\n"
                "In complevel Vanilla, '-skill 0' disables all monsters.", myargv[p+1]);
      }
   }

  //!
  // @category game
  // @help
  //
  // Alias to -skill 4.
  //

  if (M_ParmExists("-uv"))
  {
    startskill = sk_hard;
    autostart = true;
  }

  //!
  // @category game
  // @help
  //
  // Alias to -skill 5.
  //

  if (M_ParmExists("-nm"))
  {
    startskill = sk_nightmare;
    autostart = true;
  }

  //!
  // @category game
  // @arg <n>
  // @vanilla
  //
  // Start playing on episode n (1-99)
  //

  if ((p = M_CheckParm ("-episode")) && p < myargc-1)
    {
      startepisode = M_ParmArgToInt(p);
      if (startepisode >= 1 && startepisode <= 99)
       {
         startmap = 1;
         autostart = true;
       }
      else
       {
          I_Error("Invalid parameter '%s' for -episode, valid values are 1-99.",
                  myargv[p+1]);
       }
    }

  //!
  // @arg <n>
  // @category net
  // @vanilla
  //
  // For multiplayer games: exit each level after n minutes.
  //

  if ((p = M_CheckParm ("-timer")) && p < myargc-1 && deathmatch)
    {
      timelimit = M_ParmArgToInt(p);
      I_Printf(VB_INFO, "Levels will end after %d minute%s.", timelimit, timelimit>1 ? "s" : "");
    }

  //!
  // @category net
  // @vanilla
  //
  // Austin Virtual Gaming: end levels after 20 minutes.
  //

  if ((p = M_CheckParm ("-avg")) && p < myargc-1 && deathmatch)
    {
    timelimit = 20;
    I_Printf(VB_INFO, "Austin Virtual Gaming: Levels will end after 20 minutes.");
    }

  //!
  // @category game
  // @arg <x> <y>|<xy>
  // @vanilla
  // @help
  //
  // Start a game immediately, warping to ExMy (Doom 1) or MAPxy (Doom 2).
  //

  if (((p = M_CheckParm ("-warp")) ||      // killough 5/2/98
       (p = M_CheckParm ("-wart"))) && p < myargc-1)
  {
    if (gamemode == commercial)
      {
        startmap = M_ParmArgToInt(p);
        autostart = true;
      }
    else    // 1/25/98 killough: fix -warp xxx from crashing Doom 1 / UD
      // [crispy] only if second argument is not another option
      if (p < myargc-2 && myargv[p+2][0] != '-')
        {
          startepisode = M_ParmArgToInt(p);
          startmap = M_ParmArg2ToInt(p);
          autostart = true;
        }
      // [crispy] allow second digit without space in between for Doom 1
      else
        {
          int em = M_ParmArgToInt(p);
          startepisode = em / 10;
          startmap = em % 10;
          autostart = true;
        }
    // [FG] fast-forward demo to the desired map
    playback_warp = startmap;
  }

  //jff 1/22/98 add command line parms to disable sound and music
  {
    //!
    // @vanilla
    //
    // Disable all sound output.
    //

    int nosound = M_CheckParm("-nosound");

    //!
    // @vanilla
    //
    // Disable music.
    //

    nomusicparm = nosound || M_CheckParm("-nomusic");

    //!
    // @vanilla
    //
    // Disable sound effects.
    //

    nosfxparm   = nosound || M_CheckParm("-nosfx");
  }
  //jff end of sound/music command line parms

  // killough 3/2/98: allow -nodraw generally

  //!
  // @category video
  // @vanilla
  //
  // Disable rendering the screen entirely.
  //

  nodrawers = M_CheckParm ("-nodraw");

  //!
  // @category video
  // @vanilla
  //
  // Disable blitting the screen.
  //

  noblit = M_CheckParm ("-noblit");

  M_InitConfig();

  I_PutChar(VB_INFO, '\n');

  M_LoadDefaults();  // load before initing other systems

  bodyquesize = default_bodyquesize; // killough 10/98

  // 1/18/98 killough: Z_Init call moved to i_main.c

  // init subsystems

  W_InitMultipleFiles();

  // Check for wolf levels
  haswolflevels = (W_CheckNumForName("map31") >= 0);

  // process deh in IWAD

  //!
  // @category mod
  //
  // Avoid loading DEHACKED lumps embedded into WAD files.
  //

  if (!M_ParmExists("-nodeh"))
  {
    W_ProcessInWads("DEHACKED", ProcessDehLump, PROCESS_IWAD);
  }

  // process .deh files specified on the command line with -deh or -bex.
  D_ProcessDehCommandLine();

  // process deh in wads and .deh files from autoload directory
  // before deh in wads from -file parameter
  AutoloadIWadDir(AutoLoadPatches);

  // killough 10/98: now process all deh in wads
  if (!M_ParmExists("-nodeh"))
  {
    W_ProcessInWads("DEHACKED", ProcessDehLump, PROCESS_PWAD);
  }

  // process .deh files from PWADs autoload directories
  AutoloadPWadDir(AutoLoadPatches);

  PostProcessDeh();

  W_ProcessInWads("BRGHTMPS", R_ParseBrightmaps, PROCESS_PWAD);

  // Moved after WAD initialization because we are checking the COMPLVL lump
  G_ReloadDefaults(false); // killough 3/4/98: set defaults just loaded.
  // jff 3/24/98 this sets startskill if it was -1

  // Check for -file in shareware
  if (modifiedgame)
    {
      // These are the lumps that will be checked in IWAD,
      // if any one is not present, execution will be aborted.
      static const char name[23][8]= {
        "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
        "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
        "dphoof","bfgga0","heada1","cybra1","spida1d1" };
      int i;

      if (gamemode == shareware)
        I_Error("\nYou cannot -file with the shareware version. Register!");

      // Check for fake IWAD with right name,
      // but w/o all the lumps of the registered version.
      if (gamemode == registered)
        for (i = 0;i < 23; i++)
          if (W_CheckNumForName(name[i])<0 &&
              (W_CheckNumForName)(name[i],ns_sprites)<0) // killough 4/18/98
            I_Error("\nThis is not the registered version.");
    }

  //!
  // @category mod
  //
  // Disable UMAPINFO loading.
  //

  if (!M_ParmExists("-nomapinfo"))
  {
    W_ProcessInWads("UMAPINFO", G_ParseMapInfo, PROCESS_IWAD | PROCESS_PWAD);
  }

  G_ParseCompDatabase();

  D_SetSavegameDirectory();

  V_InitColorTranslation(); //jff 4/24/98 load color translation lumps

  // killough 2/22/98: copyright / "modified game" / SPA banners removed

  // Ty 04/08/98 - Add 5 lines of misc. data, only if nonblank
  // The expectation is that these will be set in a .bex file
  if (*startup1) I_Printf(VB_INFO, "%s", startup1);
  if (*startup2) I_Printf(VB_INFO, "%s", startup2);
  if (*startup3) I_Printf(VB_INFO, "%s", startup3);
  if (*startup4) I_Printf(VB_INFO, "%s", startup4);
  if (*startup5) I_Printf(VB_INFO, "%s", startup5);
  // End new startup strings

  //!
  // @category game
  // @arg <ps>
  // @vanilla
  //
  // Load the game on page p (0-7) in slot s (0-7). Use 255 to load an auto save.
  //

  p = M_CheckParmWithArgs("-loadgame", 1);
  if (p)
  {
    startloadgame = M_ParmArgToInt(p);
  }
  else
  {
    // Not loading a game
    startloadgame = -1;
  }

  // Allows PWAD HELP2 screen for DOOM 1 wads (using Ultimate Doom IWAD).
  pwad_help2 = gamemode == retail && W_IsWADLump(W_CheckNumForName("HELP2"));

  W_ProcessInWads("SNDINFO", S_ParseSndInfo, PROCESS_IWAD | PROCESS_PWAD);

  W_ProcessInWads("TRAKINFO", S_ParseTrakInfo, PROCESS_IWAD | PROCESS_PWAD);
  D_SetupDemoLoop();

  I_Printf(VB_INFO, "M_Init: Init miscellaneous info.");
  M_Init();

  I_Printf(VB_INFO, "R_Init: Init DOOM refresh daemon - ");
  R_Init();

  I_Printf(VB_INFO, "P_Init: Init Playloop state.");
  P_Init();

  I_Printf(VB_INFO, "I_Init: Setting up machine state.");
  I_InitTimer();
  I_InitGamepad();
  I_InitSound();
  I_InitMusic();

  I_Printf(VB_INFO, "NET_Init: Init network subsystem.");
  NET_Init();

  // Initial netgame startup. Connect to server etc.
  D_ConnectNetGame();

  I_Printf(VB_INFO, "D_CheckNetGame: Checking network game status.");
  D_CheckNetGame();

  G_UpdateSideMove();
  G_UpdateAngleFunctions();
  G_UpdateLocalViewFunction();
  G_UpdateGamepadVariables();
  G_UpdateMouseVariables();
  R_UpdateViewAngleFunction();
  WS_Init();

  G_SetTimeScale();

  I_Printf(VB_INFO, "S_Init: Setting up sound.");
  S_Init(snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/ );

  I_Printf(VB_INFO, "HU_Init: Setting up heads up display.");

  I_Printf(VB_INFO, "ST_Init: Init status bar.");
  ST_Init();
  MN_SetHUFontKerning();

  // andrewj: voxel support
  I_Printf(VB_INFO, "VX_Init: ");
  VX_Init();

  I_PutChar(VB_INFO, '\n');

  idmusnum = -1; //jff 3/17/98 insure idmus number is blank

  // check for a driver that wants intermission stats
  // [FG] replace with -statdump implementation from Chocolate Doom
  if ((p = M_CheckParm ("-statdump")) && p<myargc-1)
    {
      I_AtExit(StatDump, true);
      I_Printf(VB_INFO, "External statistics registered.");
    }

  //!
  // @arg <min:sec>
  // @category demo
  //
  // Skip min:sec time during viewing of the demo.
  // "-warp <x> -skipsec <min:sec>" will skip min:sec time on level x.
  //

  p = M_CheckParmWithArgs("-skipsec", 1);
  if (p)
    {
      float min, sec;

      if (sscanf(myargv[p+1], "%f:%f", &min, &sec) == 2)
      {
        playback_skiptics = (int) ((60 * min + sec) * TICRATE);
      }
      else if (sscanf(myargv[p+1], "%f", &sec) == 1)
      {
        playback_skiptics = (int) (sec * TICRATE);
      }
      else
      {
        I_Error("Invalid parameter '%s' for -skipsec, should be min:sec", myargv[p+1]);
      }
    }

  // start the apropriate game based on parms

  // killough 12/98: 
  // Support -loadgame with -record and reimplement -recordfrom.

  //!
  // @arg <s> <demo>
  // @category demo
  //
  // Record a demo, loading from the given save slot. Equivalent to -loadgame
  // <s> -record <demo>.
  //

  p = M_CheckParmWithArgs("-recordfrom", 2);
  if (p)
  {
    startloadgame = M_ParmArgToInt(p);
    G_RecordDemo(myargv[p + 2]);
  }
  else
  {
    //!
    // @arg <demo1> <demo2>
    // @category demo
    //
    // Allows continuing <demo1> after it ends or when the user presses the join
    // demo key, the result getting saved as <demo2>. Equivalent to -playdemo
    // <demo1> -record <demo2>.
    //

    p = M_CheckParmWithArgs("-recordfromto", 2);
    if (p)
    {
      G_DeferedPlayDemo(myargv[p + 1]);
      singledemo = true;              // quit after one demo
      G_RecordDemo(myargv[p + 2]);
    }
    else
    {
      p = M_CheckParmWithArgs("-loadgame", 1);
      if (p)
        startloadgame = M_ParmArgToInt(p);

      //!
      // @arg <demo>
      // @category demo
      // @vanilla
      // @help
      //
      // Record a demo named demo.lmp.
      //

      if ((p = M_CheckParm("-record")) && ++p < myargc)
	{
	  autostart = true;
	  G_RecordDemo(myargv[p]);
	}
    }
  }

  //!
  // @arg <demo>
  // @category demo
  //
  // Plays the given demo as fast as possible.
  //

  if ((p = M_CheckParm("-fastdemo")) && ++p < myargc)
  {                      // killough
      fastdemo = true;   // run at fastest speed possible
      timingdemo = true; // show stats after quit
      G_DeferedPlayDemo(myargv[p]);
      singledemo = true; // quit after one demo
  }

  //!
  // @arg <demo>
  // @category demo
  // @vanilla
  //
  // Play back the demo named demo.lmp, determining the framerate
  // of the screen.
  //

  else if ((p = M_CheckParm("-timedemo")) && ++p < myargc)
  {
      singletics = true;
      timingdemo = true; // show stats after quit
      G_DeferedPlayDemo(myargv[p]);
      singledemo = true; // quit after one demo
  }

  //!
  // @arg <demo>
  // @category demo
  // @vanilla
  // @help
  //
  // Play back the demo named demo.lmp.
  //

  else if ((p = M_CheckParm("-playdemo")) && ++p < myargc)
  {
      G_DeferedPlayDemo(myargv[p]);
      singledemo = true; // quit after one demo
  }
  else
  {
      // [FG] no demo playback
      playback_warp = -1;
      playback_skiptics = 0;
  }

  if (fastdemo)
  {
    I_SetFastdemoTimer(true);
  }

  // [FG] init graphics (video.widedelta) before HUD widgets
  I_InitGraphics();
  I_InitKeyboard();

  MN_InitMenuStrings();
  MN_InitFreeLook();

  // Auto save slot is 255 for -loadgame command.
  if (startloadgame == 255 && !demorecording && gameaction != ga_playdemo
      && !netgame)
  {
    char *file = G_AutoSaveName();
    G_LoadAutoSave(file, true);
    free(file);
  }
  else if (startloadgame >= 0 && startloadgame <= 77) // Page 0-7, slot 0-7.
  {
    char *file;
    file = G_SaveGameName(startloadgame);
    G_LoadGame(file, startloadgame, true); // killough 5/15/98: add command flag
    free(file);
  }
  else
    if (!singledemo)                    // killough 12/98
    {
      if (autostart || netgame)
	{
	  G_InitNew(startskill, startepisode, startmap);
	  // [crispy] no need to write a demo header in demo continue mode
	  if (demorecording && gameaction != ga_playdemo)
	    G_BeginRecording();
	}
      else
	D_StartTitle();                 // start up intro loop
    }

  // killough 12/98: inlined D_DoomLoop

  if (!demorecording)
  {
    I_AtExitPrio(D_EndDoom, false, "D_EndDoom", exit_priority_last);
  }

  TryRunTics();

  D_StartGameLoop();

  for (;;)
    {
      // frame syncronous IO operations
      I_StartFrame ();

      TryRunTics (); // will run at least one tic

      // Update display, next frame, with current state.
      if (screenvisible)
        D_Display();
    }
}

void D_BindMiscVariables(void)
{
  BIND_BOOL_GENERAL(quit_prompt, true, "Show quit prompt");
  BIND_BOOL_GENERAL(quit_sound, false, "Play quit sound");
  BIND_NUM_GENERAL(show_endoom, ENDOOM_OFF, ENDOOM_OFF, ENDOOM_ALWAYS,
    "Show ENDOOM screen (0 = Off; 1 = PWAD Only; 2 = Always)");
  BIND_BOOL_GENERAL(demobar, false, "Show demo progress bar");
  BIND_NUM_GENERAL(screen_melt, wipe_Melt, wipe_None, wipe_Fizzle,
    "Screen wipe effect (0 = None; 1 = Melt; 2 = Crossfade; 3 = Fizzlefade)");
  BIND_BOOL_GENERAL(palette_changes, true, "Palette changes when taking damage or picking up items");
  BIND_NUM_GENERAL(organize_savefiles, -1, -1, 1,
    "Organize save files");
  M_BindStr("net_player_name", &net_player_name, DEFAULT_PLAYER_NAME, wad_no,
    "Network setup player name");

  M_BindBool("colored_blood", &colored_blood, NULL, false, ss_enem, wad_no,
             "Allow colored blood");
}

//----------------------------------------------------------------------------
//
// $Log: d_main.c,v $
// Revision 1.47  1998/05/16  09:16:51  killough
// Make loadgame checksum friendlier
//
// Revision 1.46  1998/05/12  10:32:42  jim
// remove LEESFIXES from d_main
//
// Revision 1.45  1998/05/06  15:15:46  jim
// Documented IWAD routines
//
// Revision 1.44  1998/05/03  22:26:31  killough
// beautification, declarations, headers
//
// Revision 1.43  1998/04/24  08:08:13  jim
// Make text translate tables lumps
//
// Revision 1.42  1998/04/21  23:46:01  jim
// Predefined lump dumper option
//
// Revision 1.39  1998/04/20  11:06:42  jim
// Fixed print of IWAD found
//
// Revision 1.37  1998/04/19  01:12:19  killough
// Fix registered check to work with new lump namespaces
//
// Revision 1.36  1998/04/16  18:12:50  jim
// Fixed leak
//
// Revision 1.35  1998/04/14  08:14:18  killough
// Remove obsolete adaptive_gametics code
//
// Revision 1.34  1998/04/12  22:54:41  phares
// Remaining 3 Setup screens
//
// Revision 1.33  1998/04/11  14:49:15  thldrmn
// Allow multiple deh/bex files
//
// Revision 1.32  1998/04/10  06:31:50  killough
// Add adaptive gametic timer
//
// Revision 1.31  1998/04/09  09:18:17  thldrmn
// Added generic startup strings for BEX use
//
// Revision 1.30  1998/04/06  04:52:29  killough
// Allow demo_insurance=2, fix fps regression wrt redrawsbar
//
// Revision 1.29  1998/03/31  01:08:11  phares
// Initial Setup screens and Extended HELP screens
//
// Revision 1.28  1998/03/28  15:49:37  jim
// Fixed merge glitches in d_main.c and g_game.c
//
// Revision 1.27  1998/03/27  21:26:16  jim
// Default save dir offically . now
//
// Revision 1.26  1998/03/25  18:14:21  jim
// Fixed duplicate IWAD search in .
//
// Revision 1.25  1998/03/24  16:16:00  jim
// Fixed looking for wads message
//
// Revision 1.23  1998/03/24  03:16:51  jim
// added -iwad and -save parms to command line
//
// Revision 1.22  1998/03/23  03:07:44  killough
// Use G_SaveGameName, fix some remaining default.cfg's
//
// Revision 1.21  1998/03/18  23:13:54  jim
// Deh text additions
//
// Revision 1.19  1998/03/16  12:27:44  killough
// Remember savegame slot when loading
//
// Revision 1.18  1998/03/10  07:14:58  jim
// Initial DEH support added, minus text
//
// Revision 1.17  1998/03/09  07:07:45  killough
// print newline after wad files
//
// Revision 1.16  1998/03/04  08:12:05  killough
// Correctly set defaults before recording demos
//
// Revision 1.15  1998/03/02  11:24:25  killough
// make -nodraw -noblit work generally, fix ENDOOM
//
// Revision 1.14  1998/02/23  04:13:55  killough
// My own fix for m_misc.c warning, plus lots more (Rand's can wait)
//
// Revision 1.11  1998/02/20  21:56:41  phares
// Preliminarey sprite translucency
//
// Revision 1.10  1998/02/20  00:09:00  killough
// change iwad search path order
//
// Revision 1.9  1998/02/17  06:09:35  killough
// Cache D_DoomExeDir and support basesavegame
//
// Revision 1.8  1998/02/02  13:20:03  killough
// Ultimate Doom, -fastdemo -nodraw -noblit support, default_compatibility
//
// Revision 1.7  1998/01/30  18:48:15  phares
// Changed textspeed and textwait to functions
//
// Revision 1.6  1998/01/30  16:08:59  phares
// Faster end-mission text display
//
// Revision 1.5  1998/01/26  19:23:04  phares
// First rev with no ^Ms
//
// Revision 1.4  1998/01/26  05:40:12  killough
// Fix Doom 1 crashes on -warp with too few args
//
// Revision 1.3  1998/01/24  21:03:04  jim
// Fixed disappearence of nomonsters, respawn, or fast mode after demo play or IDCLEV
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
