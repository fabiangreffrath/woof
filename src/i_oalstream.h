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

#include "doomtype.h"
#include "al.h"

typedef struct
{
    boolean (*I_OpenStream)(void *data, ALsizei size, ALenum *format,
                            ALsizei *freq, ALsizei *frame_size);
    uint32_t (*I_FillStream)(byte *data, uint32_t frames);
    void (*I_RestartStream)(void);
    void (*I_CloseStream)(void);
} stream_module_t;

#endif
