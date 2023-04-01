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
//    Typedefs related to to textures etc.,
//    isolated here to make it easier separating modules.
//    
//-----------------------------------------------------------------------------


#ifndef __D_TEXTUR__
#define __D_TEXTUR__

#include "doomtype.h"


// NOTE: Checking all BOOM sources, there is nothing used called pic_t.

//
// Flats?
//
// a pic is an unmasked block of pixels
typedef struct
{
  byte  width;
  byte  height;
  byte  data;
} pic_t;


#endif

//----------------------------------------------------------------------------
//
// $Log: d_textur.h,v $
// Revision 1.3  1998/05/04  21:34:18  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:33  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
