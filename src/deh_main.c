//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Guilherme Miranda
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
//
// Main dehacked code
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_items.h"
#include "d_iwad.h"
#include "deh_bex_music.h"
#include "deh_bex_sounds.h"
#include "deh_bex_sprites.h"
#include "deh_defs.h"
#include "deh_frame.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_misc.h"
#include "deh_strings.h"
#include "deh_thing.h"
#include "doomtype.h"
#include "i_glob.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

static const char *deh_signatures[] =
{
    "Patch File for DeHackEd v2.3",
    "Patch File for DeHackEd v3.0",
    NULL
};

static deh_section_t *deh_section_types[] =
{
    // Original DEH blocks
    &deh_section_ammo,
    &deh_section_cheat,
    &deh_section_frame,
    &deh_section_misc,
    &deh_section_pointer,
    &deh_section_sound,
    &deh_section_text,
    &deh_section_thing,
    &deh_section_weapon,
    // Boom BEX
    &deh_section_bex_codepointers,
    &deh_section_bex_includes,
    &deh_section_bex_partimes,
    &deh_section_bex_strings,
    // Eternity BEX
    &deh_section_bex_helper,
    &deh_section_bex_music,
    &deh_section_bex_sounds,
    &deh_section_bex_sprites,
    NULL
};

static boolean deh_initialized = false;
static boolean post_process = false;

// If false, dehacked cheat replacements are ignored.
boolean deh_apply_cheats = true;

static char **deh_filenames;

void AddDEHFileName(const char *filename)
{
    static int i;
    deh_filenames = I_Realloc(deh_filenames, (i + 2) * sizeof(*deh_filenames));
    deh_filenames[i++] = M_StringDuplicate(filename);
    deh_filenames[i] = NULL;
}

char **DEH_GetFileNames(void)
{
    return deh_filenames;
}

int DEH_ParseBexBitFlags(int ivalue, char *value, const bex_bitflags_t flags[], int len)
{
    if (!ivalue)
    {
        for (; (value = strtok(value, ",+| \t\f\r")); value = NULL)
        {
            for (int i = 0; i < len; i++)
            {
                if (!strcasecmp(value, flags[i].flag))
                {
                    ivalue |= flags[i].bits;
                    break;
                }
            }
        }
    }
    return ivalue;
}

void DEH_Checksum(sha1_digest_t digest)
{
    sha1_context_t sha1_context;
    SHA1_Init(&sha1_context);

    for (unsigned int i = 0; deh_section_types[i] != NULL; ++i)
    {
        if (deh_section_types[i]->sha1_hash != NULL)
        {
            deh_section_types[i]->sha1_hash(&sha1_context);
        }
    }

    SHA1_Final(digest, &sha1_context);
}

// Called on startup to call the Init functions
static void InitializeSections(void)
{
    for (unsigned int i = 0; deh_section_types[i] != NULL; ++i)
    {
        if (deh_section_types[i]->init != NULL)
        {
            deh_section_types[i]->init();
        }
    }
}

void DEH_Init(void) // [crispy] un-static
{
    //!
    // @category mod
    //
    // Ignore cheats in dehacked files.
    //
    if (M_CheckParm("-nocheats") > 0)
    {
        deh_apply_cheats = false;
    }

    // Call init functions for all the section definitions.
    InitializeSections();
    deh_initialized = true;
}

// Given a section name, get the section structure which corresponds
static deh_section_t *GetSectionByName(char *name)
{
    for (unsigned int i = 0; deh_section_types[i] != NULL; ++i)
    {
        if (!strcasecmp(deh_section_types[i]->name, name))
        {
            return deh_section_types[i];
        }
    }

    return NULL;
}

// Is the string passed just whitespace?
static boolean IsWhitespace(char *s)
{
    for (; *s; ++s)
    {
        if (!isspace(*s))
        {
            return false;
        }
    }

    return true;
}

// Strip whitespace from the start and end of a string
char *CleanString(char *s) // [crispy] un-static
{
    // Leading whitespace
    while (*s && isspace(*s))
    {
        ++s;
    }

    // Trailing whitespace
    char *strending = s + strlen(s) - 1;
    while (strlen(s) > 0 && isspace(*strending))
    {
        *strending = '\0';
        --strending;
    }

    return s;
}

// This pattern is used a lot of times in different sections,
// an assignment is essentially just a statement of the form:
//
// Variable Name = Value
//
// The variable name can include spaces or any other characters.
// The string is split on the '=', essentially.
//
// Returns true if read correctly
boolean DEH_ParseAssignment(char *line, char **variable_name, char **value)
{
    // find the equals
    char *p = strchr(line, '=');
    if (p == NULL)
    {
        return false;
    }

    // variable name at the start
    // turn the '=' into a \0 to terminate the string here

    *p = '\0';
    *variable_name = CleanString(line);

    // value immediately follows the '='
    *value = CleanString(p + 1);

    return true;
}

extern void DEH_SaveLineStart(deh_context_t *context);
extern void DEH_RestoreLineStart(deh_context_t *context);

static boolean CheckSignatures(deh_context_t *context)
{
    // [crispy] save pointer to start of line (should be 0 here)
    DEH_SaveLineStart(context);

    // Read the first line
    char *line = DEH_ReadLine(context, false);
    if (line == NULL)
    {
        return false;
    }

    // Check all signatures to see if one matches
    for (size_t i = 0; deh_signatures[i] != NULL; ++i)
    {
        if (!strcasecmp(deh_signatures[i], line))
        {
            return true;
        }
    }

    // [crispy] not a valid signature, try parsing this line again
    // and see if it starts with a section marker
    DEH_RestoreLineStart(context);

    return false;
}

// Parses a dehacked file by reading from the context
static void DEH_ParseContext(deh_context_t *context)
{
    deh_section_t *current_section = NULL;
    deh_section_t *prev_section = NULL; // [crispy] remember previous line parser
    void *tag = NULL;
    char *line;

    // Read the header and check it matches the signature
    if (!CheckSignatures(context))
    {
        // [crispy] make non-fatal
        // DEH_Debug(context, "Invalid DeHackEd signature found.");
    }

    // Read the file
    while (!DEH_HadError(context))
    {
        // Read the next line. We only allow the special extended parsing
        // for the BEX [STRINGS] section.
        boolean extended = current_section != NULL && !strcasecmp(current_section->name, "[STRINGS]");

        // [crispy] save pointer to start of line, just in case
        DEH_SaveLineStart(context);
        line = DEH_ReadLine(context, extended);

        // end of file?
        if (line == NULL)
        {
            return;
        }

        while (line[0] != '\0' && isspace(line[0]))
        {
            ++line;
        }

        // comments
        if (line[0] == '#')
        {
            continue;
        }

        if (IsWhitespace(line))
        {
            if (current_section != NULL)
            {
                // end of section
                if (current_section->end != NULL)
                {
                    current_section->end(context, tag);
                }

                // [crispy] if this was a BEX line parser, remember it in case
                // the next section does not start with a section marker
                if (current_section->name[0] == '[')
                {
                    prev_section = current_section;
                }
                else
                {
                    prev_section = NULL;
                }

                current_section = NULL;
            }
        }
        else
        {
            if (current_section != NULL)
            {
                // parse this line
                current_section->line_parser(context, line, tag);
            }
            else
            {
                // possibly the start of a new section
                char section_name[20];
                sscanf(line, "%19s", section_name);
                current_section = GetSectionByName(section_name);

                if (current_section != NULL)
                {
                    tag = current_section->start(context, line);
                }
                else if (prev_section != NULL)
                {
                    // [crispy] try this line again with the previous line parser
                    DEH_RestoreLineStart(context);
                    current_section = prev_section;
                    prev_section = NULL;
                }
                else
                {
                    // DEH_Debug(context, "Unkown DeHackEd section name '%s', WAD may not work properlly!", section_name);
                }
            }
        }
    }
}

// Parses a dehacked file
int DEH_LoadFile(const char *filename)
{
    post_process = true;
    if (!deh_initialized)
    {
        DEH_Init();
    }

    deh_context_t *context = DEH_OpenFile(filename);
    if (context == NULL)
    {
        I_Printf(VB_ERROR, "DEH_LoadFile: Unable to open '%s'.", filename);
        return 0;
    }

    AddDEHFileName(filename);

    DEH_ParseContext(context);

    if (DEH_HadError(context))
    {
        I_Error("Error parsing dehacked file");
    }

    DEH_CloseFile(context);

    return 1;
}

// Load all dehacked patches from the given directory.
void DEH_AutoLoadPatches(const char *path)
{
    const char *filename;
    glob_t *glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED, "*.deh", "*.bex");

    for (;;)
    {
        filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }
        DEH_LoadFile(filename);
    }

    I_EndGlob(glob);
}

// Load dehacked file from WAD lump.
void DEH_LoadLump(int lumpnum)
{
    post_process = true;
    if (!deh_initialized)
    {
        DEH_Init();
    }

    deh_context_t *context = DEH_OpenLump(lumpnum);
    if (context == NULL)
    {
        I_Printf(VB_WARNING, "DEH_LoadFile: Unable to open lump %i", lumpnum);
        return;
    }

    DEH_ParseContext(context);

    // If there was an error while parsing, abort.
    if (DEH_HadError(context))
    {
        I_Error("Error parsing dehacked lump");
    }

    DEH_CloseFile(context);
}

void DEH_LoadLumpByName(const char *name)
{
    int lumpnum = W_CheckNumForName(name);
    if (lumpnum == -1)
    {
        I_Printf(VB_WARNING, "DEH_LoadLumpByName: '%s' lump not found", name);
    }

    DEH_LoadLump(lumpnum);
}

// Check the command line for -deh argument, and others.
void DEH_ParseCommandLine(void)
{
    //!
    // @category mod
    // @arg <files>
    //
    // Load the given dehacked/bex patch(es)
    //
    int p = M_CheckParm("-deh");

    if (p > 0)
    {
        ++p;
        while (p < myargc && myargv[p][0] != '-')
        {
            char *filename = D_TryFindWADByName(myargv[p]);
            DEH_LoadFile(filename);
            free(filename);
            ++p;
        }
    }
}

void DEH_PostProcess(void)
{
    // sanity-check bfgcells and bfg ammopershot
    if (deh_set_bfgcells
        && weaponinfo[wp_bfg].intflags & WIF_ENABLEAPS
        && deh_bfg_cells_per_shot != weaponinfo[wp_bfg].ammopershot)
    {
        I_Error("Mismatch between bfgcells and bfg ammo per shot "
                "modifications! Check your dehacked.");
    }

    DEH_SetBannerGameDescription();

    if (post_process)
    {
        DEH_ValidateStateArgs();
    }

    // [FG] fix desyncs by SSG-flash correction
    if (DEH_CheckSafeState(S_DSGUNFLASH1) && states[S_DSGUNFLASH1].tics == 5)
    {
        states[S_DSGUNFLASH1].tics = 4;
    }

    DEH_FreeTables();
}

void DEH_InitTables(void)
{
    DEH_InitStates();
    DEH_InitSprites();
    DEH_InitSFX();
    DEH_InitMusic();
    DEH_InitMobjInfo();
}

void DEH_FreeTables(void)
{
    DEH_FreeStates();
    DEH_FreeSprites();
    DEH_FreeSFX();
    DEH_FreeMusic();
}
