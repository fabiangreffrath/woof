//
//  Copyright (C) 2026 Roman Fomin
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

#include "params_table.h"
#include "m_argv.h"
#include "i_printf.h"

#include <stdio.h>

const param_def_t param_table[] = {
    { "-help", TypeFlag, SectionGeneral,
      "Print command line help and exit." },
    { "--help", TypeFlag, SectionGeneral,
      "Print command line help and exit." },
    { "-version", TypeFlag, SectionGeneral,
      "Print version information and exit." },
    { "-generate-doc", TypeOptArg, SectionGeneral,
      "Generate markdown documentation of command line parameters. Optional: [file] (default: cmdline.md)" },
    { "-devparm", TypeFlag, SectionDebug,
      "Developer mode. Enables extra debug output and features." },
    { "-beta", TypeFlag, SectionDebug,
      "Beta emulation mode. Requires betagrph.wad." },
    { "-config", TypeString, SectionGeneral,
      "Specify a configuration file to load instead of the default. Argument: <file>" },
    { "-nooptions", TypeFlag, SectionGeneral,
      "Do not load the default configuration file." },
    { "-file", TypeFileList, SectionGeneral,
      "Load the specified PWAD files. Multiple files can be specified. Arguments: <files>..." },
    { "-iwad", TypeString, SectionGeneral,
      "Specify an IWAD file to use. Argument: <file>" },
    { "-save", TypeString, SectionGeneral,
      "Specify a path from which to load and save games. If the directory does not exist then it will automatically be created. Argument: <directory>" },
    { "-shotdir", TypeString, SectionGeneral,
      "Specify a path to save screenshots. If the directory does not exist then it will automatically be created. Argument: <directory>" },

    { "-skill", TypeInt, SectionGame,
      "Set the game skill, 1-5 (1: easiest, 5: hardest). A skill of 0 disables all monsters only in -complevel vanilla. Argument: <skill>" },
    { "-uv", TypeFlag, SectionGame,
      "Alias to -skill 4 (Ultra-Violence)." },
    { "-nm", TypeFlag, SectionGame,
      "Alias to -skill 5 (Nightmare)." },
    { "-episode", TypeInt, SectionGame,
      "Start playing on episode n (1-99). Argument: <n>" },
    { "-warp", TypeOneOrTwoArgs, SectionGame,
      "Start a game immediately, warping to ExMy (Doom 1) or MAPxy (Doom 2). Arguments: <x> <y>|<xy>" },
    { "-wart", TypeOneOrTwoArgs, SectionGame,
      "Alias for -warp." },
    { "-nomonsters", TypeFlag, SectionGame,
      "Disable monsters." },
    { "-nomo", TypeFlag, SectionGame,
      "Alias for -nomonsters." },
    { "-respawn", TypeFlag, SectionGame,
      "Monsters respawn after being killed." },
    { "-fast", TypeFlag, SectionGame,
      "Monsters move faster." },
    { "-pistolstart", TypeFlag, SectionGame,
      "Enables automatic pistol starts on each level." },
    { "-coop_spawns", TypeFlag, SectionGame,
      "Start single player game with items spawns as in cooperative netgame." },
    { "-dog", TypeFlag, SectionGame,
      "Enable dog companion (MBF feature)." },
    { "-dogs", TypeOptArg, SectionGame,
      "Enable dog companion with optional count (MBF feature). Optional argument: [n]" },
    { "-turbo", TypeOptArg, SectionGame,
      "Turbo mode. The player's speed is multiplied by x%. If unspecified, x defaults to 200. Values are rounded up to 10 and down to 400. Optional argument: [%]" },
    { "-loadgame", TypeInt, SectionGame,
      "Load the game on page p (0-7) in slot s (0-7). Use 255 to load an auto save. Argument: <ps>" },
    { "-chain_episodes", TypeFlag, SectionGame,
      "Chain episodes together in single player." },
    { "-gameversion", TypeString, SectionCompat,
      "Emulate a specific version of Doom. Valid values are \"1.9\", \"ultimate\", \"final\", \"final2\", \"chex\". Requires -complevel vanilla. Argument: <version>" },

    { "-server", TypeFlag, SectionNet,
      "Start a multiplayer server, listening for connections." },
    { "-dedicated", TypeFlag, SectionNet,
      "Start a dedicated server, routing packets but not participating in the game itself." },
    { "-connect", TypeString, SectionNet,
      "Connect to a multiplayer server running on the given address. Argument: <address>" },
    { "-autojoin", TypeFlag, SectionNet,
      "Automatically search the local LAN for a multiplayer server and join it." },
    { "-privateserver", TypeFlag, SectionNet,
      "When running a server, don't register with the global master server. Implies -server." },
    { "-solo-net", TypeFlag, SectionNet,
      "Run a solo network game (for testing)." },
    { "-dup", TypeInt, SectionNet,
      "Duplicate packets for network testing. Argument: <n>" },
    { "-extratics", TypeInt, SectionNet,
      "Extra tics for network testing. Argument: <n>" },
    { "-port", TypeInt, SectionNet,
      "Specify the network port to use. Argument: <port>" },
    { "-servername", TypeString, SectionNet,
      "Set the server name for multiplayer. Argument: <name>" },
    { "-timer", TypeInt, SectionNet,
      "For multiplayer games: exit each level after n minutes. Argument: <n>" },
    { "-avg", TypeFlag, SectionNet,
      "Austin Virtual Gaming: end levels after 20 minutes." },
    { "-deathmatch", TypeFlag, SectionNet,
      "Start a deathmatch game." },
    { "-altdeath", TypeFlag, SectionNet,
      "Start a deathmatch 2.0 game. Weapons do not stay in place and all items respawn after 30 seconds." },
    { "-dm3", TypeFlag, SectionNet,
      "Start a deathmatch 3.0 game. Weapons stay in place and all items respawn after 30 seconds." },
    { "-norollback", TypeFlag, SectionNet,
      "Disable rollback netcode." },
    { "-left", TypeFlag, SectionNet,
      "Set player to left split-screen." },
    { "-right", TypeFlag, SectionNet,
      "Set player to right split-screen." },
    { "-drone", TypeFlag, SectionNet,
      "Run as a drone (no display)." },
    { "-nodes", TypeInt, SectionNet,
      "Number of nodes for network game. Argument: <n>" },
    { "-frags", TypeInt, SectionNet,
      "Frag limit for deathmatch. Argument: <n>" },

    { "-playdemo", TypeString, SectionDemo,
      "Play back the demo named demo.lmp. Argument: <demo>" },
    { "-record", TypeString, SectionDemo,
      "Record a demo named demo.lmp. Argument: <demo>" },
    { "-recordfrom", TypeTwoArgs, SectionDemo,
      "Record a demo, loading from the given save slot. Equivalent to -loadgame <slot> -record <demo>. Arguments: <slot> <demo>" },
    { "-recordfromto", TypeTwoArgs, SectionDemo,
      "Allows continuing <demo1> after it ends or when the user presses the join demo key, the result getting saved as <demo2>. Equivalent to -playdemo <demo1> -record <demo2>. Arguments: <demo1> <demo2>" },
    { "-fastdemo", TypeString, SectionDemo,
      "Plays the given demo as fast as possible. Argument: <demo>" },
    { "-timedemo", TypeString, SectionDemo,
      "Play back the demo named demo.lmp, determining the framerate of the screen. Argument: <demo>" },
    { "-levelstat", TypeFlag, SectionDemo,
      "Write level statistics upon exit to levelstat.txt." },
    { "-shorttics", TypeFlag, SectionDemo,
      "Play with low turning resolution to emulate demo recording." },
    { "-longtics", TypeFlag, SectionDemo,
      "Use high turning resolution for demo recording." },
    { "-tas", TypeFlag, SectionDemo,
      "Lifts strict mode restrictions according to DSDA rules." },
    { "-skipsec", TypeString, SectionDemo,
      "Skip min:sec time during viewing of the demo. \"-warp <x> -skipsec <min:sec>\" will skip min:sec time on level x. Argument: <min:sec>" },
    { "-statdump", TypeOptArg, SectionDemo,
      "Dump statistics to file. Use \"-\" for stdout. Optional argument: [file]" },
    { "-maxdemo", TypeInt, SectionDemo,
      "Maximum demo size in bytes. Argument: <size>" },
    { "-dumptranmap", TypeTwoArgs, SectionDemo,
      "Dump translation map to file. Arguments: <file> <map>" },

    { "-complevel", TypeString, SectionCompat,
      "Emulate a specific version of Doom/Boom/MBF. Valid values are \"vanilla\", \"boom\", \"mbf\", \"mbf21\". Argument: <version>" },
    { "-cl", TypeString, SectionCompat,
      "Alias for -complevel." },
    { "-nocheats", TypeFlag, SectionCompat,
      "Disable cheat codes." },
    { "-force_old_zdoom_nodes", TypeFlag, SectionCompat,
      "Force use of old ZDoom node builder." },

    { "-fullscreen", TypeFlag, SectionVideo,
      "Start in fullscreen mode." },
    { "-window", TypeFlag, SectionVideo,
      "Start in windowed mode." },
    { "-borderless", TypeFlag, SectionVideo,
      "Start in borderless windowed mode." },
    { "-noblit", TypeFlag, SectionVideo,
      "Disable blitting the screen." },
    { "-nodraw", TypeFlag, SectionVideo,
      "Disable rendering the screen entirely." },
    { "-nograbmouse", TypeFlag, SectionVideo,
      "Don't grab mouse cursor." },
    { "-uncapped", TypeFlag, SectionVideo,
      "Uncapped framerate mode." },
    { "-nouncapped", TypeFlag, SectionVideo,
      "Disable uncapped framerate mode." },
    { "-1", TypeFlag, SectionVideo,
      "Set render scale factor 1x." },
    { "-2", TypeFlag, SectionVideo,
      "Set render scale factor 2x." },
    { "-3", TypeFlag, SectionVideo,
      "Set render scale factor 3x." },

    { "-deh", TypeFileList, SectionMod,
      "Load the given dehacked/bex patch(es). Arguments: <files>..." },
    { "-nodeh", TypeFlag, SectionMod,
      "Avoid loading DEHACKED lumps embedded into WAD files." },
    { "-noautoload", TypeFlag, SectionMod,
      "Disable auto-loading of .wad and .deh files." },
    { "-nomapinfo", TypeFlag, SectionMod,
      "Disable UMAPINFO loading." },
    { "-blockmap", TypeFlag, SectionMod,
      "Rebuild blockmap." },
    { "-bsp", TypeFlag, SectionMod,
      "Rebuild BSP." },
    { "-reject_pad_with_ff", TypeFlag, SectionMod,
      "Pad reject map with 0xFF." },
    { "-tranmap", TypeFlag, SectionMod,
      "Force translation map rebuild." },

    { "-setmem", TypeInt, SectionDebug,
      "Set memory allocation limit in MB. Argument: <MB>" },
    { "-nogui", TypeFlag, SectionDebug,
      "Disable GUI error dialogs." },
    { "-spechit", TypeInt, SectionDebug,
      "Spechit overflow emulation. Argument: <n>" },
    { "-quiet", TypeFlag, SectionDebug,
      "Suppress non-error output." },
    { "-verbose", TypeFlag, SectionDebug,
      "Enable verbose output." },
};

const int param_table_size = arrlen(param_table);

static const char *section_names[] = {
    [SectionGeneral] = "General options",
    [SectionGame] = "Game start options",
    [SectionNet] = "Networking options",
    [SectionDemo] = "Demo options",
    [SectionCompat] = "Compatibility",
    [SectionVideo] = "Video options",
    [SectionMod] = "Dehacked and WAD merging",
    [SectionDebug] = "Debug options",
};

const char *M_GetParamHelpString(void)
{
    static char buffer[16384];
    char *p = buffer;
    int remaining = sizeof(buffer);
    param_section_t current_section = -1;

    p += snprintf(p, remaining, "Usage: woof [options]\n\n");
    remaining = sizeof(buffer) - (p - buffer);

    for (int i = 0; i < param_table_size; i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            p += snprintf(p, remaining, "\n%s:\n", section_names[current_section]);
            remaining = sizeof(buffer) - (p - buffer);
        }

        p += snprintf(p, remaining, "  %s  %s\n", param->name, param->help);
        remaining = sizeof(buffer) - (p - buffer);
    }

    p += snprintf(p, remaining, "\nSee the wiki for a complete list of supported command line options.\n");
    return buffer;
}

void M_PrintParamHelp(void)
{
    I_Printf(VB_ALWAYS, "%s", M_GetParamHelpString());
}

void M_GenerateParamMarkdown(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        I_Printf(VB_ERROR, "Failed to open %s for writing", filename);
        return;
    }

    fprintf(f, "# Command Line Parameters\n\n");
    fprintf(f, "This document is auto-generated from the source code.\n\n");

    param_section_t current_section = -1;

    for (int i = 0; i < param_table_size; i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            fprintf(f, "## %s\n\n", section_names[current_section]);
        }

        fprintf(f, "### `%s`\n\n", param->name);
        fprintf(f, "%s\n\n", param->help);
        fprintf(f, "---\n\n");
    }

    fclose(f);
    I_Printf(VB_INFO, "Generated markdown documentation: %s", filename);
}