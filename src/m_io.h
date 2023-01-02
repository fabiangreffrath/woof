//  Copyright (C) 2004 James Haley
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
//

#ifndef M_IO_INCLUDED
#define M_IO_INCLUDED

#ifdef _MSC_VER
  #include <direct.h>
  #include <io.h>
  #define F_OK 0
  #define W_OK 2
  #define R_OK 4
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#elif !defined (_WIN32)
  #include <unistd.h>
  #ifndef O_BINARY
    #define O_BINARY 0
  #endif
#else
  #include <unistd.h>
#endif

#endif // M_IO_INCLUDED
