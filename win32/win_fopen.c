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

FILE* D_fopen(const char *filename, const char *mode)
{
  wchar_t wpath[MAX_PATH];
  wchar_t wmode[MAX_PATH];
  int fn_len = strlen(filename);
  int m_len  = strlen(mode);
  int w_fn_len = 0;
  int w_m_len = 0;

  if (fn_len == 0) 
    return NULL;
  if (m_len == 0)
    return NULL;

  w_fn_len = MultiByteToWideChar(CP_UTF8, 0, filename, fn_len, wpath, fn_len);
  if (w_fn_len >= MAX_PATH)
    return NULL;
  wpath[w_fn_len] = L'\0';

  w_m_len = MultiByteToWideChar(CP_UTF8, 0, mode, m_len, wmode, m_len);
  if(w_m_len >= MAX_PATH)
    return NULL;
  wmode[w_m_len] = L'\0';

  return _wfopen(wpath, wmode);
}
#endif
