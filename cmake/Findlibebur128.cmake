# Variables defined:
#  libebur128_FOUND
#  libebur128_INCLUDE_DIR
#  libebur128_LIBRARY

find_package(PkgConfig QUIET)
pkg_check_modules(PC_libebur128 libebur128 QUIET)

find_library(libebur128_LIBRARY
    NAMES ebur128
    HINTS "${PC_libebur128_LIBDIR}")

find_path(libebur128_INCLUDE_DIR
    NAMES ebur128.h
    HINTS "${PC_libebur128_INCLUDEDIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libebur128
    REQUIRED_VARS libebur128_LIBRARY libebur128_INCLUDE_DIR)

if(libebur128_FOUND)
    if(NOT TARGET libebur128::libebur128)
        add_library(libebur128::libebur128 UNKNOWN IMPORTED)
        set_target_properties(libebur128::libebur128 PROPERTIES
            IMPORTED_LOCATION "${libebur128_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${libebur128_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(libebur128_LIBRARY libebur128_INCLUDE_DIR)
