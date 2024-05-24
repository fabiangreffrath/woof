//
//  Copyright (C) 2023 Fabian Greffrath
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
// DESCRIPTION: clean interface for logging to console
//
//-----------------------------------------------------------------------------

#ifndef __I_PRINT__
#define __I_PRINT__

#include "doomtype.h"

typedef enum
{
    VB_ALWAYS,
    VB_ERROR,
    VB_WARNING,
    VB_INFO,
    VB_DEBUG,
    VB_MAX
} verbosity_t;

int I_ConsoleStdout(void);

void I_InitPrintf(void);
void I_Printf(verbosity_t prio, const char *msg, ...) PRINTF_ATTR(2, 3);
void I_PutChar(verbosity_t prio, int c);

#endif
