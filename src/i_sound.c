//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2023 Fabian Greffrath
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
//      System interface for sound.
//
//-----------------------------------------------------------------------------

#include <math.h>
#include <string.h>

#include "i_sound.h"

#include "doomstat.h"
#include "doomtype.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_array.h"
#include "mn_menu.h"
#include "p_mobj.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "m_config.h"

static int snd_module;

static const sound_module_t *sound_modules[] =
{
    &sound_mbf_module,
    &sound_3d_module,
#if defined(HAVE_AL_BUFFER_CALLBACK)
    &sound_pcs_module,
#endif
};

static const sound_module_t *sound_module;

static music_module_t *music_modules[] =
{
#if defined(HAVE_ALSA)
    &music_oal_module,
    &music_mid_module,
#else
    &music_mid_module,
    &music_oal_module,
#endif
};

static music_module_t *active_module = NULL;
static music_module_t *midi_module = NULL;

static int midi_player_menu;
static const char *midi_player_string = "";

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
static boolean snd_init = false;

typedef struct
{
    // SFX id of the playing sound effect.
    // Used to catch duplicates (like chainsaw).
    sfxinfo_t *sfx;

    boolean enabled;
    // haleyjd 06/16/08: unique id number
    int idnum;
} channel_info_t;

static channel_info_t channelinfo[MAX_CHANNELS];

// [FG] variable pitch bend range
static int pitch_bend_range;

// Pitch to stepping lookup.
static float steptable[256];

//
// StopChannel
//
// cph
// Stops a sound, unlocks the data
//
static void StopChannel(int channel)
{
#ifdef RANGECHECK
    // haleyjd 02/18/05: bounds checking
    if (channel < 0 || channel >= MAX_CHANNELS)
    {
        return;
    }
#endif

    if (channelinfo[channel].enabled)
    {
        sound_module->StopSound(channel);

        channelinfo[channel].enabled = false;
    }
}

//
// I_AdjustSoundParams
//
// Outputs adjusted volume, separation, and priority from the sound module.
// Returns false if no sound should be played.
//
boolean I_AdjustSoundParams(const mobj_t *listener, const mobj_t *source,
                            int chanvol, int *vol, int *sep, int *pri)
{
    if (!snd_init)
    {
        return false;
    }

    return sound_module->AdjustSoundParams(listener, source, chanvol, vol, sep, pri);
}

//
// I_UpdateSoundParams
//
// Changes sound parameters in response to stereo panning and relative location
// change.
//
void I_UpdateSoundParams(int channel, int volume, int separation)
{
    if (!snd_init)
    {
        return;
    }

#ifdef RANGECHECK
    if (channel < 0 || channel >= MAX_CHANNELS)
    {
        I_Error("I_UpdateSoundParams: channel out of range");
    }
#endif

    sound_module->UpdateSoundParams(channel, volume, separation);
}

void I_UpdateListenerParams(const mobj_t *listener)
{
    if (!snd_init || !sound_module->UpdateListenerParams)
    {
        return;
    }

    sound_module->UpdateListenerParams(listener);
}

void I_DeferSoundUpdates(void)
{
    if (!snd_init)
    {
        return;
    }

    sound_module->DeferUpdates();
}

void I_ProcessSoundUpdates(void)
{
    if (!snd_init)
    {
        return;
    }

    sound_module->ProcessUpdates();
}

//
// I_SetChannels
//
// Init internal lookups (raw data, mixing buffer, channels).
// This function sets up internal lookups used during
//  the mixing process.
//
void I_SetChannels(void)
{
    int i;
    const double base = pitch_bend_range / 100.0;

    // Okay, reset internal mixing channels to zero.
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        memset(&channelinfo[i], 0, sizeof(channel_info_t));
    }

    // This table provides step widths for pitch parameters.
    for (i = 0; i < arrlen(steptable); i++)
    {
        // [FG] variable pitch bend range
        steptable[i] = pow(base, (double)(2 * (i - NORM_PITCH)) / NORM_PITCH);
    }
}

//
// I_SetSfxVolume
//
void I_SetSfxVolume(int volume)
{
    // Identical to DOS.
    // Basically, this should propagate
    //  the menu/config file setting
    //  to the state variable used in
    //  the mixing.

    snd_SfxVolume = volume;
}

// jff 1/21/98 moved music volume down into MUSIC API with the rest

//
// I_GetSfxLumpNum
//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    if (sfx->lumpnum == -1)
    {
        char namebuf[16];

        memset(namebuf, 0, sizeof(namebuf));

        strcpy(namebuf, "DS");
        strcpy(namebuf + 2, sfx->name);

        sfx->lumpnum = W_CheckNumForName(namebuf);
    }

    return sfx->lumpnum;
}

// Almost all of the sound code from this point on was
// rewritten by Lee Killough, based on Chi's rough initial
// version.

//
// I_StartSound
//
// This function adds a sound to the list of currently
// active sounds, which is maintained as a given number
// of internal channels. Returns a free channel.
//
int I_StartSound(sfxinfo_t *sfx, int vol, int sep, int pitch)
{
    static unsigned int id = 0;
    int channel;

    if (!snd_init)
    {
        return -1;
    }

    // haleyjd 06/03/06: look for an unused hardware channel
    for (channel = 0; channel < MAX_CHANNELS; channel++)
    {
        if (channelinfo[channel].enabled == false)
        {
            break;
        }
    }

    // all used? don't play the sound. It's preferable to miss a sound
    // than it is to cut off one already playing, which sounds weird.
    if (channel == MAX_CHANNELS)
    {
        return -1;
    }

    StopChannel(channel);

    if (sound_module->CacheSound(sfx) == false)
    {
        return -1;
    }

    channelinfo[channel].sfx = sfx;
    channelinfo[channel].enabled = true;
    channelinfo[channel].idnum = id++; // give the sound a unique id

    I_UpdateSoundParams(channel, vol, sep);

    float step = (pitch == NORM_PITCH) ? 1.0f : steptable[pitch];

    if (sound_module->StartSound(channel, sfx, step) == false)
    {
        I_Printf(VB_WARNING, "I_StartSound: Error playing sfx.");
        StopChannel(channel);
        return -1;
    }

    return channel;
}

//
// I_StopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
void I_StopSound(int channel)
{
    if (!snd_init)
    {
        return;
    }

#ifdef RANGECHECK
    if (channel < 0 || channel >= MAX_CHANNELS)
    {
        I_Error("I_StopSound: channel out of range");
    }
#endif

    StopChannel(channel);
}

//
// I_SoundIsPlaying
//
// haleyjd: wow, this can actually do something in the Windows version :P
//
boolean I_SoundIsPlaying(int channel)
{
    if (!snd_init)
    {
        return false;
    }

#ifdef RANGECHECK
    if (channel < 0 || channel >= MAX_CHANNELS)
    {
        I_Error("I_SoundIsPlaying: channel out of range");
    }
#endif

    return sound_module->SoundIsPlaying(channel);
}

//
// I_SoundID
//
// haleyjd: returns the unique id number assigned to a specific instance
// of a sound playing on a given channel. This is required to make sure
// that the higher-level sound code doesn't start updating sounds that have
// been displaced without it noticing.
//
int I_SoundID(int channel)
{
    if (!snd_init)
    {
        return 0;
    }

#ifdef RANGECHECK
    if (channel < 0 || channel >= MAX_CHANNELS)
    {
        I_Error("I_SoundID: channel out of range\n");
    }
#endif

    return channelinfo[channel].idnum;
}

//
// I_ShutdownSound
//
// atexit handler.
//
void I_ShutdownSound(void)
{
    if (!snd_init)
    {
        return;
    }

    sound_module->ShutdownSound();

    snd_init = false;
}

// [FG] add links for likely missing sounds

struct
{
    const int from, to;
} static const sfx_subst[] = {
    {sfx_secret, sfx_itmbk },
    {sfx_itmbk,  sfx_getpow},
    {sfx_getpow, sfx_itemup},
    {sfx_itemup, sfx_None  },

    {sfx_splash, sfx_oof   },
    {sfx_ploosh, sfx_oof   },
    {sfx_lvsiz,  sfx_oof   },
    {sfx_splsml, sfx_None  },
    {sfx_plosml, sfx_None  },
    {sfx_lavsml, sfx_None  },
};

//
// I_InitSound
//

void I_InitSound(void)
{
    if (nosfxparm && nomusicparm)
    {
        return;
    }

    I_Printf(VB_INFO, "I_InitSound:");

    sound_module = sound_modules[snd_module];

    if (!sound_module->InitSound())
    {
        I_Printf(VB_ERROR, "I_InitSound: Failed to initialize sound.");
        return;
    }

    I_AtExit(I_ShutdownSound, true);

    MN_UpdateAdvancedSoundItems(snd_module != SND_MODULE_3D);

    snd_init = true;

    if (nosfxparm)
    {
        return;
    }

    // [FG] precache all sound effects

    I_Printf(VB_INFO, " Precaching all sound effects... ");
    for (int i = 1; i < num_sfx; i++)
    {
        // DEHEXTRA has turned S_sfx into a sparse array
        if (!S_sfx[i].name)
        {
            continue;
        }
        sound_module->CacheSound(&S_sfx[i]);
    }
    I_Printf(VB_INFO, "done.");

    // [FG] add links for likely missing sounds
    for (int i = 0; i < arrlen(sfx_subst); i++)
    {
        sfxinfo_t *from = &S_sfx[sfx_subst[i].from],
                  *to = &S_sfx[sfx_subst[i].to];

        if (from->lumpnum == -1)
        {
            from->link = to;
        }
    }
}

boolean I_AllowReinitSound(void)
{
    if (!snd_init)
    {
        I_Printf(VB_WARNING,
                 "I_AllowReinitSound: Sound was never initialized.");
        return false;
    }

    return sound_module->AllowReinitSound();
}

void I_SetSoundModule(void)
{
    int i;

    if (!snd_init)
    {
        I_Printf(VB_WARNING, "I_SetSoundModule: Sound was never initialized.");
        return;
    }

    if (snd_module < 0 || snd_module >= arrlen(sound_modules))
    {
        I_Printf(VB_WARNING, "I_SetSoundModule: Invalid choice.");
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        StopChannel(i);
    }

    sound_module->ShutdownModule();

    sound_module = sound_modules[snd_module];

    if (!sound_module->ReinitSound())
    {
        I_Printf(VB_WARNING, "I_SetSoundModule: Failed to reinitialize sound.");
    }

    MN_UpdateAdvancedSoundItems(snd_module != SND_MODULE_3D);
}

void I_SetMidiPlayer(void)
{
    if (nomusicparm)
    {
        return;
    }

    const int device = midi_player_menu;

    if (midi_module)
    {
        midi_module->I_ShutdownMusic();
    }

    int count_devices = 0;

    for (int i = 0; i < arrlen(music_modules); ++i)
    {
        const char **strings = music_modules[i]->I_DeviceList();

        if (device >= count_devices
            && device < count_devices + array_size(strings))
        {
            if (music_modules[i]->I_InitMusic(device - count_devices))
            {
                midi_module = music_modules[i];
                midi_player_string = strings[device - count_devices];
                return;
            }
        }

        count_devices += array_size(strings);
    }

    // Fall back the the first module that initializes, device 0.

    count_devices = 0;

    for (int i = 0; i < arrlen(music_modules); ++i)
    {
        const char **strings = music_modules[i]->I_DeviceList();

        if (music_modules[i]->I_InitMusic(0))
        {
            midi_module = music_modules[i];
            midi_player_menu = count_devices;
            midi_player_string = strings[0];
            return;
        }

        count_devices += array_size(strings);
    }

    I_Error("I_SetMidiPlayer: No music module could be initialized");
}

boolean I_InitMusic(void)
{
    if (nomusicparm)
    {
        return false;
    }

    // Always initialize the OpenAL module, it is used for software synth and
    // non-MIDI music streaming.

    I_OAL_InitStream();

    I_AtExit(I_ShutdownMusic, true);

    const char **strings = I_DeviceList();
    for (int i = 0; i < array_size(strings); ++i)
    {
        if (!strcasecmp(strings[i], midi_player_string))
        {
            midi_player_menu = i;
            break;
        }
    }

    I_SetMidiPlayer();

    return true;
}

void I_ShutdownMusic(void)
{
    if (active_module)
    {
        if (active_module != midi_module)
        {
            midi_module->I_ShutdownMusic();
        }
        active_module->I_ShutdownMusic();
    }
    I_OAL_ShutdownStream();
}

void I_SetMusicVolume(int volume)
{
    if (active_module)
    {
        active_module->I_SetMusicVolume(volume);
    }
}

void I_PauseSong(void *handle)
{
    if (active_module)
    {
        active_module->I_PauseSong(handle);
    }
}

void I_ResumeSong(void *handle)
{
    if (active_module)
    {
        active_module->I_ResumeSong(handle);
    }
}

boolean IsMid(byte *mem, int len)
{
    return len > 4 && !memcmp(mem, "MThd", 4);
}

boolean IsMus(byte *mem, int len)
{
    return len > 4 && !memcmp(mem, "MUS\x1a", 4);
}

void *I_RegisterSong(void *data, int size)
{
    for (int i = 0; i < arrlen(music_modules); ++i)
    {
        void *result = music_modules[i]->I_RegisterSong(data, size);
        if (result)
        {
            active_module = music_modules[i];
            active_module->I_SetMusicVolume(snd_MusicVolume);
            return result;
        }
    }
    active_module = NULL;
    return NULL;
}

void I_PlaySong(void *handle, boolean looping)
{
    if (active_module)
    {
        active_module->I_PlaySong(handle, looping);
    }
}

void I_StopSong(void *handle)
{
    if (active_module)
    {
        active_module->I_StopSong(handle);
    }
}

void I_UnRegisterSong(void *handle)
{
    if (active_module)
    {
        active_module->I_UnRegisterSong(handle);
    }
}

const char **I_DeviceList(void)
{
    static const char **devices = NULL;

    if (array_size(devices))
    {
        return devices;
    }

    for (int i = 0; i < arrlen(music_modules); ++i)
    {
        const char **strings = music_modules[i]->I_DeviceList();

        for (int k = 0; k < array_size(strings); ++k)
        {
            array_push(devices, strings[k]);
        }
    }

    return devices;
}

void I_BindSoundVariables(void)
{
    M_BindNum("sfx_volume", &snd_SfxVolume, NULL, 8, 0, 15, ss_none, wad_no,
        "Sound effects volume");
    M_BindNum("music_volume", &snd_MusicVolume, NULL, 8, 0, 15, ss_none, wad_no,
        "Music volume");
    BIND_BOOL(pitched_sounds, false,
        "Variable pitch for sound effects");
    BIND_NUM(pitch_bend_range, 120, 100, 300,
        "Variable pitch bend range (100 = None)");
    BIND_BOOL_GENERAL(full_sounds, false, "Play sounds in full length (prevents cutoffs)");
    M_BindNum("snd_channels", &default_numChannels, NULL,
        MAX_CHANNELS, 1, MAX_CHANNELS, ss_none, wad_no,
        "Maximum number of simultaneous sound effects");
    BIND_NUM_GENERAL(snd_module, SND_MODULE_MBF, 0, NUM_SND_MODULES - 1,
        "Sound module (0 = Standard; 1 = OpenAL 3D; 2 = PC Speaker Sound)");
    for (int i = 0; i < arrlen(sound_modules); ++i)
    {
        if (sound_modules[i]->BindVariables)
        {
            sound_modules[i]->BindVariables();
        }
    }
    BIND_NUM(midi_player_menu, 0, 0, UL, "MIDI Player menu index");
    M_BindStr("midi_player_string", &midi_player_string, "", wad_no,
              "MIDI Player string");
    for (int i = 0; i < arrlen(music_modules); ++i)
    {
        music_modules[i]->BindVariables();
    }
}
