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
// DESCRIPTION:
//     OPL internal interface.
//


#ifndef OPL_INTERNAL_H
#define OPL_INTERNAL_H

#include "opl.h"

typedef int (*opl_init_func)(unsigned int port_base, int num_chips);
typedef void (*opl_shutdown_func)(void);
typedef unsigned int (*opl_read_port_func)(int chip, opl_port_t port);
typedef void (*opl_write_port_func)(int chip, opl_port_t port, unsigned int value);
typedef void (*opl_set_callback_func)(uint64_t us,
                                      opl_callback_t callback,
                                      void *data);
typedef void (*opl_clear_callbacks_func)(void);
typedef void (*opl_adjust_callbacks_func)(float value);

typedef struct
{
    const char *name;

    opl_init_func init_func;
    opl_shutdown_func shutdown_func;
    opl_read_port_func read_port_func;
    opl_write_port_func write_port_func;
    opl_set_callback_func set_callback_func;
    opl_clear_callbacks_func clear_callbacks_func;
    opl_adjust_callbacks_func adjust_callbacks_func;
} opl_driver_t;

extern opl_driver_t opl_emu_driver;

// Sample rate to use when doing software emulation.

#endif /* #ifndef OPL_INTERNAL_H */

