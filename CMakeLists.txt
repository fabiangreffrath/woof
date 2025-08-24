cmake_minimum_required(VERSION 3.15...4.0)

option(WITH_FLUIDSYNTH "Use FluidSynth if available" ON)
if(WITH_FLUIDSYNTH)
    list(APPEND VCPKG_MANIFEST_FEATURES "fluidsynth")
endif()

option(WITH_XMP "Use libxmp if available" ON)
if(WITH_XMP)
    list(APPEND VCPKG_MANIFEST_FEATURES "libxmp")
endif()

option(ENABLE_WERROR "Treat warnings as errors" OFF)
option(ENABLE_ASAN "Enable ASan" OFF)
option(ENABLE_UBSAN "Enable UBSan" OFF)
option(ENABLE_TSAN "Enable TSan" OFF)
option(ENABLE_HARDENING "Enable hardening flags" OFF)
option(ENABLE_LTO "Enable link time optimization" OFF)

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
    #include <windows.h>
    int main()
    {
        CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                              TIMER_ALL_ACCESS);
        return 0;
    }
    "
    HAVE_HIGH_RES_TIMER
)
check_c_source_compiles(
    "
    #include <immintrin.h>
    int main()
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
    int main()
    {
        vec a = (vec){0, 1, 2, 3};
        return 0;
    }
    "
    HAVE_EXT_VECTOR_TYPE
)

set(CMAKE_FIND_FRAMEWORK NEVER)

# Library requirements.
find_package(SDL3 REQUIRED)
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
endif()
if(WITH_XMP)
    find_package(libxmp)
endif()

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
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION soundfonts)
    endif()
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    if(EXISTS "${CMAKE_SOURCE_DIR}/soundfonts" AND FluidSynth_FOUND)
        install(DIRECTORY soundfonts/
            DESTINATION "share/${PROJECT_SHORTNAME}/soundfonts")
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
                OUTPUT=${CPACK_PACKAGE_FILE_NAME}.AppImage
                VERSION=$<IF:$<BOOL:${CPACK_PACKAGE_VERSION}>,${CPACK_PACKAGE_VERSION},0.1.0>
                ${LINUXDEPLOY_EXECUTABLE}
                --appimage-extract-and-run
                --appdir=${CPACK_TEMPORARY_DIRECTORY}
                --executable=$<TARGET_FILE:woof>
                --desktop-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/applications/io.github.fabiangreffrath.woof.desktop
                --icon-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/icons/hicolor/128x128/apps/woof.png
                --output=appimage)
        ]])
    elseif(APPLE)
        set(CPACK_GENERATOR External)
        set(CPACK_EXTERNAL_ENABLE_STAGING YES)
        set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
        set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${PROJECT_BINARY_DIR}/macbundle-generate.cmake")
        set(CPACK_SYSTEM_NAME "mac")
        set(CPACK_BUILD_DIR "${PROJECT_BINARY_DIR}")

        file(GENERATE
            OUTPUT "${PROJECT_BINARY_DIR}/macbundle-generate.cmake"
            CONTENT [[
                set(CPACK_OUTPUT_DIR "${CPACK_BUILD_DIR}/${CPACK_PACKAGE_FILE_NAME}")
                file(MAKE_DIRECTORY ${CPACK_OUTPUT_DIR})

                file(COPY_FILE
                    $<TARGET_FILE:woof>
                    ${CPACK_OUTPUT_DIR}/woof)
                file(COPY_FILE
                    $<TARGET_FILE:woof-setup>
                    ${CPACK_OUTPUT_DIR}/woof-setup)

                file(COPY_FILE
                    ${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/woof/woof.pk3
                    ${CPACK_OUTPUT_DIR}/woof.pk3)
                file(COPY
                    ${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/woof/soundfonts
                    DESTINATION ${CPACK_OUTPUT_DIR})

                file(CONFIGURE
                    OUTPUT ${CPACK_OUTPUT_DIR}/Troubleshooting.txt
                    CONTENT "If you are getting errors like 'libzip.5.5.dylib cant be opened because Apple cannot check it for malicious software.' Run the following command in the woof folder:\n\nxattr -dr com.apple.quarantine path/to/folder")

                execute_process(COMMAND zip -r ${CPACK_PACKAGE_FILE_NAME}.zip ${CPACK_PACKAGE_FILE_NAME})
            ]])
else()
    set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_SOURCE_GENERATOR TGZ ZIP)
set(CPACK_SOURCE_IGNORE_FILES "/.git/;/build;/.vs/;/out/;CMakeSettings.json")
set(CPACK_STRIP_FILES FALSE)
include(CPack)

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
