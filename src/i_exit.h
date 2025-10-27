//
// Copyright(C) 2022-2025 Fabian Greffrath
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
//    I_AtExit() and I_AtSignal() implementations.
//

#ifndef __I_EXIT__
#define __I_EXIT__

#include "doomtype.h"

typedef enum
{
    exit_priority_first,
    exit_priority_normal,
    exit_priority_last,
    exit_priority_verylast,
    exit_priority_max,
} exit_priority_t;

typedef void (*atexit_func_t)(void);

void I_AtExitPrio(atexit_func_t func, boolean run_if_error, const char *name,
                  exit_priority_t priority);
#define I_AtExit(func, run_if_error) \
    I_AtExitPrio(func, run_if_error, #func, exit_priority_normal)

void I_AtSignal(atexit_func_t func);

NORETURN void I_SafeExit(int rc);

void I_Signal(void);

#endif
