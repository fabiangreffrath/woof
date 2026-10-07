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

static boolean InitStream_MP3(int device)
{
    return true;
}

static boolean OpenStream_MP3(void *data, ALsizei size, ALenum *format,
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

static int FillStream_MP3(void *buffer, int buffer_samples)
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

static void PlayStream_MP3(boolean looping)
{
    stream_looping = looping;
    mp3dec_ex_seek(&dec, 0);
}

static void CloseStream_MP3(void)
{
    mp3dec_ex_close(&dec);
}

static void ShutdownStream_MP3(void)
{
    ;
}

static const char **DeviceList_MP3(void)
{
    return NULL;
}

static void BindVariables_MP3(void)
{
    ;
}

static const char *MusicFormat_MP3(void)
{
    static char buffer[16];
    M_snprintf(buffer, sizeof(buffer), "MP%d", dec.info.layer);
    return buffer;
}

stream_module_t stream_mp3_module = {
    .InitStream = InitStream_MP3,
    .OpenStream = OpenStream_MP3,
    .FillStream = FillStream_MP3,
    .PlayStream = PlayStream_MP3,
    .CloseStream = CloseStream_MP3,
    .ShutdownStream = ShutdownStream_MP3,
    .DeviceList = DeviceList_MP3,
    .BindVariables = BindVariables_MP3,
    .MusicFormat = MusicFormat_MP3,
};
