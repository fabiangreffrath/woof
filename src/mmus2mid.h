//-----------------------------------------------------------------------------
//
// $Id: mmus2mid.h,v 1.6 1998/05/10 23:00:48 jim Exp $
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
//-----------------------------------------------------------------------------

#if !defined( MMUS2MID_H )
#define MMUS2MID_H

// error codes

typedef enum
{
  MUSDATACOR,    // MUS data corrupt 
  TOOMCHAN,      // Too many channels 
  MEMALLOC,      // Memory allocation error 
  MUSDATAMT,     // MUS file empty 
  BADMUSCTL,     // MUS event 5 or 7 found 
  BADSYSEVT,     // MUS system event not in 10-14 range 
  BADCTLCHG,     // MUS control change larger than 9 
  TRACKOVF,      // MIDI track exceeds allocation 
  BADMIDHDR,     // bad midi header detected 
} error_code_t;

// some names for integers of various sizes, all unsigned 
typedef unsigned char UBYTE;  // a one-byte int 
typedef unsigned short UWORD; // a two-byte int 
// proff: changed from unsigned int to unsigned long to avoid warning
typedef unsigned long ULONG;   // a four-byte int (assumes int 4 bytes) 

#ifndef DJGPP // proff: This is from allegro.h
#define MIDI_TRACKS           32       

typedef struct MIDI                    /* a midi file */
{
   int divisions;                      /* number of ticks per quarter note */
   struct {
      unsigned char *data;             /* MIDI message stream */
      int len;                         /* length of the track data */
   } track[MIDI_TRACKS]; 
} MIDI;
#endif // DJGPP

int mmus2mid(UBYTE *mus,MIDI *mid, UWORD division, int nocomp);
int MIDIToMidi(MIDI *mididata,UBYTE **mid,int *midlen);
int MidiToMIDI(UBYTE *mid,MIDI *mididata);

#endif

//----------------------------------------------------------------------------
//
// $Log: mmus2mid.h,v $
// Revision 1.6  1998/05/10  23:00:48  jim
// formatted/documented mmus2mid
//
// Revision 1.5  1998/02/08  15:15:44  jim
// Added native midi support
//
// Revision 1.4  1998/01/26  19:27:17  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/21  16:56:22  jim
// Music fixed, defaults for cards added
//
// Revision 1.2  1998/01/19  23:40:33  rand
// Added Id: string at top of file
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
