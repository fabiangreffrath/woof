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

#include "doomstat.h"
#include "i_sound.h"
#include "w_wad.h"

int snd_module;

static const sound_module_t *sound_modules[] =
{
    &sound_mbf_module,
    &sound_3d_module,
    //&sound_pcsound_module,
};

static const sound_module_t *sound_module;

// Music modules
extern music_module_t music_win_module;
extern music_module_t music_mac_module;
extern music_module_t music_fl_module;
extern music_module_t music_oal_module;
extern music_module_t music_opl_module;

typedef struct
{
    music_module_t *module;
    int num_devices;
} music_modules_t;

static music_modules_t music_modules[] =
{
#if defined(_WIN32)
    { &music_win_module, 1 },
#elif defined(__APPLE__)
    { &music_mac_module, 1 },
#endif
#if defined(HAVE_FLUIDSYNTH)
    { &music_fl_module, 1 },
#endif
    { &music_opl_module, 1 },
};

static music_module_t *midi_player_module = NULL;
static music_module_t *active_module = NULL;

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
static boolean snd_init = false;

typedef struct {
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *sfx;

  boolean enabled;
  // haleyjd 06/16/08: unique id number
  int idnum;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup.
float steptable[256];

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
    return;
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
    return false;

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
    return;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_UpdateSoundParams: channel out of range");
#endif

  sound_module->UpdateSoundParams(channel, volume, separation);
}

void I_UpdateListenerParams(const mobj_t *listener)
{
    if (!snd_init || !sound_module->UpdateListenerParams)
        return;

    sound_module->UpdateListenerParams(listener);
}

void I_DeferSoundUpdates(void)
{
    if (!snd_init)
        return;

    sound_module->DeferUpdates();
}

void I_ProcessSoundUpdates(void)
{
    if (!snd_init)
        return;

    sound_module->ProcessUpdates();
}

// [FG] variable pitch bend range
int pitch_bend_range;

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
    steptable[i] = pow(base, (double)(2 * (i - NORM_PITCH)) / NORM_PITCH); // [FG] variable pitch bend range
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
    strcpy(namebuf+2, sfx->name);

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
    return -1;

  // haleyjd 06/03/06: look for an unused hardware channel
  for (channel = 0; channel < MAX_CHANNELS; channel++)
  {
    if (channelinfo[channel].enabled == false)
      break;
  }

  // all used? don't play the sound. It's preferable to miss a sound
  // than it is to cut off one already playing, which sounds weird.
  if (channel == MAX_CHANNELS)
    return -1;

  StopChannel(channel);

  if (sound_module->CacheSound(sfx) == false)
    return -1;

  channelinfo[channel].sfx = sfx;
  channelinfo[channel].enabled = true;
  channelinfo[channel].idnum = id++; // give the sound a unique id

  I_UpdateSoundParams(channel, vol, sep);

  if (sound_module->StartSound(channel, sfx, pitch) == false)
  {
    fprintf(stderr, "I_StartSound: Error playing sfx.\n");
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
    return;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_StopSound: channel out of range");
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
    return false;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_SoundIsPlaying: channel out of range");
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
    return 0;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_SoundID: channel out of range\n");
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

struct {
  const int from, to;
} static const sfx_subst[] = {
  {sfx_secret, sfx_itmbk},
  {sfx_itmbk,  sfx_getpow},
  {sfx_getpow, sfx_itemup},
  {sfx_itemup, sfx_None},

  {sfx_splash, sfx_oof},
  {sfx_ploosh, sfx_oof},
  {sfx_lvsiz,  sfx_oof},
  {sfx_splsml, sfx_None},
  {sfx_plosml, sfx_None},
  {sfx_lavsml, sfx_None},
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

    printf("I_InitSound:\n");

    sound_module = sound_modules[snd_module];

    if (!sound_module->InitSound())
    {
        fprintf(stderr, "I_InitSound: Failed to initialize sound.\n");
        return;
    }

    I_AtExit(I_ShutdownSound, true);

    snd_init = true;

    // [FG] precache all sound effects
    if (!nosfxparm)
    {
      int i;

      printf(" Precaching all sound effects... ");
      for (i = 1; i < num_sfx; i++)
      {
        // DEHEXTRA has turned S_sfx into a sparse array
        if (!S_sfx[i].name)
          continue;

        sound_module->CacheSound(&S_sfx[i]);
      }
      printf("done.\n");

      // [FG] add links for likely missing sounds
      for (i = 0; i < arrlen(sfx_subst); i++)
      {
        sfxinfo_t *from = &S_sfx[sfx_subst[i].from],
                    *to = &S_sfx[sfx_subst[i].to];

        if (from->lumpnum == -1)
        {
          from->link = to;
          from->pitch = NORM_PITCH;
          from->volume = 0;
        }
      }
    }
}

void I_UpdateUserSoundSettings(void)
{
    if (!snd_init)
    {
        return;
    }

    sound_module->UpdateUserSoundSettings();
}

boolean I_AllowReinitSound(void)
{
    if (!snd_init)
    {
        fprintf(stderr, "I_AllowReinitSound: Sound was never initialized.\n");
        return false;
    }

    return sound_module->AllowReinitSound();
}

void I_SetSoundModule(int device)
{
    int i;

    if (!snd_init)
    {
        fprintf(stderr, "I_SetSoundModule: Sound was never initialized.\n");
        return;
    }

    if (device < 0 || device >= arrlen(sound_modules))
    {
        fprintf(stderr, "I_SetSoundModule: Invalid choice.\n");
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        StopChannel(i);
    }

    sound_module = sound_modules[device];

    if (!sound_module->ReinitSound())
    {
        fprintf(stderr, "I_SetSoundModule: Failed to reinitialize sound.\n");
    }
}

int midi_player; // current music module

static void MidiPlayerFallback(void)
{
    // Fall back the the first module that initializes, device 0.
    int i;

    for (i = 0; i < arrlen(music_modules); i++)
    {
        if (music_modules[i].module->I_InitMusic(0))
        {
            midi_player = i;
            midi_player_module = music_modules[midi_player].module;
            active_module = midi_player_module;
            return;
        }
    }

    I_Error("MidiPlayerFallback: No music module could be initialized");
}

void I_SetMidiPlayer(int device)
{
    int i, accum;

    if (nomusicparm)
    {
        return;
    }

    midi_player_module->I_ShutdownMusic();

    for (i = 0, accum = 0; i < arrlen(music_modules); ++i)
    {
        int num_devices = music_modules[i].num_devices;

        if (device >= accum && device < accum + num_devices)
        {
            midi_player_module = music_modules[i].module;
            midi_player = i;
            device -= accum;
            break;
        }

        accum += num_devices;
    }

    if (midi_player_module->I_InitMusic(device))
    {
        active_module = midi_player_module;
        return;
    }

    MidiPlayerFallback();
}

boolean I_InitMusic(void)
{
    if (nomusicparm)
    {
        return false;
    }

    // Always initialize the OpenAL module, it is used for software synth and
    // non-MIDI music streaming.

    music_oal_module.I_InitMusic(0);

    I_AtExit(I_ShutdownMusic, true);

    if (midi_player < arrlen(music_modules))
    {
        midi_player_module = music_modules[midi_player].module;
        if (midi_player_module->I_InitMusic(DEFAULT_MIDI_DEVICE))
        {
            active_module = midi_player_module;
            return true;
        }
    }

    MidiPlayerFallback();

    return true;
}

void I_ShutdownMusic(void)
{
    music_oal_module.I_ShutdownMusic();
    midi_player_module->I_ShutdownMusic();
}

void I_SetMusicVolume(int volume)
{
    active_module->I_SetMusicVolume(volume);
}

void I_PauseSong(void *handle)
{
    active_module->I_PauseSong(handle);
}

void I_ResumeSong(void *handle)
{
    active_module->I_ResumeSong(handle);
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
    if (IsMus(data, size) || IsMid(data, size))
    {
        active_module = midi_player_module;
    }
    else
    {
        // Not a MIDI file. We have to shutdown the OPL module due to
        // implementation details.

        if (midi_player_module == &music_opl_module)
        {
            midi_player_module->I_ShutdownMusic();
        }

        // Try to open file with SndFile or XMP.

        active_module = &music_oal_module;
    }

    active_module->I_SetMusicVolume(snd_MusicVolume);
    return active_module->I_RegisterSong(data, size);
}

void I_PlaySong(void *handle, boolean looping)
{
    active_module->I_PlaySong(handle, looping);
}

void I_StopSong(void *handle)
{
    active_module->I_StopSong(handle);
}

void I_UnRegisterSong(void *handle)
{
    active_module->I_UnRegisterSong(handle);
}

// Get a list of devices for all music modules. Retrieve the selected device, as
// each module manages and stores its own devices independently.

int I_DeviceList(const char *devices[], int size, int *current_device)
{
    int i, accum;

    *current_device = 0;

    for (i = 0, accum = 0; i < arrlen(music_modules); ++i)
    {
        int numdev, curdev;
        music_module_t *module = music_modules[i].module;

        numdev = module->I_DeviceList(devices + accum, size - accum, &curdev);

        music_modules[i].num_devices = numdev;

        if (midi_player == i)
        {
            *current_device = accum + curdev;
        }

        accum += numdev;
    }

    return accum;
}
