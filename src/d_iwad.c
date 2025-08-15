//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Search for and locate an IWAD file, and initialize according
//     to the IWAD type.
//

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_iwad.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"

static const iwad_t iwads[] = {
    {"doom2.wad",     doom2,         commercial,   "DOOM II: Hell on Earth"         },
    {"plutonia.wad",  pack_plut,     commercial,   "Final DOOM: Plutonia Experiment"},
    {"tnt.wad",       pack_tnt,      commercial,   "Final DOOM: TNT - Evilution"    },
    // "doom.wad" may be retail or registered
    {"doom.wad",      doom,          indetermined, "DOOM"                           },
    {"doom.wad",      doom,          registered,   "DOOM Registered"                },
    {"doom.wad",      doom,          retail,       "The Ultimate DOOM"              },
    // "doomu.wad" alias to allow retail wad to coexist with registered in the same folder
    {"doomu.wad",     doom,          retail,       "The Ultimate DOOM"              },
    {"doom1.wad",     doom,          shareware,    "DOOM Shareware"                 },
    {"doom2f.wad",    doom2,         commercial,   "DOOM II: L'Enfer sur Terre"     },
    {"freedoom2.wad", pack_freedoom, commercial,   "Freedoom: Phase 2"              },
    {"freedoom1.wad", pack_freedoom, retail,       "Freedoom: Phase 1"              },
    {"freedm.wad",    pack_freedoom, commercial,   "FreeDM"                         },
    {"chex.wad",      pack_chex,     retail,       "Chex Quest"                     },
    {"chex3v.wad",    pack_chex3v,   retail,       "Chex Quest 3: Vanilla Edition"  },
    {"chex3d2.wad",   pack_chex3v,   commercial,   "Chex Quest 3: Modding Edition"  },
    {"hacx.wad",      pack_hacx,     commercial,   "HACX: Twitch n' Kill"           },
    {"rekkrsa.wad",   pack_rekkr,    retail,       "REKKR"                          },
    {"rekkrsl.wad",   pack_rekkr,    retail,       "REKKR: Sunken Land"             },
};

static const char *const gamemode_str[] = {
    "Shareware mode",
    "Registered mode",
    "Commercial mode",
    "Retail mode",
    "Unknown mode"
};

#define SUB_DIRS(str, ...) (char *[]){str, ##__VA_ARGS__, NULL}

// Array of locations to search for IWAD files
#define M_ARRAY_INIT_CAPACITY 32
#include "m_array.h"

static char **iwad_dirs;

// Return the path where the executable lies -- Lee Killough

char *D_DoomExeDir(void)
{
    static char *base;

    if (base == NULL) // cache multiple requests
    {
        const char *result = SDL_GetBasePath();

        if (result != NULL)
        {
            base = M_DirName(result);
        }
        else
        {
            base = M_DirName(myargv[0]);
        }
    }

    return base;
}

// [FG] get the path to the default configuration dir to use

char *D_DoomPrefDir(void)
{
    static char *dir;

    if (dir == NULL)
    {
#if !defined(_WIN32) || defined(_WIN32_WCE)
        // Configuration settings are stored in an OS-appropriate path
        // determined by SDL.  On typical Unix systems, this might be
        // ~/.local/share/chocolate-doom.  On Windows, we behave like
        // Vanilla Doom and save in the current directory.

        char *result = SDL_GetPrefPath("", PROJECT_SHORTNAME);
        if (result != NULL)
        {
            dir = M_DirName(result);
            SDL_free(result);
        }
        else
#endif /* #ifndef _WIN32 */
        {
            dir = D_DoomExeDir();
        }

        M_MakeDirectory(dir);
    }

    return dir;
}

// This is Windows-specific code that automatically finds the location
// of installed IWAD files.  The registry is inspected to find special
// keys installed by the Windows installers for various CD versions
// of Doom.  From these keys we can deduce where to find an IWAD.

#if defined(_WIN32) && !defined(_WIN32_WCE)

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

typedef struct
{
    HKEY root;
    char *path;
    char *value;
} registry_value_t;

typedef struct
{
    registry_value_t key;
    char **subdirs;
} root_path_t;

#  define UNINSTALLER_STRING "\\uninstl.exe /S "

// Keys installed by the various CD editions.  These are actually the
// commands to invoke the uninstaller and look like this:
//
// C:\Program Files\Path\uninstl.exe /S C:\Program Files\Path
//
// With some munging we can find where Doom was installed.

// [AlexMax] From the persepctive of a 64-bit executable, 32-bit registry
// keys are located in a different spot.
#  if _WIN64
#    define SOFTWARE_KEY "Software\\Wow6432Node"
#  else
#    define SOFTWARE_KEY "Software"
#  endif

#  define UNINSTALL_KEY \
      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
#  define GOG_REG(num) \
      {HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\" #num, "path"}
#  define STEAM_REG(num) \
      {HKEY_LOCAL_MACHINE, UNINSTALL_KEY "\\Steam App " #num, "InstallLocation"}

static registry_value_t uninstall_values[] =
{
    // Ultimate Doom, CD version (Depths of Doom trilogy)

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\"
                     "Uninstall\\Ultimate Doom for Windows 95",
        "UninstallString",
    },

    // Doom II, CD version (Depths of Doom trilogy)

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\"
                     "Uninstall\\Doom II for Windows 95",
        "UninstallString",
    },

    // Final Doom

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\"
                     "Uninstall\\Final Doom for Windows 95",
        "UninstallString",
    },

    // Shareware version

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\"
                     "Uninstall\\Doom Shareware for Windows 95",
        "UninstallString",
    },
};

// Values installed by the GOG.com versions

static const root_path_t gog_paths[] =
{
    // Doom II
    {GOG_REG(1435848814), SUB_DIRS("doom2")},

    // Doom 3: BFG Edition
    {GOG_REG(1135892318), SUB_DIRS("base\\wads")},

    // Final Doom
    {GOG_REG(1435848742), SUB_DIRS("Plutonia", "TNT")},

    // Ultimate Doom
    {GOG_REG(1435827232), SUB_DIRS(".")},

    // DOOM Unity port
    {GOG_REG(2015545325), SUB_DIRS("DOOM_Data\\StreamingAssets")},

    // DOOM II Unity port
    {GOG_REG(1426071866), SUB_DIRS("DOOM II_Data\\StreamingAssets")},

    // DOOM + DOOM II
    {GOG_REG(1413291984), SUB_DIRS("dosdoom\\base",
                                   "dosdoom\\base\\doom2",
                                   "dosdoom\\base\\plutonia",
                                   "dosdoom\\base\\tnt",
                                   ".")},
};

// Values installed by the Steam versions

static const root_path_t steam_paths[] =
{
    // Ultimate Doom, Doom I Enhanced, DOOM + DOOM II
    {STEAM_REG(2280), SUB_DIRS("base",
                               "base\\doom2",
                               "base\\plutonia",
                               "base\\tnt",
                               "rerelease",
                               "rerelease\\DOOM_Data\\StreamingAssets")},

    // Doom II, Doom II Enhanced
    {STEAM_REG(2300), SUB_DIRS("base",
                               "masterbase\\doom2",
                               "finaldoombase",
                               "rerelease\\DOOM II_Data\\StreamingAssets")},

    // Master Levels for Doom II
    {STEAM_REG(9160), SUB_DIRS("doom2")},

    // Final Doom
    {STEAM_REG(2290), SUB_DIRS("base")},

    // Doom 3: BFG Edition
    {STEAM_REG(208200), SUB_DIRS("base\\wads")},

    // Doom Eternal
    {STEAM_REG(782330), SUB_DIRS("base\\classicwads")},
};

// Values installed by other versions (non-GOG.com, non-Steam)

static const root_path_t misc_paths[] = 
{
    // Doom Collector's Edition
    {
        {
            HKEY_LOCAL_MACHINE,
            SOFTWARE_KEY "\\Activision\\DOOM Collector's Edition\\v1.0",
            "INSTALLPATH"
        },
        SUB_DIRS("Ultimate Doom", "Doom2", "Final Doom")
    },
};

static char *GetRegistryString(registry_value_t *reg_val)
{
    HKEY key;
    DWORD len;
    DWORD valtype;
    char *result;

    // Open the key (directory where the value is stored)

    if (RegOpenKeyEx(reg_val->root, reg_val->path, 0, KEY_READ, &key)
        != ERROR_SUCCESS)
    {
        return NULL;
    }

    result = NULL;

    // Find the type and length of the string, and only accept strings.

    if (RegQueryValueEx(key, reg_val->value, NULL, &valtype, NULL, &len)
            == ERROR_SUCCESS
        && valtype == REG_SZ)
    {
        // Allocate a buffer for the value and read the value

        result = malloc(len + 1);

        if (RegQueryValueEx(key, reg_val->value, NULL, &valtype,
                            (unsigned char *)result, &len)
            != ERROR_SUCCESS)
        {
            free(result);
            result = NULL;
        }
        else
        {
            // Ensure the value is null-terminated
            result[len] = '\0';
        }
    }

    // Close the key

    RegCloseKey(key);

    return result;
}

// Check for the uninstall strings from the CD versions

static void CheckUninstallStrings(void)
{
    unsigned int i;

    for (i = 0; i < arrlen(uninstall_values); ++i)
    {
        char *val;
        char *path;
        char *unstr;

        val = GetRegistryString(&uninstall_values[i]);

        if (val == NULL)
        {
            continue;
        }

        unstr = strstr(val, UNINSTALLER_STRING);

        if (unstr == NULL)
        {
            free(val);
        }
        else
        {
            path = unstr + strlen(UNINSTALLER_STRING);

            array_push(iwad_dirs, path);
        }
    }
}

// Check for GOG.com, Steam, and Doom: Collector's Edition

static void CheckInstallRootPaths(const root_path_t *root_paths, int length)
{
    unsigned int i;

    for (i = 0; i < length; ++i)
    {
        char *install_path;
        char *subpath;
        unsigned int j;

        registry_value_t key = root_paths[i].key;
        install_path = GetRegistryString(&key);

        if (install_path == NULL || *install_path == '\0')
        {
            continue;
        }

        for (j = 0; root_paths[i].subdirs[j] != NULL; ++j)
        {
            subpath = M_StringJoin(install_path, DIR_SEPARATOR_S,
                                   root_paths[i].subdirs[j]);
            array_push(iwad_dirs, subpath);
        }

        free(install_path);
    }
}

// Default install directories for DOS Doom

static void CheckDOSDefaults(void)
{
    // These are the default install directories used by the deice
    // installer program:

    array_push(iwad_dirs, "\\doom2");    // Doom II
    array_push(iwad_dirs, "\\plutonia"); // Final Doom
    array_push(iwad_dirs, "\\tnt");
    array_push(iwad_dirs, "\\doom_se"); // Ultimate Doom
    array_push(iwad_dirs, "\\doom");    // Shareware / Registered Doom
    array_push(iwad_dirs, "\\dooms");   // Shareware versions
    array_push(iwad_dirs, "\\doomsw");
}

#endif

// Add IWAD directories parsed from splitting a path string containing
// paths separated by PATH_SEPARATOR. 'suffix' is a string to concatenate
// to the end of the paths before adding them.
static void AddIWADPath(const char *path, const char *suffix)
{
    char *left, *p, *dup_path;

    dup_path = M_StringDuplicate(path);

    // Split into individual dirs within the list.
    left = dup_path;

    for (;;)
    {
        p = strchr(left, PATH_SEPARATOR);
        if (p != NULL)
        {
            // Break at the separator and use the left hand side
            // as another iwad dir
            *p = '\0';

            array_push(iwad_dirs, M_StringJoin(left, suffix));
            left = p + 1;
        }
        else
        {
            break;
        }
    }

    if (*left != '\0')
    {
        array_push(iwad_dirs, M_StringJoin(left, suffix));
    }

    free(dup_path);
}

#if !defined(_WIN32)
// Add standard directories where IWADs are located on Unix systems.
// To respect the freedesktop.org specification we support overriding
// using standard environment variables. See the XDG Base Directory
// Specification:
// <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>
static void AddXdgDirs(void)
{
    char *env = M_DataDir();

    // We support $XDG_DATA_HOME/games/doom (which will usually be
    // ~/.local/share/games/doom) as a user-writeable extension to
    // the usual /usr/share/games/doom location.
    array_push(iwad_dirs, M_StringJoin(env, "/games/doom"));

    // Quote:
    // > $XDG_DATA_DIRS defines the preference-ordered set of base
    // > directories to search for data files in addition to the
    // > $XDG_DATA_HOME base directory. The directories in $XDG_DATA_DIRS
    // > should be seperated with a colon ':'.
    // >
    // > If $XDG_DATA_DIRS is either not set or empty, a value equal to
    // > /usr/local/share/:/usr/share/ should be used.
    env = M_getenv("XDG_DATA_DIRS");
    if (env == NULL)
    {
        // (Trailing / omitted from paths, as it is added below)
        env = "/usr/local/share:/usr/share";
    }

    // The "standard" location for IWADs on Unix that is supported by most
    // source ports is /usr/share/games/doom - we support this through the
    // XDG_DATA_DIRS mechanism, through which it can be overridden.
    AddIWADPath(env, "/games/doom");

    // The convention set by RBDOOM-3-BFG is to install Doom 3: BFG
    // Edition into this directory, under which includes the Doom
    // Classic WADs.
    AddIWADPath(env, "/games/doom3bfg/base/wads");
}

#  if !defined(__MACOSX__)
// Steam on Linux allows installing some select Windows games,
// including the classic Doom series (running DOSBox via Wine).  We
// could parse *.vdf files to more accurately detect installation
// locations, but the defaults are likely to be good enough for just
// about everyone.

typedef struct
{
    char *basedir;
    char **subdirs;
} root_path_t;

static const root_path_t steam_paths[] =
{
    // Ultimate Doom, Doom I Enhanced, DOOM + DOOM II
    {"Ultimate Doom", SUB_DIRS("base",
                               "base/doom2",
                               "base/plutonia",
                               "base/tnt",
                               "rerelease",
                               "rerelease/DOOM_Data/StreamingAssets")},

    // Doom II, Doom II Enhanced
    {"Doom 2", SUB_DIRS("base",
                        "masterbase/doom2",
                        "finaldoombase",
                        "rerelease/DOOM II_Data/StreamingAssets")},

    // Master Levels for Doom II
    {"Master Levels of Doom", SUB_DIRS("doom2")},

    // Final Doom
    {"Final Doom", SUB_DIRS("base")},

    // Doom 3: BFG Edition
    {"DOOM 3 BFG Edition", SUB_DIRS("base/wads")},

    // Doom Eternal
    {"DOOMEternal", SUB_DIRS("base/classicwads")},
};

static void AddSteamDirs(void)
{
    char *homedir, *steampath, *subpath;

    homedir = M_HomeDir();
    steampath = M_StringJoin(homedir, "/.steam/root/steamapps/common");

    for (int i = 0; i < arrlen(steam_paths); i++)
    {
        for (int j = 0; steam_paths[i].subdirs[j] != NULL; j++)
        {
            subpath = M_StringJoin(DIR_SEPARATOR_S, steam_paths[i].basedir,
                                   DIR_SEPARATOR_S, steam_paths[i].subdirs[j]);
            AddIWADPath(steampath, subpath);
        }
    }

    free(steampath);
}
#  endif // __MACOSX__
#endif   // !_WIN32

//
// Build a list of IWAD files
//

static char **iwad_dirs_append;

void BuildIWADDirList(void)
{
    char *env;

    if (array_size(iwad_dirs) > 0)
    {
        return;
    }

    // Look in the current directory.  Doom always does this.
    array_push(iwad_dirs, ".");

    // Next check the directory where the executable is located. This might
    // be different from the current directory.
    // D_DoomPrefDir() returns the executable directory on Windows,
    // and a user-writable config directory everywhere else.
    array_push(iwad_dirs, D_DoomPrefDir());

    // Add DOOMWADDIR if it is in the environment
    env = M_getenv("DOOMWADDIR");
    if (env != NULL)
    {
        array_push(iwad_dirs, env);
    }

    // Add dirs from DOOMWADPATH:
    env = M_getenv("DOOMWADPATH");
    if (env != NULL)
    {
        AddIWADPath(env, "");
    }

#ifdef _WIN32

    // Search the registry and find where IWADs have been installed.

    CheckUninstallStrings();
    CheckInstallRootPaths(gog_paths, arrlen(gog_paths));
    CheckInstallRootPaths(steam_paths, arrlen(steam_paths));
    CheckInstallRootPaths(misc_paths, arrlen(misc_paths));
    CheckDOSDefaults();

#else
    AddXdgDirs();
#  if !defined(__MACOSX__)
    AddSteamDirs();
#  endif
#endif

    char **dir;
    array_foreach(dir, iwad_dirs_append)
    {
        array_push(iwad_dirs, *dir);
    }
}

//
// Searches WAD search paths for an WAD with a specific filename.
//

char *D_FindWADByName(const char *name)
{
    char *probe;

    // Absolute path?

    probe = M_FileCaseExists(name);
    if (probe != NULL)
    {
        return probe;
    }

    BuildIWADDirList();

    // Search through all IWAD paths for a file with the given name.

    char **dir;
    array_foreach(dir, iwad_dirs)
    {
        // As a special case, if this is in DOOMWADDIR or DOOMWADPATH,
        // the "directory" may actually refer directly to an IWAD
        // file.

        probe = M_FileCaseExists(*dir);
        if (probe != NULL)
        {
            return probe;
        }
        free(probe);

        // Construct a string for the full path

        char *path = M_StringJoin(*dir, DIR_SEPARATOR_S, name);

        probe = M_FileCaseExists(path);
        if (probe != NULL)
        {
            return probe;
        }

        free(path);
    }

    // File not found

    return NULL;
}

#define FindWithExtensions(filename, ...)                               \
    FindWithExtensionsInternal(filename, (const char *[]){__VA_ARGS__}, \
                               sizeof((const char *[]){__VA_ARGS__})    \
                                   / sizeof(const char *))

static char *FindWithExtensionsInternal(const char *filename,
                                        const char *ext[], size_t n)
{
    char *path = NULL;

    for (int i = 0; i < n; ++i)
    {
        char *s = M_StringJoin(filename, ext[i]);
        path = D_FindWADByName(s);
        free(s);
        if (path != NULL)
        {
            break;
        }
    }

    return path;
}

//
// D_TryWADByName
//
// Searches for a WAD by its filename, or returns a copy of the filename
// if not found.
//

char *D_TryFindWADByName(const char *filename)
{
    char *result;

    if (!strrchr(M_BaseName(filename), '.'))
    {
        result = FindWithExtensions(filename, ".wad", ".zip", ".lmp");
    }
    else
    {
        result = D_FindWADByName(filename);
    }

    if (result != NULL)
    {
        return result;
    }
    else
    {
        return M_StringDuplicate(filename);
    }
}

char *D_FindLMPByName(const char *filename)
{
    if (!strrchr(M_BaseName(filename), '.'))
    {
        return FindWithExtensions(filename, ".lmp");
    }
    else
    {
        return D_FindWADByName(filename);
    }
}

//
// D_FindIWADFile
//

char *D_FindIWADFile(void)
{
    char *result;

    //!
    // Specify an IWAD file to use.
    //
    // @arg <file>
    // @help
    //

    int iwadparm = M_CheckParmWithArgs("-iwad", 1);

    if (iwadparm)
    {
        // Search through IWAD dirs for an IWAD with the given name.

        char *iwadfile = myargv[iwadparm + 1];

        char *file = AddDefaultExtension(iwadfile, ".wad");

        result = D_FindWADByName(file);

        if (result == NULL)
        {
            I_Error("IWAD file '%s' not found!", file);
        }
        else
        {
            char *iwad_dir = M_DirName(result);
            array_push(iwad_dirs_append, iwad_dir);
        }

        free(file);
    }
    else
    {
        int i;

        // Search through the list and look for an IWAD

        result = NULL;

        for (i = 0; result == NULL && i < arrlen(iwads); ++i)
        {
            result = D_FindWADByName(iwads[i].name);
        }
    }

    return result;
}

boolean D_IsIWADName(const char *name)
{
    int i;

    for (i = 0; i < arrlen(iwads); i++)
    {
        if (!strcasecmp(name, iwads[i].name))
        {
            return true;
        }
    }

    return false;
}

const iwad_t **D_GetIwads(void)
{
    const iwad_t **result;
    int result_len;
    char *filename;
    int i;

    result = malloc(sizeof(iwad_t *) * (arrlen(iwads) + 1));
    result_len = 0;

    // Try to find all IWADs

    for (i = 0; i < arrlen(iwads); ++i)
    {
        filename = D_FindWADByName(iwads[i].name);

        if (filename != NULL)
        {
            result[result_len] = &iwads[i];
            ++result_len;
        }
    }

    // End of list

    result[result_len] = NULL;

    return result;
}

GameMission_t D_GetGameMissionByIWADName(const char *name)
{
    for (int i = 0; i < arrlen(iwads); ++i)
    {
        if (!strcasecmp(name, iwads[i].name))
        {
            return iwads[i].mission;
        }
    }

    return none;
}

void D_GetModeAndMissionByIWADName(const char *name, GameMode_t *mode,
                                   GameMission_t *mission)
{
    for (int i = 0; i < arrlen(iwads); ++i)
    {
        if (!strcasecmp(name, iwads[i].name))
        {
            *mode = iwads[i].mode;
            *mission = iwads[i].mission;
            return;
        }
    }
    *mode = indetermined;
    *mission = none;
}

const char *D_GetIWADDescription(const char *name, GameMode_t mode,
                                 GameMission_t mission)
{
    for (int i = 0; i < arrlen(iwads); ++i)
    {
        if (!strcasecmp(name, iwads[i].name) && mode == iwads[i].mode
            && mission == iwads[i].mission)
        {
            return iwads[i].description;
        }
    }
    return gamemode_str[mode];
}
