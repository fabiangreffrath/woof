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

// ID24 EndFinale - [EA] Woof extension to the spec
typedef enum bunny_line_e
{
  BUNNY_LEFT,
  BUNNY_UP,
  BUNNY_RIGHT,
  BUNNY_DOWN,
} bunny_line_t;

// ID24 EndFinale - Custom "bunny" scroller
typedef struct end_bunny_s
{
  bunny_line_t  orientation;  // Left, up, right or down
  const char   *stitchimage;  // e.g PFUB2
  int           overlay;      // e.g END0
  int           overlaycount; // How many frames?
  int           overlaysound; // Sound index
  int           overlayx;
  int           overlayy;
} end_bunny_t;

// ID24 EndFinale - Custom cast call, per-callee frame
typedef struct cast_frame_s
{
  const char *lump;
  boolean     flipped;
  int         duration;
  int         sound;
} cast_frame_t;

// ID24 EndFinale - Custom cast call, callee
typedef struct cast_entry_s
{
  const char   *name;             // BEX [STRINGS] mnemonic
  int           alertsound;
  cast_frame_t *aliveframes;
  cast_frame_t *deathframes;
  int           aliveframescount; // Book-keeping
  int           deathframescount; // Book-keeping
} cast_entry_t;

// [EA] Not sure why it is like this, but this is the setup in the official
// Legacy of Rust's finale lumps. Why the awkward nesting? Not obviously useful,
// but keeping it anyway as a mirror of the actual JSON and just in case.
typedef struct end_cast_s
{
  cast_entry_t *castanims;
  int           castanimscount; // Book-keeping
} end_cast_t;

// ID24 EndFinale
typedef struct end_finale_s
{
  end_type_t   type;
  const char  *music;      // e.g. `D_EVIL` or `D_BUNNY` or `D_VICTOR`
  const char  *background; // e.g. `BOSSBACK` or `PFUB1` or `ENDPIC`
  boolean      musicloops;
  boolean      donextmap;
  end_bunny_t  bunny;
  end_cast_t   castrollcall;
} end_finale_t;

end_finale_t *D_ParseEndFinale(const char lump[9]);

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
