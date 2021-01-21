include(CheckLibraryExists)
include(ExternalProject)

# Adds the cmake directory to the CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 3.12 is needed for modern FindPython.
cmake_minimum_required(VERSION 3.12)
project("Woof"
    VERSION 3.1.0
    DESCRIPTION "Woof! is a continuation of Lee Killough's Doom source port MBF targeted at modern systems."
    HOMEPAGE_URL "https://github.com/fabiangreffrath/woof"
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

# Hardcoded defines added to configure and resource files.
set(PROJECT_COMPANY "Fabian Greffrath and contributors")
set(PROJECT_COPYRIGHT "Copyright (C) 1993-2021")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PROJECT_TARNAME "woof")
set(PROJECT_VERSION_RC "3,1,0,0")

# Toggle-able defines added at compile-time.
option("${PROJECT_NAME}_INSTRUMENTED" "Enable memory allocation statistics" OFF)
option("${PROJECT_NAME}_RANGECHECK" "Enable bounds-checking of performance-sensitive functions" ON)

# Compiler environment requirements.
check_library_exists(m pow "" m_FOUND)

# Library requirements.
#
# In some instances, we can do useful things if a library can't be found.

# Zlib is needed by both Woof and SDL_image.
#
# If it doesn't exist and we're on Windows, we compile it ourselves.  We're
# only helpful like this on Windows, since on Linux it probably meant you
# forgot to install your distro's zlib-dev equivalent.
set(ZLIB_ROOT "${ZLIB_ROOT}" CACHE PATH "Location of ZLIB library directory")
if(NOT ZLIB_ROOT)
    if(WIN32)
        # On Windows, we can compile zlib during the build process as an
        # external project.  SDL_image should be fine with our copy of
        # zlib1.dll, since it's supposed to be backwards-compatible.
        message(STATUS "Using ZLIB External Project")

        # This directory needs to exist ahead of time, otherwise CMake
        # complains about a missing directory later.
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/local/include")

        # The location of our include directory and import library.  These
        # cache variables are usually set by the finder script, but we set
        # them here and let the finder script pick up on them.
        set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/local/include" CACHE PATH "")
        set(ZLIB_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/local/lib/zlib${CMAKE_IMPORT_LIBRARY_SUFFIX}" CACHE FILEPATH "")
        mark_as_advanced(ZLIB_INCLUDE_DIR, ZLIB_LIBRARY_RELEASE)

        # The actual shared library.
        set(ZLIB_DLL "${CMAKE_CURRENT_BINARY_DIR}/local/bin/zlib1.dll")

        # Our toolchain needs to be an absolute path so we can pass it to the
        # external project we're about to compile.
        if(CMAKE_TOOLCHAIN_FILE)
            get_filename_component(
                CMAKE_ABSOLUTE_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}"
                REALPATH BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
            )
        else()
            set(CMAKE_ABSOLUTE_TOOLCHAIN_FILE "")
        endif()

        if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
            set(ZLIB_SHARED_LINKER_FLAGS "-static-libgcc")
        else()
            set(ZLIB_SHARED_LINKER_FLAGS "")
        endif()

        # Compile zlib ourselves.  This is run at build-time, not compile-time.
        ExternalProject_Add(
            zlib-project
            URL "https://www.zlib.net/zlib-1.2.11.tar.gz"
            URL_HASH SHA256=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1
            INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/local"
            DOWNLOAD_NO_PROGRESS TRUE
            CMAKE_ARGS
                --no-warn-unused-cli
                "-DCMAKE_BUILD_TYPE=Release"
                "-DCMAKE_CONFIGURATION_TYPES=Release"
                "-DCMAKE_IMPORT_LIBRARY_PREFIX_C="
                "-DCMAKE_SHARED_LIBRARY_PREFIX_C="
                "-DBUILD_SHARED_LIBS=True"
                "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_ABSOLUTE_TOOLCHAIN_FILE}"
                "-DCMAKE_SHARED_LINKER_FLAGS=${ZLIB_SHARED_LINKER_FLAGS}"
            BUILD_COMMAND "${CMAKE_COMMAND}" --build . --target zlib --config Release
            BUILD_BYPRODUCTS ${ZLIB_LIBRARY_RELEASE} ${ZLIB_DLL}
            INSTALL_COMMAND
                "${CMAKE_COMMAND}" "-DMSVC=${MSVC}"
                "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallZlib.cmake"
            EXCLUDE_FROM_ALL ON
        )

        # Synthesize an imported target with our compiled library.
        add_library(ZLIB::ZLIB SHARED IMPORTED)
        add_dependencies(ZLIB::ZLIB zlib-project)
        set_target_properties(
            ZLIB::ZLIB PROPERTIES
            IMPORTED_IMPLIB "${ZLIB_LIBRARY_RELEASE}"
            IMPORTED_LOCATION "${ZLIB_DLL}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
        )
    endif()
endif()
find_package(ZLIB)

# SDL2.  On Windows, we use the official development library.
if(NOT SDL2_DIR)
    if(WIN32)
        message(STATUS "Downloading SDL2 Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip"
                EXPECTED_HASH SHA256=232071cf7d40546cde9daeddd0ec30e8a13254c3431be1f60e1cdab35a968824)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-VC.zip")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/release/SDL2-devel-2.0.14-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz"
                EXPECTED_HASH SHA256=405eaff3eb18f2e08fe669ef9e63bc9a8710b7d343756f238619761e9b60407d)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2-mingw.tar.gz")
        endif()
        set(SDL2_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2-2.0.14")
    endif()
endif()
find_package(SDL2 2.0.7)

# SDL2_image.  On Windows, we use the official development library.
if(NOT SDL2_IMAGE_DIR)
    if(WIN32)
        message(STATUS "Downloading SDL2_image Development Library...")
        if(MSVC)
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.0.5-VC.zip"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_image-VC.zip"
                EXPECTED_HASH SHA256=a180f9b75c4d3fbafe02af42c42463cc7bc488e763cfd1ec2ffb75678b4387ac)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_image-VC.zip")
        else()
            file(DOWNLOAD
                "https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.0.5-mingw.tar.gz"
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_image-mingw.tar.gz"
                EXPECTED_HASH SHA256=41d9e5ff98aa84cf66e6c63c78e7c346746982fa53d3f36633423cc9177f986c)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
                "${CMAKE_CURRENT_BINARY_DIR}/SDL2_image-mingw.tar.gz")
        endif()
        set(SDL2_IMAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/SDL2_image-2.0.5")
    endif()
endif()
find_package(SDL2_image)

# SDL2_mixer.  On Windows, we use the official development library.
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
find_package(SDL2_mixer 2.0.2)

# SDL2_net.  On Windows, we use the official development library.
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
add_subdirectory(midiproc)
