include(WoofSettings)

set(WOOF_SOURCES
    am_map.c               am_map.h
    d_demoloop.c           d_demoloop.h
                           d_englsh.h
                           d_event.h
    d_items.c              d_items.h
    d_iwad.c               d_iwad.h
    d_loop.c               d_loop.h
    d_main.c               d_main.h
    d_net.c
                           d_player.h
                           d_think.h
                           d_ticcmd.h
                           doomdata.h
                           doomdef.h
    doomstat.c             doomstat.h
                           doomtype.h
    deh_ammo.c
    deh_bex_helper.c
    deh_bex_includes.c
    deh_bex_music.c        deh_bex_music.h
    deh_bex_partimes.c     deh_bex_partimes.h
    deh_bex_pointers.c
    deh_bex_sounds.c       deh_bex_sounds.h
    deh_bex_sprites.c      deh_bex_sprites.h
    deh_bex_strings.c
    deh_cheat.c
    deh_defs.h
    deh_frame.c
    deh_io.c               deh_io.h
    deh_main.c             deh_main.h
    deh_mapping.c          deh_mapping.h
    deh_misc.c             deh_misc.h
    deh_pointer.c
    deh_sound.c
    deh_strings.c          deh_strings.h
    deh_text.c
    deh_thing.c
    deh_weapon.c
    f_finale.c             f_finale.h
    f_wipe.c               f_wipe.h
    g_compatibility.c      g_compatibility.h
    g_game.c               g_game.h
    g_input.c              g_input.h
    g_nextweapon.c         g_nextweapon.h
    g_rewind.c             g_rewind.h
    g_umapinfo.c           g_umapinfo.h
    hu_command.c           hu_command.h
    hu_coordinates.c       hu_coordinates.h
    hu_crosshair.c         hu_crosshair.h
    hu_obituary.c          hu_obituary.h
    i_3dsound.c
    i_endoom.c             i_endoom.h
    i_exit.c               i_exit.h
    i_flickstick.c         i_flickstick.h
    i_gamepad.c            i_gamepad.h
    i_glob.c               i_glob.h
    i_gyro.c               i_gyro.h
    i_input.c              i_input.h
    i_main.c
    i_mbfsound.c
    i_midimusic.c
    i_minimp3.c
                           i_oalcommon.h
    i_oalequalizer.c       i_oalequalizer.h
    i_oalmusic.c
    i_oalsound.c           i_oalsound.h
                           i_oalstream.h
    i_oplmusic.c
    i_printf.c             i_printf.h
    i_region.c             i_region.h
    i_richpresence.c       i_richpresence.h
    i_rumble.c             i_rumble.h
    i_sndfile.c            i_sndfile.h
    i_sound.c              i_sound.h
    i_system.c             i_system.h
    i_timer.c              i_timer.h
    i_video.c              i_video.h
    info.c                 info.h
    kf_file.c
    kf_memory.c
    m_arena.c              m_arena.h
    m_argv.c               m_argv.h
                           m_array.h
    m_bbox.c               m_bbox.h
    m_cheat.c              m_cheat.h
    m_config.c             m_config.h
    m_hashmap.c            m_hashmap.h
                           m_fixed.h
    m_input.c              m_input.h
    m_io.c                 m_io.h
    m_json.c               m_json.h
    m_scanner.c            m_scanner.h
    mn_font.c              mn_font.h
    mn_menu.c              mn_menu.h
    mn_setup.c             mn_internal.h
    m_misc.c               m_misc.h
    m_random.c             m_random.h
    mn_snapshot.c          mn_snapshot.h
                           m_swap.h
                           m_vector.h
    memio.c                memio.h
    midifallback.c         midifallback.h
    midifile.c             midifile.h
    midiout.c              midiout.h
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
    net_netlib.c           net_netlib.h
    net_server.c           net_server.h
    net_structrw.c         net_structrw.h
                           p_action.h
    p_ambient.c            p_ambient.h
    p_ceilng.c
    p_dirty.c              p_dirty.h
    p_doors.c
    p_enemy.c              p_enemy.h
    p_extnodes.c           p_extnodes.h
    p_floor.c
    p_genlin.c
    p_inter.c              p_inter.h
                           p_keyframe.h
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
    p_udmf.c               p_udmf.h
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
    r_skydefs.c            r_skydefs.h
                           r_srgb.h
                           r_state.h
    r_swirl.c              r_swirl.h
    r_things.c             r_things.h
    r_tranmap.c            r_tranmap.h
    r_voxel.c              r_voxel.h
    s_musinfo.c            s_musinfo.h
    s_sndinfo.c            s_sndinfo.h
    s_sound.c              s_sound.h
    s_trakinfo.c           s_trakinfo.h
    sounds.c               sounds.h
    st_sbardef.c           st_sbardef.h
    st_carousel.c          st_carousel.h
    st_stuff.c             st_stuff.h
    st_widgets.c           st_widgets.h
    statdump.c             statdump.h
    tables.c               tables.h
    v_flextran.c           v_flextran.h
    v_patch.c              v_patch.h
    v_trans.c              v_trans.h
    v_video.c              v_video.h
    w_wad.c                w_wad.h
                           w_internal.h
    w_file.c
    w_zip.c
    wi_stuff.c             wi_stuff.h
    wi_interlvl.c          wi_interlvl.h
    ws_stuff.c             ws_stuff.h
    z_zone.c               z_zone.h
)

# Standard target definition
add_executable(woof ${WOOF_SOURCES})
target_woof_settings(woof)
target_include_directories(woof PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")

if(WIN32)
    # Stamp out and compile resource file on Windows.
    configure_file(resource.rc.in "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
    target_sources(woof PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")

    # MSVC tries to supply a default manifest and complains when it finds ours
    # unless we specifically tell it not to.
    if(MSVC)
        target_link_options(
            woof
            PRIVATE "/MANIFEST:NO"
            "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,/SUBSYSTEM:CONSOLE,/SUBSYSTEM:WINDOWS>"
        )
    else()
        target_link_options(woof PRIVATE
            "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,-mconsole,-mwindows>")
    endif()
endif()

target_link_libraries(
    woof
    PRIVATE SDL3::SDL3
            OpenAL::OpenAL
            libebur128::libebur128
            SndFile::sndfile
            yyjson::yyjson
            pffft::pffft
            miniz::miniz
            netlib
            md5
            spng
            sha1
            opl
            textscreen
)

target_include_directories(woof PRIVATE "${MINIMP3_INCLUDE_DIRS}/minimp3")

# Some platforms require standard libraries to be linked against.
if(HAVE_LIBM)
    target_link_libraries(woof PRIVATE m)
endif()

if(HAVE_AL_BUFFER_CALLBACK)
    target_sources(woof PRIVATE i_pcsound.c)
endif()

if(WIN32)
    target_link_libraries(woof PRIVATE winmm shlwapi)
endif()

if(APPLE)
    target_link_libraries(
      woof
      PRIVATE -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit
              -Wl,-framework,CoreServices -Wl,-framework,CoreAudio
              -Wl,-framework,CoreMIDI
    )
endif()

if(HAVE_ALSA)
    target_include_directories(woof PRIVATE ${ALSA_INCLUDE_DIR})
    target_link_libraries(woof PRIVATE ALSA::ALSA)
endif()

if(HAVE_FLUIDSYNTH)
    target_sources(woof PRIVATE i_flmusic.c)
    target_link_libraries(woof PRIVATE FluidSynth::libfluidsynth)
endif()

if(HAVE_LIBXMP)
    target_sources(woof PRIVATE i_xmp.c)
    target_link_libraries(woof PRIVATE libxmp::xmp)
endif()

if(HAVE_DISCORD_RPC)
    target_link_libraries(woof PRIVATE DiscordRPC::discord-rpc)
endif()

target_compile_definitions(woof PRIVATE
    $<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,WOOF_DEBUG,WOOF_RELEASE>)

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

# Setup tool
set(SETUP_SOURCES
    d_iwad.c                d_iwad.h
    i_exit.c                i_exit.h
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
    net_netlib.c            net_netlib.h
    net_structrw.c          net_structrw.h
    z_zone.c                z_zone.h
)

if(WIN32)
    # Stamp out and compile resource file on Windows.
    configure_file(setup-res.rc.in "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
    list(APPEND SETUP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
endif()

add_executable(woof-setup WIN32 ${SETUP_SOURCES})
target_woof_settings(woof-setup)

target_include_directories(woof-setup PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries(
  woof-setup
  PRIVATE SDL3::SDL3
          textscreen
          setup
          netlib
)

if(MSVC)
    target_link_options(woof-setup PRIVATE "/MANIFEST:NO")
endif()

# Base file
add_dependencies(woof woof-base)

if(MSVC)
    add_custom_command(
        TARGET woof
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${BASE_PK3_PATH}"
                "$<TARGET_FILE_DIR:woof>"
    )
endif()

# Install
#
# Files to package in our distribution.
if(WIN32)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21 AND NOT VCPKG_TOOLCHAIN)
        install(
            TARGETS woof woof-setup
            RUNTIME_DEPENDENCIES 
                PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
                POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
                DIRECTORIES $<TARGET_FILE_DIR:woof> $ENV{PATH}
            RUNTIME DESTINATION .
        )
    else()
        install(TARGETS woof woof-setup RUNTIME DESTINATION .)
    endif()
else()
    install(TARGETS woof woof-setup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
