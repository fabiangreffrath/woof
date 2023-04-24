# Variables defined:
#  SNDFILE_FOUND
#  SNDFILE_INCLUDE_DIR
#  SNDFILE_LIBRARY
#
# Environment variables used:
#  SNDFILE_ROOT

find_package(PkgConfig QUIET)
pkg_check_modules(PC_SndFile QUIET sndfile)

set(SndFile_VERSION ${PC_SndFile_VERSION})

find_path(SndFile_INCLUDE_DIR sndfile.h
    HINTS
    ${PC_SndFile_INCLUDEDIR}
    ${PC_SndFile_INCLUDE_DIRS}
    ${SndFile_ROOT})

find_file(SndFile_DLL
    NAMES sndfile.dll libsndfile.dll libsndfile-1.dll
    PATH_SUFFIXES bin
    HINTS ${PC_SndFile_PREFIX})

find_library(SndFile_LIBRARY
    NAMES sndfile libsndfile
    HINTS
    ${PC_SndFile_LIBDIR}
    ${PC_SndFile_LIBRARY_DIRS}
    ${SndFile_ROOT})

if(SndFile_DLL OR SndFile_LIBRARY MATCHES ".so|.dylib")
    set(_sndfile_library_type SHARED)
else()
    set(_sndfile_library_type STATIC)
endif()

get_flags_from_pkg_config("${_sndfile_library_type}" "PC_SndFile" "_sndfile")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SndFile
    REQUIRED_VARS
    SndFile_LIBRARY
    SndFile_INCLUDE_DIR
    VERSION_VAR
    SndFile_VERSION)

if(SndFile_FOUND)
    if(NOT TARGET SndFile::sndfile)
        add_library(SndFile::sndfile ${_sndfile_library_type} IMPORTED)
        set_target_properties(SndFile::sndfile
            PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SndFile_INCLUDE_DIR}"
            INTERFACE_COMPILE_OPTIONS "${_sndfile_compile_options}"
            INTERFACE_LINK_LIBRARIES "${_sndfile_link_libraries}"
            INTERFACE_LINK_DIRECTORIES "${_sndfile_link_directories}"
            INTERFACE_LINK_OPTIONS "${_sndfile_link_options}")
    endif()

    if(SndFile_DLL)
        set_target_properties(SndFile::sndfile
            PROPERTIES
            IMPORTED_LOCATION "${SndFile_DLL}"
            IMPORTED_IMPLIB "${SndFile_LIBRARY}")
    else()
      set_target_properties(SndFile::sndfile
            PROPERTIES
            IMPORTED_LOCATION "${SndFile_LIBRARY}")
    endif()

    set(SndFile_LIBRARIES SndFile::sndfile)
    set(SndFile_INCLUDE_DIRS "${SndFile_INCLUDE_DIR}")
endif()

mark_as_advanced(SndFile_LIBRARY SndFile_INCLUDE_DIR SndFile_DLL)
