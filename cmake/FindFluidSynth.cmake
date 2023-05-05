#[=======================================================================[.rst:
FindFluidSynth
-------

Finds the FluidSynth library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``FluidSynth::libfluidsynth``
  The FluidSynth library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``FluidSynth_FOUND``
  True if the system has the FluidSynth library.
``FluidSynth_INCLUDE_DIRS``
  Include directories needed to use FluidSynth.
``FluidSynth_LIBRARIES``
  Libraries needed to link to FluidSynth.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``FluidSynth_INCLUDE_DIR``
  The directory containing ``FluidSynth.h``.
``FluidSynth_LIBRARY``
  The path to the FluidSynth library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_FluidSynth QUIET fluidsynth)

find_path(FluidSynth_INCLUDE_DIR
  NAMES fluidsynth.h
  HINTS "${PC_FluidSynth_INCLUDEDIR}")

find_library(FluidSynth_LIBRARY
  NAMES fluidsynth libfluidsynth
  HINTS "${PC_FluidSynth_LIBDIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FluidSynth
  REQUIRED_VARS "FluidSynth_LIBRARY" "FluidSynth_INCLUDE_DIR"
  VERSION_VAR "FluidSynth_VERSION")

if(FluidSynth_FOUND)
  if(NOT TARGET FluidSynth::libfluidsynth)
    add_library(FluidSynth::libfluidsynth UNKNOWN IMPORTED)
    set_target_properties(FluidSynth::libfluidsynth
      PROPERTIES IMPORTED_LOCATION "${FluidSynth_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${FluidSynth_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(FluidSynth_INCLUDE_DIR FluidSynth_LIBRARY)
