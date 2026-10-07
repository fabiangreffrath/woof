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

#include <signal.h>
#include <stdlib.h>

#include "i_exit.h"
#include "i_printf.h"

typedef struct atexit_listentry_s
{
    atexit_func_t func;
    boolean run_on_error;
    struct atexit_listentry_s *next;
    const char *name;
} atexit_listentry_t;

// Schedule a function to be called when the program exits.
// If run_if_error is true, the function is called if the exit
// is due to an error (I_Error)

static atexit_listentry_t *exit_funcs[exit_priority_max];
static exit_priority_t exit_priority;

void I_AtExitPrio(atexit_func_t func, boolean run_on_error, const char *name,
                  exit_priority_t priority)
{
    atexit_listentry_t *entry = malloc(sizeof(*entry));
    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = exit_funcs[priority];
    entry->name = name;
    exit_funcs[priority] = entry;
}

// I_SafeExit
// This function is called instead of exit() by functions that might be called
// during the exit process (i.e. after exit() has already been called)

void I_SafeExit(int rc)
{
    atexit_listentry_t *entry;

    // Run through all exit functions

    for (; exit_priority < exit_priority_max; ++exit_priority)
    {
        while ((entry = exit_funcs[exit_priority]))
        {
            exit_funcs[exit_priority] = exit_funcs[exit_priority]->next;

            if (rc == 0 || entry->run_on_error)
            {
                I_Printf(VB_DEBUG, "Exit Sequence[%d]: %s (%d)", exit_priority,
                         entry->name, rc);
                entry->func();
            }
        }
    }

    exit(rc);
}

// Schedule a function to be called when the program receives a signal.

static atexit_listentry_t *atsignal_funcs;

void I_AtSignal(atexit_func_t func)
{
    atexit_listentry_t *entry = malloc(sizeof(*entry));
    entry->func = func;
    entry->next = atsignal_funcs;
    atsignal_funcs = entry;
}

static void I_SignalHandler(int sig)
{
    signal(sig, SIG_DFL);

    atexit_listentry_t *entry;

    while ((entry = atsignal_funcs))
    {
        atsignal_funcs = atsignal_funcs->next;
        entry->func();
    }

    raise(sig);
}

void I_Signal(void)
{
    const int sigs[] = {SIGABRT, SIGFPE, SIGILL, SIGSEGV};

    for (int i = 0; i < arrlen(sigs); i++)
    {
        signal(sigs[i], I_SignalHandler);
    }
}
