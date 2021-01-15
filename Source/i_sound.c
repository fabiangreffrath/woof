// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sound.c,v 1.15 1998/05/03 22:32:33 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
#include "SDL_audio.h"
#include "SDL_mixer.h"
#include <math.h>

#include "z_zone.h"
#include "doomstat.h"
#include "i_sound.h"
#include "i_midipipe.h"
#include "memio.h"
#include "mus2mid.h"
#include "w_wad.h"
#include "g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "d_main.h"
#include "d_io.h"

// Needed for calling the actual sound output.
int SAMPLECOUNT = 512;

// haleyjd
#define MAX_CHANNELS 32

// [FG] precache all sound effects
boolean precache_sounds;

int snd_card;   // default.cfg variables for digi and midi drives
int mus_card;   // jff 1/18/98

int default_snd_card;  // killough 10/98: add default_ versions
int default_mus_card;

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
boolean snd_init = false;
boolean mus_init = false;

int detect_voices; //jff 3/4/98 enables voice detection prior to install_sound
//jff 1/22/98 make these visible here to disable sound/music on install err

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

#define SOUNDHDRSIZE 8

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
      Uint32 samplerate, samplelen;

      // haleyjd: this should always be called (if lump is already loaded,
      // W_CacheLumpNum handles that for us).
      data = (byte *)W_CacheLumpNum(lump, PU_STATIC);

      // Check the header, and ensure this is a valid sound
      if(data[0] != 0x03 || data[1] != 0x00)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      samplerate = (data[3] << 8) | data[2];
      samplelen  = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

      // don't play sounds that think they're longer than they really are
      if(samplelen > lumplen - SOUNDHDRSIZE)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      // [FG] do not connect pitch-shifted samples to a sound SFX
      if (pitch == NORM_PITCH)
      {
         sfx_alen = (Uint32)(((ULong64)samplelen * snd_samplerate) / samplerate);
         // [FG] double up twice: 8 -> 16 bit and mono -> stereo
         sfx->alen = 4 * sfx_alen;
         sfx->data = precache_sounds ? (malloc)(sfx->alen) : Z_Malloc(sfx->alen, PU_STATIC, &sfx->data);
         sfx_data = sfx->data;
      }
      else
      {
         // [FG] spoof sound samplerate if using randomly pitched sounds
         samplerate = (Uint32)(((ULong64)samplerate * steptable[pitch]) >> 16);
         sfx_alen = (Uint32)(((ULong64)samplelen * snd_samplerate) / samplerate);
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
            int d = (((unsigned int)src[j  ] * (0x10000 - stepremainder)) +
                     ((unsigned int)src[j+1] * stepremainder)) >> 16;

            if(d > 255)
               d = 255;
            else if(d < 0)
               d = 0;

            // [FG] expand 8->16 bits, mono->stereo
            sample = (d-128)*256;
            dest[2*i] = dest[2*i+1] = sample;

            stepremainder += step;
            j += (stepremainder >> 16);

            stepremainder &= 0xffff;
         }
         // fill remainder (if any) with final sample byte
         for(; i < sfx_alen; ++i)
            dest[2*i] = dest[2*i+1] = sample;
      }
      // [FG] double up twice: 8 -> 16 bit and mono -> stereo
      sfx_alen *= 4;

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
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; ++i)
   {
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   }
   
   // This table provides step widths for pitch parameters.
   for(i=-128 ; i<128 ; i++)
   {
      steptablemid[i] = (int)(pow(2.0, (double)i / 64.0) * 65536.0); // [FG] pimp (was 1.2)
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

//
// I_InitSound
//
// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer
//
void I_InitSound(void)
{   
   if(!nosfxparm)
   {
      int audio_buffers;

      printf("I_InitSound: ");

      /* Initialize variables */
      audio_buffers = SAMPLECOUNT * snd_samplerate / 11025;

      // haleyjd: the docs say we should do this
      if(SDL_InitSubSystem(SDL_INIT_AUDIO))
      {
         printf("Couldn't initialize SDL audio.\n");
         snd_card = 0;
         mus_card = 0;
         return;
      }
  
      if(Mix_OpenAudioDevice(snd_samplerate, MIX_DEFAULT_FORMAT, 2, audio_buffers, NULL, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) < 0)
      {
         printf("Couldn't open audio with desired format.\n");
         snd_card = 0;
         mus_card = 0;
         return;
      }

      SAMPLECOUNT = audio_buffers;
      // [FG] let SDL_Mixer do the actual sound mixing
      Mix_AllocateChannels(MAX_CHANNELS);
      printf("Configured audio device with %d samples/slice.\n", SAMPLECOUNT);

      atexit(I_ShutdownSound);

      snd_init = true;

      // [FG] precache all sound effects
      if (precache_sounds)
      {
         int i;

         printf("Precaching all sound effects...");
         for (i = 1; i < NUMSFX; i++)
         {
            addsfx(&S_sfx[i], 0, NORM_PITCH);
         }
         stopchan(0);
         printf("done.\n");
      }

      // haleyjd 04/11/03: don't use music if sfx aren't init'd
      // (may be dependent, docs are unclear)
      if(!nomusicparm)
         I_InitMusic();
   }   
}

///
// MUSIC API.
//

#include "m_misc.h"
#include "m_misc2.h"

// Only one track at a time
static Mix_Music *music = NULL;

static boolean music_initialized = false;
static boolean musicpaused = false;
static int current_music_volume;

// Shutdown music

void I_ShutdownMusic(void)
{
   if (music_initialized)
   {
   #if defined(_WIN32)
      I_MidiPipe_ShutdownServer();
   #endif
      Mix_HaltMusic();
      music_initialized = false;
   }
}

// Initialize music subsystem

void I_InitMusic(void)
{
   music_initialized = true;

   // Initialize SDL_Mixer for MIDI music playback
   Mix_Init(MIX_INIT_MID | MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_MP3); // [crispy] initialize some more audio formats

   // Once initialization is complete, the temporary Timidity config
   // file can be removed.

   // RemoveTimidityConfig();

   // If snd_musiccmd is set, we need to call Mix_SetMusicCMD to
   // configure an external music playback program.

   // if (strlen(snd_musiccmd) > 0)
   // {
   //     Mix_SetMusicCMD(snd_musiccmd);
   // }

#if defined(_WIN32)
    // [AM] Start up midiproc to handle playing MIDI music.
    // Don't enable it for GUS, since it handles its own volume just fine.
    // if (snd_musicdevice != SNDDEVICE_GUS)
    // {
         I_MidiPipe_InitServer();
    // }
#endif

    //return music_initialized;
}

//
// SDL_mixer's native MIDI music playing does not pause properly.
// As a workaround, set the volume to 0 when paused.
//

static void UpdateMusicVolume(void)
{
   int vol;

   if (musicpaused)
   {
      vol = 0;
   }
   else
   {
      vol = (current_music_volume * MIX_MAX_VOLUME) / 15;
   }

#if defined(_WIN32)
   I_MidiPipe_SetVolume(vol);
#endif
   Mix_VolumeMusic(vol);
}

// Set music volume

void I_SetMusicVolume(int volume)
{
   // Internal state variable.
   current_music_volume = volume;

   UpdateMusicVolume();
}

// Start playing a mid

void I_PlaySong(int handle, int looping)
{
   int loops;

   if (!music_initialized)
   {
      return;
   }

   if (!midi_server_registered)
   {
      return;
   }

   if (looping)
   {
      loops = -1;
   }
   else
   {
      loops = 1;
   }

#if defined(_WIN32)
   if (midi_server_registered)
   {
      I_MidiPipe_PlaySong(loops);
   }
   else
#endif
   {
      Mix_PlayMusic(music, loops);
   }
}

void I_PauseSong(int handle)
{
   if (!music_initialized)
   {
      return;
   }

   musicpaused = true;

   UpdateMusicVolume();
}

void I_ResumeSong(int handle)
{
   if (!music_initialized)
   {
      return;
   }

   musicpaused = false;

   UpdateMusicVolume();
}

void I_StopSong(int handle)
{
   if (!music_initialized)
   {
      return;
   }

#if defined(_WIN32)
   if (midi_server_registered)
   {
      I_MidiPipe_StopSong();
   }
   else
#endif
   {
      Mix_HaltMusic();
   }
}

void I_UnRegisterSong(int handle)
{
   if (!music_initialized)
   {
      return;
   }

#if defined(_WIN32)
   if (midi_server_registered)
   {
      I_MidiPipe_UnregisterSong();
   }
   else
#endif
   {
      Mix_FreeMusic(music);
   }
}

static boolean ConvertMus(byte *musdata, int len, const char *filename)
{
   MEMFILE *instream;
   MEMFILE *outstream;
   void *outbuf;
   size_t outbuf_len;
   int result;

   instream = mem_fopen_read(musdata, len);
   outstream = mem_fopen_write();

   result = mus2mid(instream, outstream);

   if (result == 0)
   {
     mem_get_buf(outstream, &outbuf, &outbuf_len);

     M_WriteFile(filename, outbuf, outbuf_len);
   }

   mem_fclose(instream);
   mem_fclose(outstream);

   return result;
}

int I_RegisterSong(void *data, int len)
{
   char *filename;

   if (!music_initialized)
   {
     return 0;
   }

   // MUS files begin with "MUS"
   // Reject anything which doesnt have this signature

   filename = M_TempFile("doom"); // [crispy] generic filename

   // [crispy] Reverse Choco's logic from "if (MIDI)" to "if (not MUS)"
   // MUS is the only format that requires conversion,
   // let SDL_Mixer figure out the others
   /*
   if (IsMid(data, len) && len < MAXMIDLENGTH)
   */
   if (len < 4 || memcmp(data, "MUS\x1a", 4)) // [crispy] MUS_HEADER_MAGIC
   {
     M_WriteFile(filename, data, len);
   }
   else
   {
     // Assume a MUS file and try to convert
     ConvertMus(data, len, filename);
   }

   // Load the MIDI. In an ideal world we'd be using Mix_LoadMUS_RW()
   // by now, but Mix_SetMusicCMD() only works with Mix_LoadMUS(), so
   // we have to generate a temporary file.

#if defined(_WIN32)
   // [AM] If we do not have an external music command defined, play
   //      music with the MIDI server.
   if (midi_server_initialized)
   {
      music = NULL;
      if (!I_MidiPipe_RegisterSong(filename))
      {
         fprintf(stderr, "Error loading midi: %s\n",
                  "Could not communicate with midiproc.");
      }
   }
   else
#endif
   {
      music = Mix_LoadMUS(filename);
      if (music == NULL)
      {
         // Failed to load
         fprintf(stderr, "Error loading midi: %s\n", Mix_GetError());
      }

      // Remove the temporary MIDI file; however, when using an external
      // MIDI program we can't delete the file. Otherwise, the program
      // won't find the file to play. This means we leave a mess on
      // disk :(

      // if (strlen(snd_musiccmd) == 0)
      // {
            remove(filename);
      // }
   }

   //free(filename);

   return music != NULL;
}

//----------------------------------------------------------------------------
//
// $Log: i_sound.c,v $
// Revision 1.15  1998/05/03  22:32:33  killough
// beautification, use new headers/decls
//
// Revision 1.14  1998/03/09  07:11:29  killough
// Lock sound sample data
//
// Revision 1.13  1998/03/05  00:58:46  jim
// fixed autodetect not allowed in allegro detect routines
//
// Revision 1.12  1998/03/04  11:51:37  jim
// Detect voices in sound init
//
// Revision 1.11  1998/03/02  11:30:09  killough
// Make missing sound lumps non-fatal
//
// Revision 1.10  1998/02/23  04:26:44  killough
// Add variable pitched sound support
//
// Revision 1.9  1998/02/09  02:59:51  killough
// Add sound sample locks
//
// Revision 1.8  1998/02/08  15:15:51  jim
// Added native midi support
//
// Revision 1.7  1998/01/26  19:23:27  phares
// First rev with no ^Ms
//
// Revision 1.6  1998/01/23  02:43:07  jim
// Fixed failure to not register I_ShutdownSound with atexit on install_sound error
//
// Revision 1.4  1998/01/23  00:29:12  killough
// Fix SSG reload by using frequency stored in lump
//
// Revision 1.3  1998/01/22  05:55:12  killough
// Removed dead past changes, changed destroy_sample to stop_sample
//
// Revision 1.2  1998/01/21  16:56:18  jim
// Music fixed, defaults for cards added
//
// Revision 1.1.1.1  1998/01/19  14:02:57  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
