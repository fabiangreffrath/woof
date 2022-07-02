//
// Copyright(C) 2022 Roman Fomin
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
//
// DESCRIPTION:
//      Check Windows version
//

#include "win_version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

typedef long (__stdcall *PRTLGETVERSION)(PRTL_OSVERSIONINFOEXW);

int I_CheckWindows11(void)
{
    PRTLGETVERSION  pRtlGetVersion = (PRTLGETVERSION)GetProcAddress(
        GetModuleHandle("ntdll.dll"), "RtlGetVersion");

    if (pRtlGetVersion)
    {
        OSVERSIONINFOEXW info;

        memset(&info, 0, sizeof(OSVERSIONINFOEXW));
        info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

        pRtlGetVersion((PRTL_OSVERSIONINFOEXW)&info);

        if (info.dwPlatformId == VER_PLATFORM_WIN32_NT)
        {
            if (info.dwMajorVersion == 10)
            {
                if (info.dwBuildNumber >= 22000)
                    return 1;
            }
        }
    }

    return 0;
}
