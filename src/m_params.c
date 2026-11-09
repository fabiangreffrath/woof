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
#include "m_params_table.h"

#include "i_printf.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"

#include <stdio.h>

const param_def_t *M_FindParam(const char *arg)
{
    for (int i = 0; i < PARAM_TABLE_SIZE; i++)
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
    param_section_t current_section = SectionNone;

    p += M_snprintf(p, remaining, "Usage: " PROJECT_SHORTNAME " [options]\n\n");
    remaining = sizeof(buffer) - (p - buffer);

    for (int i = 0; i < PARAM_TABLE_SIZE; i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            p += M_snprintf(p, remaining, "\n%s:\n", section_names[current_section]);
            remaining = sizeof(buffer) - (p - buffer);
        }

        // Build option signature with argument based on type
        char *arg;
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

    param_section_t current_section = SectionNone;

    for (int i = 0; i < PARAM_TABLE_SIZE; i++)
    {
        const param_def_t *param = &param_table[i];

        if (param->section != current_section)
        {
            current_section = param->section;
            fprintf(f, "### %s\n\n", section_names[current_section]);
        }

        // Build option signature with argument based on type
        char opt_sig[256];
        char *arg;
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

    fclose(f);
    I_Printf(VB_INFO, "Generated markdown documentation: %s", filename);
}
