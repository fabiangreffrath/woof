// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: r_defs.h,v 1.18 1998/04/27 02:12:59 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2015-2020 Fabian Greffrath
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
//      support maps with NODES in compressed or uncompressed ZDBSP format or DeePBSP format
//
//-----------------------------------------------------------------------------


#ifndef __P_EXTNODES__
#define __P_EXTNODES__

typedef enum
{
    MFMT_DOOMBSP = 0x000,
    MFMT_DEEPBSP = 0x001,
    MFMT_ZDBSPX  = 0x002,
    MFMT_ZDBSPZ  = 0x004,
} mapformat_t;

extern mapformat_t P_CheckMapFormat (int lumpnum);

extern void P_LoadSegs_DeePBSP (int lump);
extern void P_LoadSubsectors_DeePBSP (int lump);
extern void P_LoadNodes_DeePBSP (int lump);
extern void P_LoadNodes_ZDBSP (int lump, boolean compressed);

#endif
