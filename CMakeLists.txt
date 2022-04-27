include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(ExternalProject)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.6)

project("Woof"
    VERSION 9.0.0
    LANGUAGES C)

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

if(MINGW)
    # 3.16 is needed for GET_RUNTIME_DEPENDENCIES.
    cmake_minimum_required(VERSION 3.16)
endif()

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2021")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_TARNAME "woof")
if(MSVC_VERSION LESS 1400)
    set(WOOF_ICON "woof8.ico")
else()
    set(WOOF_ICON "woof.ico")
endif()
set(PROJECT_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0") # ${PROJECT_VERSION_TWEAK}

if(NOT WIN32)
    set(WOOFDATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_TARNAME}" CACHE PATH "Path to install supplemental files" FORCE)
endif()

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_INSTRUMENTED" "Enable memory allocation statistics" OFF)
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)
option("${PROJECT_NAME}_STRICT" "Prefer original MBF code paths over demo compatiblity with PrBoom+" OFF)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)
check_include_file("dirent.h" HAVE_DIRENT_H)
check_symbol_exists(strsignal "string.h" HAVE_STRSIGNAL)

# Library requirements.
#
# In some instances, we can do useful things if a library can't be found.

# SDL2.  On Windows, we use the official development library.
if(NOT SDL2_DIR)
    if(WIN32 AND (MSVC OR CMAKE_CROSSCOMPILING))
        message(STATUS "Downloading SDL2 Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.20-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                EXPECTED_HASH SHA256=5b1512ca6c9d2427bd2147da01e5e954241f8231df12f54a7074dccde416df18)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.20-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                EXPECTED_HASH SHA256=38094d82a857d6c62352e5c5cdec74948c5b4d25c59cbd298d6d233568976bd1)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2-2.0.20")
    endif()
endif()
find_package(SDL2 2.0.7)

# SDL2_mixer.  On Windows, we use the official development library.
if(NOT SDL2_MIXER_DIR)
    if(WIN32 AND (MSVC OR CMAKE_CROSSCOMPILING))
        message(STATUS "Downloading SDL2_mixer Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.0.4-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip"
                EXPECTED_HASH SHA256=258788438b7e0c8abb386de01d1d77efe79287d9967ec92fbb3f89175120f0b0)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.0.4-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz"
                EXPECTED_HASH SHA256=14250b2ade20866c7b17cf1a5a5e2c6f3920c443fa3744f45658c8af405c09f1)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-2.0.4")
    endif()
endif()
find_package(SDL2_mixer 2.0.2)

# SDL2_net.  On Windows, we use the official development library.
if(NOT SDL2_NET_DIR)
    if(WIN32 AND (MSVC OR CMAKE_CROSSCOMPILING))
        message(STATUS "Downloading SDL2_net Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_net/release/SDL2_net-devel-2.0.1-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip"
                EXPECTED_HASH SHA256=c1e423f2068adc6ff1070fa3d6a7886700200538b78fd5adc36903a5311a243e)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_net/release/SDL2_net-devel-2.0.1-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz"
                EXPECTED_HASH SHA256=fe0652ab1bdbeae277d7550f2ed686a37a5752f7a624f54f19cf1bd6ba5cb9ff)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-2.0.1")
    endif()
endif()
find_package(SDL2_net)

# FluidSynth backend
find_package(FluidSynth)

# Python 3.
#
# Optional, not needed for most things.  If you're missing it you need to
# install it yourself.
find_package(Python3 COMPONENTS Interpreter)

configure_file(config.h.in config.h)

# Files that should be installed with the install target.
configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${PROJECT_BINARY_DIR}/WoofInstall.cmake")
if(WIN32)
    if(NOT CMAKE_CROSSCOMPILING)
        install(FILES "${PROJECT_BINARY_DIR}/COPYING.txt" DESTINATION .)
        install(FILES "${PROJECT_BINARY_DIR}/README.txt" DESTINATION .)
    else()
        install(FILES COPYING DESTINATION .)
        install(FILES README.md DESTINATION .)
    endif()
    install(FILES betagrph.wad DESTINATION .)
    install(FILES betalevl.wad DESTINATION .)
    install(FILES watermap.wad DESTINATION .)
    install(DIRECTORY autoload/ DESTINATION autoload)
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION soundfonts)
    endif()
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_TARNAME}")
    install(FILES README.md DESTINATION "share/doc/${PROJECT_TARNAME}")
    install(FILES betagrph.wad DESTINATION "share/${PROJECT_TARNAME}")
    install(FILES betalevl.wad DESTINATION "share/${PROJECT_TARNAME}")
    install(FILES watermap.wad DESTINATION "share/${PROJECT_TARNAME}")
    install(DIRECTORY autoload/ DESTINATION "share/${PROJECT_TARNAME}/autoload")
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION "share/${PROJECT_TARNAME}/soundfonts")
    endif()
endif()

# Generate distribution packages with CPack.
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_SOURCE_GENERATOR TGZ ZIP)
set(CPACK_SOURCE_IGNORE_FILES "/.git/;/build;/.vs/;/out/;CMakeSettings.json")
set(CPACK_STRIP_FILES TRUE)
include(CPack)

# Where to find other CMakeLists.txt files.
add_subdirectory(data)
add_subdirectory(docs)
add_subdirectory(examples)
add_subdirectory(opl)
add_subdirectory(textscreen)
add_subdirectory(Source)
add_subdirectory(toolsrc)
add_subdirectory(win32)
