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

typedef enum
{
    Int16,
    Float,
    //IMA4,
    //MSADPCM
} format_t;

boolean Load_SNDFile(void *data, ALenum *format, byte **wavdata, ALsizei *size, ALsizei *freq)
{
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    SF_VIRTUAL_IO sfvio =
    {
        sfvio_get_filelen,
        sfvio_seek,
        sfvio_read,
        NULL,
        sfvio_tell
    };
    sfvio_data_t sfdata;
    sf_count_t num_frames;

    format_t sample_format = Int16;
    ALint byteblockalign = 0;
    ALenum local_format = AL_NONE;
    void *local_wavdata = NULL;

    sfdata.data = data;
    sfdata.length = *size;
    sfdata.offset = 0;

    memset(&sfinfo, 0, sizeof(sfinfo));

    sndfile = sf_open_virtual(&sfvio, SFM_READ, &sfinfo, &sfdata);

    if (!sndfile)
    {
        fprintf(stderr, "sf_open_virtual: %s\n", sf_strerror(sndfile));
        return false;
    }

    if (sfinfo.frames <= 0 || sfinfo.channels <= 0)
    {
        sf_close(sndfile);
        return false;
    }

    // Detect a suitable format to load. Formats like Vorbis and Opus use float
    // natively, so load as float to avoid clipping when possible. Formats
    // larger than 16-bit can also use float to preserve a bit more precision.

    switch ((sfinfo.format & SF_FORMAT_SUBMASK))
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
#if 0
        case 0x0080/*SF_FORMAT_MPEG_LAYER_I*/:
        case 0x0081/*SF_FORMAT_MPEG_LAYER_II*/:
        case 0x0082/*SF_FORMAT_MPEG_LAYER_III*/:
#endif
            if (alIsExtensionPresent("AL_EXT_FLOAT32"))
                sample_format = Float;
            break;
    }

    if (sample_format == Int16)
    {
        byteblockalign = sfinfo.channels * 2;
    }
    else if (sample_format == Float)
    {
        byteblockalign = sfinfo.channels * 4;
    }

    // Figure out the OpenAL format from the file and desired sample type.
    if (sfinfo.channels == 1)
    {
        if (sample_format == Int16)
            local_format = AL_FORMAT_MONO16;
        else if (sample_format == Float)
            local_format = AL_FORMAT_MONO_FLOAT32;
    }
    else if(sfinfo.channels == 2)
    {
        if (sample_format == Int16)
            local_format = AL_FORMAT_STEREO16;
        else if (sample_format == Float)
            local_format = AL_FORMAT_STEREO_FLOAT32;
    }

    if (local_format == AL_NONE)
    {
        fprintf(stderr, "Unsupported channel count: %d\n", sfinfo.channels);
        sf_close(sndfile);
        return false;
    }

    local_wavdata = malloc((size_t)(sfinfo.frames * byteblockalign));

    if (sample_format == Int16)
        num_frames = sf_readf_short(sndfile, local_wavdata, sfinfo.frames);
    else if (sample_format == Float)
        num_frames = sf_readf_float(sndfile, local_wavdata, sfinfo.frames);

    if (num_frames < sfinfo.frames)
    {
        fprintf(stderr, "sf_readf: %s\n", sf_strerror(sndfile));
        sf_close(sndfile);
        free(local_wavdata);
        return false;
    }

    sf_close(sndfile);

    *wavdata = (byte *)local_wavdata;
    *format = local_format;
    *size = (ALsizei)(num_frames * byteblockalign);
    *freq = sfinfo.samplerate;

    return true;
}
