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
//      Simple basic typedefs, isolated here to make it easier
//       separating modules.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#include <stddef.h> // size_t, NULL
#include <stdint.h> // [FG] intptr_t types

#include "config.h"

#if __bool_true_false_are_defined
typedef int boolean;
#else
typedef enum {false, true} boolean;
#endif

typedef uint8_t byte;

typedef byte pixel_t;

// This could be wider for >8 bit display. Indeed, true color support is
// posibble precalculating 24bpp lightmap/colormap LUT. from darkening PLAYPAL
// to all black. Could use even more than 32 levels.

typedef byte lighttable_t;

// [FG] common definitions from Chocolate Doom

// #define macros to provide functions missing in Windows.
// Outside Windows, we use strings.h for str[n]casecmp.

#if !HAVE_DECL_STRCASECMP || !HAVE_DECL_STRNCASECMP
  #include <string.h>
  #if !HAVE_DECL_STRCASECMP
    #define strcasecmp stricmp
  #endif
  #if !HAVE_DECL_STRNCASECMP
    #define strncasecmp strnicmp
  #endif
#else
  #include <strings.h>
#endif

#ifdef _WIN32
 #define DIR_SEPARATOR '\\'
 #define DIR_SEPARATOR_S "\\"
 #define PATH_SEPARATOR ';'
#else
 #define DIR_SEPARATOR '/'
 #define DIR_SEPARATOR_S "/"
 #define PATH_SEPARATOR ':'
#endif

#define arrlen(array) (sizeof(array) / sizeof(*array))

#ifndef MIN
 #define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
 #define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef BETWEEN
 #define BETWEEN(l,u,x) ((l)>(x)?(l):(x)>(u)?(u):(x))
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

#if defined(__GNUC__) || defined(__clang__)
 #define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
 #define PRINTF_ARG_ATTR(x) __attribute__((format_arg(x)))
#else
 #define PRINTF_ATTR(fmt, first)
 #define PRINTF_ARG_ATTR(x)
#endif

#if defined(__GNUC__) || defined(__clang__)
 #define NORETURN __attribute__((noreturn))
#elif defined (_MSC_VER)
 #define NORETURN __declspec(noreturn)
#else
 #define NORETURN
#endif

// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.

#if defined(__GNUC__)
 #define PACKED_PREFIX
 #if defined(_WIN32) && !defined(__clang__)
  #define PACKED_SUFFIX __attribute__((packed,gcc_struct))
 #else
  #define PACKED_SUFFIX __attribute__((packed))
 #endif
#elif defined(__WATCOMC__)
 #define PACKED_PREFIX _Packed
 #define PACKED_SUFFIX
#else
 #define PACKED_PREFIX
 #define PACKED_SUFFIX
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: doomtype.h,v $
// Revision 1.3  1998/05/03  23:24:33  killough
// beautification
//
// Revision 1.2  1998/01/26  19:26:43  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
