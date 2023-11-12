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
pkg_check_modules(PC_OpenAL QUIET OpenAL)

find_path(OPENAL_INCLUDE_DIR al.h
  PATH_SUFFIXES include/AL include/OpenAL include AL OpenAL
  HINTS "${PC_OpenAL_INCLUDEDIR}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_OpenAL_ARCH_DIR libs/Win64)
else()
  set(_OpenAL_ARCH_DIR libs/Win32)
endif()

find_library(OPENAL_LIBRARY
  NAMES OpenAL al openal OpenAL32
  PATH_SUFFIXES libx32 lib64 lib libs64 libs ${_OpenAL_ARCH_DIR}
  HINTS "${PC_OpenAL_LIBDIR}")

unset(_OpenAL_ARCH_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenAL
  REQUIRED_VARS OPENAL_LIBRARY OPENAL_INCLUDE_DIR
  VERSION_VAR OPENAL_VERSION_STRING)

mark_as_advanced(OPENAL_LIBRARY OPENAL_INCLUDE_DIR)

if(OPENAL_FOUND AND NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL UNKNOWN IMPORTED)
    set_target_properties(OpenAL::OpenAL PROPERTIES
      IMPORTED_LOCATION "${OPENAL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENAL_INCLUDE_DIR}")
endif()
