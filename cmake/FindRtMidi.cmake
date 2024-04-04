# Variables defined:
#  RtMidi_FOUND
#  RtMidi_INCLUDE_DIR
#  RtMidi_LIBRARY

find_package(PkgConfig)
pkg_check_modules(PC_RtMidi rtmidi)

find_library(RtMidi_LIBRARY
    NAMES rtmidi
    HINTS "${PC_RtMidi_LIBDIR}")

find_path(RtMidi_INCLUDE_DIR
    NAMES rtmidi_c.h
    HINTS "${PC_RtMidi_INCLUDEDIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RtMidi
    REQUIRED_VARS RtMidi_LIBRARY RtMidi_INCLUDE_DIR)

if(RtMidi_FOUND)
    if(NOT TARGET RtMidi::rtmidi)
        add_library(RtMidi::rtmidi UNKNOWN IMPORTED)
        set_target_properties(RtMidi::rtmidi PROPERTIES
            IMPORTED_LOCATION "${RtMidi_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RtMidi_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(RtMidi_LIBRARY RtMidi_INCLUDE_DIR)
