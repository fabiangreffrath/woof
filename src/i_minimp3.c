//
// Copyright(C) 2025 Roman Fomin
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

#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#include "i_oalstream.h"
#include "i_printf.h"
#include "m_misc.h"

static mp3dec_ex_t dec;

static boolean stream_looping;

static boolean I_MP3_InitStream(int device)
{
    return true;
}

static boolean I_MP3_OpenStream(void *data, ALsizei size, ALenum *format,
                                ALsizei *freq, ALsizei *frame_size)
{
    if (mp3dec_ex_open_buf(&dec, data, size, MP3D_SEEK_TO_SAMPLE))
    {
        I_Printf(VB_DEBUG, "I_MP3_OpenStream: Failed to open MP3");
        return false;
    }

    if (dec.info.channels < 1 || dec.info.channels > 2)
    {
        I_Printf(VB_WARNING, "I_MP3_OpenStream: Unsupported number of channels");
        return false;
    }

    *format = dec.info.channels == 1 ? AL_FORMAT_MONO_FLOAT32
                                     : AL_FORMAT_STEREO_FLOAT32;
    *freq = dec.info.hz;
    *frame_size = dec.info.channels * sizeof(float);
    return true;
}

static int I_MP3_FillStream(void *buffer, int buffer_samples)
{
    size_t amount = dec.info.channels * buffer_samples;
    size_t readed = mp3dec_ex_read(&dec, buffer, amount);

    if (readed != amount)
    {
        if (dec.last_error)
        {
            I_Printf(VB_ERROR, "I_MP3_FillStream: Failed to decode");
        }
        else if (stream_looping)
        {
            mp3dec_ex_seek(&dec, 0);
        }
    }

    return readed / dec.info.channels;
}

static void I_MP3_PlayStream(boolean looping)
{
    stream_looping = looping;
    mp3dec_ex_seek(&dec, 0);
}

static void I_MP3_CloseStream(void)
{
    mp3dec_ex_close(&dec);
}

static void I_MP3_ShutdownStream(void)
{
    ;
}

static const char **I_MP3_DeviceList(void)
{
    return NULL;
}

static void I_MP3_BindVariables(void)
{
    ;
}

static const char *I_MP3_MusicFormat(void)
{
    static char buffer[16];
    M_snprintf(buffer, sizeof(buffer), "MP%d", dec.info.layer);
    return buffer;
}

stream_module_t stream_mp3_module =
{
    I_MP3_InitStream,
    I_MP3_OpenStream,
    I_MP3_FillStream,
    I_MP3_PlayStream,
    I_MP3_CloseStream,
    I_MP3_ShutdownStream,
    I_MP3_DeviceList,
    I_MP3_BindVariables,
    I_MP3_MusicFormat,
};
