# Microsoft Developer Studio Project File - Name="WinMBF" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=WinMBF - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "WinMBF.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WinMBF.mak" CFG="WinMBF - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WinMBF - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "WinMBF - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "WinMBF - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "c:\software dev\sdl-1.2.7\include" /I "c:\software dev\sdl_mixer-1.2.5\include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MY_SDL_VER" /D "DOGS" /D "BETA" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 sdl_mixer.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib sdl.lib sdlmain.lib sdl_mixer.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "WinMBF - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "c:\software dev\sdl-1.2.12\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MY_SDL_VER" /D "DOGS" /D "BETA" /D "RANGECHECK" /D "INSTRUMENTED" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib sdl.lib sdlmain.lib sdl_mixer.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "WinMBF - Win32 Release"
# Name "WinMBF - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "AM"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Am_map.c
# End Source File
# End Group
# Begin Group "D"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\D_deh.c
# End Source File
# Begin Source File

SOURCE=.\Source\D_items.c
# End Source File
# Begin Source File

SOURCE=.\Source\D_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\D_net.c
# End Source File
# End Group
# Begin Group "F"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\F_finale.c
# End Source File
# Begin Source File

SOURCE=.\Source\F_wipe.c
# End Source File
# End Group
# Begin Group "G"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\G_game.c
# End Source File
# End Group
# Begin Group "HU"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Hu_lib.c
# End Source File
# Begin Source File

SOURCE=.\Source\Hu_stuff.c
# End Source File
# End Group
# Begin Group "I"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\I_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\I_net.c
# End Source File
# Begin Source File

SOURCE=.\Source\I_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\I_system.c
# End Source File
# Begin Source File

SOURCE=.\Source\I_video.c
# End Source File
# End Group
# Begin Group "M"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\M_argv.c
# End Source File
# Begin Source File

SOURCE=.\Source\M_bbox.c
# End Source File
# Begin Source File

SOURCE=.\Source\M_cheat.c
# End Source File
# Begin Source File

SOURCE=.\Source\M_menu.c
# End Source File
# Begin Source File

SOURCE=.\Source\M_misc.c
# End Source File
# Begin Source File

SOURCE=.\Source\M_random.c
# End Source File
# End Group
# Begin Group "P"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\P_ceilng.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_doors.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_enemy.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_floor.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_genlin.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_inter.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_lights.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_map.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_maputl.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_plats.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_saveg.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_setup.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_sight.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_spec.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_switch.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_telept.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_tick.c
# End Source File
# Begin Source File

SOURCE=.\Source\P_user.c
# End Source File
# End Group
# Begin Group "R"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\R_bsp.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_data.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_draw.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_plane.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_segs.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_sky.c
# End Source File
# Begin Source File

SOURCE=.\Source\R_things.c
# End Source File
# End Group
# Begin Group "S"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\S_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\Sounds.c
# End Source File
# End Group
# Begin Group "ST"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\St_lib.c
# End Source File
# Begin Source File

SOURCE=.\Source\St_stuff.c
# End Source File
# End Group
# Begin Group "V"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\V_video.c
# End Source File
# End Group
# Begin Group "W"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\W_wad.c
# End Source File
# End Group
# Begin Group "WI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Wi_stuff.c
# End Source File
# End Group
# Begin Group "Z"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Z_zone.c
# End Source File
# End Group
# Begin Group "Doom"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Doomdef.c
# End Source File
# Begin Source File

SOURCE=.\Source\Doomstat.c
# End Source File
# Begin Source File

SOURCE=.\Source\Dstrings.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Source\Info.c
# End Source File
# Begin Source File

SOURCE=.\Source\Mmus2mid.c
# End Source File
# Begin Source File

SOURCE=.\Source\Tables.c
# End Source File
# Begin Source File

SOURCE=.\Source\Version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "AM_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Am_map.h
# End Source File
# End Group
# Begin Group "D_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\D_deh.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_englsh.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_event.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_french.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_io.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_items.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_main.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_player.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_textur.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_think.h
# End Source File
# Begin Source File

SOURCE=.\Source\D_ticcmd.h
# End Source File
# End Group
# Begin Group "Doom_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Doomdata.h
# End Source File
# Begin Source File

SOURCE=.\Source\Doomdef.h
# End Source File
# Begin Source File

SOURCE=.\Source\Doomstat.h
# End Source File
# Begin Source File

SOURCE=.\Source\Doomtype.h
# End Source File
# Begin Source File

SOURCE=.\Source\Dstrings.h
# End Source File
# End Group
# Begin Group "F_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\F_finale.h
# End Source File
# Begin Source File

SOURCE=.\Source\F_wipe.h
# End Source File
# End Group
# Begin Group "G_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\G_game.h
# End Source File
# End Group
# Begin Group "HU_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Hu_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\Hu_stuff.h
# End Source File
# End Group
# Begin Group "I_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\I_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\I_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\I_system.h
# End Source File
# Begin Source File

SOURCE=.\Source\I_video.h
# End Source File
# End Group
# Begin Group "M_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\M_argv.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_bbox.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_cheat.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_fixed.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_menu.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_misc.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_random.h
# End Source File
# Begin Source File

SOURCE=.\Source\M_swap.h
# End Source File
# End Group
# Begin Group "P_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\P_enemy.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_inter.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_map.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_maputl.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_mobj.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_pspr.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_saveg.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_setup.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_spec.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_tick.h
# End Source File
# Begin Source File

SOURCE=.\Source\P_user.h
# End Source File
# End Group
# Begin Group "R_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\R_bsp.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_data.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_defs.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_draw.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_main.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_plane.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_segs.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_sky.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_state.h
# End Source File
# Begin Source File

SOURCE=.\Source\R_things.h
# End Source File
# End Group
# Begin Group "S_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\S_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\Sounds.h
# End Source File
# End Group
# Begin Group "ST_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\St_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\St_stuff.h
# End Source File
# End Group
# Begin Group "V_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\V_video.h
# End Source File
# End Group
# Begin Group "W_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\W_wad.h
# End Source File
# End Group
# Begin Group "WI_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Wi_stuff.h
# End Source File
# End Group
# Begin Group "Z_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\Z_zone.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Source\Info.h
# End Source File
# Begin Source File

SOURCE=.\Source\Mmus2mid.h
# End Source File
# Begin Source File

SOURCE=.\Source\Tables.h
# End Source File
# Begin Source File

SOURCE=.\Source\Version.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
