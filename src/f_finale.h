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
//   Related to f_finale.c, which is called at the end of a level
//    
//-----------------------------------------------------------------------------


#ifndef __F_FINALE__
#define __F_FINALE__

#include "doomtype.h"

struct event_s;

//
// FINALE
//

// Called by main loop.
boolean F_Responder(struct event_s *ev);

// Called by main loop.
void F_Ticker (void);

// Called by main loop.
void F_Drawer (void);

void F_StartFinale (void);


//
// ID24 EndFinale extensions
//

// Custom EndFinale type
typedef enum end_type_e
{
  END_ART,    // Plain graphic, e.g CREDIT, VICTORY2, ENDPIC
  END_SCROLL, // Custom "bunny" scroller
  END_CAST,   // Custom cast call
} end_type_t;

// Custom cast call, per-callee frame
typedef struct cast_frame_s
{
  char        lump[9];
  boolean     flipped;
  int         duration;
  int         sound;
} cast_frame_t;

// Custom cast call, callee
typedef struct cast_entry_s
{
  const char   *name;             // BEX [STRINGS] mnemonic
  int           alertsound;
  cast_frame_t *aliveframes;
  cast_frame_t *deathframes;
  int           aliveframescount; // Book-keeping
  int           deathframescount; // Book-keeping
} cast_anim_t;


// ID24 EndFinale
typedef struct end_finale_s
{
  end_type_t   type;
  char         music[9];      // e.g. `D_EVIL` or `D_BUNNY` or `D_VICTOR`
  char         background[9]; // e.g. `BOSSBACK` or `PFUB1` or `ENDPIC`
  boolean      musicloops;
  boolean      donextmap;
  char         bunny_stitchimage[9];
  int          bunny_overlay;
  int          bunny_overlaycount;
  int          bunny_overlaysound;
  int          bunny_overlayx;
  int          bunny_overlayy;
  int          cast_animscount;
  cast_anim_t *cast_anims;
} end_finale_t;

end_finale_t *F_ParseEndFinale(const char lump[9]);

#endif

//----------------------------------------------------------------------------
//
// $Log: f_finale.h,v $
// Revision 1.3  1998/05/04  21:58:52  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:47  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
