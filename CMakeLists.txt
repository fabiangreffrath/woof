cmake_minimum_required(VERSION 3.15...4.0)

option(WITH_FLUIDSYNTH "Use FluidSynth if available" ON)
if(WITH_FLUIDSYNTH)
    list(APPEND VCPKG_MANIFEST_FEATURES "fluidsynth")
endif()

option(WITH_XMP "Use libxmp if available" ON)
if(WITH_XMP)
    list(APPEND VCPKG_MANIFEST_FEATURES "libxmp")
endif()

option(WITH_DISCORD_RPC "Use discord-rpc if available" ON)
if(WITH_DISCORD_RPC)
    list(APPEND VCPKG_MANIFEST_FEATURES "discord-rpc")
endif()

option(ENABLE_WERROR "Treat warnings as errors" OFF)
option(ENABLE_ASAN "Enable ASan" OFF)
option(ENABLE_UBSAN "Enable UBSan" OFF)
option(ENABLE_TSAN "Enable TSan" OFF)
option(ENABLE_HARDENING "Enable hardening flags" OFF)
option(ENABLE_LTO "Enable link time optimization" OFF)
option(ENABLE_FONTGEN "Generate textscreen font" OFF)

option(WOOF_RANGECHECK "Enable bounds-checking of performance-sensitive functions" ON)
option(WOOF_STRICT
       "Prefer original MBF code paths over demo compatiblity with PrBoom+" OFF)

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG
       "Lookup package config files before using find modules" ON)
option(CMAKE_COLOR_DIAGNOSTICS "Colored diagnostics" ON)

# X_VCPKG_APPLOCAL_DEPS_INSTALL automatically installs dependencies.
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

# Adds the cmake directory to the CMake include path.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project(
    "Woof"
    VERSION 15.2.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS
        "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE
        "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
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
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2025")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_SHORTNAME "woof")
set(PROJECT_APPID "io.github.fabiangreffrath.woof")
set(BASE_PK3 "woof.pk3")
set(WOOF_ICON "woof.ico")
set(SETUP_ICON "setup.ico")
set(PROJECT_VERSION_RC
    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT HAVE_LTO OUTPUT output)
    if(NOT HAVE_LTO)
        message(WARNING "IPO is not supported: ${output}")
    endif()
endif()

include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

# Compiler environment requirements.
check_library_exists(m pow "" HAVE_LIBM)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_symbol_exists(getpwuid "unistd.h;sys/types.h;pwd.h" HAVE_GETPWUID)
check_c_source_compiles(
    "
    #include <immintrin.h>
    int main(void)
    {
        int tmp = _div64(4, 2, NULL);
        return 0;
    }
    "
    HAVE__DIV64
)
check_c_source_compiles(
    "
    typedef float vec __attribute__((ext_vector_type(4)));
    int main(void)
    {
        vec a = (vec){0, 1, 2, 3};
        return 0;
    }
    "
    HAVE_EXT_VECTOR_TYPE
)

set(CMAKE_FIND_FRAMEWORK NEVER)

# Library requirements.
find_package(SDL3 3.3 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(libebur128 REQUIRED)
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

if(SndFile_VERSION VERSION_GREATER_EQUAL "1.1.0")
   set(HAVE_SNDFILE_MPEG TRUE)
endif()

find_package(pffft QUIET)
find_package(miniz QUIET)
find_package(yyjson QUIET)
find_path(MINIMP3_INCLUDE_DIRS "minimp3/minimp3.h" NO_CACHE)

if(WITH_FLUIDSYNTH)
    find_package(FluidSynth)
    if(FluidSynth_FOUND)
        set(HAVE_FLUIDSYNTH TRUE)
    endif()
endif()

if(WITH_XMP)
    find_package(libxmp)
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
endif()

if(WITH_DISCORD_RPC)
    find_package(DiscordRPC)
    if(DiscordRPC_FOUND)
        set(HAVE_DISCORD_RPC TRUE)
    endif()
endif()

# Python 3
find_package(Python3 COMPONENTS Interpreter)

configure_file(config.h.in config.h)

if(WIN32)
    set(WOOF_INSTALL_BIN_DIR  ".")
else()
    include(GNUInstallDirs)
    set(WOOF_INSTALL_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
endif()

if(WIN32)
    install(FILES COPYING DESTINATION . RENAME COPYING.txt)
    if(HAVE_FLUIDSYNTH)
        install(DIRECTORY DESTINATION soundfonts)
    endif()
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    if(EXISTS "${CMAKE_SOURCE_DIR}/soundfonts" AND HAVE_FLUIDSYNTH)
        install(DIRECTORY soundfonts/
            DESTINATION "share/${PROJECT_SHORTNAME}/soundfonts")
    endif()
endif()

set(BASE_PK3_PATH "${CMAKE_BINARY_DIR}/src/${BASE_PK3}")

# Where to find other CMakeLists.txt files.
add_subdirectory(base)
add_subdirectory(data)
add_subdirectory(opl)
add_subdirectory(textscreen)
add_subdirectory(third-party)
add_subdirectory(src)
add_subdirectory(setup)
add_subdirectory(man)
add_subdirectory(netlib)
add_subdirectory(cmake/packaging)
