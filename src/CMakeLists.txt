include(WoofSettings)

set(WOOF_SOURCES
    am_map.c               am_map.h
                           beta.h
                           cross.h
    d_deh.c                d_deh.h
                           d_englsh.h
                           d_event.h
                           d_french.h
    d_items.c              d_items.h
    d_iwad.c               d_iwad.h
    d_loop.c               d_loop.h
    d_main.c               d_main.h
    d_net.c
                           d_player.h
    d_quit.c               d_quit.h
                           d_think.h
                           d_ticcmd.h
                           dogs.h
                           doomdata.h
                           doomdef.h
    doomstat.c             doomstat.h
                           doomtype.h
    dsdhacked.c            dsdhacked.h
    dstrings.c             dstrings.h
    f_finale.c             f_finale.h
    f_wipe.c               f_wipe.h
                           font.h
    g_game.c               g_game.h
    hu_lib.c               hu_lib.h
    hu_obituary.c          hu_obituary.h
    hu_stuff.c             hu_stuff.h
    i_3dsound.c
    i_endoom.c             i_endoom.h
    i_gamepad.c            i_gamepad.h
    i_glob.c               i_glob.h
    i_input.c              i_input.h
    i_main.c
    i_mbfsound.c
    i_midimusic.c
    i_oalmusic.c
    i_oalsound.c           i_oalsound.h
                           i_oalstream.h
    i_oplmusic.c
    i_printf.c             i_printf.h
    i_sndfile.c            i_sndfile.h
    i_sound.c              i_sound.h
    i_system.c             i_system.h
    i_timer.c              i_timer.h
    i_video.c              i_video.h
    info.c                 info.h
    m_argv.c               m_argv.h
                           m_array.h
    m_bbox.c               m_bbox.h
    m_cheat.c              m_cheat.h
    m_config.c             m_config.h
                           m_fixed.h
    m_input.c              m_input.h
    m_io.c                 m_io.h
    mn_font.c              mn_font.h
    mn_menu.c              mn_menu.h
    mn_setup.c             mn_setup.h
    m_misc.c               m_misc.h
    m_random.c             m_random.h
    mn_snapshot.c          mn_snapshot.h
                           m_swap.h
    memio.c                memio.h
    midifallback.c         midifallback.h
    midifile.c             midifile.h
    mus2mid.c              mus2mid.h
    nano_bsp.c             nano_bsp.h
    net_client.c           net_client.h
    net_common.c           net_common.h
    net_dedicated.c        net_dedicated.h
                           net_defs.h
    net_gui.c              net_gui.h
    net_io.c               net_io.h
    net_loop.c             net_loop.h
    net_packet.c           net_packet.h
    net_query.c            net_query.h
    net_sdl.c              net_sdl.h
    net_server.c           net_server.h
    net_structrw.c         net_structrw.h
                           p_action.h
    p_ceilng.c
    p_doors.c
    p_enemy.c              p_enemy.h
    p_extnodes.c           p_extnodes.h
    p_floor.c
    p_genlin.c
    p_inter.c              p_inter.h
    p_lights.c
    p_map.c                p_map.h
    p_maputl.c             p_maputl.h
    p_mobj.c               p_mobj.h
    p_plats.c
    p_pspr.c               p_pspr.h
    p_saveg.c              p_saveg.h
    p_setup.c              p_setup.h
    p_sight.c
    p_spec.c               p_spec.h
    p_switch.c
    p_telept.c
    p_tick.c               p_tick.h
    p_user.c               p_user.h
    r_bmaps.c              r_bmaps.h
    r_bsp.c                r_bsp.h
    r_data.c               r_data.h
                           r_defs.h
    r_draw.c               r_draw.h
    r_main.c               r_main.h
    r_plane.c              r_plane.h
    r_segs.c               r_segs.h
    r_sky.c                r_sky.h
                           r_state.h
    r_swirl.c              r_swirl.h
    r_things.c             r_things.h
    r_voxel.c              r_voxel.h
    s_musinfo.c            s_musinfo.h
    s_sound.c              s_sound.h
    sounds.c               sounds.h
    st_lib.c               st_lib.h
    st_stuff.c             st_stuff.h
    statdump.c             statdump.h
    tables.c               tables.h
    u_mapinfo.c            u_mapinfo.h
    u_scanner.c            u_scanner.h
    v_flextran.c           v_flextran.h
    v_trans.c              v_trans.h
    v_video.c              v_video.h
    version.c              version.h
    w_wad.c                w_wad.h
    wi_stuff.c             wi_stuff.h
    z_zone.c               z_zone.h
    ../miniz/miniz.c       ../miniz/miniz.h)

# Standard target definition
if(WIN32)
    add_library(woof SHARED EXCLUDE_FROM_ALL ${WOOF_SOURCES})
    target_compile_definitions(woof PRIVATE WIN_LAUNCHER)

    # Stamp out and compile resource file on Windows.
    configure_file(resource.rc.in "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")

    add_executable(woof-com WIN32 ../win32/win_launcher.c
        "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
    target_woof_settings(woof-com)
    target_link_libraries(woof-com PRIVATE ${SDL2_LIBRARIES} woof)

    add_executable(woof-exe WIN32 ../win32/win_launcher.c
        "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
    target_woof_settings(woof-exe)
    target_link_libraries(woof-exe PRIVATE ${SDL2_LIBRARIES} woof)

    # MSVC tries to supply a default manifest and complains when it finds ours
    # unless we specifically tell it not to.
    if(MSVC)
        target_link_options(woof-com PRIVATE "/MANIFEST:NO" "/SUBSYSTEM:CONSOLE")
        target_link_options(woof-exe PRIVATE "/MANIFEST:NO")
    else()
        target_link_options(woof-com PRIVATE "-mconsole")
    endif()
else()
    add_executable(woof ${WOOF_SOURCES})
endif()

target_woof_settings(woof)
target_include_directories(woof PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/../" "../miniz")

target_link_libraries(woof PRIVATE
    ${SDL2_LIBRARIES}
    SDL2_net::SDL2_net
    OpenAL::OpenAL
    SndFile::sndfile
    PortMidi::portmidi
    opl
    textscreen)

# Some platforms require standard libraries to be linked against.
if(HAVE_LIBM)
    target_link_libraries(woof PRIVATE m)
endif()

if(HAVE_AL_BUFFER_CALLBACK)
    target_sources(woof PRIVATE i_pcsound.c)
endif()

if(WIN32)
    target_include_directories(woof PRIVATE "../win32")
    if(MSVC)
        target_sources(woof PRIVATE
            ../win32/win_opendir.c ../win32/win_opendir.h)
    endif()
endif()

if(FluidSynth_FOUND)
    target_sources(woof PRIVATE i_flmusic.c)
    target_link_libraries(woof PRIVATE FluidSynth::libfluidsynth)
endif()

if(libxmp_FOUND)
    target_sources(woof PRIVATE i_xmp.c)
    target_link_libraries(woof PRIVATE libxmp::xmp)
endif()

# Optional features.
#
# Our defines are not namespaced, so we pass them at compile-time instead of
# using config.h.
if(WOOF_RANGECHECK)
    target_compile_definitions(woof PRIVATE RANGECHECK)
endif()
if(WOOF_STRICT)
    target_compile_definitions(woof PRIVATE MBF_STRICT)
endif()

target_compile_definitions(woof PRIVATE MINIZ_NO_TIME)

set(SETUP_SOURCES
    d_iwad.c                d_iwad.h
    i_main.c
    i_printf.c              i_printf.h
    i_system.c              i_system.h
    i_timer.c               i_timer.h
    m_argv.c                m_argv.h
                            m_array.h
    m_io.c                  m_io.h
    m_misc.c                m_misc.h
    net_io.c                net_io.h
    net_packet.c            net_packet.h
    net_query.c             net_query.h
    net_sdl.c               net_sdl.h
    net_structrw.c          net_structrw.h
    version.c               version.h
    z_zone.c                z_zone.h)

if(WIN32)
    # Stamp out and compile resource file on Windows.
    configure_file(setup-res.rc.in "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
    list(APPEND SETUP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
endif()

add_executable(woof-setup WIN32 ${SETUP_SOURCES})
target_woof_settings(woof-setup)

target_include_directories(woof-setup PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries(woof-setup PRIVATE
    ${SDL2_LIBRARIES}
    SDL2_net::SDL2_net
    textscreen
    setup)

if(MSVC)
    target_link_options(woof-setup PRIVATE "/MANIFEST:NO")
endif()

# Install
#
# Rename woof-com and woof-exe executables.
if(WIN32)
    add_custom_command(TARGET woof-com POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE_DIR:woof-com>/$<TARGET_FILE_NAME:woof-com>"
        "$<TARGET_FILE_DIR:woof-com>/woof.com")
    add_custom_command(TARGET woof-exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE_DIR:woof-exe>/$<TARGET_FILE_NAME:woof-exe>"
        "$<TARGET_FILE_DIR:woof-exe>/woof.exe")
endif()

# Files to package in our distribution.
if(WIN32)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21 AND NOT VCPKG_TOOLCHAIN)
        install(TARGETS woof woof-setup
                RUNTIME_DEPENDENCIES 
                    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
                    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
                    DIRECTORIES $<TARGET_FILE_DIR:woof> $ENV{PATH}
                RUNTIME DESTINATION .)
    else()
        install(TARGETS woof woof-setup RUNTIME DESTINATION .)
    endif()
    install(FILES "$<TARGET_FILE_DIR:woof-com>/woof.com" DESTINATION .)
    install(FILES "$<TARGET_FILE_DIR:woof-exe>/woof.exe" DESTINATION .)
else()
    install(TARGETS woof woof-setup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Copy autoload directory content to target directory.
add_custom_command(TARGET woof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:woof>/autoload"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/autoload"
    "$<TARGET_FILE_DIR:woof>/autoload" VERBATIM)
