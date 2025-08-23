#[=======================================================================[.rst:
FindOpenAL
----------

Finds Open Audio Library (OpenAL).

Projects using this module should use ``#include "al.h"`` to include the OpenAL
header file, **not** ``#include <AL/al.h>``.  The reason for this is that the
latter is not entirely portable.  Windows/Creative Labs does not by default put
their headers in ``AL/`` and macOS uses the convention ``<OpenAL/al.h>``.

IMPORTED Targets
^^^^^^^^^^^^^^^^

``OpenAL::OpenAL``
  The OpenAL library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``OPENAL_FOUND``
  If false, do not try to link to OpenAL
``OPENAL_INCLUDE_DIR``
  OpenAL include directory
``OPENAL_LIBRARY``
  Path to the OpenAL library
``OPENAL_VERSION_STRING``
  Human-readable string containing the version of OpenAL
#]=======================================================================]

if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/opt/openal-soft/lib/pkgconfig")
    else()
        set(ENV{PKG_CONFIG_PATH} "/usr/local/opt/openal-soft/lib/pkgconfig")
    endif()
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(PC_openal IMPORTED_TARGET OpenAL)

if(PC_openal_FOUND)
    if(NOT TARGET OpenAL::OpenAL)
        add_library(OpenAL::OpenAL ALIAS PkgConfig::PC_openal)
    endif()
    set(OpenAL_FOUND TRUE)
    set(OpenAL_VERSION ${PC_openal_VERSION})
    return()
endif()

find_path(
    OpenAL_INCLUDE_DIR
    NAMES al.h
    PATH_SUFFIXES include/AL include/OpenAL include AL OpenAL
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_OpenAL_ARCH_DIR libs/Win64)
else()
    set(_OpenAL_ARCH_DIR libs/Win32)
endif()

find_library(
    OpenAL_LIBRARY
    NAMES OpenAL al openal OpenAL32
    PATH_SUFFIXES libx32 lib64 lib libs64 libs ${_OpenAL_ARCH_DIR}
)

unset(_OpenAL_ARCH_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenAL REQUIRED_VARS OpenAL_LIBRARY
                                                       OpenAL_INCLUDE_DIR)

if(OpenAL_FOUND AND NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL UNKNOWN IMPORTED)
    set_target_properties(
        OpenAL::OpenAL
        PROPERTIES IMPORTED_LOCATION "${OpenAL_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES "${OpenAL_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(OpenAL_LIBRARY OpenAL_INCLUDE_DIR)
