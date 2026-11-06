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

#include "m_params.h"

#include "i_printf.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"

#include <stdio.h>

static const param_def_t param_table[] = {
    { "-help", TypeFlag, SectionGeneral,
      "Print command line help and exit." },
    { "--help", TypeFlag, SectionGeneral,
      "Print command line help and exit." },
    { "-version", TypeFlag, SectionGeneral,
      "Print version information and exit." },
    { "-doc", TypeOptArg, SectionGeneral,
      "Generate markdown documentation of command line parameters. "
      "Optional: [file] (default: cmdline.md)" },
    { "-beta", TypeFlag, SectionGeneral,
      "Beta emulation mode. Requires betagrph.wad." },
    { "-config", TypeString, SectionGeneral,
      "Load main configuration from the specified file, instead of the default." },
    { "-nooptions", TypeFlag, SectionGeneral,
      "Do not load the default configuration file." },
    { "-file", TypeFileList, SectionGeneral,
      "Load the specified PWAD files." },
    { "-iwad", TypeString, SectionGeneral,
      "Specify an IWAD file to use." },
    { "-save", TypeString, SectionGeneral,
      "Specify a path from which to load and save games. "
      "If the directory does not exist then it will automatically be created." },
    { "-shotdir", TypeString, SectionGeneral,
      "Specify a path to save screenshots. "
      "If the directory does not exist then it will automatically be created." },

    { "-skill", TypeInt, SectionGame,
      "Set the game skill, 1-5 (1: easiest, 5: hardest). "
      "A skill of 0 disables all monsters only in -complevel vanilla." },
    { "-uv", TypeFlag, SectionGame,
      "Alias to -skill 4 (Ultra-Violence)." },
    { "-nm", TypeFlag, SectionGame,
      "Alias to -skill 5 (Nightmare)." },
    { "-episode", TypeInt, SectionGame,
      "Start playing on episode n (1-99)." },
    { "-warp", TypeOneOrTwoArgs, SectionGame,
      "Start a game immediately, warping to ExMy (Doom 1) or MAPxy (Doom 2)." },
    { "-wart", TypeOneOrTwoArgs, SectionGame,
      "Alias for -warp." },
    { "-nomonsters", TypeFlag, SectionGame,
      "Disable monsters." },
    { "-nomo", TypeFlag, SectionGame,
      "Alias for -nomonsters." },
    { "-nocheats", TypeFlag, SectionGame,
      "Disable cheat codes." },
    { "-respawn", TypeFlag, SectionGame,
      "Monsters respawn after being killed." },
    { "-fast", TypeFlag, SectionGame,
      "Monsters move faster." },
    { "-pistolstart", TypeFlag, SectionGame,
      "Enables automatic pistol starts on each level." },
    { "-coop_spawns", TypeFlag, SectionGame,
      "Start single player game with items spawns as in cooperative netgame." },
    { "-dog", TypeFlag, SectionGame,
      "Enables a single helper dog." },
    { "-dogs", TypeOptArg, SectionGame,
      "Overrides the current number of helper dogs, setting it to n." },
    { "-turbo", TypeOptArg, SectionGame,
      "Turbo mode. The player's speed is multiplied by x%. "
      "If unspecified, x defaults to 200. Values are rounded up to 10 and down to 400." },
    { "-loadgame", TypeInt, SectionGame,
      "Load the game on page p (0-7) in slot s (0-7). Use 255 to load an auto save." },
    { "-chain_episodes", TypeFlag, SectionGame,
      "Completing one episode leads to the next without interruption." },

    { "-server", TypeFlag, SectionNet,
      "Start a multiplayer server, listening for connections." },
    { "-dedicated", TypeFlag, SectionNet,
      "Start a dedicated server, routing packets but not participating in the game itself." },
    { "-connect", TypeString, SectionNet,
      "Connect to a multiplayer server running on the given address." },
    { "-autojoin", TypeFlag, SectionNet,
      "Automatically search the local LAN for a multiplayer server and join it." },
    { "-privateserver", TypeFlag, SectionNet,
      "When running a server, don't register with the global master server. "
      "Implies -server." },
    { "-solo-net", TypeFlag, SectionNet,
      "Run a solo network game (for testing)." },
    { "-dup", TypeInt, SectionNet,
      "Reduce the resolution of the game by a factor of n, "
      "reducing the amount of network bandwidth needed." },
    { "-extratics", TypeInt, SectionNet,
      "Send n extra tics in every packet as insurance against dropped packets." },
    { "-port", TypeInt, SectionNet,
      "Use the specified UDP port for communications, instead of the default (2342)." },
    { "-servername", TypeString, SectionNet,
      "When starting a network server, specify a name for the server." },
    { "-timer", TypeInt, SectionNet,
      "For multiplayer games: exit each level after n minutes." },
    { "-avg", TypeFlag, SectionNet,
      "Austin Virtual Gaming: end levels after 20 minutes." },
    { "-deathmatch", TypeFlag, SectionNet,
      "Start a deathmatch game." },
    { "-altdeath", TypeFlag, SectionNet,
      "Start a deathmatch 2.0 game. "
      "Weapons do not stay in place and all items respawn after 30 seconds." },
    { "-dm3", TypeFlag, SectionNet,
      "Start a deathmatch 3.0 game. "
      "Weapons stay in place and all items respawn after 30 seconds." },
    { "-norollback", TypeFlag, SectionNet,
      "Disable rollback netcode." },
    { "-left", TypeFlag, SectionNet,
      "Run as the left screen in three screen mode." },
    { "-nodes", TypeInt, SectionNet,
      "Autostart the netgame when n nodes (clients) have joined the server." },
    { "-frags", TypeInt, SectionNet,
      "The -frags option allows you to deathmatch until one player achieves n frags, "
      "at which time the level ends and scores are displayed. "
      "If n is not specified the match is to 10 frags." },

    { "-playdemo", TypeString, SectionDemo,
      "Play back the demo named demo.lmp." },
    { "-record", TypeString, SectionDemo,
      "Record a demo named demo.lmp." },
    { "-recordfrom", TypeTwoArgs, SectionDemo,
      "Record a demo, loading from the given save slot. "
      "Equivalent to -loadgame <slot> -record <demo>." },
    { "-recordfromto", TypeTwoArgs, SectionDemo,
      "Allows continuing <demo1> after it ends or when the user presses the "
      "join demo key, the result getting saved as <demo2>. "
      "Equivalent to -playdemo <demo1> -record <demo2>." },
    { "-fastdemo", TypeString, SectionDemo,
      "Plays the given demo as fast as possible." },
    { "-timedemo", TypeString, SectionDemo,
      "Play back the demo named demo.lmp, determining the framerate of the screen." },
    { "-levelstat", TypeFlag, SectionDemo,
      "Write level statistics upon exit to levelstat.txt." },
    { "-shorttics", TypeFlag, SectionDemo,
      "Play with low turning resolution to emulate demo recording." },
    { "-longtics", TypeFlag, SectionDemo,
      "Use high turning resolution for demo recording." },
    { "-tas", TypeFlag, SectionDemo,
      "Lifts strict mode restrictions according to DSDA rules." },
    { "-skipsec", TypeString, SectionDemo,
      "Skip min:sec time during viewing of the demo. "
      "\"-warp <x> -skipsec <min:sec>\" will skip min:sec time on level x." },
    { "-statdump", TypeOptArg, SectionDemo,
      "Dump statistics information to the specified file on the levels that were played. "
      "The output from this option matches the output from statdump.exe "
      "(see ctrlapi.zip in the /idgames archive)." },
    { "-maxdemo", TypeInt, SectionDemo,
      "Sets the initial size of the demo recording buffer (KiB). "
      "This is no longer a hard limit, and the buffer will expand if the given limit is exceeded." },
    { "-dumptranmap", TypeTwoArgs, SectionDemo,
      "Dump tranmap lump, given an alpha level (opacity percentage). "
      "Valid values are 0 through to 99." },

    { "-complevel", TypeString, SectionCompat,
      "Emulate a specific version of Doom/Boom/MBF. "
      "Valid values are \"vanilla\", \"boom\", \"mbf\", \"mbf21\"." },
    { "-cl", TypeString, SectionCompat,
      "Alias for -complevel." },
    { "-gameversion", TypeString, SectionCompat,
      "Emulate a specific version of Doom. "
      "Valid values are \"1.9\", \"ultimate\", \"final\", \"final2\", \"chex\". "
      "Requires -complevel vanilla." },
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
      "Don't scale up the screen. Implies -window." },
    { "-2", TypeFlag, SectionVideo,
      "Double up the screen to 2x its normal size. Implies -window." },
    { "-3", TypeFlag, SectionVideo,
      "Triple up the screen to 3x its normal size. Implies -window." },

    { "-deh", TypeFileList, SectionMod,
      "Load the given dehacked/bex patch(es)." },
    { "-nodeh", TypeFlag, SectionMod,
      "Avoid loading DEHACKED lumps embedded into WAD files." },
    { "-noautoload", TypeFlag, SectionMod,
      "Disable auto-loading of .wad and .deh files." },
    { "-nomapinfo", TypeFlag, SectionMod,
      "Disable UMAPINFO loading." },
    { "-blockmap", TypeFlag, SectionMod,
      "Forces a (re-)building of the BLOCKMAP lumps for loaded maps." },
    { "-bsp", TypeFlag, SectionMod,
      "Forces rebuilding of nodes." },
    { "-reject_pad_with_ff", TypeFlag, SectionMod,
      "Pad reject map with 0xFF." },
    { "-tranmap", TypeFlag, SectionMod,
      "Forces the (re-)building of the translucency table." },

    { "-setmem", TypeInt, SectionDebug,
      "Specify DOS version to emulate for NULL pointer dereference emulation. "
      "Supported versions are: dos622, dos71, dosbox. "
      "The default is to emulate DOS 7.1 (Windows 98)." },
    { "-nogui", TypeFlag, SectionDebug,
      "Don't pop up a GUI dialog box to show the error message." },
    { "-spechit", TypeInt, SectionDebug,
      "Use the specified magic value when emulating spechit overruns." },
    { "-quiet", TypeFlag, SectionDebug,
      "Suppress non-error output." },
    { "-verbose", TypeFlag, SectionDebug,
      "Enable verbose output." },
    { "-devparm", TypeFlag, SectionDebug,
      "Developer mode. Enables extra debug output and features." },
};

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

const param_def_t *M_FindParam(const char *arg)
{
    for (int i = 0; i < arrlen(param_table); i++)
    {
        if (!strcasecmp(arg, param_table[i].name))
        {
            return &param_table[i];
        }
    }
    return NULL;
}

const char *M_GetParamHelpString(void)
{
    static char buffer[16384];
    char *p = buffer;
    int remaining = sizeof(buffer);
    param_section_t current_section = -1;

    p += M_snprintf(p, remaining, "Usage: woof [options]\n\n");
    remaining = sizeof(buffer) - (p - buffer);

    for (int i = 0; i < arrlen(param_table); i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            p += M_snprintf(p, remaining, "\n%s:\n", section_names[current_section]);
            remaining = sizeof(buffer) - (p - buffer);
        }

        // Build option signature with argument based on type
        const char *arg = "";
        switch (param->type)
        {
            case TypeString:
            case TypeInt:
            case TypeFloat:
                arg = " <arg>";
                break;
            case TypeFileList:
                arg = " <files>...";
                break;
            case TypeTwoArgs:
                arg = " <arg1> <arg2>";
                break;
            case TypeOneOrTwoArgs:
                arg = " <arg1> [<arg2>]";
                break;
            case TypeOptArg:
                arg = " [<arg>]";
                break;
            default:
                arg = "";
                break;
        }

        p += M_snprintf(p, remaining, "  %s%s  %s\n", param->name, arg, param->help);
        remaining = sizeof(buffer) - (p - buffer);
    }

    return buffer;
}

void M_PrintParamHelp(void)
{
    I_Printf(VB_ALWAYS, "%s", M_GetParamHelpString());
}

void M_GenerateParamMarkdown(const char *filename)
{
    FILE *f = M_fopen(filename, "w");
    if (!f)
    {
        I_Printf(VB_ERROR, "Failed to open %s for writing", filename);
        return;
    }

    // GitHub-friendly markdown
    // For man page: pandoc -s -t man --metadata title="WOOF" --metadata section=6 --metadata date="DATE" file.md -o woof.6
    fprintf(f, "# " PROJECT_NAME "\n\n");
    fprintf(f, PROJECT_NAME " is a continuation of Lee Killough's Doom source port MBF targeted at modern systems\n\n");

    fprintf(f, "## SYNOPSIS\n\n");
    fprintf(f, "`%s` [*options*]\n\n", PROJECT_SHORTNAME);

    fprintf(f, "## OPTIONS\n\n");

    param_section_t current_section = -1;

    for (int i = 0; i < arrlen(param_table); i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            fprintf(f, "### %s\n\n", section_names[current_section]);
        }

        // Build option signature with argument based on type
        char opt_sig[256];
        const char *arg = "";
        switch (param->type)
        {
            case TypeString:
            case TypeInt:
            case TypeFloat:
                arg = " <arg>";
                break;
            case TypeFileList:
                arg = " <files>...";
                break;
            case TypeTwoArgs:
                arg = " <arg1> <arg2>";
                break;
            case TypeOneOrTwoArgs:
                arg = " <arg1> [<arg2>]";
                break;
            case TypeOptArg:
                arg = " [<arg>]";
                break;
            default:
                arg = "";
                break;
        }
        M_snprintf(opt_sig, sizeof(opt_sig), "`%s%s`", param->name, arg);

        // Definition list format - works for both pandoc (man) and GitHub
        fprintf(f, "**%s**\n", opt_sig);
        fprintf(f, ":   %s\n\n", param->help);
    }

    fprintf(f, "## SEE ALSO\n\n");
    fprintf(f, "[Wiki](https://github.com/fabiangreffrath/woof/wiki)\n");

    fclose(f);
    I_Printf(VB_INFO, "Generated markdown documentation: %s", filename);
}
