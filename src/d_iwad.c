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

#include "SDL.h"

#include <stdlib.h>
#include <string.h>

#include "d_iwad.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"

static const iwad_t iwads[] = {
    {"doom2.wad",     doom2,      commercial,   vanilla,  "Doom II"                   },
    {"plutonia.wad",  pack_plut,  commercial,   vanilla,
     "Final Doom: Plutonia Experiment"                                                },
    {"tnt.wad",       pack_tnt,   commercial,   vanilla,  "Final Doom: TNT: Evilution"},
 // "doom.wad" may be retail or registered
    {"doom.wad",      doom,       indetermined, vanilla,  "Doom"                      },
    {"doom1.wad",     doom,       indetermined, vanilla,  "Doom Shareware"            },
    {"doom2f.wad",    doom2,      commercial,   vanilla,  "Doom II: L'Enfer sur Terre"},
    {"chex.wad",      pack_chex,  retail,       vanilla,  "Chex Quest"                },
    {"hacx.wad",      pack_hacx,  commercial,   vanilla,  "Hacx"                      },
    {"freedoom2.wad", doom2,      commercial,   freedoom, "Freedoom: Phase 2"         },
    {"freedoom1.wad", doom,       retail,       freedoom, "Freedoom: Phase 1"         },
    {"freedm.wad",    doom2,      commercial,   freedoom, "FreeDM"                    },
    {"rekkrsa.wad",   pack_rekkr, retail,       vanilla,  "REKKR"                     },
    {"rekkrsl.wad",   pack_rekkr, retail,       vanilla,  "REKKR: Sunken Land"        },
    {"miniwad.wad",   doom2,      commercial,   miniwad,  "miniwad"                   },
    {"miniwad1.wad",  doom,       retail,       miniwad,  "miniwadm"                  },
    {"miniwad2.wad",  doom2,      commercial,   miniwad,  "miniwadm"                  }
};

// "128 IWAD search directories should be enough for anybody".

#define MAX_IWAD_DIRS 128

// Array of locations to search for IWAD files

static boolean iwad_dirs_built = false;
char *iwad_dirs[MAX_IWAD_DIRS];
int num_iwad_dirs = 0;

static void AddIWADDir(char *dir)
{
    if (num_iwad_dirs < MAX_IWAD_DIRS)
    {
        iwad_dirs[num_iwad_dirs] = dir;
        ++num_iwad_dirs;
    }
}

// Return the path where the executable lies -- Lee Killough

char *D_DoomExeDir(void)
{
    static char *base;

    if (base == NULL) // cache multiple requests
    {
        char *result;

        result = SDL_GetBasePath();
        if (result != NULL)
        {
            base = M_DirName(result);
            SDL_free(result);
        }
        else
        {
            base = M_DirName(myargv[0]);
        }
    }

    return base;
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

// Values installed by the GOG.com and Collector's Edition versions

static registry_value_t root_path_keys[] =
{
    // Doom Collector's Edition

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\Activision\\DOOM Collector's Edition\\v1.0",
        "INSTALLPATH",
    },

    // Doom II

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\1435848814",
        "PATH",
    },

    // Doom 3: BFG Edition

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\1135892318",
        "PATH",
    },

    // Final Doom

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\1435848742",
        "PATH",
    },

    // Ultimate Doom

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\1435827232",
        "PATH",
    },

    // DOOM Unity port

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\2015545325",
        "PATH"
    },

    // DOOM II Unity port

    {
        HKEY_LOCAL_MACHINE,
        SOFTWARE_KEY "\\GOG.com\\Games\\1426071866",
        "PATH"
    },
};

// Subdirectories of the above install path, where IWADs are installed.

static char *root_path_subdirs[] =
{
    ".",        "Doom2", "Final Doom", "Ultimate Doom",
    "Plutonia", "TNT",   "base\\wads",
};

// Location where Steam is installed

static registry_value_t steam_install_location =
{
    HKEY_LOCAL_MACHINE,
    SOFTWARE_KEY "\\Valve\\Steam",
    "InstallPath",
};

// Subdirs of the steam install directory where IWADs are found

static char *steam_install_subdirs[] =
{
    "steamapps\\common\\doom 2\\base", "steamapps\\common\\final doom\\base",
    "steamapps\\common\\ultimate doom\\base",

    // From Doom 3: BFG Edition:

    "steamapps\\common\\DOOM 3 BFG Edition\\base\\wads",

    "steamapps\\common\\ultimate doom\\rerelease\\DOOM_Data\\StreamingAssets",
    "steamapps\\common\\doom 2\\rerelease\\DOOM II_Data\\StreamingAssets",
    "steamapps\\common\\doom 2\\finaldoombase"
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

            AddIWADDir(path);
        }
    }
}

// Check for GOG.com and Doom: Collector's Edition

static void CheckInstallRootPaths(void)
{
    unsigned int i;

    for (i = 0; i < arrlen(root_path_keys); ++i)
    {
        char *install_path;
        char *subpath;
        unsigned int j;

        install_path = GetRegistryString(&root_path_keys[i]);

        if (install_path == NULL)
        {
            continue;
        }

        for (j = 0; j < arrlen(root_path_subdirs); ++j)
        {
            subpath = M_StringJoin(install_path, DIR_SEPARATOR_S,
                                   root_path_subdirs[j], NULL);
            AddIWADDir(subpath);
        }

        free(install_path);
    }
}

// Check for Doom downloaded via Steam

static void CheckSteamEdition(void)
{
    char *install_path;
    char *subpath;
    size_t i;

    install_path = GetRegistryString(&steam_install_location);

    if (install_path == NULL)
    {
        return;
    }

    for (i = 0; i < arrlen(steam_install_subdirs); ++i)
    {
        subpath = M_StringJoin(install_path, DIR_SEPARATOR_S,
                               steam_install_subdirs[i], NULL);

        AddIWADDir(subpath);
    }

    free(install_path);
}

// Default install directories for DOS Doom

static void CheckDOSDefaults(void)
{
    // These are the default install directories used by the deice
    // installer program:

    AddIWADDir("\\doom2");    // Doom II
    AddIWADDir("\\plutonia"); // Final Doom
    AddIWADDir("\\tnt");
    AddIWADDir("\\doom_se"); // Ultimate Doom
    AddIWADDir("\\doom");    // Shareware / Registered Doom
    AddIWADDir("\\dooms");   // Shareware versions
    AddIWADDir("\\doomsw");
}

#endif

// Returns true if the specified path is a path to a file
// of the specified name.

static boolean DirIsFile(const char *path, const char *filename)
{
    return strchr(path, DIR_SEPARATOR) != NULL
           && !strcasecmp(M_BaseName(path), filename);
}

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

            AddIWADDir(M_StringJoin(left, suffix, NULL));
            left = p + 1;
        }
        else
        {
            break;
        }
    }

    AddIWADDir(M_StringJoin(left, suffix, NULL));

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
    char *env, *tmp_env;

    // Quote:
    // > $XDG_DATA_HOME defines the base directory relative to which
    // > user specific data files should be stored. If $XDG_DATA_HOME
    // > is either not set or empty, a default equal to
    // > $HOME/.local/share should be used.
    env = M_getenv("XDG_DATA_HOME");
    tmp_env = NULL;

    if (env == NULL)
    {
        char *homedir = M_getenv("HOME");
        if (homedir == NULL)
        {
            homedir = "/";
        }

        tmp_env = M_StringJoin(homedir, "/.local/share", NULL);
        env = tmp_env;
    }

    // We support $XDG_DATA_HOME/games/doom (which will usually be
    // ~/.local/share/games/doom) as a user-writeable extension to
    // the usual /usr/share/games/doom location.
    AddIWADDir(M_StringJoin(env, "/games/doom", NULL));
    free(tmp_env);

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
static void AddSteamDirs(void)
{
    char *homedir, *steampath;

    homedir = M_getenv("HOME");
    if (homedir == NULL)
    {
        homedir = "/";
    }
    steampath = M_StringJoin(homedir, "/.steam/root/steamapps/common", NULL);

    AddIWADPath(steampath, "/Doom 2/base");
    AddIWADPath(steampath, "/Master Levels of Doom/doom2");
    AddIWADPath(steampath, "/Ultimate Doom/base");
    AddIWADPath(steampath, "/Final Doom/base");
    AddIWADPath(steampath, "/DOOM 3 BFG Edition/base/wads");
    free(steampath);
}
#  endif // __MACOSX__
#endif   // !_WIN32

//
// Build a list of IWAD files
//

void BuildIWADDirList(void)
{
    char *env;

    if (iwad_dirs_built)
    {
        return;
    }

    // Look in the current directory.  Doom always does this.
    AddIWADDir(".");

    // Next check the directory where the executable is located. This might
    // be different from the current directory.
    AddIWADDir(D_DoomExeDir());

    // Add DOOMWADDIR if it is in the environment
    env = M_getenv("DOOMWADDIR");
    if (env != NULL)
    {
        AddIWADDir(env);
    }

    // Add dirs from DOOMWADPATH:
    env = M_getenv("DOOMWADPATH");
    if (env != NULL)
    {
        AddIWADPath(env, "");
    }

    // [FG] Add plain HOME directory
    env = M_getenv("HOME");
    if (env != NULL)
    {
        AddIWADDir(env);
    }

#if defined(WOOFDATADIR)
    // [FG] Add a build-time configurable data directory
    AddIWADDir(WOOFDATADIR);
#endif

#ifdef _WIN32

    // Search the registry and find where IWADs have been installed.

    CheckUninstallStrings();
    CheckInstallRootPaths();
    CheckSteamEdition();
    CheckDOSDefaults();

#else
    AddXdgDirs();
#  if !defined(__MACOSX__)
    AddSteamDirs();
#  endif
#endif

    // Don't run this function again.

    iwad_dirs_built = true;
}

//
// Searches WAD search paths for an WAD with a specific filename.
//

char *D_FindWADByName(const char *name)
{
    char *path;
    char *probe;
    int i;

    // Absolute path?

    probe = M_FileCaseExists(name);
    if (probe != NULL)
    {
        return probe;
    }

    BuildIWADDirList();

    // Search through all IWAD paths for a file with the given name.

    for (i = 0; i < num_iwad_dirs; ++i)
    {
        // As a special case, if this is in DOOMWADDIR or DOOMWADPATH,
        // the "directory" may actually refer directly to an IWAD
        // file.

        probe = M_FileCaseExists(iwad_dirs[i]);
        if (DirIsFile(iwad_dirs[i], name) && probe != NULL)
        {
            return probe;
        }
        free(probe);

        // Construct a string for the full path

        path = M_StringJoin(iwad_dirs[i], DIR_SEPARATOR_S, name, NULL);

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

static char *FindWithExtensions(const char *filename, ...)
{
    char *path = NULL;
    va_list args;

    va_start(args, filename);
    while (true)
    {
        const char *arg = va_arg(args, const char *);
        if (arg == NULL)
        {
            break;
        }

        char *s = M_StringJoin(filename, arg, NULL);
        path = D_FindWADByName(s);
        free(s);
        if (path != NULL)
        {
            break;
        }
    }
    va_end(args);

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
        result = FindWithExtensions(filename, ".wad", ".zip", ".lmp", NULL);
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

//
// D_FindIWADFile
//

char *D_FindIWADFile(GameMode_t *mode, GameMission_t *mission,
                     GameVariant_t *variant)
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

        char *file = malloc(strlen(iwadfile) + 5);
        AddDefaultExtension(strcpy(file, iwadfile), ".wad");

        result = D_FindWADByName(file);

        if (result == NULL)
        {
            I_Error("IWAD file '%s' not found!", file);
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

    if (result)
    {
        int i;
        const char *name = M_BaseName(result);

        for (i = 0; i < arrlen(iwads); ++i)
        {
            if (!strcasecmp(name, iwads[i].name))
            {
                *mode = iwads[i].mode;
                *mission = iwads[i].mission;
                *variant = iwads[i].variant;
                break;
            }
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
