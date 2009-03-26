// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: s_sound.c,v 1.11 1998/05/03 22:57:06 killough Exp $
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
//
// DESCRIPTION:  Platform-independent sound code
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: s_sound.c,v 1.11 1998/05/03 22:57:06 killough Exp $";

// killough 3/7/98: modified to allow arbitrary listeners in spy mode
// killough 5/2/98: reindented, removed useless code, beautified

#include "doomstat.h"
#include "s_sound.h"
#include "i_sound.h"
#include "r_main.h"
#include "m_random.h"
#include "w_wad.h"

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST (1200<<FRACBITS)

// Distance to origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
//
// killough 12/98: restore original
// #define S_CLOSE_DIST (160<<FRACBITS)

#define S_CLOSE_DIST (200<<FRACBITS)

#define S_ATTENUATOR ((S_CLIPPING_DIST-S_CLOSE_DIST)>>FRACBITS)

// Adjustable by menu.
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

//jff 1/22/98 make sound enabling variables readable here
extern int snd_card, mus_card;
extern boolean nosfxparm, nomusicparm;
//jff end sound enabling variables readable here

typedef struct channel_s
{
  sfxinfo_t *sfxinfo;      // sound information (if null, channel avail.)
  const mobj_t *origin;    // origin of sound
  int volume;              // volume scale value for effect -- haleyjd 05/29/06
  int pitch;               // pitch modifier -- haleyjd 06/03/06
  int handle;              // handle of the sound being played
  int o_priority;          // haleyjd 09/27/06: stored priority value
  int priority;            // current priority value
  int singularity;         // haleyjd 09/27/06: stored singularity value
  int idnum;               // haleyjd 09/30/06: unique id num for sound event
} channel_t;

// the set of channels available
static channel_t *channels;

// These are not used, but should be (menu).
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume = 15;

// Maximum volume of music. Useless so far.
int snd_MusicVolume = 15;

// whether songs are mus_paused
static boolean mus_paused;

// music currently being played
static musicinfo_t *mus_playing;

// following is set
//  by the defaults code in M_misc:
// number of channels available
int numChannels;
int default_numChannels;  // killough 9/98

//jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

//
// Internals.
//

//
// S_StopChannel
//
// Stops a sound channel.
//
static void S_StopChannel(int cnum)
{
#ifdef RANGECHECK
   if(cnum < 0 || cnum >= numChannels)
      I_Error("S_StopChannel: handle %d out of range\n", cnum);
#endif

   if(channels[cnum].sfxinfo)
   {
      if(I_SoundIsPlaying(channels[cnum].handle))
         I_StopSound(channels[cnum].handle);      // stop the sound playing
      
      // haleyjd 09/27/06: clear the entire channel
      memset(&channels[cnum], 0, sizeof(channel_t));
   }
}

//
// S_AdjustSoundParams
//
// Alters a playing sound's volume and stereo separation to account for
// the position and angle of the listener relative to the source.
//
// haleyjd: added channel volume scale value
// haleyjd: added priority scaling
//
static int S_AdjustSoundParams(const mobj_t *listener, const mobj_t *source,
                               int chanvol, int *vol, int *sep, int *pitch,
                               int *pri)
{
   fixed_t adx, ady, dist;
   angle_t angle;
   int basevolume;            // haleyjd

   // haleyjd 08/12/04: we cannot adjust a sound for a NULL listener.
   if(!listener)
      return 1;

   // haleyjd 05/29/06: this function isn't supposed to be called for NULL sources
#ifdef RANGECHECK
   if(!source)
      I_Error("S_AdjustSoundParams: NULL source\n");
#endif
   
   // calculate the distance to sound origin
   //  and clip it if necessary
   //
   // killough 11/98: scale coordinates down before calculations start
   // killough 12/98: use exact distance formula instead of approximation
   
   adx = abs((listener->x >> FRACBITS) - (source->x >> FRACBITS));
   ady = abs((listener->y >> FRACBITS) - (source->y >> FRACBITS));
   
   if(ady > adx)
      dist = adx, adx = ady, ady = dist;
   
   dist = adx ? FixedDiv(adx, finesine[(tantoangle[FixedDiv(ady,adx) >> DBITS]
                                        + ANG90) >> ANGLETOFINESHIFT]) : 0;

   // haleyjd 05/29/06: allow per-channel volume scaling
   basevolume = (snd_SfxVolume * chanvol) / 15;

   if(!dist)  // killough 11/98: handle zero-distance as special case
   {
      *sep = NORM_SEP;
      *vol = basevolume;
      return *vol > 0;
   }

   if(dist > S_CLIPPING_DIST >> FRACBITS)
      return 0;
  
   // angle of source to listener
   angle = R_PointToAngle2(listener->x, listener->y, source->x, source->y);
   
   if(angle <= listener->angle)
      angle += 0xffffffff;
   angle -= listener->angle;
   angle >>= ANGLETOFINESHIFT;

   // stereo separation
   *sep = NORM_SEP - FixedMul(S_STEREO_SWING>>FRACBITS,finesine[angle]);

   // volume calculation
   *vol = dist < S_CLOSE_DIST >> FRACBITS ? basevolume :
      basevolume * ((S_CLIPPING_DIST>>FRACBITS)-dist) /
      S_ATTENUATOR;

   // haleyjd 09/27/06: decrease priority with volume attenuation
   *pri = *pri + (127 - *vol);
   
   if(*pri > 255) // cap to 255
      *pri = 255;

  return *vol > 0;
}

//
// S_getChannel :
//
//   If none available, return -1.  Otherwise channel #.
//   haleyjd 09/27/06: fixed priority/singularity bugs
//   Note that a higher priority number means lower priority!
//
static int S_getChannel(const mobj_t *origin, sfxinfo_t *sfxinfo,
                        int priority, int singularity)
{
   // channel number to use
   int cnum;
   int lowestpriority = -1; // haleyjd
   int lpcnum = -1;

   // haleyjd 09/28/06: moved this here. If we kill a sound already
   // being played, we can use that channel. There is no need to
   // search for a free one again because we already know of one.

   // kill old sound
   // killough 12/98: replace is_pickup hack with singularity flag
   // haleyjd 06/12/08: only if subchannel matches
   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      if(channels[cnum].sfxinfo &&
         channels[cnum].singularity == singularity &&
         channels[cnum].origin == origin)
      {
         S_StopChannel(cnum);
         break;
      }
   }
   
   // Find an open channel
   if(cnum == numChannels)
   {
      // haleyjd 09/28/06: it isn't necessary to look for playing sounds in
      // the same singularity class again, as we just did that above. Here
      // we are looking for an open channel. We will also keep track of the
      // channel found with the lowest sound priority while doing this.
      for(cnum = 0; cnum < numChannels && channels[cnum].sfxinfo; ++cnum)
      {
         if(channels[cnum].priority > lowestpriority)
         {
            lowestpriority = channels[cnum].priority;
            lpcnum = cnum;
         }
      }
   }

   // None available?
   if(cnum == numChannels)
   {
      // Look for lower priority
      // haleyjd: we have stored the channel found with the lowest priority
      // in the loop above
      if(priority > lowestpriority)
         return -1;                  // No lower priority.  Sorry, Charlie.
      else
      {
         S_StopChannel(lpcnum);      // Otherwise, kick out lowest priority.
         cnum = lpcnum;
      }
   }

#ifdef RANGECHECK
   if(cnum >= numChannels)
      I_Error("S_getChannel: handle %d out of range\n", cnum);
#endif
   
   return cnum;
}


void S_StartSound(const mobj_t *origin, int sfx_id)
{
   int sep, pitch, o_priority, priority, singularity, cnum, handle;
   int volumeScale = 127;
   int volume = snd_SfxVolume;
   sfxinfo_t *sfx;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

#ifdef RANGECHECK
   // check for bogus sound #
   if(sfx_id < 1 || sfx_id > NUMSFX)
      I_Error("Bad sfx #: %d", sfx_id);
#endif

   sfx = &S_sfx[sfx_id];
   
   // Initialize sound parameters
   if(sfx->link)
   {
      pitch = sfx->pitch;
      volumeScale += sfx->volume;      
   }
   else
      pitch = NORM_PITCH;

   // haleyjd 09/29/06: rangecheck volumeScale now!
   if(volumeScale < 0)
      volumeScale = 0;
   else if(volumeScale > 127)
      volumeScale = 127;

   // haleyjd: modified so that priority value is always used
   // haleyjd: also modified to get and store proper singularity value
   o_priority = priority = sfx->priority;
   singularity = sfx->singularity;

   // Check to see if it is audible, modify the params
   // killough 3/7/98, 4/25/98: code rearranged slightly
   
   if(!origin || origin == players[displayplayer].mo)
   {
      sep = NORM_SEP;
      volume = (volume * volumeScale) / 15; // haleyjd 05/29/06: scale volume
      if(volume < 1)
         return;
      if(volume > 127)
         volume = 127;
   }
   else
   {
      if(!S_AdjustSoundParams(players[displayplayer].mo, origin, volumeScale,
                              &volume, &sep, &pitch, &priority))
         return;
      else if(origin->x == players[displayplayer].mo->x &&
              origin->y == players[displayplayer].mo->y)
         sep = NORM_SEP;
  }

   if(pitched_sounds)
   {
      // hacks to vary the sfx pitches
      if(sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
         pitch += 8 - (M_Random()&15);
      else if(sfx_id != sfx_itemup && sfx_id != sfx_tink)
         pitch += 16 - (M_Random()&31);
      
      if(pitch < 0)
         pitch = 0;
      
      if(pitch > 255)
         pitch = 255;
   }

   // try to find a channel
   if((cnum = S_getChannel(origin, sfx, priority, singularity)) < 0)
      return;

#ifdef RANGECHECK
   if(cnum < 0 || cnum >= numChannels)
      I_Error("S_StartSfxInfo: handle %d out of range\n", cnum);
#endif

   channels[cnum].sfxinfo = sfx;
   channels[cnum].origin  = origin;

   while(sfx->link)
      sfx = sfx->link;     // sf: skip thru link(s)

   // Assigns the handle to one of the channels in the mix/output buffer.
   handle = I_StartSound(sfx, cnum, volume, sep, pitch, priority);

   // haleyjd: check to see if the sound was started
   if(handle >= 0)
   {
      channels[cnum].handle = handle;
      
      // haleyjd 05/29/06: record volume scale value
      // haleyjd 06/03/06: record pitch too (wtf is going on here??)
      // haleyjd 09/27/06: store priority and singularity values (!!!)
      channels[cnum].volume      = volumeScale;
      channels[cnum].pitch       = pitch;
      channels[cnum].o_priority  = o_priority;  // original priority
      channels[cnum].priority    = priority;    // scaled priority
      channels[cnum].singularity = singularity;
      channels[cnum].idnum       = I_SoundID(handle); // unique instance id
   }
   else // haleyjd: the sound didn't start, so clear the channel info
      memset(&channels[cnum], 0, sizeof(channel_t));

}

//
// S_StopSound
//
void S_StopSound(const mobj_t *origin)
{
   int cnum;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      if(channels[cnum].sfxinfo && channels[cnum].origin == origin)
      {
         S_StopChannel(cnum);
         break;
      }
   }
}

//
// Stop and resume music, during game PAUSE.
//

void S_PauseSound(void)
{
   if(mus_playing && !mus_paused)
   {
      I_PauseSong(mus_playing->handle);
      mus_paused = true;
   }
}

void S_ResumeSound(void)
{
   if(mus_playing && mus_paused)
   {
      I_ResumeSong(mus_playing->handle);
      mus_paused = false;
   }
}

//
// Updates music & sounds
//

void S_UpdateSounds(const mobj_t *listener)
{
   int cnum;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;
   
   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      channel_t *c = &channels[cnum];
      sfxinfo_t *sfx = c->sfxinfo;

      // haleyjd: has this software channel lost its hardware channel?
      if(c->idnum != I_SoundID(c->handle))
      {
         // clear the channel and keep going
         memset(c, 0, sizeof(channel_t));
         continue;
      }

      if(sfx)
      {
         if(I_SoundIsPlaying(c->handle))
         {
            // initialize parameters
            int volume = snd_SfxVolume;
            int pitch = c->pitch; // haleyjd 06/03/06: use channel's pitch!
            int sep = NORM_SEP;
            int pri = c->o_priority; // haleyjd 09/27/06: priority
            
            // check non-local sounds for distance clipping
            // or modify their params

            if(c->origin && listener != c->origin) // killough 3/20/98
            {
               if(!S_AdjustSoundParams(listener, 
                                       c->origin, 
                                       c->volume, 
                                       &volume,
                                       &sep, 
                                       &pitch,
                                       &pri))
                  S_StopChannel(cnum);
               else
               {
                  I_UpdateSoundParams(c->handle, volume, sep, pitch);
                  c->priority = pri; // haleyjd
               }
            }
         }
         else   // if channel is allocated but sound has stopped, free it
            S_StopChannel(cnum);
        }
    }
}

void S_SetMusicVolume(int volume)
{
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;

#ifdef RANGECHECK
   if(volume < 0 || volume > 16)
      I_Error("Attempt to set music volume at %d\n", volume);
#endif

   // haleyjd: I don't think it should do this in SDL
#if 0
   I_SetMusicVolume(127);
#endif

   I_SetMusicVolume(volume);
   snd_MusicVolume = volume;
}


void S_SetSfxVolume(int volume)
{
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;
   
#ifdef RANGECHECK
   if(volume < 0 || volume > 127)
      I_Error("Attempt to set sfx volume at %d", volume);
#endif
   
   snd_SfxVolume = volume;
}

void S_ChangeMusic(int musicnum, int looping)
{
   musicinfo_t *music;
   
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;
   
   if(musicnum <= mus_None || musicnum >= NUMMUSIC)
      I_Error("Bad music number %d", musicnum);
   
   music = &S_music[musicnum];
   
   if(mus_playing == music)
      return;
   
   // shutdown old music
   S_StopMusic();
   
   // get lumpnum if neccessary
   if(!music->lumpnum)
   {
      char namebuf[9];
      sprintf(namebuf, "d_%s", music->name);
      music->lumpnum = W_GetNumForName(namebuf);
   }
   
   // load & register it
   music->data   = W_CacheLumpNum(music->lumpnum, PU_STATIC);
   // julian: added lump length
   music->handle = I_RegisterSong(music->data, W_LumpLength(music->lumpnum));
   
   // play it
   I_PlaySong(music->handle, looping);
   
   mus_playing = music;
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
   S_ChangeMusic(m_id, false);
}

void S_StopMusic(void)
{
   if(!mus_playing)
      return;
   
   if(mus_paused)
      I_ResumeSong(mus_playing->handle);
   
   I_StopSong(mus_playing->handle);
   I_UnRegisterSong(mus_playing->handle);
   Z_ChangeTag(mus_playing->data, PU_CACHE);
   
   mus_playing->data = NULL;
   mus_playing = NULL;
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start(void)
{
   int cnum,mnum;
   
   // kill all playing sounds at start of level
   //  (trust me - a good idea)
   
   //jff 1/22/98 skip sound init if sound not enabled
   if(snd_card && !nosfxparm)
   {
      for(cnum = 0; cnum < numChannels; ++cnum)
      {
         if(channels[cnum].sfxinfo)
            S_StopChannel(cnum);
      }
   }

   //jff 1/22/98 return if music is not enabled
   if (!mus_card || nomusicparm)
      return;
   
   // start new music for the level
   mus_paused = 0;
   
   if(idmusnum!=-1)
      mnum = idmusnum; //jff 3/17/98 reload IDMUS music if not -1
   else
   {
      if (gamemode == commercial)
         mnum = mus_runnin + gamemap - 1;
      else
      {
         static const int spmus[] =     // Song - Who? - Where?
         {
            mus_e3m4,     // American     e4m1
            mus_e3m2,     // Romero       e4m2
            mus_e3m3,     // Shawn        e4m3
            mus_e1m5,     // American     e4m4
            mus_e2m7,     // Tim  e4m5
            mus_e2m4,     // Romero       e4m6
            mus_e2m6,     // J.Anderson   e4m7 CHIRON.WAD
            mus_e2m5,     // Shawn        e4m8
            mus_e1m9      // Tim          e4m9
         };

         if(gameepisode < 4)
            mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
         else
            mnum = spmus[gamemap-1];
      }
   }

   S_ChangeMusic(mnum, true);
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(int sfxVolume, int musicVolume)
{
   //jff 1/22/98 skip sound init if sound not enabled
   if(snd_card && !nosfxparm)
   {
      printf("S_Init: default sfx volume %d\n", sfxVolume);  // killough 8/8/98
      
      // haleyjd
      I_SetChannels();
      
      S_SetSfxVolume(sfxVolume);
      
      // Allocating the internal channels for mixing
      // (the maximum numer of sounds rendered
      // simultaneously) within zone memory.
      
      // killough 10/98:
      channels = calloc(numChannels = default_numChannels, sizeof(channel_t));
   }

   S_SetMusicVolume(musicVolume);
   
   // no sounds are playing, and they are not mus_paused
   mus_paused = 0;
}

//----------------------------------------------------------------------------
//
// $Log: s_sound.c,v $
// Revision 1.11  1998/05/03  22:57:06  killough
// beautification, #include fix
//
// Revision 1.10  1998/04/27  01:47:28  killough
// Fix pickups silencing player weapons
//
// Revision 1.9  1998/03/23  03:39:12  killough
// Fix spy-mode sound effects
//
// Revision 1.8  1998/03/17  20:44:25  jim
// fixed idmus non-restore, space bug
//
// Revision 1.7  1998/03/09  07:32:57  killough
// ATTEMPT to support hearing with displayplayer's hears
//
// Revision 1.6  1998/03/04  07:46:10  killough
// Remove full-volume sound hack from MAP08
//
// Revision 1.5  1998/03/02  11:45:02  killough
// Make missing sounds non-fatal
//
// Revision 1.4  1998/02/02  13:18:48  killough
// stop incorrect looping of music (e.g. bunny scroll)
//
// Revision 1.3  1998/01/26  19:24:52  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/23  01:50:49  jim
// Added music/sound options, and enables
//
// Revision 1.1.1.1  1998/01/19  14:03:04  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
