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

#include "SDL.h"
#include "al.h"
#include "alc.h"
#include "alext.h"
#include "ebur128.h"

#include <stdlib.h>

#include "doomtype.h"
#include "i_oalcommon.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_sound.h"
#include "m_array.h"
#include "m_config.h"

// Define the number of buffers and buffer size (in milliseconds) to use. 4
// buffers with 4096 samples each gives a nice per-chunk size, and lets the
// queue last for about half second at 44.1khz.
#define NUM_BUFFERS    4
#define BUFFER_SAMPLES 4096

static stream_module_t *all_modules[] =
{
#if defined (HAVE_FLUIDSYNTH)
    &stream_fl_module,
#endif
    &stream_opl_module,
    &stream_snd_module,
#if defined(HAVE_LIBXMP)
    &stream_xmp_module,
#endif
};

static stream_module_t *stream_modules[] =
{
    &stream_snd_module,
#if defined(HAVE_LIBXMP)
    &stream_xmp_module,
#endif
};

static stream_module_t *midi_modules[] =
{
#if defined (HAVE_FLUIDSYNTH)
    &stream_fl_module,
#endif
    &stream_opl_module,
};

static stream_module_t *active_module;

typedef struct
{
    // These are the buffers and source to play out through OpenAL with
    ALuint buffers[NUM_BUFFERS];
    ALuint source;

    byte *data;
    boolean looping;

    ALfloat gain;
    ALfloat auto_gain;

    // The format of the output stream
    ALenum format;
    ALsizei freq;
    ALsizei frame_size;
    int channels;
    int total_frames;
} stream_player_t;

static stream_player_t player;

static SDL_Thread *player_thread_handle;
static SDL_atomic_t player_thread_running;

static boolean music_initialized;

static ebur128_state *ebur_state;
boolean auto_gain;

static void ShutdownAutoGain(void)
{
    if (ebur_state)
    {
        ebur128_destroy(&ebur_state);
    }
}

static void InitAutoGain(void)
{
    if (player.format == AL_FORMAT_MONO16 || player.format == AL_FORMAT_MONO_FLOAT32)
    {
        player.channels = 1;
    }
    else
    {
        player.channels = 2;
    }

    ebur_state = ebur128_init(player.channels, player.freq, EBUR128_MODE_S
        | EBUR128_MODE_I | EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_HISTOGRAM);

    player.auto_gain = 1.0f;
    player.total_frames = 0;
}

static void AutoGain(uint32_t frames)
{
    if (!auto_gain)
    {
        return;
    }

    if (player.format == AL_FORMAT_MONO16 || player.format == AL_FORMAT_STEREO16)
    {
        ebur128_add_frames_short(ebur_state, (int16_t *)player.data, frames);
    }
    else
    {
        ebur128_add_frames_float(ebur_state, (float *)player.data, frames);
    }

    player.total_frames += frames;

    const float target = -23.0f; // UFS

    boolean failed = false;
    double momentary = 0.0;
    double shortterm = 0.0;
    double global = 0.0;
    double relative = 0.0;

    if (EBUR128_SUCCESS != ebur128_loudness_momentary(ebur_state, &momentary))
    {
        failed = true;
    }

    if (EBUR128_SUCCESS != ebur128_loudness_shortterm(ebur_state, &shortterm))
    {
        failed = true;
    }

    if (EBUR128_SUCCESS != ebur128_loudness_global(ebur_state, &global))
    {
        failed = true;
    }

    if (EBUR128_SUCCESS != ebur128_relative_threshold(ebur_state, &relative))
    {
        failed = true;
    }

    if (player.total_frames < 3 * BUFFER_SAMPLES && !failed)
    {
        if (momentary > -70.0)
        {
            float diff = target - momentary;
            player.auto_gain = DB_TO_GAIN(diff);
        }
    }

    if (relative > -70.0 && momentary > relative && !failed)
    {
        double peak_L = 0.0;
        double peak_R = 0.0;

        if (EBUR128_SUCCESS != ebur128_prev_sample_peak(ebur_state, 0, &peak_L))
        {
            failed = true;
        }

        if (player.channels == 2)
        {
            if (EBUR128_SUCCESS
                != ebur128_prev_sample_peak(ebur_state, 1, &peak_R))
            {
                failed = true;
            }
        }

        if (!failed)
        {
            const float weight_m = 0.1f;
            const float weight_s = 1.0f;
            const float weight_i = 1.0f;
            float loudness = (weight_m * momentary + weight_s * shortterm
                              + weight_i * global)
                             / (weight_m + weight_s + weight_i);

            float diff = target - loudness;

            float gain = DB_TO_GAIN(diff);

            double peak = (peak_L > peak_R) ? peak_L : peak_R;

            if (peak >= 0.00001 && gain * peak < 1.0f)
            {
                player.auto_gain = gain;
            }
        }
    }

    alSourcef(player.source, AL_GAIN, player.gain * player.auto_gain);
}

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
            AutoGain(frames);
        }

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
        {
            break;
        }

        if (frames > 0)
        {
            AutoGain(frames);
        }

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

    while (SDL_AtomicGet(&player_thread_running))
    {
        if (!UpdatePlayer())
        {
            SDL_AtomicSet(&player_thread_running, 0);
        }

        SDL_Delay(1);
    }

    return 0;
}

boolean I_OAL_InitStream(void)
{
    if (alcGetCurrentContext() == NULL || music_initialized)
    {
        return false;
    }

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

void I_OAL_ShutdownStream(void)
{
    if (!music_initialized)
    {
        return;
    }

    for (int i = 0; i < arrlen(stream_modules); ++i)
    {
        stream_modules[i]->I_ShutdownStream();
    }

    alDeleteSources(1, &player.source);
    alDeleteBuffers(NUM_BUFFERS, player.buffers);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_ShutdownMusic: Failed to delete object IDs.");
    }

    memset(&player, 0, sizeof(stream_player_t));

    music_initialized = false;
}

static boolean I_OAL_InitMusic(int device)
{
    int count_devices = 0;

    for (int i = 0; i < arrlen(midi_modules); ++i)
    {
        const char **strings = midi_modules[i]->I_DeviceList();

        if (device >= count_devices
            && device < count_devices + array_size(strings))
        {
            return midi_modules[i]->I_InitStream(device - count_devices);
        }

        count_devices += array_size(strings);
    }

    return false;
}

#if defined(HAVE_FLUIDSYNTH)
static int fl_gain;
#endif

static int opl_gain;

static void I_OAL_SetMusicVolume(int volume)
{
    if (!music_initialized)
    {
        return;
    }

    player.gain = (ALfloat)volume / 15.0f;

    if (!auto_gain)
    {
        if (active_module == &stream_opl_module)
        {
            player.gain *= (ALfloat)DB_TO_GAIN(opl_gain);
        }
    #if defined(HAVE_FLUIDSYNTH)
        else if (active_module == &stream_fl_module)
        {
            player.gain *= (ALfloat)DB_TO_GAIN(fl_gain);
        }
    #endif

        alSourcef(player.source, AL_GAIN, player.gain);
    }
}

static void I_OAL_PauseSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    alSourcePause(player.source);
}

static void I_OAL_ResumeSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    alSourcePlay(player.source);
}

static void I_OAL_PlaySong(void *handle, boolean looping)
{
    if (!music_initialized)
    {
        return;
    }

    player.looping = looping;

    alSourcePlay(player.source);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_PlaySong: Error starting playback.");
        return;
    }

    SDL_AtomicSet(&player_thread_running, 1);
    player_thread_handle = SDL_CreateThread(PlayerThread, NULL, NULL);
}

static void I_OAL_StopSong(void *handle)
{
    if (!music_initialized || !SDL_AtomicGet(&player_thread_running))
    {
        return;
    }

    alSourceStop(player.source);

    SDL_AtomicSet(&player_thread_running, 0);
    SDL_WaitThread(player_thread_handle, NULL);

    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_StopSong: Error stopping playback.");
    }
}

static void I_OAL_UnRegisterSong(void *handle)
{
    if (!music_initialized)
    {
        return;
    }

    if (active_module)
    {
        active_module->I_CloseStream();
    }

    ShutdownAutoGain();

    if (player.data)
    {
        free(player.data);
        player.data = NULL;
    }
}

static void I_OAL_ShutdownMusic(void)
{
    if (!music_initialized)
    {
        return;
    }

    I_OAL_StopSong(NULL);
    I_OAL_UnRegisterSong(NULL);

    for (int i = 0; i < arrlen(midi_modules); ++i)
    {
        midi_modules[i]->I_ShutdownStream();
    }
}

// Prebuffers some audio from the file, and starts playing the source.

static void *I_OAL_RegisterSong(void *data, int len)
{
    if (!music_initialized)
    {
        return NULL;
    }

    for (int i = 0; i < arrlen(all_modules); ++i)
    {
        if (all_modules[i]->I_OpenStream(data, len, &player.format,
                                            &player.freq, &player.frame_size))
        {
            active_module = all_modules[i];
            InitAutoGain();
            return (void *)1;
        }
    }

    return NULL;
}

static const char **I_OAL_DeviceList(void)
{
    static const char **devices = NULL;

    if (array_size(devices))
    {
        return devices;
    }

    for (int i = 0; i < arrlen(midi_modules); ++i)
    {
        const char **strings = midi_modules[i]->I_DeviceList();

        for (int k = 0; k < array_size(strings); ++k)
        {
            array_push(devices, strings[k]);
        }
    }

    return devices;
}

static midiplayertype_t I_OAL_MidiPlayerType(void)
{
#if defined (HAVE_FLUIDSYNTH)
    if (active_module == &stream_fl_module)
    {
        return midiplayer_fluidsynth;
    }
#endif
    if (active_module == &stream_opl_module)
    {
        return midiplayer_opl;
    }
    return midiplayer_none;
}

static void I_OAL_BindVariables(void)
{
    BIND_BOOL_MUSIC(auto_gain, true, "Auto Gain");
#if defined (HAVE_FLUIDSYNTH)
    BIND_NUM_MUSIC(fl_gain, 0, -20, 20, "[FluidSynth] Gain [dB]");
#endif
    BIND_NUM_MUSIC(opl_gain, 0, -20, 20, "[OPL3 Emulation] Gain [dB]");
    for (int i = 0; i < arrlen(midi_modules); ++i)
    {
        midi_modules[i]->BindVariables();
    }
}

static const char *I_OAL_MusicFormat(void)
{
    if (active_module)
    {
        return active_module->I_MusicFormat();
    }
    return "None";
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
    I_OAL_BindVariables,
    I_OAL_MidiPlayerType,
    I_OAL_MusicFormat,
};
