// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sound.c,v 1.15 1998/05/03 22:32:33 killough Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      System interface for sound.
//
//-----------------------------------------------------------------------------

// haleyjd
#include "SDL.h"
#include "SDL_mixer.h"
#include <math.h>

#include "doomstat.h"
#include "i_sndfile.h"
#include "i_sound.h"
#include "w_wad.h"

// Music modules
extern music_module_t music_win_module;
extern music_module_t music_fl_module;
extern music_module_t music_sdl_module;
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
#else
    { &music_sdl_module, 1 },
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

// haleyjd 10/28/05: updated for Julian's music code, need full quality now
int snd_samplerate;
char *snd_resampling_mode;
static Uint16 mix_format;
static int mix_channels;

typedef struct {
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *sfx;
  // The channel data pointer.
  unsigned char* data;
  // [FG] let SDL_Mixer do the actual sound mixing
  Mix_Chunk chunk;
  // haleyjd 06/16/08: unique id number
  int idnum;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup, unused.
float steptable[256];

//
// StopChannel
//
// cph 
// Stops a sound, unlocks the data 
//
static void StopChannel(int channel)
{
  int cnum;

#ifdef RANGECHECK
  // haleyjd 02/18/05: bounds checking
  if (channel < 0 || channel >= MAX_CHANNELS)
    return;
#endif

  if (channelinfo[channel].data)
  {
    Mix_HaltChannel(channel);

    // [FG] immediately free samples not connected to a sound SFX
    if (channelinfo[channel].sfx == NULL)
    {
      free(channelinfo[channel].data);
    }
    channelinfo[channel].data = NULL;

    if (channelinfo[channel].sfx)
    {
      // haleyjd 06/03/06: see if we can free the sound
      for (cnum = 0; cnum < MAX_CHANNELS; cnum++)
      {
        if (cnum == channel)
          continue;

        if (channelinfo[cnum].sfx &&
            channelinfo[cnum].sfx->data == channelinfo[channel].sfx->data)
        {
          return; // still being used by some channel
        }
      }
    }
  }

  channelinfo[channel].sfx = NULL;
}

#define SOUNDHDRSIZE 8

// [FG] support multi-channel samples by converting them to mono first
static Uint8 *ConvertToMono(Uint8 **data, SDL_AudioSpec *sample, Uint32 *len)
{
  SDL_AudioCVT cvt;

  if (sample->channels < 1)
  {
    return NULL;
  }

  if (SDL_BuildAudioCVT(&cvt,
                        sample->format, sample->channels, sample->freq,
                        sample->format,                1, sample->freq) < 0)
  {
    fprintf(stderr, "SDL_BuildAudioCVT: %s\n", SDL_GetError());
    return NULL;
  }

  cvt.len = *len;
  cvt.buf = (Uint8 *)SDL_malloc(cvt.len * cvt.len_mult); // [FG] will call SDL_FreeWAV() on this later
  memset(cvt.buf, 0, cvt.len * cvt.len_mult);
  memcpy(cvt.buf, *data, cvt.len);

  if (SDL_ConvertAudio(&cvt) < 0)
  {
    SDL_free(cvt.buf);
    fprintf(stderr, "SDL_ConvertAudio: %s\n", SDL_GetError());
    return NULL;
  }

  SDL_FreeWAV(*data);

  sample->channels = 1;
  *data = cvt.buf;
  *len = cvt.len_cvt;

  return *data;
}

// Allocate a new sound chunk and pitch-shift an existing sound up-or-down
// into it, based on chocolate-doom/src/i_sdlsound.c:PitchShift().
static void PitchShift(sfxinfo_t *sfx, int pitch, Mix_Chunk *chunk)
{
  Sint16 *inp, *outp;
  Sint16 *srcbuf, *dstbuf;
  Uint32 srclen, dstlen;

  srcbuf = (Sint16 *)sfx->data;
  srclen = sfx->alen;

  dstlen = (int)(srclen * steptable[pitch]);

  // ensure that the new buffer is an even length
  if ((dstlen % 2) == 0)
  {
    dstlen++;
  }

  dstbuf = (Sint16 *)malloc(dstlen);

  // loop over output buffer. find corresponding input cell, copy over
  for (outp = dstbuf; outp < dstbuf + dstlen/2; outp++)
  {
    inp = srcbuf + (int)((float)(outp - dstbuf) * srclen / dstlen);
    *outp = *inp;
  }

  chunk->abuf = (Uint8 *)dstbuf;
  chunk->alen = dstlen;
}

//
// CacheSound
//
// haleyjd: needs to take a sfxinfo_t ptr, not a sound id num
// haleyjd 06/03/06: changed to return boolean for failure or success
//
static boolean CacheSound(sfxinfo_t *sfx, int channel, int pitch)
{
  int lumpnum, lumplen;
  Uint8 *lumpdata = NULL, *wavdata = NULL;
  Mix_Chunk *const chunk = &channelinfo[channel].chunk;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
  {
    I_Error("CacheSound: channel out of range!\n");
  }
#endif

  // haleyjd 02/18/05: null ptr check
  if (!snd_init || !sfx)
  {
    return false;
  }

  StopChannel(channel);

  lumpnum = I_GetSfxLumpNum(sfx);

  if (lumpnum < 0)
  {
    return false;
  }

  lumplen = W_LumpLength(lumpnum);

  // haleyjd 10/08/04: do not play zero-length sound lumps
  if (lumplen <= SOUNDHDRSIZE)
  {
    return false;
  }

  // haleyjd 06/03/06: rewrote again to make sound data properly freeable
  while (sfx->data == NULL)
  {
    SDL_AudioSpec sample;
    SDL_AudioCVT cvt;
    Uint8 *sampledata;
    Uint32 samplelen;

    // haleyjd: this should always be called (if lump is already loaded,
    // W_CacheLumpNum handles that for us).
    lumpdata = (Uint8 *)W_CacheLumpNum(lumpnum, PU_STATIC);

    // Check the header, and ensure this is a valid sound
    if (lumpdata[0] == 0x03 && lumpdata[1] == 0x00)
    {
      sample.freq = (lumpdata[3] <<  8) |  lumpdata[2];
      samplelen   = (lumpdata[7] << 24) | (lumpdata[6] << 16) |
                    (lumpdata[5] <<  8) |  lumpdata[4];

      // don't play sounds that think they're longer than they really are
      if (samplelen > lumplen - SOUNDHDRSIZE)
      {
        break;
      }

      sampledata = lumpdata + SOUNDHDRSIZE;

      // All Doom sounds are 8-bit
      sample.format = AUDIO_U8;
      sample.channels = 1;
    }
    else
    {
      samplelen = lumplen;

      if (Load_SNDFile(lumpdata, &sample, &wavdata, &samplelen) == NULL)
      {
        break;
      }

      if (sample.channels != 1)
      {
        if (ConvertToMono(&wavdata, &sample, &samplelen) == NULL)
        {
          break;
        }
      }

      sampledata = wavdata;
    }

    // Convert sound to target samplerate
    if (SDL_BuildAudioCVT(&cvt,
                          sample.format, sample.channels, sample.freq,
                          mix_format, mix_channels, snd_samplerate) < 0)
    {
      fprintf(stderr, "SDL_BuildAudioCVT: %s\n", SDL_GetError());
      break;
    }

    cvt.len = samplelen;
    cvt.buf = (Uint8 *)malloc(cvt.len * cvt.len_mult);
    // [FG] clear buffer (cvt.len * cvt.len_mult >= cvt.len_cvt)
    memset(cvt.buf, 0, cvt.len * cvt.len_mult);
    memcpy(cvt.buf, sampledata, cvt.len);

    if (SDL_ConvertAudio(&cvt) < 0)
    {
      free(cvt.buf);
      fprintf(stderr, "SDL_ConvertAudio: %s\n", SDL_GetError());
      break;
    }

    sfx->data = cvt.buf;
    sfx->alen = cvt.len_cvt;
  }

  // don't need original lump data any more
  if (lumpdata)
  {
    Z_Free(lumpdata);
  }
  if (wavdata)
  {
    SDL_FreeWAV(wavdata);
  }

  if (sfx->data == NULL)
  {
    sfx->lumpnum = -2; // [FG] don't try again
    return false;
  }

  // [FG] let SDL_Mixer do the actual sound mixing
  chunk->allocated = 1;
  chunk->volume = MIX_MAX_VOLUME;

  if (pitch != NORM_PITCH)
  {
    PitchShift(sfx, pitch, chunk);

    // [FG] do not connect pitch-shifted samples to a sound SFX
    channelinfo[channel].sfx = NULL;
  }
  else
  {
    chunk->abuf = sfx->data;
    chunk->alen = sfx->alen;

    // Preserve sound SFX id
    channelinfo[channel].sfx = sfx;
  }

  channelinfo[channel].data = chunk->abuf;

  return true;
}

int forceFlipPan;

//
// I_UpdateSoundParams
//
// Changes sound parameters in response to stereo panning and relative location
// change.
//
void I_UpdateSoundParams(int channel, int volume, int separation)
{
  int rightvol;
  int leftvol;

  if (!snd_init)
    return;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_UpdateSoundParams: channel out of range");
#endif

  // SoM 7/1/02: forceFlipPan accounted for here
  if (forceFlipPan)
    separation = 254 - separation;

  // [FG] linear stereo volume separation
  leftvol = ((254 - separation) * volume) / 127;
  rightvol = ((separation) * volume) / 127;

  if (leftvol < 0)
    leftvol = 0;
  else if (leftvol > 255)
    leftvol = 255;
  if (rightvol < 0)
    rightvol = 0;
  else if (rightvol > 255)
    rightvol = 255;

  Mix_SetPanning(channel, leftvol, rightvol);
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
int I_StartSound(sfxinfo_t *sound, int vol, int sep, int pitch, boolean loop)
{
  static unsigned int id = 0;
  int channel;

  if (!snd_init)
    return -1;

  // haleyjd 06/03/06: look for an unused hardware channel
  for (channel = 0; channel < MAX_CHANNELS; channel++)
  {
    if (channelinfo[channel].data == NULL)
      break;
  }

  // all used? don't play the sound. It's preferable to miss a sound
  // than it is to cut off one already playing, which sounds weird.
  if (channel == MAX_CHANNELS)
    return -1;

  if (CacheSound(sound, channel, pitch))
  {
    channelinfo[channel].idnum = id++; // give the sound a unique id
    Mix_PlayChannelTimed(channel, &channelinfo[channel].chunk, loop ? -1 : 0, -1);
    I_UpdateSoundParams(channel, vol, sep);
  }
  else
    channel = -1;

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
int I_SoundIsPlaying(int channel)
{
  if (!snd_init)
    return false;

#ifdef RANGECHECK
  if (channel < 0 || channel >= MAX_CHANNELS)
    I_Error("I_SoundIsPlaying: channel out of range");
#endif

  return Mix_Playing(channel);
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

// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
//  allegro does this now

void I_UpdateSound(void)
{
  int i;

  // Check all channels to see if a sound has finished

  for (i = 0; i < MAX_CHANNELS; i++)
  {
    if (channelinfo[i].data && !I_SoundIsPlaying(i))
    {
      // Sound has finished playing on this channel,
      // but sound data has not been released to cache

      StopChannel(i);
    }
  }
}

//
// I_ShutdownSound
//
// atexit handler.
//
void I_ShutdownSound(void)
{
  if (snd_init)
  {
    Mix_CloseAudio();
    snd_init = 0;
  }
}

// Calculate slice size, the result must be a power of two.

static int GetSliceSize(void)
{
    int limit;
    int n;

    limit = snd_samplerate / TICRATE;

    // Try all powers of two, not exceeding the limit.

    for (n=0;; ++n)
    {
        // 2^n <= limit < 2^n+1 ?

        if ((1 << (n + 1)) > limit)
        {
            return (1 << n);
        }
    }

    // Should never happen?

    return 1024;
}

//
// I_InitSound
//

void I_InitSound(void)
{
  if (!nosfxparm || !nomusicparm)
  {
    printf("I_InitSound: ");

    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, snd_resampling_mode);

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
      printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
      return;
    }

    if (Mix_OpenAudioDevice(snd_samplerate, AUDIO_S16SYS, 2, GetSliceSize(), NULL,
                            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) < 0)
    {
      printf("Couldn't open audio with desired format.\n");
      return;
    }

    // [FG] feed actual sample frequency back into config variable
    Mix_QuerySpec(&snd_samplerate, &mix_format, &mix_channels);
    printf("Configured audio device with %.1f kHz (%s%d%s), %d channels.\n",
           (float)snd_samplerate / 1000,
           SDL_AUDIO_ISFLOAT(mix_format) ? "F" : SDL_AUDIO_ISSIGNED(mix_format) ? "S" : "U",
           (int)SDL_AUDIO_BITSIZE(mix_format),
           SDL_AUDIO_BITSIZE(mix_format) > 8 ? (SDL_AUDIO_ISBIGENDIAN(mix_format) ? "MSB" : "LSB") : "",
           mix_channels);

    // [FG] let SDL_Mixer do the actual sound mixing
    Mix_AllocateChannels(MAX_CHANNELS);

    I_AtExit(I_ShutdownSound, true);

    snd_init = true;

    // [FG] precache all sound effects
    if (!nosfxparm)
    {
      int i;

      printf("Precaching all sound effects...");
      for (i = 1; i < num_sfx; i++)
      {
        // DEHEXTRA has turned S_sfx into a sparse array
        if (!S_sfx[i].name)
          continue;

        CacheSound(&S_sfx[i], 0, NORM_PITCH);
      }
      StopChannel(0);
      printf("done.\n");
    }
  }
}

int midi_player; // current music module

void I_SetMidiPlayer(int device)
{
    int i, accum;

    if (midi_player_module)
    {
        midi_player_module->I_ShutdownMusic();
        midi_player_module = NULL;
    }

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

    if (!midi_player_module->I_InitMusic(device))
    {
        midi_player_module = music_modules[0].module;
        if (midi_player_module != &music_sdl_module)
        {
            midi_player_module->I_InitMusic(0);
        }
    }
    active_module = midi_player_module;
}

boolean I_InitMusic(void)
{
    // haleyjd 04/11/03: don't use music if sfx aren't init'd
    // (may be dependent, docs are unclear)
    if (nomusicparm)
    {
        return false;
    }

    // always initilize SDL music
    music_sdl_module.I_InitMusic(0);

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

    // Fall back to module 0 device 0.
    midi_player = 0;
    midi_player_module = music_modules[0].module;
    if (midi_player_module != &music_sdl_module)
    {
       midi_player_module->I_InitMusic(0);
    }
    active_module = midi_player_module;

    return true;
}

void I_ShutdownMusic(void)
{
    music_sdl_module.I_ShutdownMusic();

    if (midi_player_module && midi_player_module != &music_sdl_module)
    {
        midi_player_module->I_ShutdownMusic();
    }
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
        if (midi_player_module)
        {
            active_module = midi_player_module;
            return active_module->I_RegisterSong(data, size);
        }
    }

    if (midi_player_module == &music_opl_module)
    {
        midi_player_module->I_ShutdownMusic();
    }

    active_module = &music_sdl_module;
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
