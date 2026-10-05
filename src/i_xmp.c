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

static boolean InitStream_XMP(int device)
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

static boolean OpenStream_XMP(void *data, ALsizei size, ALenum *format,
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

static int FillStream_XMP(void *buffer, int buffer_samples)
{
    int ret = xmp_play_buffer(context, buffer, buffer_samples * 4,
                              stream_looping ? 0 : 1);

    if (ret < 0)
    {
        if (stream_looping)
        {
            xmp_restart_module(context);
            xmp_set_position(context, 0);
        }
        else
        {
            return 0;
        }
    }

    return buffer_samples;
}

static void PlayStream_XMP(boolean looping)
{
    if (!context)
    {
        return;
    }

    stream_looping = looping;
    xmp_start_player(context, SND_SAMPLERATE, 0);
    xmp_set_player(context, XMP_PLAYER_VOLUME, 100);
}

static void CloseStream_XMP(void)
{
    if (!context)
    {
        return;
    }

    xmp_stop_module(context);
    xmp_end_player(context);
    xmp_release_module(context);
}

static void ShutdownStream_XMP(void)
{
    if (!context)
    {
        return;
    }

    xmp_free_context(context);
    context = NULL;
}

static const char **DeviceList_XMP(void)
{
    return NULL;
}

static void BindVariables_XMP(void)
{
    ;
}

static const char *MusicFormat_XMP(void)
{
    if (!context)
    {
        return "Unknown";
    }

    static struct xmp_module_info info;
    xmp_get_module_info(context, &info);
    return info.mod->type;
}

stream_module_t stream_xmp_module = {
    .InitStream = InitStream_XMP,
    .OpenStream = OpenStream_XMP,
    .FillStream = FillStream_XMP,
    .PlayStream = PlayStream_XMP,
    .CloseStream = CloseStream_XMP,
    .ShutdownStream = ShutdownStream_XMP,
    .DeviceList = DeviceList_XMP,
    .BindVariables = BindVariables_XMP,
    .MusicFormat = MusicFormat_XMP,
};
