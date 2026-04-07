# Variables defined: spng_FOUND spng_INCLUDE_DIR spng_LIBRARY

find_package(PkgConfig QUIET)
pkg_check_modules(PC_spng IMPORTED_TARGET spng)

if(PC_spng_FOUND)
    if(NOT TARGET spng::spng)
        add_library(spng::spng ALIAS PkgConfig::PC_spng)
    endif()
    set(spng_FOUND TRUE)
    set(spng_VERSION ${PC_spng_VERSION})
    return()
endif()

find_library(spng_LIBRARY NAMES spng libspng)

find_path(spng_INCLUDE_DIR NAMES spng.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(spng REQUIRED_VARS spng_LIBRARY
                                                       spng_INCLUDE_DIR)

if(spng_FOUND)
    if(NOT TARGET spng::spng)
        add_library(spng::spng UNKNOWN IMPORTED)
        set_target_properties(
            spng::spng
            PROPERTIES IMPORTED_LOCATION "${spng_LIBRARY}"
                       INTERFACE_INCLUDE_DIRECTORIES "${spng_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(spng_LIBRARY spng_INCLUDE_DIR)
