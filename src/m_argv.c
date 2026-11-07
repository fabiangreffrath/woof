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
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_params.h"

#include <stdio.h>

int    myargc;
char **myargv;

//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
//

int M_CheckParmWithArgs(const char *check, int num_args)
{
    for (int i = 1; i < myargc - num_args; i++)
    {
        if (!strcasecmp(check, myargv[i]))
        {
            return i;
        }
    }

    return 0;
}

int M_CheckParm(const char *check)
{
    return M_CheckParmWithArgs(check, 0);
}

//
// M_ParmExists
//
// Returns true if the given parameter exists in the program's command
// line arguments, false if not.
//

boolean M_ParmExists(const char *check)
{
    return M_CheckParm(check) != 0;
}

static int ArgToInt(int p, int arg)
{
    int result;

    if (p + arg >= myargc)
    {
        I_Error("No parameter for '%s'.", myargv[p]);
    }

    if (sscanf(myargv[p + arg], " %d", &result) != 1)
    {
        I_Error("Invalid parameter '%s' for %s, must be a number.",
                myargv[p + arg], myargv[p]);
    }

    return result;
}

int M_ParmArgToInt(int p)
{
    return ArgToInt(p, 1);
}

int M_ParmArg2ToInt(int p)
{
    return ArgToInt(p, 2);
}

static int CountArgs(int p)
{
    int count = 0;
    ++p;
    while (p < myargc && myargv[p][0] != '-')
    {
        ++count;
        ++p;
    }
    return count;
}

static void ValidateArgs(int p, const param_def_t *param)
{
    int num_args = CountArgs(p);

    switch (param->type)
    {
        case TypeFlag:
            if (num_args > 0)
            {
                I_Error("Option '%s' does not take arguments.", myargv[p]);
            }
            break;

        case TypeString:
        case TypeInt:
        case TypeFloat:
            if (num_args < 1)
            {
                I_Error("Option '%s' requires an argument.", myargv[p]);
            }
            if (num_args > 1)
            {
                I_Error("Option '%s' takes only one argument.", myargv[p]);
            }
            break;

        case TypeFileList:
            if (num_args < 1)
            {
                I_Error("Option '%s' requires at least one file argument.",
                        myargv[p]);
            }
            break;

        case TypeTwoArgs:
            if (num_args != 2)
            {
                I_Error("Option '%s' requires exactly two arguments.",
                        myargv[p]);
            }
            break;

        case TypeOneOrTwoArgs:
            if (num_args < 1 || num_args > 2)
            {
                I_Error("Option '%s' requires one or two arguments.",
                        myargv[p]);
            }
            break;

        case TypeOptArg:
            if (num_args > 1)
            {
                I_Error("Option '%s' takes at most one optional argument.",
                        myargv[p]);
            }
            break;
    }
}

void M_CheckCommandLine(void)
{
    int p = 1;

    while (p < myargc)
    {
        // skip response file
        if (myargv[p][0] == '@')
        {
            ++p;
            continue;
        }

        // Must start with -
        if (myargv[p][0] != '-')
        {
            I_Error("Unexpected argument '%s'. Expected an option starting "
                    "with '-'.",
                    myargv[p]);
        }

        const param_def_t *param = M_FindParam(myargv[p]);

        if (!param)
        {
            I_Error("No such option '%s'.", myargv[p]);
        }

        ValidateArgs(p, param);

        // Special handling for -file and -deh (variable number of args)
        if (param->type == TypeFileList)
        {
            // Skip all file arguments until next option
            ++p;
            while (p < myargc && myargv[p][0] != '-')
            {
                ++p;
            }
        }
        else if (param->type == TypeTwoArgs || param->type == TypeOneOrTwoArgs)
        {
            // Skip 1 or 2 arguments
            int num_args = CountArgs(p);
            p += 1 + num_args;
        }
        else if (param->type == TypeOptArg)
        {
            // Skip 0 or 1 argument
            int num_args = CountArgs(p);
            p += 1 + num_args;
        }
        else
        {
            // Skip the option and its required arguments
            int num_args = CountArgs(p);
            p += 1 + num_args;
        }
    }
}

void M_PrintHelpString(void)
{
    I_Printf(VB_ALWAYS, "%s", M_GetParamHelpString());
}

//----------------------------------------------------------------------------
//
// $Log: m_argv.c,v $
// Revision 1.5  1998/05/03  22:51:40  killough
// beautification
//
// Revision 1.4  1998/05/01  14:26:14  killough
// beautification
//
// Revision 1.3  1998/05/01  14:23:29  killough
// beautification
//
// Revision 1.2  1998/01/26  19:23:40  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
