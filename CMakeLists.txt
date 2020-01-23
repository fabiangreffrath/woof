include(CheckLibraryExists)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 3.12 is needed for modern FindPython.
cmake_minimum_required(VERSION 3.12)
project("Woof"
    VERSION 0.9.0
    DESCRIPTION "Woof! is a continuation of MBF, the Doom source port created by Lee Killough, for modern systems."
    HOMEPAGE_URL "https://github.com/fabiangreffrath/woof"
    LANGUAGES C)

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2020")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_TARNAME "woof")
set(PROJECT_VERSION_RC "0,9,0,0")

configure_file(cmake/config.h.in config.h)

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_BETA" "Enable beta simulation feature" ON)
option("${PROJECT_NAME}_DOGS" "Enable helper dogs feature" ON)
option("${PROJECT_NAME}_INSTRUMENTED" "Enable memory allocation statistics" OFF)
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)
check_library_exists(dl dlopen "" dl_FOUND)

# Library requirements.
find_package(SDL2)
find_package(SDL2_mixer)
find_package(SDL2_net)
find_package(Python3 COMPONENTS Interpreter)

add_subdirectory(data)
add_subdirectory(Source)

# Generate distribution packages with CPack.
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    # TODO: Not implemented yet.
    set(CPACK_GENERATOR DEB RPM)
endif()
set(CPACK_SOURCE_GENERATOR TGZ ZIP)
set(CPACK_STRIP_FILES TRUE)
include(CPack)
install(FILES COPYING DESTINATION "." RENAME COPYING.txt)
install(FILES README.md DESTINATION "." RENAME README.txt)
