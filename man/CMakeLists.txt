if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    find_program(PANDOC_EXECUTABLE pandoc)

    # params_dump runs on the build host at install time, so a cross build
    # cannot produce it with the target toolchain.
    if(NOT DEFINED WOOF_HOST_C_COMPILER)
        if(CMAKE_CROSSCOMPILING)
            find_program(WOOF_HOST_C_COMPILER
                NAMES cc gcc clang
                DOC "C compiler for host tools such as params_dump"
            )
        else()
            set(WOOF_HOST_C_COMPILER "${CMAKE_C_COMPILER}")
        endif()
    endif()

    if(PANDOC_EXECUTABLE AND WOOF_HOST_C_COMPILER)
        set(PARAMS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../src/m_params.c")
        set(PARAMS_DUMP "${CMAKE_CURRENT_BINARY_DIR}/params_dump_bin${CMAKE_EXECUTABLE_SUFFIX}")

        # m_params.c doubles as the parameter dumper when built standalone.
        if(CMAKE_CROSSCOMPILING)
            add_custom_command(
                OUTPUT "${PARAMS_DUMP}"
                COMMAND "${WOOF_HOST_C_COMPILER}" -DPARAMS_DUMP
                        "-I${CMAKE_CURRENT_SOURCE_DIR}/../src"
                        "${PARAMS_SOURCE}" -o "${PARAMS_DUMP}"
                DEPENDS "${PARAMS_SOURCE}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/../src/m_params.h"
                VERBATIM
            )
            add_custom_target(params_dump_host ALL DEPENDS "${PARAMS_DUMP}")
        else()
            add_executable(params_dump_host "${PARAMS_SOURCE}")
            set_target_properties(
                params_dump_host
                PROPERTIES OUTPUT_NAME params_dump_bin
            )
            target_compile_definitions(params_dump_host PRIVATE PARAMS_DUMP)
            target_include_directories(
                params_dump_host
                PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
            )
        endif()

        string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bash-completion")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bash-completion/woof.template.in
            ${CMAKE_CURRENT_BINARY_DIR}/bash-completion/woof.template @ONLY
        )

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/WoofInstall.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake @ONLY
        )

        install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake)

        # Install generated files
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_SHORTNAME}.6"
                DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bash-completion/${PROJECT_SHORTNAME}"
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions
        )
    endif()
endif()
