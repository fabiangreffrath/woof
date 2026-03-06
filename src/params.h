
static const char *params[] = {
"-devparm",
"-help",
"-nomusic",
"-nosfx",
"-nosound",
"-quiet",
"-verbose",
"-version",
"-beta",
"-coop_spawns",
"-dog",
"-fast",
"-nm",
"-nomo",
"-nomonsters",
"-pistolstart",
"-respawn",
"-uv",
"-1",
"-2",
"-3",
"-fullscreen",
"-noblit",
"-nodraw",
"-nograbmouse",
"-nouncapped",
"-uncapped",
"-window",
"-altdeath",
"-autojoin",
"-avg",
"-deathmatch",
"-dedicated",
"-dm3",
"-left",
"-oldsync",
"-privateserver",
"-right",
"-server",
"-solo-net",
"-blockmap",
"-bsp",
"-force_old_zdoom_nodes",
"-noautoload",
"-nocheats",
"-nodeh",
"-noextras",
"-nomapinfo",
"-nooptions",
"-reject_pad_with_ff",
"-tranmap",
"-levelstat",
"-longtics",
"-shorttics",
"-tas",
"-nogui",
};

static const char *params_with_args[] = {
"-config",
"-file",
"-iwad",
"-save",
"-shotdir",
"-dogs",
"-episode",
"-loadgame",
"-skill",
"-speed",
"-turbo",
"-warp",
"-connect",
"-dup",
"-extratics",
"-frags",
"-nodes",
"-port",
"-servername",
"-timer",
"-deh",
"-dumptranmap",
"-fastdemo",
"-maxdemo",
"-playdemo",
"-record",
"-recordfrom",
"-recordfromto",
"-skipsec",
"-timedemo",
"-cl",
"-complevel",
"-gameversion",
"-setmem",
"-spechit",
"-statdump",
};

#define HELP_STRING "Usage: woof [options] \n\
\n\
General options: \n\
  -file <files>  Load the specified PWAD files.\n\
  -iwad <file>   Specify an IWAD file to use.\n\
\n\
Game start options: \n\
  -nm                 Alias to -skill 5.\n\
  -nomo               Alias to -nomonsters.\n\
  -nomonsters         Disable monsters.\n\
  -pistolstart        Enables automatic pistol starts on each level.\n\
  -skill <skill>      Set the game skill, 1-5 (1: easiest, 5: hardest). A\n\
                      skill of 0 disables all monsters only in -complevel\n\
                      vanilla.\n\
  -uv                 Alias to -skill 4.\n\
  -warp <x> <y>|<xy>  Start a game immediately, warping to ExMy (Doom 1) or\n\
                      MAPxy (Doom 2).\n\
\n\
Networking options: \n\
  -autojoin           Automatically search the local LAN for a multiplayer\n\
                      server and join it.\n\
  -connect <address>  Connect to a multiplayer server running on the given\n\
                      address.\n\
  -privateserver      When running a server, don't register with the global\n\
                      master server. Implies -server.\n\
  -server             Start a multiplayer server, listening for connections.\n\
\n\
Dehacked and WAD merging: \n\
  -deh <files>  Load the given dehacked/bex patch(es).\n\
\n\
Demo options: \n\
  -levelstat        Write level statistics upon exit to levelstat.txt\n\
  -playdemo <demo>  Play back the demo named demo.lmp.\n\
  -record <demo>    Record a demo named demo.lmp.\n\
  -shorttics        Play with low turning resolution to emulate demo\n\
                    recording.\n\
  -tas              Lifts strict mode restrictions according to DSDA rules.\n\
\n\
Compatibility: \n\
  -cl <version>         Alias to -complevel.\n\
  -complevel <version>  Emulate a specific version of Doom/Boom/MBF. Valid\n\
                        values are \"vanilla\", \"boom\", \"mbf\", \"mbf21\".\n\
\n\
See CMDLINE.txt for a complete list of supported command line options.\n"
