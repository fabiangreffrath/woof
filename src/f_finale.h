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
typedef enum ef_type_e {
  END_ART,    // Plain graphic, e.g CREDIT, VICTORY2, ENDPIC
  END_SCROLL, // Custom "bunny" scroller
  END_CAST,   // Custom cast call
} ef_type_t;

// ID24 EndFinale - Custom "bunny" scroller
typedef struct ef_scroll_s {
  char stitchimage[9]; // e.g PFUB2
  int  overlay;        // e.g END0, END1, and so on
  int  overlaycount;
  int  overlaysound;   // Sound index
  int  overlayx;
  int  overlayy;
} ef_scroll_t;

// ID24 EndFinale - Custom cast call
typedef struct cast_frame_s {
  char    lump[9];   // Enemy sprite
  boolean flipped;
  int     durations;
  int     sound;     // Sound index
} cast_frame_t;

// ID24 EndFinale - Custom cast call
typedef struct cast_entry_s {
  char         *name;        // BEX [STRINGS] mnemonic
  int           alertsound;  // Sound index
  cast_frame_t *aliveframes; // Before pressing the "use" key
  cast_frame_t *deathframes; // After pressing the "use" key
} cast_entry_t;

typedef struct ef_cast_s {
  // Not sure why it is like this, but this is how it is setup in Legacy of Rust 1.2
  cast_entry_t *castanims;
} ef_cast_t;

typedef struct end_finale_s {
  ef_type_t     type;
  char          music[9];      // e.g. `D_EVIL`
  char          background[9]; // e.g. `BOSSBACK`
  boolean       musicloops;    // e.g. `false`
  boolean       donextmap;     // e.g. `false`
  ef_scroll_t   bunny;         // Only read if `type == END_SCROLL`
  ef_cast_t     castrollcall;  // Only read if `type == END_CAST`
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
