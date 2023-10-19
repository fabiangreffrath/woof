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
//  Nil.
//    
//-----------------------------------------------------------------------------


#ifndef __M_ARGV__
#define __M_ARGV__

#include "doomtype.h"

//
// MISC
//
extern int  myargc;
extern char **myargv;

// Returns the position of the given parameter in the arg list (0 if not found).
int M_CheckParm(const char *check);

// Same as M_CheckParm, but checks that num_args arguments are available
// following the specified argument.
int M_CheckParmWithArgs(const char *check, int num_args);

// Returns true if the given parameter exists in the program's command
// line arguments, false if not.
boolean M_ParmExists(const char *check);

void M_CheckCommandLine(void);
void M_PrintHelpString(void);

int M_ParmArgToInt(int p);
int M_ParmArg2ToInt(int p);

#endif

//----------------------------------------------------------------------------
//
// $Log: m_argv.h,v $
// Revision 1.3  1998/05/01  14:26:18  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:05  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
