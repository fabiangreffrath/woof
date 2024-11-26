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

#include <SDL3/SDL_main.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(dllexport) extern int Woof_Main(int argc, char **argv);
__declspec(dllexport) extern void Woof_Exit(void);

BOOL CtrlHandler(DWORD event)
{
    if (event == CTRL_CLOSE_EVENT)
    {
        Woof_Exit();
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)(CtrlHandler), TRUE);

    return Woof_Main(argc, argv);
}
