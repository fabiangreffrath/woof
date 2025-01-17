//
//  Copyright (C) 2025 Guilherme Miranda
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
//  DESCRIPTION:
//    ID24 End Finale Specification - User customizable finale sequence.
//

#ifndef _F_ENDFINALE_
#define _F_ENDFINALE_

#include "doomtype.h"

typedef enum {
    END_ART_SCREEN,
    END_BUNNY_SCROLLER,
    END_CAST_ROLL_CALL,
} finale_t;

typedef struct {
    // Equivalent to PFUB2; `end_finale_t::background` is equivalent PFUB1
    char stitchimage[9];
    int  overlay;
    int  overlaycount;
    int  overlaysound;
    int  overlayx;
    int  overlayy;
} finale_bunny_t;

typedef struct {
    char    lump[9];   // Enemy sprite
    boolean flipped;
    int     durations;
    int     sound;     // Sound index
} cast_call_frame_t;

typedef struct {
    char              *name;        // BEX [STRINGS] mnemonic
    int                alertsound;  // Sound index
    cast_call_frame_t *aliveframes; // Before pressing "use"
    cast_call_frame_t *deathframes; // After pressing "use"
} cast_call_entry_t;

typedef struct {
    // Not sure why it is like this, but this is how it is setup in Legacy of Rust 1.2
    cast_call_entry_t *castanims;
} finale_cast_t;

typedef struct {
    finale_t        type;
    char            music[9];      // Default: `D_EVIL`
    char            background[9]; // Default: `BOSSBACK`
    boolean         donextmap;     // Default: `false`
    finale_bunny_t *bunny;         // Only read if `type == END_BUNNY_SCROLLER`
    finale_cast_t  *castrollcall;  // Only read if `type == END_CAST_ROLL_CALL`
} end_finale_t;

#endif
