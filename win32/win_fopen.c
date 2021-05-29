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
//      unicode paths for fopen() on Windows

#include "win_fopen.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

wchar_t* ConvertToUtf8(const char *str)
{
  wchar_t *wstr = NULL;
  int wlen = 0;

  wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

  wstr = (wchar_t *) malloc(sizeof(wchar_t) * wlen);

  MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wlen);

  return wstr;
}

FILE* D_fopen(const char *filename, const char *mode)
{
  FILE *f;
  wchar_t *wname = NULL;
  wchar_t *wmode = NULL;

  wname = ConvertToUtf8(filename);
  wmode = ConvertToUtf8(mode);

  f = _wfopen(wname, wmode);

  if (wname) free(wname);
  if (wmode) free(wmode);
  return f;
}

int D_remove(const char *path)
{
  wchar_t *wpath = NULL;
  int ret;

  wpath = ConvertToUtf8(path);

  ret = _wremove(wpath);

  if (wpath) free(wpath);
  return ret;
}
#endif
