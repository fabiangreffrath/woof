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
// DESCRIPTION:
//      The not so system specific sound interface.
//
//-----------------------------------------------------------------------------

#ifndef __S_SOUND__
#define __S_SOUND__

#include "doomtype.h"

typedef enum {
  PITCH_FULL,
  PITCH_HALF,
  PITCH_NONE
} pitchrange_t;

struct mobj_s;

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

void S_StopChannels(void);

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//

// Thing at <origin> emits sound. No rumble.
#define S_StartSound(o, i) S_StartSoundPitch((o), (i), PITCH_FULL)
void S_StartSoundPitch(const struct mobj_s *origin, int sfx_id,
                       pitchrange_t pitch_range);

// Thing at <origin> emits sound. Rumbles if displayplayer is <origin>.
#define S_StartSoundEx(o, i) S_StartSoundPitchEx((o), (i), PITCH_FULL)
void S_StartSoundPitchEx(const struct mobj_s *origin, int sfx_id,
                         pitchrange_t pitch_range);
void S_StartSoundPistol(const struct mobj_s *origin, int sfx_id);
void S_StartSoundShotgun(const struct mobj_s *origin, int sfx_id);
void S_StartSoundSSG(const struct mobj_s *origin, int sfx_id);
void S_StartSoundCGun(const struct mobj_s *origin, int sfx_id);
void S_StartSoundBFG(const struct mobj_s *origin, int sfx_id);

// Thing at <origin> emits sound. Rumbles preset if displayplayer is <origin>.
void S_StartSoundPreset(const struct mobj_s *origin, int sfx_id,
                        pitchrange_t pitch_range);
void S_StartSoundPain(const struct mobj_s *origin, int sfx_id);
void S_StartSoundHitFloor(const struct mobj_s *origin, int sfx_id);

// Thing at <source> causes sound. Thing at <origin> emits sound.
// Rumbles if displayplayer is <source>.
void S_StartSoundSource(const struct mobj_s *source,
                        const struct mobj_s *origin, int sfx_id);
void S_StartSoundMissile(const struct mobj_s *source,
                         const struct mobj_s *origin, int sfx_id);

// Thing at <source> causes sound. Thing at <origin> emits sound.
// Rumbles if displayplayer is <source>, based on distance to <origin>.
void S_StartSoundOrigin(const struct mobj_s *source,
                        const struct mobj_s *origin, int sfx_id);

// Stop sound for thing at <origin>
void S_StopSound(const struct mobj_s *origin);

void S_StopAmbientSounds(void);

// [FG] play sounds in full length
extern boolean full_sounds;
// [FG] removed map objects may finish their sounds
void S_UnlinkSound(struct mobj_s *origin);

// Start music using <music_id> from sounds.h
void S_StartMusic(int music_id);

// Start music using <music_id> from sounds.h, and set whether looping
void S_ChangeMusic(int music_id, int looping);
void S_ChangeMusInfoMusic(int lumpnum, int looping);

// Stops the music fer sure.
void S_StopMusic(void);

void S_PauseSound(void);
void S_ResumeSound(void);

// Stop and resume music, during game PAUSE.
void S_PauseMusic(void);
void S_ResumeMusic(void);

void S_RestartMusic(void);

//
// Updates music & sounds
//
void S_InitListener(const struct mobj_s *listener);
void S_UpdateSounds(const struct mobj_s *listener);
void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);

// machine-independent sound params
extern int snd_channels;

// jff 3/17/98 holds last IDMUS number, or -1
extern int idmusnum;

void S_BindSoundVariables(void);

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
