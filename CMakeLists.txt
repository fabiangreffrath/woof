include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(ExternalProject)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.6)

project("Woof"
    VERSION 10.2.0
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

execute_process(COMMAND uname OUTPUT_VARIABLE uname)
if (NOT MSVC AND (uname MATCHES "^MSYS" OR uname MATCHES "^MINGW"))
    set(MSYS ON)
else()
    set(MSYS OFF)
endif()

if(MSYS)
    # 3.16 is needed for GET_RUNTIME_DEPENDENCIES.
    cmake_minimum_required(VERSION 3.16)
endif()

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2021")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_SHORTNAME "woof")
set(WOOF_ICON "woof.ico")
set(SETUP_ICON "setup.ico")
set(PROJECT_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0") # ${PROJECT_VERSION_TWEAK}

if(NOT WIN32)
    set(WOOFDATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_SHORTNAME}" CACHE PATH "Path to install supplemental files" FORCE)
endif()

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)
option("${PROJECT_NAME}_STRICT" "Prefer original MBF code paths over demo compatiblity with PrBoom+" OFF)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)
check_include_file("dirent.h" HAVE_DIRENT_H)
check_symbol_exists(strsignal "string.h" HAVE_STRSIGNAL)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)

# Library requirements.
#
# In some instances, we can do useful things if a library can't be found.

# SDL2.  On Windows, we use the official development library.
if(NOT SDL2_DIR)
    if(WIN32 AND NOT MSYS)
        message(STATUS "Downloading SDL2 Development Library...")
        set(pkgver "2.24.0")
        if(MSVC)
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL/releases/download/release-${pkgver}/SDL2-devel-${pkgver}-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                EXPECTED_HASH SHA256=97c6e2d17c0baebf4c84ede27ec42e211358f3694b9f558ead3a9c542c4a004e)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL/releases/download/release-${pkgver}/SDL2-devel-${pkgver}-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                EXPECTED_HASH SHA256=5036914a59a79db85fb92739c988b1fb3d6492c67c9333c58e888d7bd1a26701)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2-${pkgver}")
    endif()
endif()
find_package(SDL2 2.0.7)

# SDL2_mixer.  On Windows, we use the official development library.
if(NOT SDL2_MIXER_DIR)
    if(WIN32 AND NOT MSYS)
        message(STATUS "Downloading SDL2_mixer Development Library...")
        set(pkgver "2.6.2")
        if(MSVC)
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${pkgver}/SDL2_mixer-devel-${pkgver}-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip"
                EXPECTED_HASH SHA256=7f050663ccc7911bb9c57b11e32ca79578b712490186b8645ddbbe4e7d2fe1c9)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${pkgver}/SDL2_mixer-devel-${pkgver}-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz"
                EXPECTED_HASH SHA256=6c414d05a3b867e0d59e0f9b28ea7e5e64527e612ccf961735dc2478391315b3)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer-${pkgver}")
    endif()
endif()
find_package(SDL2_mixer 2.0.2)

# SDL2_net.  On Windows, we use the official development library.
if(NOT SDL2_NET_DIR)
    if(WIN32 AND NOT MSYS)
        message(STATUS "Downloading SDL2_net Development Library...")
        set(pkgver "2.2.0")
        if(MSVC)
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL_net/releases/download/release-${pkgver}/SDL2_net-devel-${pkgver}-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip"
                EXPECTED_HASH SHA256=f364e55babb44e47b41d039a43c640aa1f76615b726855591b555321c7d870dd)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-VC.zip"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        else()
            file(DOWNLOAD
                "https://github.com/libsdl-org/SDL_net/releases/download/release-${pkgver}/SDL2_net-devel-${pkgver}-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz"
                EXPECTED_HASH SHA256=369829e06c509d5e001fabdcbe006ff3efa934f3825dc0ae1b076f5ce9c183c4)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-mingw.tar.gz"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(SDL2_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-${pkgver}")
    endif()
endif()
find_package(SDL2_net)

# FluidSynth backend
if (NOT FluidSynth_DIR)
    if (WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P STREQUAL 4)
        message(STATUS "Downloading FluidSynth Development Library...")
        set(pkgver "2.2.8")
        file(DOWNLOAD
            "https://github.com/FluidSynth/fluidsynth/releases/download/v${pkgver}/fluidsynth-${pkgver}-winXP-x86.zip"
            "${CMAKE_CURRENT_BINARY_DIR}/fluidsynth-x86.zip"
            EXPECTED_HASH SHA256=03660c347f8cafda099afe4f1cc04080355441b8a68e09355550bd3067c798aa)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/fluidsynth-${pkgver}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xf
            "${CMAKE_CURRENT_BINARY_DIR}/fluidsynth-x86.zip"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fluidsynth-${pkgver}")
        set(FluidSynth_DIR "${CMAKE_CURRENT_BINARY_DIR}/fluidsynth-${pkgver}")
    endif()
endif()
find_package(FluidSynth)

# Python 3
find_package(Python3 COMPONENTS Interpreter)

configure_file(config.h.in config.h)

if(WIN32)
    install(FILES COPYING DESTINATION .)
    install(FILES README.md DESTINATION .)
    install(DIRECTORY docs/ DESTINATION docs)
    install(DIRECTORY examples/ DESTINATION docs/examples)
    install(DIRECTORY autoload/ DESTINATION autoload)
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION soundfonts)
    endif()
else()
    install(FILES COPYING DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    install(FILES README.md DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    install(DIRECTORY docs/ DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    install(DIRECTORY examples/ DESTINATION "share/doc/${PROJECT_SHORTNAME}/examples")
    install(DIRECTORY autoload/ DESTINATION "share/${PROJECT_SHORTNAME}/autoload")
    if(FluidSynth_FOUND)
        install(DIRECTORY soundfonts/ DESTINATION "share/${PROJECT_SHORTNAME}/soundfonts")
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
add_subdirectory(opl)
add_subdirectory(textscreen)
add_subdirectory(src)
add_subdirectory(toolsrc)
add_subdirectory(setup)
add_subdirectory(man)
