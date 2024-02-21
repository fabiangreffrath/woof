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

#ifndef TESTCONFIG_H
#define TESTCONFIG_H

#include "doomtype.h"

typedef struct execute_context_s execute_context_t;

execute_context_t *NewExecuteContext(void);
void AddCmdLineParameter(execute_context_t *context, const char *s, ...) PRINTF_ATTR(2, 3);
void PassThroughArguments(execute_context_t *context);
int ExecuteDoom(execute_context_t *context);
int FindInstalledIWADs(void);
boolean OpenFolder(const char *path);

#endif /* #ifndef TESTCONFIG_H */

