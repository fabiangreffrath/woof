# Variables defined:
#  FFTW3f_FOUND
#  FFTW3f_INCLUDE_DIR
#  FFTW3f_LIBRARY

find_package(PkgConfig QUIET)
pkg_check_modules(PC_FFTW3f QUIET FFTW3f)

find_library(FFTW3f_LIBRARY
    NAMES fftw3f
    HINTS "${PC_FFTW3f_LIBDIR}")

find_path(FFTW3f_INCLUDE_DIR
    NAMES fftw3.h
    HINTS "${PC_FFTW3f_INCLUDEDIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFTW3f
    REQUIRED_VARS FFTW3f_LIBRARY FFTW3f_INCLUDE_DIR)

if(FFTW3f_FOUND)
    if(NOT TARGET FFTW3::fftw3f)
        add_library(FFTW3::fftw3f UNKNOWN IMPORTED)
        set_target_properties(FFTW3::fftw3f PROPERTIES
            IMPORTED_LOCATION "${FFTWf_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFTW3f_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(FFTW3f_LIBRARY FFTW3f_INCLUDE_DIR)
