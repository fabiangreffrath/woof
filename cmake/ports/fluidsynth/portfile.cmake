vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO FluidSynth/fluidsynth
    REF d4a6056f9ab0df0711ca8a3ffec9af5e4456db1c
    SHA512 1014d3fde2b787078b0139a5c8ce499329ac6cbda8f9dd95570d43691177aef4872cde9e11052ab4c592ee8e7ad7a7716c57b56fe7efc0f6d874f7905e6f0e2e
    HEAD_REF master
)
# Do not use or install FindSndFileLegacy.cmake and its deps
file(REMOVE
    "${SOURCE_PATH}/cmake_admin/FindFLAC.cmake"
    "${SOURCE_PATH}/cmake_admin/Findmp3lame.cmake"
    "${SOURCE_PATH}/cmake_admin/Findmpg123.cmake"
    "${SOURCE_PATH}/cmake_admin/FindOgg.cmake"
    "${SOURCE_PATH}/cmake_admin/FindOpus.cmake"
    "${SOURCE_PATH}/cmake_admin/FindSndFileLegacy.cmake"
    "${SOURCE_PATH}/cmake_admin/FindVorbis.cmake"
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        sndfile     enable-libsndfile
)

# enable platform-specific features, force the build to fail if the required libraries are not found,
# and disable all other features to avoid system libraries to be picked up
set(WINDOWS_OPTIONS enable-dsound enable-wasapi enable-waveout enable-winmidi HAVE_MMSYSTEM_H HAVE_DSOUND_H HAVE_OBJBASE_H)
set(MACOS_OPTIONS enable-coreaudio enable-coremidi COREAUDIO_FOUND COREMIDI_FOUND)
set(LINUX_OPTIONS enable-alsa ALSA_FOUND)
set(ANDROID_OPTIONS enable-opensles OpenSLES_FOUND)
set(IGNORED_OPTIONS enable-coverage enable-dbus enable-floats enable-fpe-check enable-framework enable-jack
    enable-libinstpatch enable-midishare enable-oboe enable-openmp enable-oss enable-pipewire enable-portaudio
    enable-profiling enable-readline enable-sdl3 enable-systemd enable-trap-on-fpe enable-ubsan
    enable-ipv6 enable-network)

# disable all options
set(OPTIONS_TO_DISABLE ${WINDOWS_OPTIONS} ${MACOS_OPTIONS} ${LINUX_OPTIONS} ${ANDROID_OPTIONS})
    
foreach(_option IN LISTS OPTIONS_TO_DISABLE IGNORED_OPTIONS)
    list(APPEND DISABLED_OPTIONS "-D${_option}:BOOL=OFF")
endforeach()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        "-Dosal=cpp11"
        ${FEATURE_OPTIONS}
        ${DISABLED_OPTIONS}
        "-DCMAKE_CXX_FLAGS=${STATIC_LIBSTDPP}"
    MAYBE_UNUSED_VARIABLES
        ${OPTIONS_TO_DISABLE}
        enable-coverage
        enable-framework
        enable-ubsan
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/fluidsynth)
vcpkg_fixup_pkgconfig()

set(tools fluidsynth)
vcpkg_copy_tools(TOOL_NAMES ${tools} AUTO_CLEAN)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/man")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
    
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
