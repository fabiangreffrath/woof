//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 Fabian Greffrath
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//      Load sound lumps with libsndfile.

#include "i_sndfile.h"

#include <AL/alext.h>
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>

typedef struct sfvio_data_s
{
    char *data;
    sf_count_t length, offset;
} sfvio_data_t;

static sf_count_t sfvio_get_filelen(void *user_data)
{
    sfvio_data_t *data = user_data;

    return data->length;
}

static sf_count_t sfvio_seek(sf_count_t offset, int whence, void *user_data)
{
    sfvio_data_t *data = user_data;
    const sf_count_t len = sfvio_get_filelen(user_data);

    switch (whence)
    {
      case SEEK_SET:
        data->offset = offset;
        break;

      case SEEK_CUR:
        data->offset += offset;
        break;

      case SEEK_END:
        data->offset = len + offset;
        break;
    }

    if (data->offset > len)
    {
        data->offset = len;
    }

    if (data->offset < 0)
    {
        data->offset = 0;
    }

    return data->offset;
}

static sf_count_t sfvio_read(void *ptr, sf_count_t count, void *user_data)
{
    sfvio_data_t *data = user_data;
    const sf_count_t len = sfvio_get_filelen(user_data);
    const sf_count_t remain = len - data->offset;

    if (count > remain)
    {
        count = remain;
    }

    if (count == 0)
    {
        return count;
    }

    memcpy(ptr, data->data + data->offset, count);

    data->offset += count;

    return count;
}

static sf_count_t sfvio_tell(void *user_data)
{
    sfvio_data_t *data = user_data;

    return data->offset;
}

static SF_VIRTUAL_IO sfvio =
{
    sfvio_get_filelen,
    sfvio_seek,
    sfvio_read,
    NULL,
    sfvio_tell
};

static sf_count_t sfx_mix_mono_read_float(SNDFILE *file, float *data, sf_count_t datalen)
{
    SF_INFO info = {0};
    static float multi_data[2048];
    int k, ch, frames_read;
    sf_count_t dataout = 0;

    sf_command(file, SFC_GET_CURRENT_SF_INFO, &info, sizeof(info));

    if (info.channels == 1)
        return sf_readf_float(file, data, datalen);

    while (dataout < datalen)
    {
        int this_read = MIN(arrlen(multi_data) / info.channels, datalen - dataout);

        frames_read = sf_readf_float(file, multi_data, this_read);

        if (frames_read == 0)
            break;

        for (k = 0; k < frames_read; k++)
        {
            float mix = 0.0f;

            for (ch = 0; ch < info.channels; ch++)
                mix += multi_data[k * info.channels + ch];

            data[dataout + k] = mix / info.channels;
        }

        dataout += frames_read;
    }

    return dataout;
}

static sf_count_t sfx_mix_mono_read_short(SNDFILE *file, short *data, sf_count_t datalen)
{
    SF_INFO info = {0};
    static short multi_data[2048];
    int k, ch, frames_read;
    sf_count_t dataout = 0;

    sf_command(file, SFC_GET_CURRENT_SF_INFO, &info, sizeof(info));

    if (info.channels == 1)
        return sf_readf_short(file, data, datalen);

    while (dataout < datalen)
    {
        int this_read = MIN(arrlen(multi_data) / info.channels, datalen - dataout);

        frames_read = sf_readf_short(file, multi_data, this_read);

        if (frames_read == 0)
            break;

        for (k = 0; k < frames_read; k++)
        {
            float mix = 0.0f;

            for (ch = 0; ch < info.channels; ch++)
                mix += multi_data[k * info.channels + ch];

            data[dataout + k] = mix / info.channels;
        }

        dataout += frames_read;
    }

    return dataout;
}

typedef enum
{
    Int16,
    Float,
    //IMA4,
    //MSADPCM
} sample_format_t;

typedef struct
{
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    sfvio_data_t sfdata;

    sample_format_t sample_format;
    ALenum format;
    ALint frame_size;
} sndfile_t;

static void CloseFile(sndfile_t *file)
{
    if (file->sndfile)
    {
        sf_close(file->sndfile);
        file->sndfile = NULL;
    }
}

static boolean OpenFile(sndfile_t *file, void *data, sf_count_t size)
{
    sample_format_t sample_format;
    ALenum format;
    ALint frame_size;

    file->sfdata.data = data;
    file->sfdata.length = size;
    file->sfdata.offset = 0;
    memset(&file->sfinfo, 0, sizeof(file->sfinfo));

    file->sndfile = sf_open_virtual(&sfvio, SFM_READ, &file->sfinfo, &file->sfdata);

    if (!file->sndfile)
    {
        fprintf(stderr, "sf_open_virtual: %s\n", sf_strerror(file->sndfile));
        return false;
    }

    if (file->sfinfo.frames <= 0 || file->sfinfo.channels <= 0)
    {
        CloseFile(file);
        return false;
    }

    // Detect a suitable format to load. Formats like Vorbis and Opus use float
    // natively, so load as float to avoid clipping when possible. Formats
    // larger than 16-bit can also use float to preserve a bit more precision.

    sample_format = Int16;

    switch ((file->sfinfo.format & SF_FORMAT_SUBMASK))
    {
        case SF_FORMAT_PCM_24:
        case SF_FORMAT_PCM_32:
        case SF_FORMAT_FLOAT:
        case SF_FORMAT_DOUBLE:
        case SF_FORMAT_VORBIS:
        case SF_FORMAT_OPUS:
        case SF_FORMAT_ALAC_20:
        case SF_FORMAT_ALAC_24:
        case SF_FORMAT_ALAC_32:
#ifdef HAVE_SNDFILE_MPEG
        case SF_FORMAT_MPEG_LAYER_I:
        case SF_FORMAT_MPEG_LAYER_II:
        case SF_FORMAT_MPEG_LAYER_III:
#endif
            if (alIsExtensionPresent("AL_EXT_FLOAT32"))
                sample_format = Float;
            break;
    }

    frame_size = 1;

    if (sample_format == Int16)
    {
        frame_size = file->sfinfo.channels * 2;
    }
    else if (sample_format == Float)
    {
        frame_size = file->sfinfo.channels * 4;
    }

    // Figure out the OpenAL format from the file and desired sample type.

    format = AL_NONE;

    if (file->sfinfo.channels == 1)
    {
        if (sample_format == Int16)
            format = AL_FORMAT_MONO16;
        else if (sample_format == Float)
            format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (file->sfinfo.channels == 2)
    {
        if (sample_format == Int16)
            format = AL_FORMAT_STEREO16;
        else if (sample_format == Float)
            format = AL_FORMAT_STEREO_FLOAT32;
    }

    if (format == AL_NONE)
    {
        fprintf(stderr, "Unsupported channel count: %d\n", file->sfinfo.channels);
        CloseFile(file);
        return false;
    }

    file->sample_format = sample_format;
    file->format = format;
    file->frame_size = frame_size;

    return true;
}

boolean I_SND_LoadFile(void *data, ALenum *format, byte **wavdata,
                       ALsizei *size, ALsizei *freq)
{
    sndfile_t file;
    sf_count_t num_frames = 0;
    void *local_wavdata = NULL;

    if (OpenFile(&file, data, *size) == false)
    {
        return false;
    }

    local_wavdata = malloc(file.sfinfo.frames * file.frame_size / file.sfinfo.channels);

    if (file.sample_format == Int16)
    {
        num_frames = sfx_mix_mono_read_short(file.sndfile, local_wavdata,
                                             file.sfinfo.frames);
    }
    else if (file.sample_format == Float)
    {
        num_frames = sfx_mix_mono_read_float(file.sndfile, local_wavdata,
                                             file.sfinfo.frames);
    }

    if (num_frames < file.sfinfo.frames)
    {
        fprintf(stderr, "sf_readf: %s\n", sf_strerror(file.sndfile));
        CloseFile(&file);
        free(local_wavdata);
        return false;
    }

    *wavdata = local_wavdata;
    *format = file.format;
    *size = num_frames * file.frame_size / file.sfinfo.channels;
    *freq = file.sfinfo.samplerate;

    CloseFile(&file);

    return true;
}

static sndfile_t stream;
static boolean looping;

boolean I_SND_OpenStream(void *data, ALsizei size, ALenum *format,
                         ALsizei *freq, ALsizei *frame_size)
{
    if (OpenFile(&stream, data, size) == false)
    {
        return false;
    }

    *format = stream.format;
    *freq = stream.sfinfo.samplerate;
    *frame_size = stream.frame_size;

    return true;
}

void I_SND_SetLooping(boolean on)
{
    looping = on;
}

uint32_t I_SND_FillStream(byte *data, uint32_t frames)
{
    sf_count_t num_frames = 0;

    if (stream.sample_format == Int16)
    {
        num_frames = sf_readf_short(stream.sndfile, (short *)data, frames);
    }
    else if (stream.sample_format == Float)
    {
        num_frames = sf_readf_float(stream.sndfile, (float *)data, frames);
    }

    if (num_frames < frames && looping)
    {
        sf_seek(stream.sndfile, 0, SEEK_SET);
    }

    return num_frames;
}

void I_SND_CloseStream(void)
{
    CloseFile(&stream);
}
