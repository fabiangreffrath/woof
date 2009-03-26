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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "c:\software dev\sdl-1.2.12\include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MY_SDL_VER" /D "DOGS" /D "BETA" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib sdl.lib sdlmain.lib sdl_mixer.lib sdl_net.lib /nologo /subsystem:console /machine:I386

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
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib sdl.lib sdlmain.lib sdl_mixer.lib sdl_net.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "WinMBF - Win32 Release"
# Name "WinMBF - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "AM"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\am_map.c
# End Source File
# End Group
# Begin Group "D"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\d_deh.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_items.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\d_net.c
# End Source File
# End Group
# Begin Group "F"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\f_finale.c
# End Source File
# Begin Source File

SOURCE=.\Source\f_wipe.c
# End Source File
# End Group
# Begin Group "G"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\g_game.c
# End Source File
# End Group
# Begin Group "HU"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\hu_lib.c
# End Source File
# Begin Source File

SOURCE=.\Source\hu_stuff.c
# End Source File
# End Group
# Begin Group "I"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\i_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\i_net.c
# End Source File
# Begin Source File

SOURCE=.\Source\i_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\i_system.c
# End Source File
# Begin Source File

SOURCE=.\Source\i_video.c
# End Source File
# End Group
# Begin Group "M"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\m_argv.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_bbox.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_cheat.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_menu.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_misc.c
# End Source File
# Begin Source File

SOURCE=.\Source\m_random.c
# End Source File
# End Group
# Begin Group "P"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\p_ceilng.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_doors.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_enemy.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_floor.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_genlin.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_inter.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_lights.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_map.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_maputl.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_plats.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_saveg.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_setup.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_sight.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_spec.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_switch.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_telept.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_tick.c
# End Source File
# Begin Source File

SOURCE=.\Source\p_user.c
# End Source File
# End Group
# Begin Group "R"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\r_bsp.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_data.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_draw.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_main.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_plane.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_segs.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_sky.c
# End Source File
# Begin Source File

SOURCE=.\Source\r_things.c
# End Source File
# End Group
# Begin Group "S"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\s_sound.c
# End Source File
# Begin Source File

SOURCE=.\Source\sounds.c
# End Source File
# End Group
# Begin Group "ST"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\st_lib.c
# End Source File
# Begin Source File

SOURCE=.\Source\st_stuff.c
# End Source File
# End Group
# Begin Group "V"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\v_video.c
# End Source File
# End Group
# Begin Group "W"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\w_wad.c
# End Source File
# End Group
# Begin Group "WI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\wi_stuff.c
# End Source File
# End Group
# Begin Group "Z"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\z_zone.c
# End Source File
# End Group
# Begin Group "Doom"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\doomdef.c
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.c
# End Source File
# Begin Source File

SOURCE=.\Source\dstrings.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Source\info.c
# End Source File
# Begin Source File

SOURCE=.\Source\mmus2mid.c
# End Source File
# Begin Source File

SOURCE=.\Source\tables.c
# End Source File
# Begin Source File

SOURCE=.\Source\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "AM_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\am_map.h
# End Source File
# End Group
# Begin Group "D_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\d_deh.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_englsh.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_event.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_french.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_io.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_items.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_main.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_player.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_textur.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_think.h
# End Source File
# Begin Source File

SOURCE=.\Source\d_ticcmd.h
# End Source File
# End Group
# Begin Group "Doom_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\doomdata.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomdef.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomstat.h
# End Source File
# Begin Source File

SOURCE=.\Source\doomtype.h
# End Source File
# Begin Source File

SOURCE=.\Source\dstrings.h
# End Source File
# End Group
# Begin Group "F_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\Source\f_wipe.h
# End Source File
# End Group
# Begin Group "G_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\g_game.h
# End Source File
# End Group
# Begin Group "HU_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\hu_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\hu_stuff.h
# End Source File
# End Group
# Begin Group "I_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\i_net.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_system.h
# End Source File
# Begin Source File

SOURCE=.\Source\i_video.h
# End Source File
# End Group
# Begin Group "M_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\m_argv.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_bbox.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_cheat.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_fixed.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_menu.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_misc.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_random.h
# End Source File
# Begin Source File

SOURCE=.\Source\m_swap.h
# End Source File
# End Group
# Begin Group "P_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\p_enemy.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_inter.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_map.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_maputl.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_mobj.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_pspr.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_setup.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_spec.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_tick.h
# End Source File
# Begin Source File

SOURCE=.\Source\p_user.h
# End Source File
# End Group
# Begin Group "R_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\r_bsp.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_data.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_defs.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_draw.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_main.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_plane.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_segs.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_sky.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_state.h
# End Source File
# Begin Source File

SOURCE=.\Source\r_things.h
# End Source File
# End Group
# Begin Group "S_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\s_sound.h
# End Source File
# Begin Source File

SOURCE=.\Source\sounds.h
# End Source File
# End Group
# Begin Group "ST_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\st_lib.h
# End Source File
# Begin Source File

SOURCE=.\Source\st_stuff.h
# End Source File
# End Group
# Begin Group "V_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\v_video.h
# End Source File
# End Group
# Begin Group "W_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\w_wad.h
# End Source File
# End Group
# Begin Group "WI_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\wi_stuff.h
# End Source File
# End Group
# Begin Group "Z_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Source\z_zone.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Source\info.h
# End Source File
# Begin Source File

SOURCE=.\Source\mmus2mid.h
# End Source File
# Begin Source File

SOURCE=.\Source\tables.h
# End Source File
# Begin Source File

SOURCE=.\Source\version.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
