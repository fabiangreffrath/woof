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
    d_net.c    d_net.h
    d_player.h
    d_textur.h
    d_think.h
    d_ticcmd.h
    dogs.h
    doomdata.h
    doomdef.c  doomdef.h
    doomstat.c doomstat.h
    doomtype.h
    dstrings.c dstrings.h
    f_finale.c f_finale.h
    f_wipe.c   f_wipe.h
    g_game.c   g_game.h
    hu_lib.c   hu_lib.h
    hu_stuff.c hu_stuff.h
    i_main.c
    i_midipipe.c i_midipipe.h
    i_net.c    i_net.h
    i_sound.c  i_sound.h
    i_system.c i_system.h
    i_video.c  i_video.h
    info.c     info.h
    m_argv.c   m_argv.h
    m_bbox.c   m_bbox.h
    m_cheat.c  m_cheat.h
    m_fixed.h
    m_menu.c   m_menu.h
    m_misc.c   m_misc.h
    m_misc2.c  m_misc2.h
    m_random.c m_random.h
    m_swap.h
    mmus2mid.c mmus2mid.h
    net_defs.h
    net_packet.c net_packet.h
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
    r_bsp.c    r_bsp.h
    r_data.c   r_data.h
    r_defs.h
    r_draw.c   r_draw.h
    r_main.c   r_main.h
    r_plane.c  r_plane.h
    r_segs.c   r_segs.h
    r_sky.c    r_sky.h
    r_state.h
    r_things.c r_things.h
    s_sound.c  s_sound.h
    s_musinfo.c s_musinfo.h
    sounds.c   sounds.h
    statdump.c statdump.h
    st_lib.c   st_lib.h
    st_stuff.c st_stuff.h
    tables.c   tables.h
    v_video.c  v_video.h
    version.c  version.h
    w_wad.c    w_wad.h
    wi_stuff.c wi_stuff.h
    z_zone.c   z_zone.h)

# Some platforms require standard libraries to be linked against.
if(m_FOUND)
    list(APPEND WOOF_LIBRARIES m)
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
target_link_libraries(woof PRIVATE ${WOOF_LIBRARIES}
    ZLIB::ZLIB SDL2::SDL2 SDL2::image SDL2::mixer SDL2::net)

if(MSVC)
    # MSVC tries to supply a default manifest and complains when it finds ours
    # unless we specifically tell it not to.
    set_target_properties(woof PROPERTIES LINK_FLAGS "/MANIFEST:NO")
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

# Assemble library files.
set(WOOF_DLLS "")
if(WIN32)
    # zlib
    get_target_property(_ZLIB_DLL ZLIB::ZLIB IMPORTED_LOCATION)
    list(APPEND WOOF_DLLS "${_ZLIB_DLL}")
    unset(_ZLIB_DLL)

    # SDL2
    list(APPEND WOOF_DLLS "${SDL2_DLL_DIR}/SDL2.dll")

    # SDL2_image
    list(APPEND WOOF_DLLS "${SDL2_IMAGE_DLL_DIR}/libpng16-16.dll")
    list(APPEND WOOF_DLLS "${SDL2_IMAGE_DLL_DIR}/SDL2_image.dll")

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
    install(TARGETS woof RUNTIME DESTINATION .)
else()
    install(TARGETS woof RUNTIME DESTINATION bin)
endif()
install(FILES ${WOOF_DLLS} DESTINATION .)
