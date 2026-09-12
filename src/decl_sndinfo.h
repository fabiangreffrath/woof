//
// Copyright(C) 2026, Fabian Greffrath
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifndef DECL_SNDINFO_H
#define DECL_SNDINFO_H

// Signature matches W_ProcessInWads()'s process callback, just like
// DECL_Parse() in decl_main.h.
void SNDINFO_Parse(int lumpnum);

#endif
