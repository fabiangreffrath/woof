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

#include "i_oalmusic.h"

#include <AL/al.h>
#include <AL/alext.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "doomtype.h"
#include "i_sndfile.h"
#include "i_sound.h"

// Define the number of buffers and buffer size (in milliseconds) to use. 4
// buffers with 8192 samples each gives a nice per-chunk size, and lets the
// queue last for almost one second at 44.1khz.
#define NUM_BUFFERS 4
#define BUFFER_SAMPLES 8192

typedef struct
{
    // These are the buffers and source to play out through OpenAL with
    ALuint buffers[NUM_BUFFERS];
    ALuint source;

    byte *data;

    // The format of the output stream
    ALenum format;
    ALsizei freq;
} stream_player_t;

static stream_player_t player;

static SDL_Thread *player_thread_handle;
static int player_thread_running;

static callback_func_t callback;

static boolean UpdatePlayer(void)
{
    ALint processed, state;

    // Get relevant source info
    alGetSourcei(player.source, AL_SOURCE_STATE, &state);
    alGetSourcei(player.source, AL_BUFFERS_PROCESSED, &processed);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "UpdatePlayer: Error checking source state\n");
        return false;
    }

    // Unqueue and handle each processed buffer
    while (processed > 0)
    {
        ALuint bufid;
        ALsizei size;

        alSourceUnqueueBuffers(player.source, 1, &bufid);
        processed--;

        // Read the next chunk of data, refill the buffer, and queue it back on
        // the source.
        if (callback)
        {
            size = callback(player.data, BUFFER_SAMPLES);
        }
        else
        {
            size = I_SND_FillStream(player.data, BUFFER_SAMPLES);
        }
        if (size > 0)
        {
            alBufferData(bufid, player.format, player.data, size, player.freq);
            alSourceQueueBuffers(player.source, 1, &bufid);
        }
        if (alGetError() != AL_NO_ERROR)
        {
            fprintf(stderr, "UpdatePlayer: Error buffering data\n");
            return false;
        }
    }

    // Make sure the source hasn't underrun
    if (state != AL_PLAYING && state != AL_PAUSED)
    {
        ALint queued;

        // If no buffers are queued, playback is finished
        alGetSourcei(player.source, AL_BUFFERS_QUEUED, &queued);
        if (queued == 0)
        {
            return false;
        }

        alSourcePlay(player.source);
        if (alGetError() != AL_NO_ERROR)
        {
            fprintf(stderr, "UpdatePlayer: Error restarting playback\n");
            return false;
        }
    }

    return true;
}

static boolean StartPlayer(void)
{
    int i;

    player.data = malloc(BUFFER_SAMPLES);

    // Rewind the source position and clear the buffer queue.
    alSourceRewind(player.source);
    alSourcei(player.source, AL_BUFFER, 0);

    // Fill the buffer queue
    for (i = 0; i < NUM_BUFFERS; i++)
    {
        ALsizei size;

        // Get some data to give it to the buffer
        if (callback)
        {
            size = callback(player.data, BUFFER_SAMPLES);
        }
        else
        {
            size = I_SND_FillStream(player.data, BUFFER_SAMPLES);
        }

        if (size < 1)
            break;

        alBufferData(player.buffers[i], player.format, player.data,
                     size, player.freq);
    }
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "StartPlayer: Error buffering for playback.\n");
        return false;
    }

    alSourceQueueBuffers(player.source, i, player.buffers);

    return true;
}

static int PlayerThread(void *unused)
{
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);

    while (player_thread_running && UpdatePlayer())
    {
        SDL_Delay(1);
    }

    return 0;
}

static boolean I_OAL_InitMusic(int device)
{
    alGenBuffers(NUM_BUFFERS, player.buffers);
    alGenSources(1, &player.source);

    // Set parameters so mono sources play out the front-center speaker and
    // won't distance attenuate.
    alSource3i(player.source, AL_POSITION, 0, 0, -1);
    alSourcei(player.source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcei(player.source, AL_ROLLOFF_FACTOR, 0);

    return true;
}

static void I_OAL_SetMusicVolume(int volume)
{
    alSourcef(player.source, AL_GAIN, (ALfloat)volume / 15.0f);
}

static void I_OAL_PauseSong(void *handle)
{
    alSourcePause(player.source);
}

static void I_OAL_ResumeSong(void *handle)
{
    alSourcePlay(player.source);
}

static void I_OAL_PlaySong(void *handle, boolean looping)
{
    if (!callback)
    {
        I_SND_SetLooping(looping);
    }

    alSourcePlay(player.source);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_PlaySong: Error starting playback.\n");
        return;
    }

    player_thread_running = true;
    player_thread_handle = SDL_CreateThread(PlayerThread, NULL, NULL);
}

static void I_OAL_StopSong(void *handle)
{
    alSourceStop(player.source);
}

static void I_OAL_UnRegisterSong(void *handle)
{
    if (player_thread_running)
    {
        player_thread_running = false;
        SDL_WaitThread(player_thread_handle, NULL);
    }

    if (!callback)
    {
        I_SND_CloseStream();
    }

    if (player.data)
    {
        free(player.data);
        player.data = NULL;
    }
}

static void I_OAL_ShutdownMusic(void)
{
    I_OAL_StopSong(NULL);
    I_OAL_UnRegisterSong(NULL);

    alDeleteSources(1, &player.source);
    alDeleteBuffers(NUM_BUFFERS, player.buffers);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_ShutdownMusic: Failed to delete object IDs.\n");
    }

    memset(&player, 0, sizeof(stream_player_t));
}

// Prebuffers some audio from the file, and starts playing the source.

static void *I_OAL_RegisterSong(void *data, int len)
{
    if (I_SND_OpenStream(data, len, &player.format, &player.freq) == false)
        return NULL;

    StartPlayer();

    return (void *)1;
}

static int I_OAL_DeviceList(const char *devices[], int size, int *current_device)
{
    *current_device = 0;
    return 0;
}

void I_OAL_SetGain(float gain)
{
    alSourcef(player.source, AL_MAX_GAIN, 10.0f);
    alSourcef(player.source, AL_GAIN, (ALfloat)gain);
}

void I_OAL_HookMusic(callback_func_t callback_func)
{
    if (callback_func)
    {
        callback = callback_func;

        player.format = AL_FORMAT_STEREO16;
        player.freq = snd_samplerate;

        StartPlayer();
        I_OAL_PlaySong(NULL, false);
    }
    else
    {
        callback = NULL;

        I_OAL_UnRegisterSong(NULL);
    }
}

music_module_t music_oal_module =
{
    I_OAL_InitMusic,
    I_OAL_ShutdownMusic,
    I_OAL_SetMusicVolume,
    I_OAL_PauseSong,
    I_OAL_ResumeSong,
    I_OAL_RegisterSong,
    I_OAL_PlaySong,
    I_OAL_StopSong,
    I_OAL_UnRegisterSong,
    I_OAL_DeviceList,
};
