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

#include "al.h"
#include "alext.h"
#include "sndfile.h"

#include "i_sndfile.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "m_swap.h"
#include "memio.h"

typedef struct
{
    unsigned int freq;
    int start_time, end_time;
} loop_metadata_t;

#define LOOP_START_TAG      "LOOP_START"
#define LOOP_END_TAG        "LOOP_END"
#define FLAC_VORBIS_COMMENT 4
#define OGG_COMMENT_HEADER  3

// Given a time string (for LOOP_START/LOOP_END), parse it and return
// the time (in # samples since start of track) it represents.
static unsigned int ParseVorbisTime(unsigned int freq, char *value)
{
    char *num_start, *p;
    unsigned int result = 0;
    char c;

    if (strchr(value, ':') == NULL)
    {
        return atoi(value);
    }

    result = 0;
    num_start = value;

    for (p = value; *p != '\0'; ++p)
    {
        if (*p == '.' || *p == ':')
        {
            c = *p;
            *p = '\0';
            result = result * 60 + atoi(num_start);
            num_start = p + 1;
            *p = c;
        }

        if (*p == '.')
        {
            return result * freq + (unsigned int)(atof(p) * freq);
        }
    }

    return (result * 60 + atoi(num_start)) * freq;
}

// Given a vorbis comment string (eg. "LOOP_START=12345"), set fields
// in the metadata structure as appropriate.
static void ParseVorbisComment(loop_metadata_t *metadata, char *comment)
{
    char *eq, *key, *value;

    eq = strchr(comment, '=');

    if (eq == NULL)
    {
        return;
    }

    key = comment;
    *eq = '\0';
    value = eq + 1;

    if (!strcmp(key, LOOP_START_TAG))
    {
        metadata->start_time = ParseVorbisTime(metadata->freq, value);
    }
    else if (!strcmp(key, LOOP_END_TAG))
    {
        metadata->end_time = ParseVorbisTime(metadata->freq, value);
    }
}

// Parse a vorbis comments structure, reading from the given file.
static void ParseVorbisComments(loop_metadata_t *metadata, MEMFILE *fs)
{
    uint32_t buf;
    unsigned int num_comments, i, comment_len;
    char *comment;

    // Skip the starting part we don't care about.
    if (mem_fread(&buf, 4, 1, fs) < 1)
    {
        return;
    }
    if (mem_fseek(fs, LONG(buf), SEEK_CUR) != 0)
    {
        return;
    }

    // Read count field for number of comments.
    if (mem_fread(&buf, 4, 1, fs) < 1)
    {
        return;
    }
    num_comments = LONG(buf);

    // Read each individual comment.
    for (i = 0; i < num_comments; ++i)
    {
        // Read length of comment.
        if (mem_fread(&buf, 4, 1, fs) < 1)
        {
            return;
        }

        comment_len = LONG(buf);

        // Read actual comment data into string buffer.
        comment = calloc(1, comment_len + 1);
        if (comment == NULL
            || mem_fread(comment, 1, comment_len, fs) < comment_len)
        {
            free(comment);
            break;
        }

        // Parse comment string.
        ParseVorbisComment(metadata, comment);
        free(comment);
    }
}

static void ParseOggFile(loop_metadata_t *metadata, MEMFILE *fs)
{
    byte buf[7];
    unsigned int offset;

    // Scan through the start of the file looking for headers. They
    // begin '[byte]vorbis' where the byte value indicates header type.
    memset(buf, 0, sizeof(buf));

    for (offset = 0; offset < 100 * 1024; ++offset)
    {
        // buf[] is used as a sliding window. Each iteration, we
        // move the buffer one byte to the left and read an extra
        // byte onto the end.
        memmove(buf, buf + 1, sizeof(buf) - 1);

        if (mem_fread(&buf[6], 1, 1, fs) < 1)
        {
            return;
        }

        if (!memcmp(buf + 1, "vorbis", 6))
        {
            if (buf[0] == OGG_COMMENT_HEADER)
            {
                ParseVorbisComments(metadata, fs);
            }
        }
    }
}

static void ParseFlacFile(loop_metadata_t *metadata, MEMFILE *fs)
{
    byte header[4];
    unsigned int block_type;
    size_t block_len;
    boolean last_block;

    // Skip header.
    if (mem_fseek(fs, 4, MEM_SEEK_SET))
    {
        return;
    }

    for (;;)
    {
        long pos = -1;

        // Read METADATA_BLOCK_HEADER:
        if (mem_fread(header, 4, 1, fs) < 1)
        {
            return;
        }

        block_type = header[0] & ~0x80;
        last_block = (header[0] & 0x80) != 0;
        block_len = (header[1] << 16) | (header[2] << 8) | header[3];

        pos = mem_ftell(fs);
        if (pos < 0)
        {
            return;
        }

        if (block_type == FLAC_VORBIS_COMMENT)
        {
            ParseVorbisComments(metadata, fs);
        }

        if (last_block)
        {
            break;
        }

        // Seek to start of next block.
        if (mem_fseek(fs, pos + block_len, SEEK_SET) != 0)
        {
            return;
        }
    }
}

static sf_count_t sfvio_get_filelen(void *user_data)
{
    MEMFILE *fs = user_data;
    long pos;
    sf_count_t len;

    pos = mem_ftell(fs);
    mem_fseek(fs, 0, MEM_SEEK_END);
    len = mem_ftell(fs);
    mem_fseek(fs, pos, MEM_SEEK_SET);

    return len;
}

static sf_count_t sfvio_seek(sf_count_t offset, int whence, void *user_data)
{
    MEMFILE *fs = user_data;

    mem_fseek(fs, offset, whence);

    return mem_ftell(fs);
}

static sf_count_t sfvio_read(void *ptr, sf_count_t count, void *user_data)
{
    return mem_fread(ptr, 1, count, (MEMFILE *)user_data);
}

static sf_count_t sfvio_tell(void *user_data)
{
    return mem_ftell((MEMFILE *)user_data);
}

static SF_VIRTUAL_IO sfvio =
{
    sfvio_get_filelen,
    sfvio_seek,
    sfvio_read,
    NULL,
    sfvio_tell
};

static sf_count_t sfx_mix_mono_read_float(SNDFILE *file, float *data,
                                          sf_count_t datalen)
{
    SF_INFO info = {0};

    sf_command(file, SFC_GET_CURRENT_SF_INFO, &info, sizeof(info));

    if (info.channels == 1)
    {
        return sf_readf_float(file, data, datalen);
    }

    static float multi_data[2048];
    int k, ch, frames_read;
    sf_count_t dataout = 0;

    while (dataout < datalen)
    {
        int this_read =
            MIN(arrlen(multi_data) / info.channels, datalen - dataout);

        frames_read = sf_readf_float(file, multi_data, this_read);

        if (frames_read == 0)
        {
            break;
        }

        for (k = 0; k < frames_read; k++)
        {
            float mix = 0.0f;

            for (ch = 0; ch < info.channels; ch++)
            {
                mix += multi_data[k * info.channels + ch];
            }

            data[dataout + k] = mix / info.channels;
        }

        dataout += frames_read;
    }

    return dataout;
}

static sf_count_t sfx_mix_mono_read_short(SNDFILE *file, short *data,
                                          sf_count_t datalen)
{
    SF_INFO info = {0};

    sf_command(file, SFC_GET_CURRENT_SF_INFO, &info, sizeof(info));

    if (info.channels == 1)
    {
        return sf_readf_short(file, data, datalen);
    }

    static short multi_data[2048];
    int k, ch, frames_read;
    sf_count_t dataout = 0;

    while (dataout < datalen)
    {
        int this_read =
            MIN(arrlen(multi_data) / info.channels, datalen - dataout);

        frames_read = sf_readf_short(file, multi_data, this_read);

        if (frames_read == 0)
        {
            break;
        }

        for (k = 0; k < frames_read; k++)
        {
            float mix = 0.0f;

            for (ch = 0; ch < info.channels; ch++)
            {
                mix += multi_data[k * info.channels + ch];
            }

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
    // IMA4,
    // MSADPCM
} sample_format_t;

typedef struct
{
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    MEMFILE *sfdata;

    sample_format_t sample_format;
    ALenum format;
    ALint frame_size;
} sndfile_t;

static void CloseFile(sndfile_t *file)
{
    if (file->sfdata)
    {
        mem_fclose(file->sfdata);
        file->sfdata = NULL;
    }
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

    file->sfdata = mem_fopen_read(data, size);
    memset(&file->sfinfo, 0, sizeof(file->sfinfo));

    file->sndfile =
        sf_open_virtual(&sfvio, SFM_READ, &file->sfinfo, file->sfdata);

    if (!file->sndfile)
    {
        I_Printf(VB_DEBUG, "SndFile: %s", sf_strerror(file->sndfile));
        return false;
    }

    if (file->sfinfo.frames <= 0 || file->sfinfo.channels <= 0)
    {
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
            {
                sample_format = Float;
            }
            break;

        case SF_FORMAT_IMA_ADPCM:
        case SF_FORMAT_MS_ADPCM:
            // ADPCM formats require setting a block alignment as specified in
            // the file, which needs to be read from the wave 'fmt ' chunk
            // manually since libsndfile doesn't provide it in a
            // format-agnostic way. For now load it as 16-bit and have
            // libsndfile do the conversion.
            sample_format = Int16;
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
        {
            format = AL_FORMAT_MONO16;
        }
        else if (sample_format == Float)
        {
            format = AL_FORMAT_MONO_FLOAT32;
        }
    }
    else if (file->sfinfo.channels == 2)
    {
        if (sample_format == Int16)
        {
            format = AL_FORMAT_STEREO16;
        }
        else if (sample_format == Float)
        {
            format = AL_FORMAT_STEREO_FLOAT32;
        }
    }

    if (format == AL_NONE)
    {
        I_Printf(VB_ERROR, "SndFile: Unsupported channel count %d.",
                 file->sfinfo.channels);
        return false;
    }

    file->sample_format = sample_format;
    file->format = format;
    file->frame_size = frame_size;

    return true;
}

static void FadeInOutMono16(short *data, ALsizei size, ALsizei freq)
{
    const int len = size / sizeof(short);
    const int fadelen = freq * FADETIME / 1000000;
    int i;

    if (len < fadelen)
    {
        return;
    }

    if (data[0])
    {
        for (i = 0; i < fadelen; i++)
        {
            data[i] = data[i] * i / fadelen;
        }
    }

    if (data[len - 1])
    {
        for (i = 0; i < fadelen; i++)
        {
            data[len - 1 - i] = data[len - 1 - i] * i / fadelen;
        }
    }
}

static void FadeInOutMonoFloat32(float *data, ALsizei size, ALsizei freq)
{
    const int len = size / sizeof(float);
    const int fadelen = freq * FADETIME / 1000000;
    int i;

    if (len < fadelen)
    {
        return;
    }

    if (fabsf(data[0]) > 0.000001f)
    {
        for (i = 0; i < fadelen; i++)
        {
            data[i] = data[i] * i / fadelen;
        }
    }

    if (fabsf(data[len - 1]) > 0.000001f)
    {
        for (i = 0; i < fadelen; i++)
        {
            data[len - 1 - i] = data[len - 1 - i] * i / fadelen;
        }
    }
}

boolean I_SND_LoadFile(void *data, ALenum *format, byte **wavdata,
                       ALsizei *size, ALsizei *freq, boolean looping)
{
    sndfile_t file = {0};
    sf_count_t num_frames = 0;
    void *local_wavdata = NULL;

    if (OpenFile(&file, data, *size) == false)
    {
        CloseFile(&file);
        return false;
    }

    local_wavdata =
        malloc(file.sfinfo.frames * file.frame_size / file.sfinfo.channels);

    if (file.sample_format == Int16)
    {
        num_frames = sfx_mix_mono_read_short(file.sndfile, local_wavdata,
                                             file.sfinfo.frames);
        *format = AL_FORMAT_MONO16;
    }
    else if (file.sample_format == Float)
    {
        num_frames = sfx_mix_mono_read_float(file.sndfile, local_wavdata,
                                             file.sfinfo.frames);
        *format = AL_FORMAT_MONO_FLOAT32;
    }

    if (num_frames < file.sfinfo.frames)
    {
        I_Printf(VB_ERROR, "sf_readf: %s", sf_strerror(file.sndfile));
        CloseFile(&file);
        free(local_wavdata);
        return false;
    }

    *size = num_frames * file.frame_size / file.sfinfo.channels;
    *freq = file.sfinfo.samplerate;

    // Fade in or out sounds that start or end at a non-zero amplitude to
    // prevent clicking.
    if (!looping)
    {
        if (*format == AL_FORMAT_MONO16)
        {
            FadeInOutMono16(local_wavdata, *size, *freq);
        }
        else if (*format == AL_FORMAT_MONO_FLOAT32)
        {
            FadeInOutMonoFloat32(local_wavdata, *size, *freq);
        }
    }

    *wavdata = local_wavdata;

    CloseFile(&file);

    return true;
}

static boolean I_SND_InitStream(int device)
{
    return true;
}

static sndfile_t stream;

static loop_metadata_t loop;
static boolean stream_looping;

static boolean I_SND_OpenStream(void *data, ALsizei size, ALenum *format,
                                ALsizei *freq, ALsizei *frame_size)
{
    MEMFILE *fs;

    if (OpenFile(&stream, data, size) == false)
    {
        CloseFile(&stream);
        return false;
    }

    fs = mem_fopen_read(data, size);

    loop.freq = stream.sfinfo.samplerate;
    loop.start_time = 0;
    loop.end_time = 0;

    switch ((stream.sfinfo.format & SF_FORMAT_TYPEMASK))
    {
        case SF_FORMAT_FLAC:
            ParseFlacFile(&loop, fs);
            break;
        case SF_FORMAT_OGG:
            ParseOggFile(&loop, fs);
            break;
    }

    mem_fclose(fs);

    *format = stream.format;
    *freq = stream.sfinfo.samplerate;
    *frame_size = stream.frame_size;

    return true;
}

static void I_SND_PlayStream(boolean looping)
{
    stream_looping = looping;
}

static int I_SND_FillStream(byte *data, int frames)
{
    sf_count_t filled = 0;
    boolean restart = false;

    if (loop.end_time)
    {
        sf_count_t pos = sf_seek(stream.sndfile, 0, SEEK_CUR);

        if (pos + frames >= loop.end_time)
        {
            frames = loop.end_time - pos;
            restart = true;
        }
    }

    if (stream.sample_format == Int16)
    {
        filled = sf_readf_short(stream.sndfile, (short *)data, frames);
    }
    else if (stream.sample_format == Float)
    {
        filled = sf_readf_float(stream.sndfile, (float *)data, frames);
    }

    if (stream_looping && (restart || filled < frames))
    {
        sf_seek(stream.sndfile, loop.start_time, SEEK_SET);
    }

    return filled;
}

static void I_SND_CloseStream(void)
{
    CloseFile(&stream);
}

static void I_SND_ShutdownStream(void)
{
    ;
}

static const char **I_SND_DeviceList(void)
{
    return NULL;
}

static void I_SND_BindVariables(void)
{
    ;
}

static const char *I_SND_MusicFormat(void)
{
    static SF_FORMAT_INFO format_info;

    int count;
    sf_command(NULL, SFC_GET_SIMPLE_FORMAT_COUNT, &count, sizeof(count));

    for (int i = 0; i < count; i++)
    {
        format_info.format = i;
        sf_command(NULL, SFC_GET_SIMPLE_FORMAT, &format_info,
                   sizeof(format_info));
        if (format_info.format == stream.sfinfo.format)
        {
            return format_info.name;
            break;
        }
    }

    return "Unknown";
}

stream_module_t stream_snd_module =
{
    I_SND_InitStream,
    I_SND_OpenStream,
    I_SND_FillStream,
    I_SND_PlayStream,
    I_SND_CloseStream,
    I_SND_ShutdownStream,
    I_SND_DeviceList,
    I_SND_BindVariables,
    I_SND_MusicFormat,
};
