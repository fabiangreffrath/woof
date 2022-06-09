include(WoofSettings)

set(WOOF_SOURCES
    am_map.c   am_map.h
    beta.h
    d_deh.c    d_deh.h
    d_englsh.h
    d_event.h
    d_french.h
    d_io.h
    d_items.c  d_items.h
    d_iwad.c   d_iwad.h
    d_main.c   d_main.h
    d_loop.c   d_loop.h
    d_net.c
    d_player.h
    d_textur.h
    d_think.h
    d_ticcmd.h
    d_quit.c   d_quit.h
    dogs.h
    doomdata.h
    doomdef.c  doomdef.h
    doomstat.c doomstat.h
    doomtype.h
    dsdhacked.c dsdhacked.h
    dstrings.c dstrings.h
    f_finale.c f_finale.h
    f_wipe.c   f_wipe.h
    g_game.c   g_game.h
    hu_lib.c   hu_lib.h
    hu_stuff.c hu_stuff.h
    i_endoom.c i_endoom.h
    i_flmusic.c
    i_glob.c   i_glob.h
    i_main.c
    i_oplmusic.c
    i_sdlmusic.c
    i_winmusic.c
    i_sound.c  i_sound.h
    i_system.c i_system.h
    i_timer.c  i_timer.h
    i_video.c  i_video.h
    info.c     info.h
    m_argv.c   m_argv.h
    m_bbox.c   m_bbox.h
    m_cheat.c  m_cheat.h
    m_input.c  m_input.h
    m_fixed.h
    m_menu.c   m_menu.h
    m_misc.c   m_misc.h
    m_misc2.c  m_misc2.h
    m_random.c m_random.h
    m_swap.h
    memio.c    memio.h
    midifile.c midifile.h
    mus2mid.c  mus2mid.h
    net_client.c net_client.h
    net_common.c net_common.h
    net_dedicated.c net_dedicated.h
    net_defs.h
    net_gui.c  net_gui.h
    net_io.c   net_io.h
    net_loop.c net_loop.h
    net_packet.c net_packet.h
    net_petname.c net_petname.h
    net_query.c net_query.h
    net_sdl.c  net_sdl.h
    net_server.c net_server.h
    net_structrw.c net_structrw.h
    p_ceilng.c
    p_doors.c
    p_enemy.c  p_enemy.h
    p_extnodes.c p_extnodes.h
    p_floor.c
    p_genlin.c
    p_inter.c  p_inter.h
    p_lights.c
    p_map.c    p_map.h
    p_maputl.c p_maputl.h
    p_mobj.c   p_mobj.h
    p_plats.c
    p_pspr.c   p_pspr.h
    p_saveg.c  p_saveg.h
    p_setup.c  p_setup.h
    p_sight.c
    p_spec.c   p_spec.h
    p_switch.c
    p_telept.c
    p_tick.c   p_tick.h
    p_user.c   p_user.h
    r_bmaps.c  r_bmaps.h
    r_bsp.c    r_bsp.h
    r_data.c   r_data.h
    r_defs.h
    r_draw.c   r_draw.h
    r_main.c   r_main.h
    r_plane.c  r_plane.h
    r_segs.c   r_segs.h
    r_sky.c    r_sky.h
    r_state.h
    r_swirl.c  r_swirl.h
    r_things.c r_things.h
    s_sound.c  s_sound.h
    s_musinfo.c s_musinfo.h
    sounds.c   sounds.h
    statdump.c statdump.h
    st_lib.c   st_lib.h
    st_stuff.c st_stuff.h
    tables.c   tables.h
    u_mapinfo.c u_mapinfo.h
    u_scanner.c u_scanner.h
    v_video.c  v_video.h
    version.c  version.h
    w_wad.c    w_wad.h
    wi_stuff.c wi_stuff.h
    z_zone.c   z_zone.h
    ../miniz/miniz.c ../miniz/miniz.h)

list(APPEND WOOF_LIBRARIES textscreen)

if(WIN32)
    list(APPEND
        WOOF_SOURCES
        ../win32/win_fopen.c   ../win32/win_fopen.h
        ../win32/win_version.c ../win32/win_version.h)
    if(MSVC)
        list(APPEND
            WOOF_SOURCES
            ../win32/win_opendir.c ../win32/win_opendir.h
            ../win32/stdint.h)
    endif()
endif()

# Some platforms require standard libraries to be linked against.
if(m_FOUND)
    list(APPEND WOOF_LIBRARIES m)
endif()

if (WIN32)
    list(APPEND WOOF_LIBRARIES winmm)
endif()

# Some platforms have SDL2main, others don't.
if(SDL2_MAIN_LIBRARY)
    list(APPEND WOOF_LIBRARIES SDL2::SDL2main)
endif()

if(WIN32)
    # Stamp out and compile resource file on Windows.
    configure_file(resource.rc.in "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
    list(APPEND WOOF_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
endif()

# Standard target definition
add_executable(woof WIN32 ${WOOF_SOURCES})
target_woof_settings(woof)
target_include_directories(woof PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")

if (FluidSynth_FOUND)
    list(APPEND WOOF_LIBRARIES FluidSynth::FluidSynth)
    target_compile_definitions(woof PRIVATE HAVE_FLUIDSYNTH)
endif()

target_link_libraries(woof PRIVATE ${WOOF_LIBRARIES}
    SDL2::SDL2 SDL2::mixer SDL2::net opl)

if(MSVC)
    # MSVC tries to supply a default manifest and complains when it finds ours
    # unless we specifically tell it not to.
    set_target_properties(woof PROPERTIES LINK_FLAGS "/MANIFEST:NO")
    # Enable console for debug build.
    set_target_properties(woof PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
endif()

# Optional features.
#
# Our defines are not namespaced, so we pass them at compile-time instead of
# using config.h.
if("${${PROJECT_NAME}_INSTRUMENTED}")
    target_compile_definitions(woof PRIVATE INSTRUMENTED)
endif()
if("${${PROJECT_NAME}_RANGECHECK}")
    target_compile_definitions(woof PRIVATE RANGECHECK)
endif()
if("${${PROJECT_NAME}_STRICT}")
    target_compile_definitions(woof PRIVATE MBF_STRICT)
endif()

target_compile_definitions(woof PRIVATE MINIZ_NO_STDIO)
target_compile_definitions(woof PRIVATE MINIZ_NO_TIME)
target_compile_definitions(woof PRIVATE MINIZ_NO_ARCHIVE_APIS)

set(SETUP_SOURCES
    i_main.c
    i_system.c              i_system.h
    i_timer.c               i_timer.h
    m_argv.c                m_argv.h
    m_misc2.c               m_misc2.h
    d_iwad.c                d_iwad.h
    net_io.c                net_io.h
    net_packet.c            net_packet.h
    net_sdl.c               net_sdl.h
    net_query.c             net_query.h
    net_structrw.c          net_structrw.h
    z_zone.c                z_zone.h
    ../win32/win_fopen.c    ../win32/win_fopen.h)

if(WIN32)
    # Stamp out and compile resource file on Windows.
    configure_file(setup-res.rc.in "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
    list(APPEND SETUP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
endif()

add_executable(woof-setup WIN32 ${SETUP_SOURCES})
target_woof_settings(woof-setup)

# Some platforms have SDL2main, others don't.
if(SDL2_MAIN_LIBRARY)
    set(SETUP_LIBRARIES SDL2::SDL2main)
endif()

target_include_directories(woof-setup PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries(woof-setup PRIVATE ${SETUP_LIBRARIES}
    SDL2::SDL2 SDL2::net textscreen setup)

if(MSVC)
    set_target_properties(woof-setup PROPERTIES LINK_FLAGS "/MANIFEST:NO")
    set_target_properties(woof-setup PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
endif()

# Install

# Assemble library files.
set(WOOF_DLLS "")
if(WIN32 AND (MSVC OR CMAKE_CROSSCOMPILING))
    # SDL2
    list(APPEND WOOF_DLLS "${SDL2_DLL_DIR}/SDL2.dll")

    # SDL2_mixer
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libFLAC-8.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libmodplug-1.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libmpg123-0.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libogg-0.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libopus-0.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libopusfile-0.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libvorbis-0.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/libvorbisfile-3.dll")
    list(APPEND WOOF_DLLS "${SDL2_MIXER_DLL_DIR}/SDL2_mixer.dll")

    # SDL2_net
    list(APPEND WOOF_DLLS "${SDL2_NET_DLL_DIR}/SDL2_net.dll")
endif()

# Copy library files to target directory.
foreach(SDL2_DLL ${WOOF_DLLS})
    add_custom_command(TARGET woof POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${SDL2_DLL}" $<TARGET_FILE_DIR:woof> VERBATIM)
endforeach()

# Files to package in our distribution.
if(WIN32)
    install(TARGETS woof woof-setup RUNTIME DESTINATION .)
else()
    install(TARGETS woof woof-setup RUNTIME DESTINATION bin)
endif()
install(FILES ${WOOF_DLLS} DESTINATION .)

# Copy autoload directory content to binary directory.
add_custom_command(TARGET woof POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
    "${CMAKE_CURRENT_BINARY_DIR}/autoload"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../autoload"
    "${CMAKE_CURRENT_BINARY_DIR}/autoload" VERBATIM)

# Try get dependencies in MSYS2 environment.
if(WIN32 AND NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
    find_program(DLLTOOL_EXECUTABLE
        NAMES dlltool dlltool.exe
    )

    find_library(MODPLUG libmodplug)

    if(MODPLUG AND DLLTOOL_EXECUTABLE)
        execute_process(COMMAND
            "${DLLTOOL_EXECUTABLE}" -I "${MODPLUG}"
            OUTPUT_VARIABLE MODPLUG_DLL_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        find_program(LIBMODPLUG_DLL NAMES ${MODPLUG_DLL_NAME})
        install(CODE "set(LIBMODPLUG_DLL \"${LIBMODPLUG_DLL}\")")
    else()
        message(WARNING "Can't find libmodplug")
    endif()

    if(FluidSynth_FOUND AND DLLTOOL_EXECUTABLE)
        execute_process(COMMAND
            "${DLLTOOL_EXECUTABLE}" -I "${FluidSynth_LIBRARIES}"
            OUTPUT_VARIABLE FLUIDSYNTH_DLL_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        find_program(FLUIDSYNTH_DLL NAMES ${FLUIDSYNTH_DLL_NAME})
        install(CODE "set(FLUIDSYNTH_DLL \"${FLUIDSYNTH_DLL}\")")
    else()
        message(WARNING "Can't find libfluidsynth")
    endif()

    install(CODE "set(MINGW_PATH \"${SDL2_DLL_DIR}\")")

    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "$<TARGET_FILE_DIR:woof>/$<TARGET_FILE_NAME:woof>"
            LIBRARIES    ${LIBMODPLUG_DLL} ${FLUIDSYNTH_DLL}
            RESOLVED_DEPENDENCIES_VAR R_DEPS
            UNRESOLVED_DEPENDENCIES_VAR U_DEPS
            DIRECTORIES ${MINGW_PATH}
            PRE_EXCLUDE_REGEXES "api-ms-*" "ext-ms-*"
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )

        file(INSTALL
            DESTINATION ${CMAKE_INSTALL_PREFIX}
            FOLLOW_SYMLINK_CHAIN
            FILES ${R_DEPS} ${LIBMODPLUG_DLL} ${FLUIDSYNTH_DLL}
        )

        list(LENGTH U_DEPS U_LENGTH)
        if("${U_LENGTH}" GREATER 0)
            message(WARNING "Unresolved dependencies: ${U_DEPS}")
        endif()
    ]])
endif()
