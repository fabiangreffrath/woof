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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//  02111-1307, USA.
//
// DESCRIPTION:
//      Load sound lumps with libsndfile.

#include "i_sndfile.h"

#if defined(HAVE_SNDFILE)

#include <sndfile.h>

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

void *Load_SNDFile(void *data, SDL_AudioSpec *sample, Uint8 **wavdata, Uint32 *samplelen)
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
    Uint32 wavlen;
    short *local_wavdata;

    sfdata.data = data;
    sfdata.length = *samplelen;
    sfdata.offset = 0;

    memset(&sfinfo, 0, sizeof(sfinfo));

    sndfile = sf_open_virtual(&sfvio, SFM_READ, &sfinfo, &sfdata);

    if (!sndfile)
    {
        fprintf(stderr, "sf_open_virtual: %s\n", sf_strerror(sndfile));
        return NULL;
    }

    if (sfinfo.frames <= 0 || sfinfo.channels <= 0)
    {
        sf_close(sndfile);
        return NULL;
    }

    wavlen = sfinfo.frames * sfinfo.channels * sizeof(short);
    local_wavdata = SDL_malloc(wavlen);

    if (!local_wavdata)
    {
        sf_close(sndfile);
        return NULL;
    }

    if (sf_readf_short(sndfile, local_wavdata, sfinfo.frames) < sfinfo.frames)
    {
        fprintf(stderr, "sf_readf_short: %s\n", sf_strerror(sndfile));
        sf_close(sndfile);
        SDL_free(local_wavdata);
        return NULL;
    }

    sf_close(sndfile);

    sample->channels = sfinfo.channels;
    sample->freq = sfinfo.samplerate;
    sample->format = AUDIO_S16;

    *wavdata = (Uint8 *)local_wavdata;
    *samplelen = wavlen;

    return sample;
}

#else

void *Load_SNDFile(void *data, SDL_AudioSpec *sample, Uint8 **wavdata, Uint32 *samplelen)
{
    SDL_RWops *RWops;

    if ((RWops = SDL_RWFromMem(data, *samplelen)) == NULL)
    {
      fprintf(stderr, "SDL_RWFromMem: %s\n", SDL_GetError());
      return NULL;
    }

    if (SDL_LoadWAV_RW(RWops, 1, sample, wavdata, samplelen) == NULL) // 1 = will call SDL_RWclose(RWops) for us
    {
      fprintf(stderr, "SDL_LoadWAV_RW: %s\n", SDL_GetError());
      return NULL;
    }

    return sample;
}

#endif
