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

static const char
rcsid[] = "$Id: i_sound.c,v 1.15 1998/05/03 22:32:33 killough Exp $";

#include <stdio.h>
//#include <allegro.h>

// haleyjd
#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_mixer.h"
#include <math.h>

#include "doomstat.h"
#include "mmus2mid.h"   //jff 1/16/98 declarations for MUS->MIDI converter
#include "i_sound.h"
#include "w_wad.h"
#include "g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "d_main.h"
#include "d_io.h"

// Needed for calling the actual sound output.
int SAMPLECOUNT = 512;

// haleyjd
#define MAX_CHANNELS 32
#define NUM_CHANNELS 256

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

// MWM 2000-01-08: Sample rate in samples/second
int snd_samplerate=11025;

// The actual output device.
int audio_fd;

typedef struct {
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *id;
  // The channel step amount...
  unsigned int step;
  // ... and a 0.16 bit remainder of last step.
  unsigned int stepremainder;
  unsigned int samplerate;
  // The channel data pointers, start and end.
  unsigned char* data;
  unsigned char* enddata;
  // Time/gametic that the channel started playing,
  //  used to determine oldest, which automatically
  //  has lowest priority.
  // In case number of active sounds exceeds
  //  available channels.
  int starttime;
  // Hardware left and right channel volume lookup.
  int *leftvol_lookup;
  int *rightvol_lookup;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup, unused.
int steptable[256];

// Volume lookups.
int vol_lookup[128*256];

/* cph 
 * stopchan
 * Stops a sound, unlocks the data 
 */
static void stopchan(int i)
{
   if(!snd_init)
      return;

   if(channelinfo[i].data) /* cph - prevent excess unlocks */
   {
      channelinfo[i].data = NULL;      
   }
}

//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
// haleyjd: needs to take a sfxinfo_t ptr, not a sound id num
//
int addsfx(sfxinfo_t *sfx, int channel)
{
   size_t len;
   int lump;

   if(!snd_init)
      return channel;

   stopchan(channel);
   
   // We will handle the new SFX.
   // Set pointer to raw data.

   // haleyjd 11/05/03: rewrote to minimize work and fully support
   // precaching

   // haleyjd: Eternity sfxinfo_t does not have a lumpnum field
   lump = I_GetSfxLumpNum(sfx);
   
   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = W_GetNumForName("DSPISTOL");

   if(!sfx->data)
      sfx->data = W_CacheLumpNum(lump, PU_STATIC);

   /* Find padded length */   
   len = W_LumpLength(lump);   
   len -= 8;

   channelinfo[channel].data = sfx->data;
   
   /* Set pointer to end of raw data. */
   channelinfo[channel].enddata = channelinfo[channel].data + len - 1;
   channelinfo[channel].samplerate = (channelinfo[channel].data[3]<<8)+channelinfo[channel].data[2];
   channelinfo[channel].data += 8; /* Skip header */
   
   channelinfo[channel].stepremainder = 0;
   // Should be gametic, I presume.
   channelinfo[channel].starttime = gametic;
   
   // Preserve sound SFX id,
   //  e.g. for avoiding duplicates of chainsaw.
   channelinfo[channel].id = sfx;
   
   return channel;
}

int forceFlipPan;

static void updateSoundParams(int handle, int volume, int seperation, int pitch)
{
   int slot = handle;
   int rightvol;
   int leftvol;
   int step = steptable[pitch];
   
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle>=MAX_CHANNELS)
      I_Error("I_UpdateSoundParams: handle out of range");
#endif
   // Set stepping
   // MWM 2000-12-24: Calculates proportion of channel samplerate
   // to global samplerate for mixing purposes.
   // Patched to shift left *then* divide, to minimize roundoff errors
   // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
   if(pitched_sounds)
      channelinfo[slot].step = step + (((channelinfo[slot].samplerate<<16)/snd_samplerate)-65536);
   else
      channelinfo[slot].step = ((channelinfo[slot].samplerate<<16)/snd_samplerate);
   
   // Separation, that is, orientation/stereo.
   //  range is: 1 - 256
   seperation += 1;

   // SoM 7/1/02: forceFlipPan accounted for here
   if(forceFlipPan)
      seperation = 257 - seperation;
   
   // Per left/right channel.
   //  x^2 seperation,
   //  adjust volume properly.
   volume *= 8;

   leftvol = volume - ((volume*seperation*seperation) >> 16);
   seperation = seperation - 257;
   rightvol= volume - ((volume*seperation*seperation) >> 16);  

   // Sanity check, clamp volume.
   if(rightvol < 0 || rightvol > 127)
      I_Error("rightvol out of bounds");
   
   if(leftvol < 0 || leftvol > 127)
      I_Error("leftvol out of bounds");
   
   // Get the proper lookup table piece
   //  for this volume level???
   channelinfo[slot].leftvol_lookup = &vol_lookup[leftvol*256];
   channelinfo[slot].rightvol_lookup = &vol_lookup[rightvol*256];
}

// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//

// Update the sound parameters. Used to control volume,
// pan, and pitch changes such as when a player turns.

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   if(!snd_init)
      return;

   SDL_LockAudio();
   updateSoundParams(handle, vol, sep, pitch);
   SDL_UnlockAudio();
}

void I_SetChannels()
{
   int i;
   int j;
   
   int *steptablemid = steptable + 128;
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; i++)
   {
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   }
   
   // This table provides step widths for pitch parameters.
   // I fail to see that this is currently used.
   for(i=-128 ; i<128 ; i++)
   {
      steptablemid[i] = (int)(pow(1.2, ((double)i/(64.0*snd_samplerate/11025)))*65536.0);
   }
   
   
   // Generates volume lookup tables
   //  which also turn the unsigned samples
   //  into signed samples.
   for(i = 0; i < 128; i++)
   {
      for(j = 0; j < 256; j++)
      {
         // proff - made this a little bit softer, because with
         // full volume the sound clipped badly (191 was 127)
         vol_lookup[i*256+j] = (i*(j-128)*256)/191;
      }
   }
}


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
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
  char namebuf[9];
  memset(namebuf, 0, sizeof(namebuf));  
  sprintf(namebuf, "ds%s", sfx->name);
  return W_CheckNumForName(namebuf);
}

// Almost all of the sound code from this point on was
// rewritten by Lee Killough, based on Chi's rough initial
// version.


// This function adds a sound to the list of currently
// active sounds, which is maintained as a given number
// of internal channels. Returns a handle.

int I_StartSound(int sfx, int   vol, int sep, int pitch, int pri)
{
  static int handle = -1;
  sfxinfo_t *sound;

  if(!snd_init)
     return 0;
  
  sound = &S_sfx[sfx];

  // move up one slot, with wraparound
  if(++handle >= MAX_CHANNELS)
    handle = 0;

   SDL_LockAudio();
   
   // haleyjd 09/03/03: this should use handle, NOT cnum, and
   // the return value is plain redundant. Whoever wrote this was
   // out of it.
   addsfx(sound, handle);
      
   updateSoundParams(handle, vol, sep, pitch);
   SDL_UnlockAudio();

  // Reference for s_sound.c to use when calling functions below
  return handle;
}

// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.

void I_StopSound (int handle)
{
   if(!snd_init)
      return;
   
#ifdef RANGECHECK
   if(handle >= MAX_CHANNELS)
      I_Error("I_StopSound: handle out of range");
#endif
   
   SDL_LockAudio();
   stopchan(handle);
   SDL_UnlockAudio();
}

// We can pretend that any sound that we've associated a handle
// with is always playing.

int I_SoundIsPlaying(int handle)
{
   if(!snd_init)
      return 0;
   
#ifdef RANGECHECK
   if(handle >= MAX_CHANNELS)
      I_Error("I_SoundIsPlaying: handle out of range");
#endif
   return (channelinfo[handle].data != NULL);
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

void I_UpdateSound( void )
{
}

static void I_SDLUpdateSound(void *userdata, Uint8 *stream, int len)
{
   // Mix current sound data.
   // Data, from raw sound, for right and left.
   register unsigned char sample;
   register int dl;
   register int dr;
   
   // Pointers in audio stream, left, right, end.
   short *leftout;
   short *rightout;
   short *leftend;

   // Step in stream, left and right, thus two.
   int step;
   
   // Mixing channel index.
   int chan;
   
   // Left and right channel
   //  are in audio stream, alternating.
   leftout  = (signed short *)stream;
   rightout = ((signed short *)stream)+1;
   step = 2;
   
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = leftout + (len / 4) * step;
   
   // Mix sounds into the mixing buffer.
   // Loop over step*SAMPLECOUNT,
   //  that is 512 values for two channels.
   while(leftout != leftend)
   {
      // Reset left/right value. 
      dl = *leftout;
      dr = *rightout;
      
      // Love thy L2 cache - made this a loop.
      // Now more channels could be set at compile time
      //  as well. Thus loop those  channels.
      for(chan = 0; chan < MAX_CHANNELS; chan++ )
      {
         // Check channel, if active.
         if(channelinfo[chan].data)
         {
            // Get the raw data from the channel. 
            // no filtering
            // sample = *channelinfo[chan].data;
            // linear filtering
            sample = (((unsigned int)channelinfo[chan].data[0] * (0x10000 - channelinfo[chan].stepremainder))
               + ((unsigned int)channelinfo[chan].data[1] * (channelinfo[chan].stepremainder))) >> 16;
            
            // Add left and right part
            //  for this channel (sound)
            //  to the current data.
            // Adjust volume accordingly.
            dl += channelinfo[chan].leftvol_lookup[sample];
            dr += channelinfo[chan].rightvol_lookup[sample];

            // Increment index ???
            channelinfo[chan].stepremainder += channelinfo[chan].step;

            // MSB is next sample???
            channelinfo[chan].data += channelinfo[chan].stepremainder >> 16;

            // Limit to LSB???
            channelinfo[chan].stepremainder &= 0xffff;
            
            // Check whether we are done.
            if(channelinfo[chan].data >= channelinfo[chan].enddata)
               stopchan(chan);
         }
      }
      
      // Clamp to range. Left hardware channel.
      if(dl > SHRT_MAX)
      {
         *leftout = SHRT_MAX;
      }
      else if(dl < SHRT_MIN)
      {
         *leftout = SHRT_MIN;
      }
      else
      {
         *leftout = (short)dl;
      }
      
      // Same for right hardware channel.
      if(dr > SHRT_MAX)
      {
         *rightout = SHRT_MAX;
      }
      else if(dr < SHRT_MIN)
      {
         *rightout = SHRT_MIN;
      }
      else
      {
         *rightout = (short)dr;
      }
      
      // Increment current pointers in stream
      leftout += step;
      rightout += step;
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

void I_ShutdownSound(void)
{
   if(snd_init)
   {
      Mix_CloseAudio();
      snd_init = 0;
   }
}

void I_InitSound(void)
{
   if(!nosfxparm)
   {
      int audio_buffers;

      puts("I_InitSound: ");

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
  
      if(Mix_OpenAudio(snd_samplerate, MIX_DEFAULT_FORMAT, 2, audio_buffers) < 0)
      {
         printf("Couldn't open audio with desired format.\n");
         snd_card = 0;
         mus_card = 0;
         return;
      }

      SAMPLECOUNT = audio_buffers;
      Mix_SetPostMix(I_SDLUpdateSound, NULL);
      printf("Configured audio device with %d samples/slice.\n", SAMPLECOUNT);

      atexit(I_ShutdownSound);

      snd_init = true;

      // haleyjd 04/11/03: don't use music if sfx aren't init'd
      // (may be dependent, docs are unclear)
      if(!nomusicparm)
         I_InitMusic();
   }   
}

///
// MUSIC API.
//

#include "mmus2mid.h"
#include "m_misc.h"

static Mix_Music *music[2] = { NULL, NULL };

const char *music_name = "eetemp.mid";

void I_ShutdownMusic(void)
{
   I_StopSong(0);
}

void I_InitMusic(void)
{
   switch(mus_card)
   {
   case -1:
      printf("I_InitMusic: Using SDL_mixer.\n");
      mus_init = true;
      break;   
   default:
      printf("I_InitMusic: Using No MIDI Device.\n");
      break;
   }
   
   atexit(I_ShutdownMusic);
}

// jff 1/18/98 changed interface to make mididata destroyable

void I_PlaySong(int handle, int looping)
{
   if(!mus_init)
      return;

   if(handle >= 0 && music[handle])
   {
      if(Mix_PlayMusic(music[handle], looping ? -1 : 0) == -1)
         I_Error("I_PlaySong: please report this error\n");
   }
}

void I_SetMusicVolume(int volume)
{
   if(!mus_init)
      return;
   
   Mix_VolumeMusic(volume*8);
}

void I_PauseSong (int handle)
{
}

void I_ResumeSong (int handle)
{
}

void I_StopSong(int handle)
{
   if(!mus_init)
      return;
   
   Mix_HaltMusic();
}

void I_UnRegisterSong(int handle)
{
   if(!mus_init)
      return;

   if(handle >= 0 && music[handle])
   {
      Mix_FreeMusic(music[handle]);
      music[handle] = NULL;
   }
}

// jff 1/16/98 created to convert data to MIDI ala Allegro

int I_RegisterSong(void *data)
{
   int err;
   MIDI mididata;
   char fullMusicName[PATH_MAX + 1];

   UBYTE *mid;
   int midlen;

   music[0] = NULL; // ensure its null

   // haleyjd: don't return negative music handles
   if(!mus_init)
      return 0;

   memset(&mididata,0,sizeof(MIDI));

   if((err = MidiToMIDI((byte *)data, &mididata)) &&    // try midi first
      (err = mmus2mid((byte *)data, &mididata, 89, 0))) // now try mus      
   {
      dprintf("Error loading midi: %d", err);
      return 0;
   }

   MIDIToMidi(&mididata,&mid,&midlen);

   // haleyjd 03/15/03: fixed for -cdrom
   if(M_CheckParm("-cdrom"))
      sprintf(fullMusicName, "%s/%s", "c:/doomdata", music_name);
   else
      sprintf(fullMusicName, "%s/%s", D_DoomExeDir(), music_name);
   
   if(!M_WriteFile(fullMusicName, mid, midlen))
   {
      dprintf("Error writing music to %s", music_name);
      free(mid);      
      return 0;
   }

   free(mid);

   music[0] = Mix_LoadMUS(fullMusicName);
   
   if(!music[0])
   {
      dprintf("Couldn't load MIDI from %s: %s\n", 
              fullMusicName, Mix_GetError());
   }
   
   return 0;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  return 0;
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
