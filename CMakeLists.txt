include(CheckLibraryExists)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 3.12 is needed for modern FindPython.
cmake_minimum_required(VERSION 3.12)
project("Woof"
    VERSION 1.0.1
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
set(PROJECT_VERSION_RC "1,0,1,0")

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_INSTRUMENTED" "Enable memory allocation statistics" OFF)
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)
check_library_exists(dl dlopen "" dl_FOUND)
check_library_exists(z inflate "" z_FOUND)

# Library requirements.
find_package(SDL2)
find_package(SDL2_mixer)
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
