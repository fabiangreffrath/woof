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

#include "xmp.h"

#include "doomtype.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_sound.h"

static xmp_context context;

static boolean stream_looping;

static void PrintError(int e)
{
    const char *msg;
    switch (e)
    {
        case -XMP_ERROR_INTERNAL:
            msg = "Internal error in libxmp.";
            break;
        case -XMP_ERROR_FORMAT:
            msg = "Unrecognized file format.";
            break;
        case -XMP_ERROR_LOAD:
            msg = "Error loading file.";
            break;
        case -XMP_ERROR_DEPACK:
            msg = "Error depacking file.";
            break;
        case -XMP_ERROR_SYSTEM:
            msg = "System error in libxmp.";
            break;
        case -XMP_ERROR_INVALID:
            msg = "Invalid parameter.";
            break;
        case -XMP_ERROR_STATE:
            msg = "Invalid player state.";
            break;
        default:
            msg = "Unknown error.";
            break;
    }
    I_Printf(VB_DEBUG, "XMP: %s", msg);
}

static boolean I_XMP_InitStream(int device)
{
    if (context)
    {
        return true;
    }

    context = xmp_create_context();

    if (!context)
    {
        I_Printf(VB_ERROR, "XMP: Failed to create context.");
        return false;
    }

    return true;
}

static boolean I_XMP_OpenStream(void *data, ALsizei size, ALenum *format,
                                ALsizei *freq, ALsizei *frame_size)
{
    if (!context)
    {
        return false;
    }

    int err = xmp_load_module_from_memory(context, data, (long)size);
    if (err < 0)
    {
        PrintError(err);
        return false;
    }

    *format = AL_FORMAT_STEREO16;
    *freq = SND_SAMPLERATE;
    *frame_size = 2 * sizeof(short);

    return true;
}

static int I_XMP_FillStream(byte *buffer, int buffer_samples)
{
    int ret = xmp_play_buffer(context, buffer, buffer_samples * 4,
                              stream_looping ? 0 : 1);

    if (ret < 0)
    {
        return 0;
    }

    return buffer_samples;
}

static void I_XMP_PlayStream(boolean looping)
{
    if (!context)
    {
        return;
    }

    stream_looping = looping;
    xmp_start_player(context, SND_SAMPLERATE, 0);
}

static void I_XMP_CloseStream(void)
{
    if (!context)
    {
        return;
    }

    xmp_stop_module(context);
    xmp_end_player(context);
    xmp_release_module(context);
}

static void I_XMP_ShutdownStream(void)
{
    if (!context)
    {
        return;
    }

    xmp_free_context(context);
    context = NULL;
}

static const char **I_XMP_DeviceList(void)
{
    return NULL;
}

stream_module_t stream_xmp_module =
{
    I_XMP_InitStream,
    I_XMP_OpenStream,
    I_XMP_FillStream,
    I_XMP_PlayStream,
    I_XMP_CloseStream,
    I_XMP_ShutdownStream,
    I_XMP_DeviceList,
};
