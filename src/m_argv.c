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
#include <stdio.h>
#include <string.h>
#include "i_printf.h"
#include "i_system.h"

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
  int i;

  for (i = 1; i < myargc - num_args; i++)
  {
    if (!strcasecmp(check, myargv[i]))
      return i;
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
    I_Error("No parameter for '%s'.", myargv[p]);

  if (sscanf(myargv[p + arg], " %d", &result) != 1)
    I_Error("Invalid parameter '%s' for %s, must be a number.", myargv[p + arg], myargv[p]);

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


#include "params.h"

static int CheckArgs(int p)
{
  int i;

  ++p;

  for (i = p; i < myargc; ++i)
  {
    if (myargv[i][0] == '-')
      break;
  }

  if (i > p)
    return i;

  return 0;
}

static int CheckNumArgs(int p, int num_args)
{
  int i;

  ++p;

  for (i = p; i < p + num_args && i < myargc; ++i)
  {
    if (myargv[i][0] == '-')
      break;
  }

  if (i == p + num_args)
    return i;

  return 0;
}

void M_CheckCommandLine(void)
{
  int p = 1;

  while (p < myargc)
  {
    int i;
    int args = 0;

    // skip response file
    if (myargv[p][0] == '@')
    {
      ++p;
      continue;
    }

    for (i = 0; i < arrlen(params_with_args); ++i)
    {
      if (!strcasecmp(myargv[p], "-file") ||
          !strcasecmp(myargv[p], "-deh"))
      {
        args = -1;
        break;
      }
      else if (!strcasecmp(myargv[p], "-warp") ||
               !strcasecmp(myargv[p], "-recordfrom") ||
               !strcasecmp(myargv[p], "-recordfromto") ||
               !strcasecmp(myargv[p], "-dumptranmap"))
      {
        args = 2;
        break;
      }
      else if (!strcasecmp(myargv[p], params_with_args[i]))
      {
        args = 1;
        break;
      }
    }

    if (args)
    {
      int check = args > 0 ? CheckNumArgs(p, args) : CheckArgs(p);

      if (check)
      {
        p = check;
      }
      // -warp can have only one parameter
      else if (!strcasecmp(myargv[p], "-warp"))
      {
        check = CheckNumArgs(p, 1);
        if (check)
          p = check;
        else
          I_Error("No parameter for '-warp'.");
      }
      // -skipsec can be negative
      else if (!strcasecmp(myargv[p], "-skipsec") && p + 1 < myargc)
      {
        float min, sec;
        if (sscanf(myargv[p + 1], "%f:%f", &min, &sec) == 2 ||
            sscanf(myargv[p + 1], "%f", &sec) == 1)
          p += 2;
        else
          I_Error("No parameter for '-skipsec'.");
      }
      // -turbo has default value
      else if (!strcasecmp(myargv[p], "-turbo"))
      {
        ++p;
      }
      // -dogs has default value
      else if (!strcasecmp(myargv[p], "-dogs"))
      {
        ++p;
      }
      // -statdump and -dehout allow "-" parameter
      else if ((!strcasecmp(myargv[p], "-statdump")) &&
                p + 1 < myargc && !strcmp(myargv[p + 1], "-"))
      {
        p += 2;
      }
      else
      {
        I_Error("No parameter for '%s'.", myargv[p]);
      }

      continue;
    }

    for (i = 0; i < arrlen(params); ++i)
    {
       if (!strcasecmp(myargv[p], params[i]))
         break;
    }

    if (i == arrlen(params))
      I_Error("No such option '%s'.", myargv[p]);

    ++p;
  }
}

void M_PrintHelpString(void)
{
  I_Printf(VB_ALWAYS, "%s", HELP_STRING);
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
