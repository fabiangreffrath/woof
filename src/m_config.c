//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2021 Fabian Greffrath
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
//
// DESCRIPTION:
//  Main loop menu stuff.
//  Default Config File.
//  PCX Screenshots.
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "config.h"
#include "d_main.h"
#include "d_iwad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_rewind.h"
#include "i_flickstick.h"
#include "i_gamepad.h"
#include "i_gyro.h"
#include "i_input.h"
#include "i_oalequalizer.h"
#include "i_printf.h"
#include "i_rumble.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_io.h"
#include "m_misc.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "r_main.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "w_wad.h"
#include "ws_stuff.h"
#include "z_zone.h"

//
// DEFAULTS
//

static boolean config_help; // jff 3/3/98

// jff 3/3/98 added min, max, and help string to all entries
// jff 4/10/98 added isstr field to specify whether value is string or int
//
//  killough 11/98: entirely restructured to allow options to be modified
//  from wads, and to consolidate with menu code

default_t *defaults = NULL;

void M_BindNum(const char *name, void *location, void *current,
               int default_val, int min_val, int max_val,
               ss_types screen, wad_allowed_t wad,
               const char *help)
{
    default_t item = { name, {.i = location}, {.i = current},
                       {.number = default_val}, {min_val, max_val},
                       number, screen, wad, help };
    array_push(defaults, item);
}

void M_BindMenuNum(const char *name, void *location, int min_val, int max_val)
{
    default_t item = { name, {.i = location}, {0}, {0}, {min_val, max_val},
                       menu, ss_none, wad_no, NULL };
    array_push(defaults, item);
}

void M_BindMenuBool(const char *name, boolean *location)
{
    M_BindMenuNum(name, location, 0, 1);
}

void M_BindBool(const char *name, boolean *location, boolean *current,
                boolean default_val, ss_types screen, wad_allowed_t wad,
                const char *help)
{
    M_BindNum(name, location, current, (int)default_val, 0, 1,
              screen, wad, help);
}

void M_BindStr(char *name, const char **location, const char *default_val,
               wad_allowed_t wad, const char *help)
{
    default_t item = { name, {.s = (char **)location}, {0}, {.string = default_val},
                       {0}, string, ss_none, wad, help };
    array_push(defaults, item);
}

void M_BindInput(const char *name, int input_id, const char *help)
{
    default_t item = { name, {0}, {0}, {0}, {UL,UL}, input, ss_keys, wad_no,
                       help, input_id };
    array_push(defaults, item);
}

void M_InitConfig(void)
{
    BIND_BOOL(config_help, true,
      "Show help strings about each variable in the config file");

    I_BindVideoVariables();
    R_BindRenderVariables();

    I_BindSoundVariables();
    S_BindSoundVariables();
    I_BindEqualizerVariables();

    MN_BindMenuVariables();
    D_BindMiscVariables();
    G_BindGameVariables();
    G_BindRewindVariables();

    G_BindGameInputVariables();
    G_BindMouseVariables();

    I_BindKeyboardVariables();
    I_BindGamepadVariables();
    I_BindRumbleVariables();
    I_BindFlickStickVariables();
    I_BindGyroVaribales();

    M_BindInputVariables();

    G_BindEnemVariables();
    G_BindCompVariables();
    G_BindWeapVariables();
    WS_BindVariables();

    ST_BindHUDVariables();
    ST_BindSTSVariables();
    AM_BindAutomapVariables();

    default_t last_entry = {NULL};
    array_push(defaults, last_entry);
}

static char *defaultfile;
static boolean defaults_loaded = false; // killough 10/98

// killough 11/98: hash function for name lookup
static unsigned default_hash(const char *name)
{
    unsigned hash = 0;
    while (*name)
    {
        hash = hash * 2 + M_ToUpper(*name++);
    }
    return hash % (array_size(defaults) - 1);
}

default_t *M_LookupDefault(const char *name)
{
    static int hash_init;
    register default_t *dp;

    // Initialize hash table if not initialized already
    if (!hash_init)
    {
        for (hash_init = 1, dp = defaults; dp->name; dp++)
        {
            unsigned h = default_hash(dp->name);
            dp->next = defaults[h].first;
            defaults[h].first = dp;
        }
    }

    // Look up name in hash table
    for (dp = defaults[default_hash(name)].first;
         dp && strcasecmp(name, dp->name); dp = dp->next)
        ;

    return dp;
}

//
// M_SaveDefaults
//

void M_SaveDefaults(void)
{
    char *tmpfile;
    register default_t *dp;
    FILE *f;
    int maxlen = 0;

    // killough 10/98: for when exiting early
    if (!defaults_loaded || !defaultfile)
    {
        return;
    }

    // get maximum config key string length
    for (dp = defaults;; dp++)
    {
        int len;
        if (!dp->name)
        {
            break;
        }
        len = strlen(dp->name);
        if (len > maxlen && len < 80)
        {
            maxlen = len;
        }
    }

    tmpfile = M_StringJoin(D_DoomPrefDir(), DIR_SEPARATOR_S, "tmp",
                           D_DoomExeName(), ".cfg");

    errno = 0;
    if (!(f = M_fopen(tmpfile, "w"))) // killough 9/21/98
    {
        goto error;
    }

    // 3/3/98 explain format of file
    // killough 10/98: use executable's name

    if (config_help
        && fprintf(f,
                   ";%s.cfg format:\n"
                   ";[min-max(default)] description of variable\n"
                   ";* at end indicates variable is settable in wads\n"
                   ";variable   value\n",
                   D_DoomExeName())
               == EOF)
    {
        goto error;
    }

    // killough 10/98: output comment lines which were read in during input

    for (dp = defaults;; dp++)
    {
        config_t value = {0};

        // If we still haven't seen any blanks,
        // Output a blank line for separation

        if (putc('\n', f) == EOF)
        {
            goto error;
        }

        if (!dp->name) // If we're at end of defaults table, exit loop
        {
            break;
        }

        if (dp->type == menu)
        {
            continue;
        }

        // jff 3/3/98 output help string
        //
        //  killough 10/98:
        //  Don't output config help if any [ lines appeared before this one.
        //  Make default values, and numeric range output, automatic.
        //
        //  Always write a help string to avoid incorrect entries
        //  in the user config

        if (config_help)
        {
            if ((dp->type == string
                     ? fprintf(f, "[(\"%s\")]", (char *)dp->defaultvalue.string)
                     : dp->limit.min == UL
                        ? dp->limit.max == UL
                           ? fprintf(f, "[?-?(%d)]", dp->defaultvalue.number)
                           : fprintf(f, "[?-%d(%d)]", dp->limit.max,
                                     dp->defaultvalue.number)
                        : dp->limit.max == UL
                           ? fprintf(f, "[%d-?(%d)]", dp->limit.min,
                                     dp->defaultvalue.number)
                           : fprintf(f, "[%d-%d(%d)]", dp->limit.min, dp->limit.max,
                                     dp->defaultvalue.number))
                       == EOF
                || fprintf(f, " %s %s\n", dp->help, dp->wad_allowed ? "*" : "")
                       == EOF)
            {
                goto error;
            }
        }

        // killough 11/98:
        // Write out original default if .wad file has modified the default

        if (dp->type == string)
        {
            value.string = dp->modified ? dp->orig_default.string : *dp->location.s;
        }
        else if (dp->type == number)
        {
            if (dp->modified)
            {
                value.number = dp->orig_default.number;
            }
            else
            {
                value.number = *dp->location.i;
            }
        }

        // jff 4/10/98 kill super-hack on pointer value
        //  killough 3/6/98:
        //  use spaces instead of tabs for uniform justification

        if (dp->type != input)
        {
            if (dp->type == number
                    ? fprintf(f, "%-*s %i\n", maxlen, dp->name, value.number) == EOF
                    : fprintf(f, "%-*s \"%s\"\n", maxlen, dp->name, value.string)
                          == EOF)
            {
                goto error;
            }
        }

        if (dp->type == input)
        {
            int i;
            const input_t *inputs = M_Input(dp->input_id);

            fprintf(f, "%-*s ", maxlen, dp->name);

            for (i = 0; i < array_size(inputs); ++i)
            {
                if (i > 0)
                {
                    fprintf(f, ", ");
                }

                switch (inputs[i].type)
                {
                    const char *s;

                    case INPUT_KEY:
                        if (inputs[i].value >= 33 && inputs[i].value <= 126)
                        {
                            // The '=', ',', and '.' keys originally meant the
                            // shifted versions of those keys, but w/o having to
                            // shift them in the game.
                            char c = inputs[i].value;
                            if (c == ',')
                            {
                                c = '<';
                            }
                            else if (c == '.')
                            {
                                c = '>';
                            }
                            else if (c == '=')
                            {
                                c = '+';
                            }

                            fprintf(f, "%c", c);
                        }
                        else
                        {
                            s = M_GetNameForKey(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    case INPUT_MOUSEB:
                        {
                            s = M_GetNameForMouseB(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    case INPUT_JOYB:
                        {
                            s = M_GetNameForJoyB(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }

            if (i == 0)
            {
                fprintf(f, "%s", "NONE");
            }

            fprintf(f, "\n");
        }
    }

    if (fclose(f) == EOF)
    {
    error:
        I_Error("Could not write defaults to %s: %s\n%s left unchanged\n",
                tmpfile, errno ? strerror(errno) : "(Unknown Error)",
                defaultfile);
        return;
    }

    M_remove(defaultfile);

    if (M_rename(tmpfile, defaultfile))
    {
        I_Error("Could not write defaults to %s: %s\n", defaultfile,
                errno ? strerror(errno) : "(Unknown Error)");
    }

    free(tmpfile);
}

//
// M_ParseOption()
//
// killough 11/98:
//
// This function parses .cfg file lines, or lines in OPTIONS lumps
//

boolean M_ParseOption(const char *p, boolean wad)
{
    char name[80], strparm[1024];
    default_t *dp;
    int parm;

    while (isspace(*p)) // killough 10/98: skip leading whitespace
    {
        p++;
    }

    // jff 3/3/98 skip lines not starting with an alphanum
    //  killough 10/98: move to be made part of main test, add comment-handling

    if (sscanf(p, "%79s %1023[^\n]", name, strparm) != 2 || !isalnum(*name)
        || !(dp = M_LookupDefault(name))
        || (*strparm == '"') == (dp->type != string)
        || (wad && !dp->wad_allowed))
    {
        return 1;
    }

    // [FG] bind mapcolor options to the mapcolor preset menu item
    if (strncmp(name, "mapcolor_", 9) == 0
        || strcmp(name, "hudcolor_titl") == 0)
    {
        default_t *dp_preset = M_LookupDefault("mapcolor_preset");
        dp->setup_menu = dp_preset->setup_menu;
    }

    if (demo_version < DV_MBF && dp->setup_menu
        && !(dp->setup_menu->m_flags & S_COSMETIC))
    {
        return 1;
    }

    if (dp->type == string) // get a string default
    {
        int len = strlen(strparm) - 1;

        while (isspace(strparm[len]))
        {
            len--;
        }

        if (strparm[len] == '"')
        {
            len--;
        }

        strparm[len + 1] = 0;

        if (wad && !dp->modified)                 // Modified by wad
        {                                         // First time modified
            dp->modified = 1;                     // Mark it as modified
            dp->orig_default.string = *dp->location.s; // Save original default
        }
        else
        {
            free(*dp->location.s); // Free old value
        }

        *dp->location.s = strdup(strparm + 1); // Change default value

        if (dp->current.s) // Current value
        {
            free(*dp->current.s);                 // Free old value
            *dp->current.s = strdup(strparm + 1); // Change current value
        }
    }
    else if (dp->type == number)
    {
        if (sscanf(strparm, "%i", &parm) != 1)
        {
            return 1; // Not A Number
        }

        // jff 3/4/98 range check numeric parameters
        if ((dp->limit.min == UL || dp->limit.min <= parm)
            && (dp->limit.max == UL || dp->limit.max >= parm))
        {
            if (wad)
            {
                if (!dp->modified) // First time it's modified by wad
                {
                    dp->modified = 1; // Mark it as modified
                    // Save original default
                    dp->orig_default.number = *dp->location.i;
                }
                if (dp->current.i) // Change current value
                {
                    *dp->current.i = parm;
                }
            }
            *dp->location.i = parm; // Change default 
        }
    }
    else if (dp->type == input)
    {
        char buffer[80];
        char *scan;

        M_InputReset(dp->input_id);

        scan = strtok(strparm, ",");

        do
        {
            if (sscanf(scan, "%79s", buffer) == 1)
            {
                if (strlen(buffer) == 1)
                {
                    // The '=', ',', and '.' keys originally meant the shifted
                    // versions of those keys, but w/o having to shift them in
                    // the game.
                    char c = buffer[0];
                    if (c == '<')
                    {
                        c = ',';
                    }
                    else if (c == '>')
                    {
                        c = '.';
                    }
                    else if (c == '+')
                    {
                        c = '=';
                    }

                    M_InputAddKey(dp->input_id, c);
                }
                else
                {
                    int value;
                    if ((value = M_GetKeyForName(buffer)) > 0)
                    {
                        M_InputAddKey(dp->input_id, value);
                    }
                    else if ((value = M_GetJoyBForName(buffer)) >= 0)
                    {
                        M_InputAddJoyB(dp->input_id, value);
                    }
                    else if ((value = M_GetMouseBForName(buffer)) >= 0)
                    {
                        M_InputAddMouseB(dp->input_id, value);
                    }
                }
            }

            scan = strtok(NULL, ",");
        } while (scan);
    }

    if (wad && dp->setup_menu)
    {
        dp->setup_menu->m_flags |= S_DISABLE;
    }

    return 0; // Success
}

//
// M_LoadOptions()
//
// killough 11/98:
// This function is used to load the OPTIONS lump.
// It allows wads to change game options.
//

void M_LoadOptions(void)
{
    int lump;

    //!
    // @category mod
    //
    // Avoid loading OPTIONS lumps embedded into WAD files.
    //

    if (!M_CheckParm("-nooptions"))
    {
        lump = W_CheckNumForName("OPTIONS");
        if (lump != -1)
        {
            int size = W_LumpLength(lump), buflen = 0;
            char *buf = NULL, *p,
                 *options = p = W_CacheLumpNum(lump, PU_STATIC);
            while (size > 0)
            {
                int len = 0;
                while (len < size && p[len++] && p[len - 1] != '\n')
                    ;
                if (len >= buflen)
                {
                    buf = I_Realloc(buf, buflen = len + 1);
                }
                strncpy(buf, p, len);
                buf[len] = 0;
                p += len;
                size -= len;
                M_ParseOption(buf, true);
            }
            free(buf);
            Z_ChangeTag(options, PU_CACHE);
        }
    }

    MN_Trans();     // reset translucency in case of change
}

//
// M_LoadDefaults
//

void M_LoadDefaults(void)
{
    register default_t *dp;
    int i;
    FILE *f;

    // set everything to base values
    //
    // phares 4/13/98:
    // provide default strings with their own malloced memory so that when
    // we leave this routine, that's what we're dealing with whether there
    // was a config file or not, and whether there were chat definitions
    // in it or not. This provides consistency later on when/if we need to
    // edit these strings (i.e. chat macros in the Chat Strings Setup screen).

    for (dp = defaults; dp->name; dp++)
    {
        if (dp->type == string)
        {
            *dp->location.s = strdup(dp->defaultvalue.string);
        }
        else if (dp->type == number)
        {
            *dp->location.i = dp->defaultvalue.number;
        }
        else if (dp->type == input)
        {
            M_InputSetDefault(dp->input_id);
        }
    }

    // Load special keys
    M_InputPredefined();

    // check for a custom default file

    if (!defaultfile)
    {
        //!
        // @arg <file>
        // @vanilla
        //
        // Load main configuration from the specified file, instead of the
        // default.
        //

        if ((i = M_CheckParm("-config")) && i < myargc - 1)
        {
            defaultfile = strdup(myargv[i + 1]);
        }
        else
        {
            defaultfile = strdup(basedefault);
        }
    }

    // read the file in, overriding any set defaults
    //
    // killough 9/21/98: Print warning if file missing, and use fgets for
    // reading

    if ((f = M_fopen(defaultfile, "r")))
    {
        char s[256];

        while (fgets(s, sizeof s, f))
        {
            M_ParseOption(s, false);
        }
    }

    defaults_loaded = true; // killough 10/98

    I_Printf(VB_INFO, "M_LoadDefaults: Load system defaults.");

    if (f)
    {
        I_Printf(VB_INFO, " default file: %s", defaultfile);
        fclose(f);
    }
    else
    {
        I_Printf(VB_WARNING,
                 " Warning: Cannot read %s -- using built-in defaults",
                 defaultfile);
    }
}

boolean M_CheckIfDisabled(const char *name)
{
    const default_t *dp = M_LookupDefault(name);
    return dp->setup_menu->m_flags & S_DISABLE;
}
