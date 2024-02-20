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
//   Globally defined strings.
// 
//-----------------------------------------------------------------------------

#include "dstrings.h"
#include "d_deh.h"

char* s_QUITMSGD = "THIS IS NO MESSAGE!\nPage intentionally left blank.";

// killough 1/18/98: remove hardcoded limit, add const:
char **endmsg[]=
{
  // DOOM1
  &s_QUITMSG,

  &s_QUITMSG1,  // "please don't leave, there's more\ndemons to toast!",
  &s_QUITMSG2,  // "let's beat it -- this is turning\ninto a bloodbath!",
  &s_QUITMSG3,  // "i wouldn't leave if i were you.\ndos is much worse.",
  &s_QUITMSG4,  // "you're trying to say you like dos\nbetter than me, right?",
  &s_QUITMSG5,  // "don't leave yet -- there's a\ndemon around that corner!",
  &s_QUITMSG6,  // "ya know, next time you come in here\ni'm gonna toast ya.",
  &s_QUITMSG7,  // "go ahead and leave. see if i care.",  // 1/15/98 killough

  // QuitDOOM II messages
  &s_QUITMSG8,  // "you want to quit?\nthen, thou hast lost an eighth!",
  &s_QUITMSG9,  // "don't go now, there's a \ndimensional shambler waiting\nat the dos prompt!",
  &s_QUITMSG10, // "get outta here and go back\nto your boring programs.",
  &s_QUITMSG11, // "if i were your boss, i'd \n deathmatch ya in a minute!",
  &s_QUITMSG12, // "look, bud. you leave now\nand you forfeit your body count!",
  &s_QUITMSG13, // "just leave. when you come\nback, i'll be waiting with a bat.",
  &s_QUITMSG14, // "you're lucky i don't smack\nyou for thinking about leaving.",  // 1/15/98 killough

  // FinalDOOM?

// Note that these ending "bad taste" strings were commented out
// in the original id code as the #else case of an #if 1
// Obviously they were internal playthings before the release of
// DOOM2 and were not intended for public use.
//
// Following messages commented out for now. Bad taste.   // phares

//  "fuck you, pussy!\nget the fuck out!",
//  "you quit and i'll jizz\nin your cystholes!",
//  "if you leave, i'll make\nthe lord drink my jizz.",
//  "hey, ron! can we say\n'fuck' in the game?",
//  "i'd leave: this is just\nmore monsters and levels.\nwhat a load.",
//  "suck it down, asshole!\nyou're a fucking wimp!",
//  "don't quit now! we're \nstill spending your money!",

  // Internal debug. Different style, too.
  &s_QUITMSGD, // "THIS IS NO MESSAGE!\nPage intentionally left blank.",  // 1/15/98 killough
};

// killough 1/18/98: remove hardcoded limit and replace with var (silly hack):
const int NUM_QUITMESSAGES = sizeof(endmsg)/sizeof(*endmsg) - 1;

  
//----------------------------------------------------------------------------
//
// $Log: dstrings.c,v $
// Revision 1.5  1998/05/04  21:34:24  thldrmn
// commenting and reformatting
//
// Revision 1.3  1998/01/27  21:11:17  phares
// Commented out last section of end msgs.
//
// Revision 1.2  1998/01/26  19:23:13  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
