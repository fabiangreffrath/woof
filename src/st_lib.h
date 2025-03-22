// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: st_lib.h,v 1.5 1998/05/11 10:44:46 jim Exp $
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
//  The status bar widget definitions and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __STLIB__
#define __STLIB__

// We are referring to patches.
#include "r_defs.h"
#include "v_video.h"  // color ranges

//
// Background and foreground screen numbers
//
#define BG 4
#define FG 0

//
// Typedefs of widgets
//

// Number widget

typedef struct
{
  // upper right-hand corner
  //  of the number (right-justified)
  int   x;
  int   y;

  // max # of digits in number
  int width;    

  // last number value
  int   oldnum;
  
  // pointer to current value
  int*  num;

  // pointer to boolean stating
  //  whether to update number
  boolean*  on;

  // list of patches for 0-9
  patch_t** p;

  // user data
  int data;
} st_number_t;

// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
typedef struct
{
  // number information
  st_number_t   n;

  // percent sign graphic
  patch_t*    p;
} st_percent_t;

// Multiple Icon widget
typedef struct
{
  // center-justified location of icons
  int     x;
  int     y;

  // last icon number
  int     oldinum;

  // pointer to current icon
  int*    inum;

  // pointer to boolean stating
  //  whether to update icon
  boolean*    on;

  // list of icons
  patch_t**   p;
  
  // user data
  int     data;
  
} st_multicon_t;

// Binary Icon widget

typedef struct
{
  // center-justified location of icon
  int     x;
  int     y;

  // last icon value
  int     oldval;

  // pointer to current icon status
  boolean*    val;

  // pointer to boolean
  //  stating whether to update icon
  boolean*    on;  

  patch_t*    p;  // icon
  int     data;   // user data
} st_binicon_t;

//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
//
void STlib_init(void);

// Number widget routines
void STlib_initNum
( st_number_t* n,
  int x,
  int y,
  patch_t** pl,
  int* num,
  boolean* on,
  int width );

void STlib_updateNum
( st_number_t* n,
  char *outrng,           //jff 1/16/98 add color translation to digit output
  boolean refresh );


// Percent widget routines
void STlib_initPercent
( st_percent_t* p,
  int x,
  int y,
  patch_t** pl,
  int* num,
  boolean* on,
  patch_t* percent );


void STlib_updatePercent
( st_percent_t* per,
  char *outrng,          //jff 1/16/98 add color translation to percent output
  int refresh );


// Multiple Icon widget routines
void STlib_initMultIcon
( st_multicon_t* mi,
  int x,
  int y,
  patch_t**   il,
  int* inum,
  boolean* on );


void STlib_updateMultIcon
( st_multicon_t* mi,
  boolean refresh );

// Binary Icon widget routines

void STlib_initBinIcon
( st_binicon_t* b,
  int x,
  int y,
  patch_t* i,
  boolean* val,
  boolean* on );

void STlib_updateBinIcon
( st_binicon_t* bi,
  boolean refresh );

#endif


//----------------------------------------------------------------------------
//
// $Log: st_lib.h,v $
// Revision 1.5  1998/05/11  10:44:46  jim
// formatted/documented st_lib
//
// Revision 1.4  1998/02/19  16:55:12  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/18  00:59:16  jim
// Addition of HUD
//
// Revision 1.2  1998/01/26  19:27:55  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

