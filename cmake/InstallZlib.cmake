# Installs the bare minimum to link against the zlib dynamic library.

if(MSVC)
    message(STATUS "Visual C++ or compatible detected, assuming .lib import library")
else()
    message(STATUS "MinGW or compatible detected, assuming .dll.a import library")
endif()

# For build systems with multiple configuration types, the build artifact
# is located in a subdirectory with the configuration name.  Note that checking
# MSVC is _not_ enough to guarantee this, because it's possible to build MSVC
# with Ninja - in fact, that's how Visual Studio's built-in CMake support does it.
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Multiple build configurations detected, assuming use of prefix")
    set(PREFIX "Release")
else()
    message(STATUS "Single build configuration detected, assuming lack of prefix")
    set(PREFIX ".")
endif()

set(INSTALL_DIR "../../../local")

execute_process(COMMAND "${CMAKE_COMMAND}" -E
    make_directory "${INSTALL_DIR}/bin")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    make_directory "${INSTALL_DIR}/include")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    make_directory "${INSTALL_DIR}/lib")

execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy "${PREFIX}/zlib1.dll" "${INSTALL_DIR}/bin")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy zconf.h "${INSTALL_DIR}/include")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy ../zlib-project/zlib.h "${INSTALL_DIR}/include")
if(MSVC)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E
        copy "${PREFIX}/zlib.lib" "${INSTALL_DIR}/lib")
else()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E
        copy "${PREFIX}/zlib.dll.a" "${INSTALL_DIR}/lib")
endif()
