//
// Copyright(C) 2023 Roman Fomin
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
//

#ifndef __I_OALSTREAM__
#define __I_OALSTREAM__

#include "al.h"

#include "doomtype.h"

typedef struct stream_module_s
{
    boolean (*I_InitStream)(int device);
    boolean (*I_OpenStream)(void *data, ALsizei size, ALenum *format,
                            ALsizei *freq, ALsizei *frame_size);
    int (*I_FillStream)(byte *data, int frames);
    void (*I_PlayStream)(boolean looping);
    void (*I_CloseStream)(void);
    void (*I_ShutdownStream)(void);
    const char **(*I_DeviceList)(int *current_device);
} stream_module_t;

#endif
