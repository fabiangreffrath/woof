
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.

// haleyjd: IO crap for MSVC

#ifndef D_IO_INCLUDED
#define D_IO_INCLUDED

#ifndef TRUE
  #define TRUE true
#endif
#ifndef FALSE
  #define FALSE false
#endif

#ifdef _MSC_VER
  #include <direct.h>
  #include <io.h>
  #define F_OK 0
  #define W_OK 2
  #define R_OK 4
  #define S_ISDIR(x) (((sbuf.st_mode & S_IFDIR)==S_IFDIR)?1:0)
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #ifndef PATH_MAX
     #define PATH_MAX _MAX_PATH
  #endif
#elif defined (__unix__)
  #include <unistd.h>
  #include <ctype.h> // tolower()
  #ifndef O_BINARY
    #define O_BINARY 0
  #endif
  #define stricmp strcasecmp
  #define strnicmp strncasecmp
  static inline char *strlwr (char *str) {
    char *c = str;
    while (*str != '\0')
    {
        *str = tolower (*str);
        str++;
    }
    return c;
  }
#else
#include <unistd.h>
#endif

#endif

// EOF


