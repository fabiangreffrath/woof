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
//      Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------

#ifndef __P_SAVEG__
#define __P_SAVEG__

#include "doomtype.h"
#include "z_zone.h"

// Persistent storage/archiving.
// These are the load / save game routines.
void P_UnArchivePlayers(void);
void P_UnArchiveWorld(void);
void P_UnArchiveThinkers(void);
void P_UnArchiveSpecials(void);

// 1/18/98 killough: add RNG info to savegame
void P_UnArchiveRNG(void);

// 2/21/98 killough: add automap info to savegame
void P_UnArchiveMap(void);

extern byte *save_p, *savebuffer;
extern size_t savegamesize;

inline static boolean saveg_check_size(size_t size)
{
    size_t offset = save_p - savebuffer;
    if (offset + size <= savegamesize)
    {
        return true;
    }
    return false;
}

// Check for overrun and realloc if necessary -- Lee Killough 1/22/98
inline static void saveg_grow(size_t size)
{
    if (saveg_check_size(size))
    {
        return;
    }

    size_t offset = save_p - savebuffer;

    while (offset + size > savegamesize)
    {
        savegamesize *= 2;
    }

    savebuffer = Z_Realloc(savebuffer, savegamesize, PU_STATIC, NULL);
    save_p = savebuffer + offset;
}

// Endian-safe integer read/write functions

inline static void savep_putbyte(byte value)
{
    *save_p++ = value;
}

inline static void saveg_write8(byte value)
{
    saveg_grow(sizeof(byte));
    savep_putbyte(value);
}

inline static void saveg_write16(short value)
{
    saveg_grow(sizeof(int16_t));
    savep_putbyte(value & 0xff);
    savep_putbyte((value >> 8) & 0xff);
}

inline static void saveg_write32(int value)
{
    saveg_grow(sizeof(int32_t));
    savep_putbyte(value & 0xff);
    savep_putbyte((value >> 8) & 0xff);
    savep_putbyte((value >> 16) & 0xff);
    savep_putbyte((value >> 24) & 0xff);
}

inline static void saveg_write64(int64_t value)
{
    saveg_grow(sizeof(int64_t));
    savep_putbyte(value & 0xff);
    savep_putbyte((value >> 8) & 0xff);
    savep_putbyte((value >> 16) & 0xff);
    savep_putbyte((value >> 24) & 0xff);
    savep_putbyte((value >> 32) & 0xff);
    savep_putbyte((value >> 40) & 0xff);
    savep_putbyte((value >> 48) & 0xff);
    savep_putbyte((value >> 56) & 0xff);
}

inline static byte saveg_read8(void)
{
    return *save_p++;
}

inline static short saveg_read16(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;

    return result;
}

inline static int saveg_read32(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;
    result |= saveg_read8() << 16;
    result |= saveg_read8() << 24;

    return result;
}

inline static int64_t saveg_read64(void)
{
    int64_t result;

    result =  (int64_t)(saveg_read8());
    result |= (int64_t)(saveg_read8()) << 8;
    result |= (int64_t)(saveg_read8()) << 16;
    result |= (int64_t)(saveg_read8()) << 24;
    result |= (int64_t)(saveg_read8()) << 32;
    result |= (int64_t)(saveg_read8()) << 40;
    result |= (int64_t)(saveg_read8()) << 48;
    result |= (int64_t)(saveg_read8()) << 56;

    return result;
}

typedef enum
{
  saveg_indetermined = -1,
  saveg_mbf,
  saveg_woof510,
  saveg_woof600,
  saveg_woof1300,
  saveg_woof1500,
  saveg_current, // saveg_woof1600
} saveg_compat_t;

extern saveg_compat_t saveg_compat;

#endif

//----------------------------------------------------------------------------
//
// $Log: p_saveg.h,v $
// Revision 1.5  1998/05/03  23:10:40  killough
// beautification
//
// Revision 1.4  1998/02/23  04:50:09  killough
// Add automap marks and properties to saved state
//
// Revision 1.3  1998/02/17  06:26:04  killough
// Remove unnecessary plat archive/unarchive funcs
//
// Revision 1.2  1998/01/26  19:27:26  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
