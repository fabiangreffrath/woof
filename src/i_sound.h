// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sound.h,v 1.4 1998/05/03 22:31:58 killough Exp $
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
//
// DESCRIPTION:
//      System interface, sound.
//
//-----------------------------------------------------------------------------

#ifndef __I_SOUND__
#define __I_SOUND__

#include <stdio.h>

#include "sounds.h"

// Adjustable by menu.
// [FG] moved here from i_sound.c
#define MAX_CHANNELS 32
// [FG] moved here from s_sound.c
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

extern int snd_samplerate;

// [FG] precache all sound SFX
extern boolean precache_sounds;
// [FG] optional low-pass filter
extern boolean lowpass_filter;
// [FG] variable pitch bend range
extern int pitch_bend_range;

// Init at program start...
void I_InitSound(void);

// ... update sound buffer and audio device at runtime...
void I_UpdateSound(void);
void I_SubmitSound(void);

// ... shut down and relase at program termination.
void I_ShutdownSound(void);

//
//  SFX I/O
//

// Initialize channels?
void I_SetChannels(void);

// Get raw data lump index for sound descriptor.
int I_GetSfxLumpNum(sfxinfo_t *sfxinfo);

// Starts a sound in a particular sound channel.
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch, 
                 int pri, boolean loop);

// Stops a sound channel.
void I_StopSound(int handle);

// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
int I_SoundIsPlaying(int handle);

// Updates the volume, separation,
//  and pitch of a sound channel.
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch);

// haleyjd
int I_SoundID(int handle);

//
//  MUSIC I/O
//

typedef struct
{
    boolean (*I_InitMusic)(void);
    void (*I_ShutdownMusic)(void);
    void (*I_SetMusicVolume)(int volume);
    void (*I_PauseSong)(void *handle);
    void (*I_ResumeSong)(void *handle);
    void *(*I_RegisterSong)(void *data, int size);
    void (*I_PlaySong)(void *handle, boolean looping);
    void (*I_StopSong)(void *handle);
    void (*I_UnRegisterSong)(void *handle);
} music_module_t;

typedef enum
{
#if defined(_WIN32)
    midi_player_win,
#else
    midi_player_sdl,
#endif
#if defined(HAVE_FLUIDSYNTH)
    midi_player_fl,
#endif
    midi_player_opl,
    num_midi_players,
} midi_player_t;

extern midi_player_t midi_player;

boolean I_InitMusic(void);
void I_ShutdownMusic(void);

// Volume.
void I_SetMusicVolume(int volume);

// PAUSE game handling.
void I_PauseSong(void *handle);
void I_ResumeSong(void *handle);

// Registers a song handle to song data.
void *I_RegisterSong(void *data, int size);

// Called by anything that wishes to start music.
//  plays a song, and when the song is done,
//  starts playing it again in an endless loop.
// Horrible thing to do, considering.
void I_PlaySong(void *handle, boolean looping);

// Stops a song over 3 seconds.
void I_StopSong(void *handle);

// See above (register), then think backwards
void I_UnRegisterSong(void *handle);

// Determine whether memory block is a .mid file
boolean IsMid(byte *mem, int len);

// Determine whether memory block is a .mus file
boolean IsMus(byte *mem, int len);

#endif

//----------------------------------------------------------------------------
//
// $Log: i_sound.h,v $
// Revision 1.4  1998/05/03  22:31:58  killough
// beautification, add some external declarations
//
// Revision 1.3  1998/02/23  04:27:08  killough
// Add variable pitched sound support
//
// Revision 1.2  1998/01/26  19:26:57  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
