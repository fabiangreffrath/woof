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
//     OPL interface.
//

#include <stdio.h>
#include <string.h>

#include "m_io.h"
#include "opl.h"
#include "opl_internal.h"

//#define OPL_DEBUG_TRACE

extern opl_driver_t opl_sdl_driver;

static opl_driver_t *drivers[] =
{
    &opl_sdl_driver,
    NULL
};

static opl_driver_t *driver = NULL;

unsigned int opl_sample_rate = 22050;

//
// Init/shutdown code.
//

// Initialize the specified driver and detect an OPL chip.  Returns
// true if an OPL is detected.

static opl_init_result_t InitDriver(opl_driver_t *_driver,
                                    unsigned int port_base)
{
    // Initialize the driver.

    if (!_driver->init_func(port_base))
    {
        return OPL_INIT_NONE;
    }

    // The driver was initialized okay, so we now have somewhere
    // to write to.  It doesn't mean there's an OPL chip there,
    // though.  Perform the detection sequence to make sure.
    // (it's done twice, like how Doom does it).

    driver = _driver;

    printf("OPL_Init: Using driver '%s'.\n", driver->name);

    return OPL_INIT_OPL3;
}

// Find a driver automatically by trying each in the list.

static opl_init_result_t AutoSelectDriver(unsigned int port_base)
{
    int i;
    opl_init_result_t result;

    for (i=0; drivers[i] != NULL; ++i)
    {
        result = InitDriver(drivers[i], port_base);
        if (result != OPL_INIT_NONE)
        {
            return result;
        }
    }

    printf("OPL_Init: Failed to find a working driver.\n");

    return OPL_INIT_NONE;
}

// Initialize the OPL library. Return value indicates type of OPL chip
// detected, if any.

opl_init_result_t OPL_Init(unsigned int port_base)
{
    char *driver_name;
    int i;
    int result;

    driver_name = M_getenv("OPL_DRIVER");

    if (driver_name != NULL)
    {
        // Search the list until we find the driver with this name.

        for (i=0; drivers[i] != NULL; ++i)
        {
            if (!strcmp(driver_name, drivers[i]->name))
            {
                result = InitDriver(drivers[i], port_base);
                if (result)
                {
                    return result;
                }
                else
                {
                    printf("OPL_Init: Failed to initialize "
                           "driver: '%s'.\n", driver_name);
                    return OPL_INIT_NONE;
                }
            }
        }

        printf("OPL_Init: unknown driver: '%s'.\n", driver_name);

        return OPL_INIT_NONE;
    }
    else
    {
        return AutoSelectDriver(port_base);
    }
}

// Shut down the OPL library.

void OPL_Shutdown(void)
{
    if (driver != NULL)
    {
        driver->shutdown_func();
        driver = NULL;
    }
}

// Set the sample rate used for software OPL emulation.

void OPL_SetSampleRate(unsigned int rate)
{
    opl_sample_rate = rate;
}

void OPL_WritePort(opl_port_t port, unsigned int value)
{
    if (driver != NULL)
    {
#ifdef OPL_DEBUG_TRACE
        printf("OPL_write: %i, %x\n", port, value);
        fflush(stdout);
#endif
        driver->write_port_func(port, value);
    }
}

unsigned int OPL_ReadPort(opl_port_t port)
{
    if (driver != NULL)
    {
        unsigned int result;

#ifdef OPL_DEBUG_TRACE
        printf("OPL_read: %i...\n", port);
        fflush(stdout);
#endif

        result = driver->read_port_func(port);

#ifdef OPL_DEBUG_TRACE
        printf("OPL_read: %i -> %x\n", port, result);
        fflush(stdout);
#endif

        return result;
    }
    else
    {
        return 0;
    }
}

//
// Higher-level functions, based on the lower-level functions above
// (register write, etc).
//

unsigned int OPL_ReadStatus(void)
{
    return OPL_ReadPort(OPL_REGISTER_PORT);
}

// Write an OPL register value

void OPL_WriteRegister(int reg, int value)
{
    int i;

    if (reg & 0x100)
    {
        OPL_WritePort(OPL_REGISTER_PORT_OPL3, reg);
    }
    else
    {
        OPL_WritePort(OPL_REGISTER_PORT, reg);
    }

    // For timing, read the register port six times after writing the
    // register number to cause the appropriate delay

    for (i=0; i<6; ++i)
    {
        OPL_ReadPort(OPL_DATA_PORT);
    }

    OPL_WritePort(OPL_DATA_PORT, value);

    // Read the register port 24 times after writing the value to
    // cause the appropriate delay

    for (i=0; i<24; ++i)
    {
        OPL_ReadStatus();
    }
}

// Initialize registers on startup

void OPL_InitRegisters(int opl3)
{
    int r;

    // Initialize level registers

    for (r=OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
    {
        OPL_WriteRegister(r, 0x3f);
    }

    // Initialize other registers
    // These two loops write to registers that actually don't exist,
    // but this is what Doom does ...
    // Similarly, the <= is also intenational.

    for (r=OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
    {
        OPL_WriteRegister(r, 0x00);
    }

    // More registers ...

    for (r=1; r < OPL_REGS_LEVEL; ++r)
    {
        OPL_WriteRegister(r, 0x00);
    }

    // Re-initialize the low registers:

    // Reset both timers and enable interrupts:
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,      0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,      0x80);

    // "Allow FM chips to control the waveform of each operator":
    OPL_WriteRegister(OPL_REG_WAVEFORM_ENABLE, 0x20);

    if (opl3)
    {
        OPL_WriteRegister(OPL_REG_NEW, 0x01);

        // Initialize level registers

        for (r=OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x3f);
        }

        // Initialize other registers
        // These two loops write to registers that actually don't exist,
        // but this is what Doom does ...
        // Similarly, the <= is also intenational.

        for (r=OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x00);
        }

        // More registers ...

        for (r=1; r < OPL_REGS_LEVEL; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x00);
        }
    }

    // Keyboard split point on (?)
    OPL_WriteRegister(OPL_REG_FM_MODE,         0x40);

    if (opl3)
    {
        OPL_WriteRegister(OPL_REG_NEW, 0x01);
    }
}

//
// Timer functions.
//

void OPL_SetCallback(uint64_t us, opl_callback_t callback, void *data)
{
    if (driver != NULL)
    {
        driver->set_callback_func(us, callback, data);
    }
}

void OPL_ClearCallbacks(void)
{
    if (driver != NULL)
    {
        driver->clear_callbacks_func();
    }
}

void OPL_AdjustCallbacks(float value)
{
    if (driver != NULL)
    {
        driver->adjust_callbacks_func(value);
    }
}

