include(CheckLibraryExists)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 3.12 is needed for modern FindPython.
cmake_minimum_required(VERSION 3.12)
project("Woof"
    VERSION 1.0.0
    DESCRIPTION "Woof! is a continuation of Lee Killough's Doom source port MBF targeted at modern systems."
    HOMEPAGE_URL "https://github.com/fabiangreffrath/woof"
    LANGUAGES C)

# Prevent in-tree builds.
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-tree builds are not supported.")
endif()

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2020")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_TARNAME "woof")
set(PROJECT_VERSION_RC "1,0,0,0")

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_INSTRUMENTED" "Enable memory allocation statistics" OFF)
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)
check_library_exists(dl dlopen "" dl_FOUND)

# Library requirements.
#
# We default to official SDL development libraries on some platforms unless
# the user overrides with a specific directory.

if(NOT SDL2_DIR)
    if(WIN32)
        message(STATUS "Downloading SDL2 Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.10-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                EXPECTED_HASH SHA256=f1930cf5033725b0e7829c7368f70f65a993bc946ca1197c36888ccecbf07891)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                EXPECTED_HASH SHA256=a90a7cddaec4996f4d7be6d80c57ec69b062e132bffc513965f99217f603274a)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz")
        endif()
        set(SDL2_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2-2.0.10")
    endif()
endif()
find_package(SDL2)

if(NOT SDL2_MIXER_DIR)
    if(WIN32)
        message(STATUS "Downloading SDL2_mixer Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.0.4-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip"
                EXPECTED_HASH SHA256=258788438b7e0c8abb386de01d1d77efe79287d9967ec92fbb3f89175120f0b0)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.0.4-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz"
                EXPECTED_HASH SHA256=14250b2ade20866c7b17cf1a5a5e2c6f3920c443fa3744f45658c8af405c09f1)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz")
        endif()
        set(SDL2_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-2.0.4")
    endif()
endif()
find_package(SDL2_mixer)

if(NOT SDL2_NET_DIR)
    if(WIN32)
        message(STATUS "Downloading SDL2_net Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_net/release/SDL2_net-devel-2.0.1-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip"
                EXPECTED_HASH SHA256=c1e423f2068adc6ff1070fa3d6a7886700200538b78fd5adc36903a5311a243e)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_net/release/SDL2_net-devel-2.0.1-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz"
                EXPECTED_HASH SHA256=fe0652ab1bdbeae277d7550f2ed686a37a5752f7a624f54f19cf1bd6ba5cb9ff)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz")
        endif()
        set(SDL2_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-2.0.1")
    endif()
endif()
find_package(SDL2_net)

find_package(Python3 COMPONENTS Interpreter)

configure_file(config.h.in config.h)

# Files that should be installed with the install target.
configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${PROJECT_BINARY_DIR}/WoofInstall.cmake")
if(WIN32)
    install(FILES "${PROJECT_BINARY_DIR}/COPYING.txt" DESTINATION .)
    install(FILES "${PROJECT_BINARY_DIR}/README.txt" DESTINATION .)
    install(FILES betagrph.wad DESTINATION .)
    install(FILES betalevl.wad DESTINATION .)
    install(FILES watermap.wad DESTINATION .)
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_TARNAME}")
    install(FILES README.md DESTINATION "share/doc/${PROJECT_TARNAME}")
    install(FILES betagrph.wad DESTINATION "share/${PROJECT_TARNAME}")
    install(FILES betalevl.wad DESTINATION "share/${PROJECT_TARNAME}")
    install(FILES watermap.wad DESTINATION "share/${PROJECT_TARNAME}")
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
add_subdirectory(Source)
add_subdirectory(toolsrc)
