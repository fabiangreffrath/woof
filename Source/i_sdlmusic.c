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
//      System interface for SDL music.
//
//-----------------------------------------------------------------------------

// haleyjd
#include "SDL.h"
#include "SDL_mixer.h"

#include "doomstat.h"
#include "i_sound.h"

///
// MUSIC API.
//

// julian (10/25/2005): rewrote (nearly) entirely

#include "mus2mid.h"
#include "memio.h"
#include "m_misc.h"
#include "m_misc2.h"

// Only one track at a time
static Mix_Music *music = NULL;

// Some tracks are directly streamed from the RWops;
// we need to free them in the end
static SDL_RWops *rw = NULL;

// Same goes for buffers that were allocated to convert music;
// since this concerns mus, we could do otherwise but this 
// approach is better for consistency
static void *music_block = NULL;

// Macro to make code more readable
#define CHECK_MUSIC(h) ((h) && music != NULL)

//
// I_ShutdownMusic
//
// atexit handler.
//
static void I_SDL_UnRegisterSong(void *handle);
static void I_SDL_ShutdownMusic(void)
{
   I_SDL_UnRegisterSong((void *)1);
}

//
// I_InitMusic
//
static boolean I_SDL_InitMusic(void)
{
   printf("I_InitMusic: Using SDL_mixer.\n");

   // Initialize SDL_Mixer for MIDI music playback
   // [crispy] initialize some more audio formats
   Mix_Init(MIX_INIT_MID | MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_MP3);

   return true;
}

// jff 1/18/98 changed interface to make mididata destroyable

static void I_SDL_SetMusicVolume(int volume);
static void I_SDL_PlaySong(void *handle, boolean looping)
{
   if(CHECK_MUSIC(handle) && Mix_PlayMusic(music, looping ? -1 : 0) == -1)
   {
      dprintf("I_PlaySong: Mix_PlayMusic failed\n");
      return;
   }
   
   // haleyjd 10/28/05: make sure volume settings remain consistent
   I_SDL_SetMusicVolume(snd_MusicVolume);
}

//
// I_SetMusicVolume
//

static int current_midi_volume;

static void I_SDL_SetMusicVolume(int volume)
{
   current_midi_volume = volume;

   // haleyjd 09/04/06: adjust to use scale from 0 to 15
   Mix_VolumeMusic((current_midi_volume * 128) / 15);
}

//
// I_PauseSong
//
static void I_SDL_PauseSong(void *handle)
{
   if(CHECK_MUSIC(handle))
   {
      // Not for mids
      if(Mix_GetMusicType(music) != MUS_MID)
         Mix_PauseMusic();
      else
      {
         // haleyjd 03/21/06: set MIDI volume to zero on pause
         Mix_VolumeMusic(0);
      }
   }
}

//
// I_ResumeSong
//
static void I_SDL_ResumeSong(void *handle)
{
   if(CHECK_MUSIC(handle))
   {
      // Not for mids
      if(Mix_GetMusicType(music) != MUS_MID)
         Mix_ResumeMusic();
      else
         Mix_VolumeMusic((current_midi_volume * 128) / 15);
   }
}

//
// I_StopSong
//
static void I_SDL_StopSong(void *handle)
{
   if(CHECK_MUSIC(handle))
      Mix_HaltMusic();
}

//
// I_UnRegisterSong
//
static void I_SDL_UnRegisterSong(void *handle)
{
   if(CHECK_MUSIC(handle))
   {   
      // Stop and free song
      I_SDL_StopSong(handle);
      Mix_FreeMusic(music);
      
      // Free RWops
      if(rw != NULL)
         SDL_FreeRW(rw);
      
      // Free music block
      if(music_block != NULL)
         free(music_block);
      
      // Reinitialize all this
      music = NULL;
      rw = NULL;
      music_block = NULL;
   }
}

//
// I_RegisterSong
//
static void *I_SDL_RegisterSong(void *data, int size)
{
   if(music != NULL)
      I_SDL_UnRegisterSong((void *)1);

   if (!IsMus(data, size))
   {
      // Workaround for SDL_mixer doesn't always detect mp3s
      // https://github.com/libsdl-org/SDL_mixer/issues/288
      const SDL_version *ver = Mix_Linked_Version();
      if (ver->major == 2 && ver->minor == 0 && (ver->patch == 2 || ver->patch == 4))
      {
        byte *magic = data;
        if (size >= 2 && magic[0] == 0xFF && magic[1] == 0xF3)
          magic[1] = 0xFA;
      }
      rw    = SDL_RWFromMem(data, size);
      music = Mix_LoadMUS_RW(rw, false);

   }
   else // Assume a MUS file and try to convert
   {
      MEMFILE *instream;
      MEMFILE *outstream;
      byte *mid;
      size_t midlen;
      int result;

      instream = mem_fopen_read(data, size);
      outstream = mem_fopen_write();

      result = mus2mid(instream, outstream);

      if (result == 0)
      {
         void *outbuf;

         mem_get_buf(outstream, &outbuf, &midlen);

         mid = malloc(midlen);
         memcpy(mid, outbuf, midlen);
      }

      mem_fclose(instream);
      mem_fclose(outstream);

      if (result)
      {
         dprintf("Error loading music: %d", result);
         return NULL;
      }

      {
         rw    = SDL_RWFromMem(mid, midlen);
         music = Mix_LoadMUS_RW(rw, false);

         if(music == NULL)
         {
            // Conversion failed, free everything
            SDL_FreeRW(rw);
            rw = NULL;
            free(mid);
         }
         else
         {
            // Conversion succeeded
            // -> save memory block to free when unregistering
            music_block = mid;
         }
      }
   }
   
   // the handle is a simple boolean
   return music;
}

music_module_t music_sdl_module =
{
    I_SDL_InitMusic,
    I_SDL_ShutdownMusic,
    I_SDL_SetMusicVolume,
    I_SDL_PauseSong,
    I_SDL_ResumeSong,
    I_SDL_RegisterSong,
    I_SDL_PlaySong,
    I_SDL_StopSong,
    I_SDL_UnRegisterSong,
};
