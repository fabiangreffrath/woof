include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)

# Adds the cmake directory to the CMake include path.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# X_VCPKG_APPLOCAL_DEPS_INSTALL automatically installs dependencies.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

cmake_minimum_required(VERSION 3.9)

project("Woof"
    VERSION 14.5.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Prevent in-tree builds.
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-tree builds are not supported.")
endif()

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2024")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_SHORTNAME "woof")
set(WOOF_ICON "woof.ico")
set(SETUP_ICON "setup.ico")
set(PROJECT_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0") # ${PROJECT_VERSION_TWEAK}

if(NOT WIN32)
    set(WOOFDATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_SHORTNAME}" CACHE PATH "Path to install supplemental files")
endif()

# Toggle-able defines added at compile-time.
option(WOOF_RANGECHECK "Enable bounds-checking of performance-sensitive functions" ON)
option(WOOF_STRICT "Prefer original MBF code paths over demo compatiblity with PrBoom+" OFF)

# Compiler environment requirements.
check_library_exists(m pow "" HAVE_LIBM)
check_include_file("dirent.h" HAVE_DIRENT_H)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG
       "Lookup package config files before using find modules" ON)

set(CMAKE_FIND_FRAMEWORK NEVER)

# Library requirements.
find_package(SDL2 2.0.18 REQUIRED)
find_package(SDL2_net REQUIRED)
find_package(OpenAL REQUIRED)
find_package(SndFile 1.0.29 REQUIRED)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)
endif()

if(ALSA_FOUND)
    set(HAVE_ALSA TRUE)
endif()

if(OPENAL_VERSION_STRING VERSION_GREATER_EQUAL "1.22.0")
    set(HAVE_AL_BUFFER_CALLBACK TRUE)
endif()

if(NOT TARGET SDL2_net::SDL2_net)
    add_library(SDL2_net::SDL2_net ALIAS SDL2_net::SDL2_net-static)
endif()

if(SndFile_VERSION VERSION_GREATER_EQUAL "1.1.0")
   set(HAVE_SNDFILE_MPEG TRUE)
endif()

find_package(FluidSynth)
find_package(libxmp)

if(FluidSynth_FOUND)
    set(HAVE_FLUIDSYNTH TRUE)
endif()

if(libxmp_FOUND)
    set(HAVE_LIBXMP TRUE)
    if(NOT TARGET libxmp::xmp)
        if(TARGET libxmp::xmp_shared)
            add_library(libxmp::xmp ALIAS libxmp::xmp_shared)
        else()
            add_library(libxmp::xmp ALIAS libxmp::xmp_static)
        endif()
    endif()
endif()

# Python 3
find_package(Python3 COMPONENTS Interpreter)

configure_file(config.h.in config.h)

if(WIN32)
    install(FILES COPYING DESTINATION . RENAME COPYING.txt)
    install(FILES README.md DESTINATION .)
    install(DIRECTORY examples/ DESTINATION docs/examples)
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION soundfonts)
    endif()
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    install(FILES README.md DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    if(EXISTS "${CMAKE_SOURCE_DIR}/examples")
        install(DIRECTORY examples/ DESTINATION "share/doc/${PROJECT_SHORTNAME}/examples")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/soundfonts" AND FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION "share/${PROJECT_SHORTNAME}/soundfonts")
    endif()
endif()

# Generate distribution packages with CPack.
if(WIN32)
    set(CPACK_GENERATOR ZIP)
elseif(LINUX)
    set(CPACK_GENERATOR External)
    set(CPACK_EXTERNAL_ENABLE_STAGING YES)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${PROJECT_BINARY_DIR}/appimage-generate.cmake")

    file(GENERATE
        OUTPUT "${PROJECT_BINARY_DIR}/appimage-generate.cmake"
        CONTENT [[
            find_program(LINUXDEPLOY_EXECUTABLE
                NAMES linuxdeploy linuxdeploy-x86_64.AppImage
                PATHS ${CPACK_PACKAGE_DIRECTORY}/linuxdeploy)

            if(NOT LINUXDEPLOY_EXECUTABLE)
                message(STATUS "Downloading linuxdeploy")
                set(LINUXDEPLOY_EXECUTABLE ${CPACK_PACKAGE_DIRECTORY}/linuxdeploy/linuxdeploy)
                file(DOWNLOAD
                    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
                    ${LINUXDEPLOY_EXECUTABLE}
                    INACTIVITY_TIMEOUT 10)
                execute_process(COMMAND
                    chmod +x ${LINUXDEPLOY_EXECUTABLE} COMMAND_ECHO STDOUT)
            endif()

            execute_process(COMMAND
                ${CMAKE_COMMAND} -E env
                OUTPUT=${CPACK_PACKAGE_FILE_NAME}.appimage
                VERSION=$<IF:$<BOOL:${CPACK_PACKAGE_VERSION}>,${CPACK_PACKAGE_VERSION},0.1.0>
                ${LINUXDEPLOY_EXECUTABLE}
                --appimage-extract-and-run
                --appdir=${CPACK_TEMPORARY_DIRECTORY}
                --executable=$<TARGET_FILE:woof>
                --desktop-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/applications/io.github.fabiangreffrath.woof.desktop
                --icon-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/icons/hicolor/128x128/apps/woof.png
                --output=appimage)
            ]])
else()
    set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_SOURCE_GENERATOR TGZ ZIP)
set(CPACK_SOURCE_IGNORE_FILES "/.git/;/build;/.vs/;/out/;CMakeSettings.json")
set(CPACK_STRIP_FILES TRUE)
include(CPack)

# Where to find other CMakeLists.txt files.
add_subdirectory(data)
add_subdirectory(opl)
add_subdirectory(textscreen)
add_subdirectory(src)
add_subdirectory(toolsrc)
add_subdirectory(setup)
add_subdirectory(docs)
add_subdirectory(man)
