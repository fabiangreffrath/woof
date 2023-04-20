include(FindPackageHandleStandardArgs)

find_library(libxmp_LIBRARY
    NAMES xmp)

find_path(libxmp_INCLUDE_PATH
    NAMES xmp.h)

find_package_handle_standard_args(libxmp
    REQUIRED_VARS libxmp_LIBRARY libxmp_INCLUDE_PATH
)

if(libxmp_FOUND)
    if(NOT TARGET libxmp::xmp_shared)
        add_library(libxmp::xmp_shared UNKNOWN IMPORTED)
        set_target_properties(libxmp::xmp_shared PROPERTIES
            IMPORTED_LOCATION "${libxmp_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${libxmp_INCLUDE_PATH}")
    endif()
endif()
