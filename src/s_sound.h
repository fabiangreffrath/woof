// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: s_sound.h,v 1.4 1998/05/03 22:57:36 killough Exp $
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
//      The not so system specific sound interface.
//
//-----------------------------------------------------------------------------

#ifndef __S_SOUND__
#define __S_SOUND__

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init(int sfxVolume, int musicVolume);

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start(void);

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//
void S_StartSound(const mobj_t *origin, int sound_id);

// Stop sound for thing at <origin>
void S_StopSound(const mobj_t *origin);

// Start music using <music_id> from sounds.h
void S_StartMusic(int music_id);

// Start music using <music_id> from sounds.h, and set whether looping
void S_ChangeMusic(int music_id, int looping);

// Stops the music fer sure.
void S_StopMusic(void);

// Stop and resume music, during game PAUSE.
void S_PauseSound(void);
void S_ResumeSound(void);

//
// Updates music & sounds
//
void S_UpdateSounds(const mobj_t *listener);
void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);

// machine-independent sound params
extern int numChannels;
extern int default_numChannels;  // killough 10/98

//jff 3/17/98 holds last IDMUS number, or -1
extern int idmusnum;

#endif

//----------------------------------------------------------------------------
//
// $Log: s_sound.h,v $
// Revision 1.4  1998/05/03  22:57:36  killough
// beautification, add external declarations
//
// Revision 1.3  1998/04/27  01:47:32  killough
// Fix pickups silencing player weapons
//
// Revision 1.2  1998/01/26  19:27:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
