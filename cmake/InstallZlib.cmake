# Installs the bare minimum to link against the zlib dynamic library.

message(STATUS "MSVC=${MSVC}")

set(INSTALL_DIR "../../../local")

execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy zlib1.dll "${INSTALL_DIR}/bin")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    make_directory "${INSTALL_DIR}/include")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy zconf.h "${INSTALL_DIR}/include")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy ../zlib-project/zlib.h "${INSTALL_DIR}/include")
execute_process(COMMAND "${CMAKE_COMMAND}" -E
    copy zlib.dll.a "${INSTALL_DIR}/lib")
