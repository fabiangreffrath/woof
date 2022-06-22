// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sound.c,v 1.15 1998/05/03 22:32:33 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2021 Fabian Greffrath
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
#include "i_sound.h"
#include "w_wad.h"
#include "d_main.h"

// haleyjd
#define MAX_CHANNELS 32

#ifndef M_PI
#define M_PI 3.14
#endif

// [FG] precache all sound effects
boolean precache_sounds;
// [FG] optional low-pass filter
boolean lowpass_filter;
// [FG] music backend
midi_player_t midi_player;
// [FG] variable pitch bend range
int pitch_bend_range;

// Music modules
extern music_module_t music_win_module;
extern music_module_t music_fl_module;
extern music_module_t music_sdl_module;
extern music_module_t music_opl_module;

static music_module_t *midi_player_module = NULL;
static music_module_t *active_module = NULL;

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
boolean snd_init = false;

// haleyjd 10/28/05: updated for Julian's music code, need full quality now
int snd_samplerate = 44100;

typedef struct {
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *id;
  // The channel data pointer.
  unsigned char* data;
  // [FG] let SDL_Mixer do the actual sound mixing
  Mix_Chunk chunk;
  // haleyjd 06/16/08: unique id number
  int idnum;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup, unused.
int steptable[256];

//
// stopchan
//
// cph 
// Stops a sound, unlocks the data 
//
static void stopchan(int handle)
{
   int cnum;

#ifdef RANGECHECK
   // haleyjd 02/18/05: bounds checking
    if(handle < 0 || handle >= MAX_CHANNELS)
       return;
#endif

   if(channelinfo[handle].data)
   {
      Mix_HaltChannel(handle);
      // [FG] immediately free samples not connected to a sound SFX
      if (channelinfo[handle].id == NULL)
      {
         Z_Free(channelinfo[handle].data);
      }
      channelinfo[handle].data = NULL;

      if(channelinfo[handle].id)
      {
         // haleyjd 06/03/06: see if we can free the sound
         for(cnum = 0; cnum < MAX_CHANNELS; ++cnum)
         {
            if(cnum == handle)
               continue;
            if(channelinfo[cnum].id &&
               channelinfo[cnum].id->data == channelinfo[handle].id->data)
               return; // still being used by some channel
         }
         
         // set sample to PU_CACHE level
         if (!precache_sounds)
         {
         Z_ChangeTag(channelinfo[handle].id->data, PU_CACHE);
         }
      }
   }

   channelinfo[handle].id = NULL;
}

static int SOUNDHDRSIZE = 8;

//
// addsfx
//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
// haleyjd: needs to take a sfxinfo_t ptr, not a sound id num
// haleyjd 06/03/06: changed to return boolean for failure or success
//
static boolean addsfx(sfxinfo_t *sfx, int channel, int pitch)
{
   size_t lumplen;
   int lump;
   // [FG] do not connect pitch-shifted samples to a sound SFX
   unsigned int sfx_alen;
   unsigned int bits;
   void *sfx_data;

#ifdef RANGECHECK
   if(channel < 0 || channel >= MAX_CHANNELS)
      I_Error("addsfx: channel out of range!\n");
#endif

   // haleyjd 02/18/05: null ptr check
   if(!snd_init || !sfx)
      return false;

   stopchan(channel);
   
   // We will handle the new SFX.
   // Set pointer to raw data.

   // haleyjd: Eternity sfxinfo_t does not have a lumpnum field
   lump = I_GetSfxLumpNum(sfx);
   
   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = W_GetNumForName("DSPISTOL");
   
   lumplen = W_LumpLength(lump);
   
   // haleyjd 10/08/04: do not play zero-length sound lumps
   if(lumplen <= SOUNDHDRSIZE)
      return false;

   // haleyjd 06/03/06: rewrote again to make sound data properly freeable
   if(sfx->data == NULL || pitch != NORM_PITCH)
   {   
      byte *data;
      Uint32 samplerate, samplelen, samplecount;
      Uint8 *wav_buffer = NULL;

      // haleyjd: this should always be called (if lump is already loaded,
      // W_CacheLumpNum handles that for us).
      data = (byte *)W_CacheLumpNum(lump, PU_STATIC);

      // [crispy] Check if this is a valid RIFF wav file
      if (lumplen > 44 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVEfmt ", 8) == 0)
      {
        SDL_RWops *RWops;
        SDL_AudioSpec wav_spec;

        RWops = SDL_RWFromMem(data, lumplen);

        Z_ChangeTag(data, PU_CACHE);

        if (SDL_LoadWAV_RW(RWops, 1, &wav_spec, &wav_buffer, &samplelen) == NULL)
        {
          fprintf(stderr, "Could not open wav file: %s\n", SDL_GetError());
          return false;
        }

        if (wav_spec.channels != 1)
        {
          fprintf(stderr, "Only mono WAV file is supported");
          SDL_FreeWAV(wav_buffer);
          return false;
        }

        if (!SDL_AUDIO_ISINT(wav_spec.format))
        {
          SDL_FreeWAV(wav_buffer);
          return false;
        }

        bits = SDL_AUDIO_BITSIZE(wav_spec.format);
        if (bits != 8 && bits != 16)
        {
          fprintf(stderr, "Only 8 or 16 bit WAV files are supported");
          SDL_FreeWAV(wav_buffer);
          return false;
        }

        samplerate = wav_spec.freq;
        data = wav_buffer;
        SOUNDHDRSIZE = 0;
      }
      // Check the header, and ensure this is a valid sound
      else if(data[0] == 0x03 && data[1] == 0x00)
      {
         samplerate = (data[3] << 8) | data[2];
         samplelen  = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

         // All Doom sounds are 8-bit
         bits = 8;

         SOUNDHDRSIZE = 8;
      }
      else
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      // don't play sounds that think they're longer than they really are
      if(samplelen > lumplen - SOUNDHDRSIZE)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      samplecount = samplelen / (bits / 8);

      // [FG] do not connect pitch-shifted samples to a sound SFX
      if (pitch == NORM_PITCH)
      {
         sfx_alen = (Uint32)(((ULong64)samplecount * snd_samplerate) / samplerate);
         // [FG] double up twice: 8 -> 16 bit and mono -> stereo
         sfx->alen = 4 * sfx_alen;
         sfx->data = precache_sounds ? malloc(sfx->alen) : Z_Malloc(sfx->alen, PU_STATIC, &sfx->data);
         sfx_data = sfx->data;
      }
      else
      {
         // [FG] spoof sound samplerate if using randomly pitched sounds
         samplerate = (Uint32)(((ULong64)samplerate * steptable[pitch]) >> 16);
         sfx_alen = (Uint32)(((ULong64)samplecount * snd_samplerate) / samplerate);
         // [FG] double up twice: 8 -> 16 bit and mono -> stereo
         channelinfo[channel].data = Z_Malloc(4 * sfx_alen, PU_STATIC, (void **)&channelinfo[channel].data);
         sfx_data = channelinfo[channel].data;
      }

      // haleyjd 04/23/08: Convert sound to target samplerate
      {  
         unsigned int i;
         Sint16 sample = 0;
         Sint16 *dest = (Sint16 *)sfx_data;
         byte *src  = data + SOUNDHDRSIZE;
         
         unsigned int step = (samplerate << 16) / snd_samplerate;
         unsigned int stepremainder = 0, j = 0;

         // do linear filtering operation
         for(i = 0; i < sfx_alen && j < samplelen - 1; ++i)
         {
            int d;

            if (bits == 16)
            {
               d = ((Sint16)(src[j  ] | (src[j+1] << 8)) * (0x10000 - stepremainder) +
                    (Sint16)(src[j+2] | (src[j+3] << 8)) * stepremainder) >> 16;

               sample = d;
            }
            else
            {
               d = (((unsigned int)src[j  ] * (0x10000 - stepremainder)) +
                    ((unsigned int)src[j+1] * stepremainder)) >> 8;

               // [FG] interpolate sfx in a 16-bit int domain, convert to signed
               sample = d - (1<<15);
            }

            dest[2*i] = dest[2*i+1] = sample;

            stepremainder += step;
            if (bits == 16)
               j += (stepremainder >> 16) * 2;
            else
               j += (stepremainder >> 16);

            stepremainder &= 0xffff;
         }
         // fill remainder (if any) with final sample byte
         for(; i < sfx_alen; ++i)
            dest[2*i] = dest[2*i+1] = sample;

        // Perform a low-pass filter on the upscaled sound to filter
        // out high-frequency noise from the conversion process.

        if (lowpass_filter)
        {
            float rc, dt, alpha;

            // Low-pass filter for cutoff frequency f:
            //
            // For sampling rate r, dt = 1 / r
            // rc = 1 / 2*pi*f
            // alpha = dt / (rc + dt)

            // Filter to the half sample rate of the original sound effect
            // (maximum frequency, by nyquist)

            dt = 1.0f / snd_samplerate;
            rc = 1.0f / (M_PI * samplerate);
            alpha = dt / (rc + dt);

            // Both channels are processed in parallel, hence [i-2]:

            for (i = 2; i < sfx_alen * 2; ++i)
            {
                dest[i] = (Sint16) (alpha * dest[i]
                                      + (1.0f - alpha) * dest[i-2]);
            }
        }
      }
      // [FG] double up twice: 8 -> 16 bit and mono -> stereo
      sfx_alen *= 4;

      if (wav_buffer)
        SDL_FreeWAV(wav_buffer);
      else
      // haleyjd 06/03/06: don't need original lump data any more
      Z_ChangeTag(data, PU_CACHE);
   }
   else
   if (!precache_sounds)
      Z_ChangeTag(sfx->data, PU_STATIC); // reset to static cache level

   // [FG] let SDL_Mixer do the actual sound mixing
   channelinfo[channel].chunk.allocated = 1;
   channelinfo[channel].chunk.volume = MIX_MAX_VOLUME;

   // [FG] do not connect pitch-shifted samples to a sound SFX
   if (pitch == NORM_PITCH)
   {
      channelinfo[channel].data = sfx->data;

      channelinfo[channel].chunk.abuf = sfx->data;
      channelinfo[channel].chunk.alen = sfx->alen;

      // Preserve sound SFX id
      channelinfo[channel].id = sfx;
   }
   else
   {
      channelinfo[channel].chunk.abuf = sfx_data;
      channelinfo[channel].chunk.alen = sfx_alen;

      channelinfo[channel].id = NULL;
   }

   return true;
}

int forceFlipPan;

//
// updateSoundParams
//
// Changes sound parameters in response to stereo panning and relative location
// change.
//
static void updateSoundParams(int handle, int volume, int separation, int pitch)
{
   int rightvol;
   int leftvol;
   
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_UpdateSoundParams: handle out of range");
#endif

   // SoM 7/1/02: forceFlipPan accounted for here
   if(forceFlipPan)
      separation = 254 - separation;

   // [FG] linear stereo volume separation
   leftvol = ((254 - separation) * volume) / 127;
   rightvol = ((separation) * volume) / 127;

   if (leftvol < 0) leftvol = 0;
   else if (leftvol > 255) leftvol = 255;
   if (rightvol < 0) rightvol = 0;
   else if (rightvol > 255) rightvol = 255;

   Mix_SetPanning(handle, leftvol, rightvol);
}

//
// SFX API
//

//
// I_UpdateSoundParams
//
// Update the sound parameters. Used to control volume,
// pan, and pitch changes such as when a player turns.
//
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   if(!snd_init)
      return;

   updateSoundParams(handle, vol, sep, pitch);
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
   
   int *steptablemid = steptable + 128;
   const double base = pitch_bend_range / 100.0;
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; ++i)
   {
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   }
   
   // This table provides step widths for pitch parameters.
   for(i=-128 ; i<128 ; i++)
   {
      steptablemid[i] = (int)(pow(base, (double)i / 64.0) * 65536.0); // [FG] variable pitch bend range
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
   char namebuf[16];

   memset(namebuf, 0, sizeof(namebuf));

   strcpy(namebuf, "DS");
   strcpy(namebuf+2, sfx->name);

   return W_CheckNumForName(namebuf);
}

// Almost all of the sound code from this point on was
// rewritten by Lee Killough, based on Chi's rough initial
// version.

//
// I_StartSound
//
// This function adds a sound to the list of currently
// active sounds, which is maintained as a given number
// of internal channels. Returns a handle.
//
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch, int pri)
{
   static unsigned int id = 0;
   int handle;
   
   if(!snd_init)
      return -1;

   // haleyjd: turns out this is too simplistic. see below.
   /*
   // SoM: reimplement hardware channel wrap-around
   if(++handle >= MAX_CHANNELS)
      handle = 0;
   */

   // haleyjd 06/03/06: look for an unused hardware channel
   for(handle = 0; handle < MAX_CHANNELS; ++handle)
   {
      if(channelinfo[handle].data == NULL)
         break;
   }

   // all used? don't play the sound. It's preferable to miss a sound
   // than it is to cut off one already playing, which sounds weird.
   if(handle == MAX_CHANNELS)
      return -1;

   if(addsfx(sound, handle, pitch))
   {
      channelinfo[handle].idnum = id++; // give the sound a unique id
      Mix_PlayChannel(handle, &channelinfo[handle].chunk, 0);
      updateSoundParams(handle, vol, sep, pitch);
   }
   else
      handle = -1;
   
   return handle;
}

//
// I_StopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
void I_StopSound(int handle)
{
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_StopSound: handle out of range");
#endif
   
   stopchan(handle);
}

//
// I_SoundIsPlaying
//
// haleyjd: wow, this can actually do something in the Windows version :P
//
int I_SoundIsPlaying(int handle)
{
   if(!snd_init)
      return false;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SoundIsPlaying: handle out of range");
#endif
 
   return Mix_Playing(handle);
}

//
// I_SoundID
//
// haleyjd: returns the unique id number assigned to a specific instance
// of a sound playing on a given channel. This is required to make sure
// that the higher-level sound code doesn't start updating sounds that have
// been displaced without it noticing.
//
int I_SoundID(int handle)
{
   if(!snd_init)
      return 0;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SoundID: handle out of range\n");
#endif

   return channelinfo[handle].idnum;
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

    for (i=0; i<MAX_CHANNELS; ++i)
    {
        if (channelinfo[i].data && !I_SoundIsPlaying(i))
        {
            // Sound has finished playing on this channel,
            // but sound data has not been released to cache

            stopchan(i);
        }
    }
}


// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime.
// It is called during Timer interrupt with SNDINTR.

void I_SubmitSound(void)
{
  //this should no longer be necessary because
  //allegro is doing all the sound mixing now
}

//
// I_ShutdownSound
//
// atexit handler.
//
void I_ShutdownSound(void)
{
   if(snd_init)
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
// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer
//
void I_InitSound(void)
{   
   if(!nosfxparm || !nomusicparm)
   {
      int audio_buffers;

      printf("I_InitSound: ");

      /* Initialize variables */
      audio_buffers = GetSliceSize();

      // haleyjd: the docs say we should do this
      // In SDL2, SDL_InitSubSystem() and SDL_Init() are interchangeable.
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
      {
         printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
         return;
      }
  
      if(Mix_OpenAudioDevice(snd_samplerate, MIX_DEFAULT_FORMAT, 2, audio_buffers, NULL, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) < 0)
      {
         printf("Couldn't open audio with desired format.\n");
         return;
      }

      // [FG] feed actual sample frequency back into config variable
      Mix_QuerySpec(&snd_samplerate, NULL, NULL);

      // [FG] let SDL_Mixer do the actual sound mixing
      Mix_AllocateChannels(MAX_CHANNELS);
      printf("Configured audio device with %d samples/slice.\n", audio_buffers);

      I_AtExit(I_ShutdownSound, true);

      snd_init = true;

      // [FG] precache all sound effects
      if (!nosfxparm && precache_sounds)
      {
         int i;

         printf("Precaching all sound effects...");
         for (i = 1; i < num_sfx; i++)
         {
            // DEHEXTRA has turned S_sfx into a sparse array
            if (!S_sfx[i].name)
              continue;

            addsfx(&S_sfx[i], 0, NORM_PITCH);
         }
         stopchan(0);
         printf("done.\n");
      }

      // haleyjd 04/11/03: don't use music if sfx aren't init'd
      // (may be dependent, docs are unclear)
      if(!nomusicparm)
      {
         // always initilize SDL music
         active_module = &music_sdl_module;
         active_module->I_InitMusic();
         I_AtExit(active_module->I_ShutdownMusic, true);

         if (midi_player == midi_player_opl)
            midi_player_module = &music_opl_module;
      #if defined(_WIN32)
         else if (midi_player == midi_player_win)
            midi_player_module = &music_win_module;
      #endif
      #if defined(HAVE_FLUIDSYNTH)
         else if (midi_player == midi_player_fl)
            midi_player_module = &music_fl_module;
      #endif

         if (midi_player_module)
         {
            active_module = midi_player_module;
            if (active_module->I_InitMusic())
            {
              I_AtExit(active_module->I_ShutdownMusic, true);
            }
            else
            {
              // fall back to Native/SDL on error
              midi_player = 0;
              midi_player_module = NULL;
              active_module = &music_sdl_module;
            }
         }
      }
   }   
}

boolean I_InitMusic(void)
{
    return active_module->I_InitMusic();
}

void I_ShutdownMusic(void)
{
    active_module->I_ShutdownMusic();
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
        if (midi_player_module)
        {
            active_module = midi_player_module;
            return active_module->I_RegisterSong(data, size);
        }
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
