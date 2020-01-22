include(CheckLibraryExists)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 3.12 is needed for modern FindPython
cmake_minimum_required(VERSION 3.12)
project("Woof!"
    VERSION 0.9.0
    DESCRIPTION "Woof! is a continuation of MBF, the Doom source port created by Lee Killough, for modern systems."
    HOMEPAGE_URL "https://github.com/fabiangreffrath/woof"
    LANGUAGES C)

set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2020")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_TARNAME "woof")
set(PROJECT_VERSION_RC "0,9,0,0")

configure_file(cmake/config.h.in config.h)

check_library_exists(m pow "" m_FOUND)
check_library_exists(dl dlopen "" dl_FOUND)
find_package(SDL2)
find_package(SDL2_mixer)
find_package(SDL2_net)
find_package(Python3 COMPONENTS Interpreter)

add_subdirectory(data)
add_subdirectory(Source)
