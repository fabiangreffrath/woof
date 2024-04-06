# Variables defined:
#  PortMidi_FOUND
#  PortMidi_INCLUDE_DIR
#  PortMidi_LIBRARY

find_package(PkgConfig)
pkg_check_modules(PC_PortMidi portmidi)

find_library(PortMidi_LIBRARY
    NAMES portmidi
    HINTS "${PC_PortMidi_LIBDIR}")

find_path(PortMidi_INCLUDE_DIR
    NAMES portmidi.h
    HINTS "${PC_PortMidi_INCLUDEDIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortMidi
    REQUIRED_VARS PortMidi_LIBRARY PortMidi_INCLUDE_DIR)

if(PortMidi_FOUND)
    if(NOT TARGET PortMidi::portmidi)
        add_library(PortMidi::portmidi UNKNOWN IMPORTED)
        set_target_properties(PortMidi::portmidi PROPERTIES
            IMPORTED_LOCATION "${PortMidi_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PortMidi_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(PortMidi_LIBRARY PortMidi_INCLUDE_DIR)
