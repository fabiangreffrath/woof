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

#include "al.h"
#include "alc.h"
#include "alext.h"
#include "SDL.h"

#include <stdlib.h>

#include "doomtype.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_sound.h"

// Define the number of buffers and buffer size (in milliseconds) to use. 4
// buffers with 4096 samples each gives a nice per-chunk size, and lets the
// queue last for about half second at 44.1khz.
#define NUM_BUFFERS 4
#define BUFFER_SAMPLES 4096

static stream_module_t *stream_modules[] =
{
    &stream_snd_module,
#if defined(HAVE_LIBXMP)
    &stream_xmp_module,
#endif
};

static stream_module_t *active_module;

typedef struct
{
    // These are the buffers and source to play out through OpenAL with
    ALuint buffers[NUM_BUFFERS];
    ALuint source;

    byte *data;
    boolean looping;

    // The format of the output stream
    ALenum format;
    ALsizei freq;
    ALsizei frame_size;
} stream_player_t;

static stream_player_t player;

static SDL_Thread *player_thread_handle;
static int player_thread_running;

static boolean music_initialized;

static SDL_mutex *music_lock;

static boolean UpdatePlayer(void)
{
    ALint processed, state;

    // Get relevant source info
    alGetSourcei(player.source, AL_SOURCE_STATE, &state);
    alGetSourcei(player.source, AL_BUFFERS_PROCESSED, &processed);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "UpdatePlayer: Error checking source state");
        return false;
    }

    // Unqueue and handle each processed buffer
    while (processed > 0)
    {
        uint32_t frames;
        ALuint bufid;
        ALsizei size;

        alSourceUnqueueBuffers(player.source, 1, &bufid);
        processed--;

        // Read the next chunk of data, refill the buffer, and queue it back on
        // the source.
        frames = active_module->I_FillStream(player.data, BUFFER_SAMPLES);

        if (frames > 0)
        {
            size = frames * player.frame_size;
            alBufferData(bufid, player.format, player.data, size, player.freq);
            alSourceQueueBuffers(player.source, 1, &bufid);
        }
        if (alGetError() != AL_NO_ERROR)
        {
            I_Printf(VB_ERROR, "UpdatePlayer: Error buffering data");
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
            I_Printf(VB_ERROR, "UpdatePlayer: Error restarting playback");
            return false;
        }
    }

    return true;
}

static boolean StartPlayer(void)
{
    int i;

    player.data = malloc(BUFFER_SAMPLES * player.frame_size);

    // Rewind the source position and clear the buffer queue.
    alSourceRewind(player.source);
    alSourcei(player.source, AL_BUFFER, 0);

    active_module->I_PlayStream(player.looping);

    // Fill the buffer queue
    for (i = 0; i < NUM_BUFFERS; i++)
    {
        uint32_t frames;
        ALsizei size;

        // Get some data to give it to the buffer
        frames = active_module->I_FillStream(player.data, BUFFER_SAMPLES);

        if (frames < 1)
            break;

        size = frames * player.frame_size;

        alBufferData(player.buffers[i], player.format, player.data, size,
                    player.freq);
    }
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "StartPlayer: Error buffering for playback.");
        return false;
    }

    alSourceQueueBuffers(player.source, i, player.buffers);

    return true;
}

static int PlayerThread(void *unused)
{
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);

    StartPlayer();

    while (true)
    {
        boolean keep_going;

        if (!UpdatePlayer())
        {
            break;
        }

        SDL_LockMutex(music_lock);
        keep_going = player_thread_running;
        SDL_UnlockMutex(music_lock);

        if (!keep_going)
        {
            break;
        }

        SDL_Delay(1);
    }

    return 0;
}

static boolean I_OAL_InitMusic(int device)
{
    if (alcGetCurrentContext() == NULL)
        return false;

    active_module = &stream_snd_module;

    alGenBuffers(NUM_BUFFERS, player.buffers);
    alGenSources(1, &player.source);

    alSourcef(player.source, AL_MAX_GAIN, 10.0f);

    // Set parameters so mono sources play out the front-center speaker and
    // won't distance attenuate.
    alSource3i(player.source, AL_POSITION, 0, 0, -1);
    alSourcei(player.source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcei(player.source, AL_ROLLOFF_FACTOR, 0);

    // Bypass speaker virtualization for music.
    if (alIsExtensionPresent("AL_SOFT_direct_channels"))
    {
        alSourcei(player.source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
    }

    // Bypass speaker virtualization for music.
    if (alIsExtensionPresent("AL_SOFT_source_spatialize"))
    {
        alSourcei(player.source, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
    }

    for (int i = 0; i < arrlen(stream_modules); ++i)
    {
        stream_modules[i]->I_InitStream(0);
    }

    music_initialized = true;

    return true;
}

int mus_gain = 100;
int opl_gain = 200;

static void I_OAL_SetMusicVolume(int volume)
{
    if (!music_initialized)
        return;

    ALfloat gain = (ALfloat)volume / 15.0f;

    if (active_module == &stream_opl_module)
    {
        gain *= (ALfloat)opl_gain / 100.0f;
    }
    else if (active_module == &stream_fl_module)
    {
        gain *= (ALfloat)mus_gain / 100.0f;
    }

    alSourcef(player.source, AL_GAIN, gain);
}

static void I_OAL_PauseSong(void *handle)
{
    if (!music_initialized)
        return;

    alSourcePause(player.source);
}

static void I_OAL_ResumeSong(void *handle)
{
    if (!music_initialized)
        return;

    alSourcePlay(player.source);
}

static void I_OAL_PlaySong(void *handle, boolean looping)
{
    if (!music_initialized)
        return;

    player.looping = looping;

    alSourcePlay(player.source);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_PlaySong: Error starting playback.");
        return;
    }

    music_lock = SDL_CreateMutex();

    player_thread_running = true;
    player_thread_handle = SDL_CreateThread(PlayerThread, NULL, NULL);
    if (player_thread_handle == NULL)
    {
        I_Printf(VB_ERROR, "Error creating thread: %s", SDL_GetError());
        player_thread_running = false;
    }
}

static void I_OAL_StopSong(void *handle)
{
    if (!music_initialized || !player_thread_running)
        return;

    alSourceStop(player.source);

    SDL_LockMutex(music_lock);
    player_thread_running = false;
    SDL_UnlockMutex(music_lock);

    SDL_WaitThread(player_thread_handle, NULL);

    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_StopSong: Error stopping playback.");
    }

    SDL_DestroyMutex(music_lock);
}

static void I_OAL_UnRegisterSong(void *handle)
{
    if (!music_initialized)
        return;

    active_module->I_CloseStream();

    if (player.data)
    {
        free(player.data);
        player.data = NULL;
    }
}

static void I_OAL_ShutdownMusic(void)
{
    if (!music_initialized)
        return;

    I_OAL_StopSong(NULL);
    I_OAL_UnRegisterSong(NULL);

    if (midi_stream_module)
    {
        midi_stream_module->I_ShutdownStream();
    }
    active_module->I_ShutdownStream();

    alDeleteSources(1, &player.source);
    alDeleteBuffers(NUM_BUFFERS, player.buffers);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_ShutdownMusic: Failed to delete object IDs.");
    }

    memset(&player, 0, sizeof(stream_player_t));

    music_initialized = false;
}

// Prebuffers some audio from the file, and starts playing the source.

static void *I_OAL_RegisterSong(void *data, int len)
{
    int i;

    if (!music_initialized)
        return NULL;

    if (IsMid(data, len) || IsMus(data, len))
    {
        if (midi_stream_module &&
            midi_stream_module->I_OpenStream(data, len, &player.format,
                                             &player.freq, &player.frame_size))
        {
            active_module = midi_stream_module;
            return (void *)1;
        }
        return NULL;
    }

    for (i = 0; i < arrlen(stream_modules); ++i)
    {
        if (stream_modules[i]->I_OpenStream(data, len, &player.format,
                                            &player.freq, &player.frame_size))
        {
            active_module = stream_modules[i];
            return (void *)1;
        }
    }

    return NULL;
}

static const char **I_OAL_DeviceList(int *current_device)
{
    return NULL;
}

static void I_OAL_UpdateMusic(void)
{
    ;
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
    I_OAL_UpdateMusic,
    I_OAL_StopSong,
    I_OAL_UnRegisterSong,
    I_OAL_DeviceList,
};
