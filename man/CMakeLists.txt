if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    find_program(PANDOC_EXECUTABLE pandoc)

    # The dumper runs on the build host at install time, so it cannot be
    # produced by a cross-compiling toolchain.
    if(PANDOC_EXECUTABLE AND NOT CMAKE_CROSSCOMPILING)
        # m_params.c doubles as the parameter dumper when built standalone.
        add_executable(params_dump "${CMAKE_CURRENT_SOURCE_DIR}/../src/m_params.c")
        target_compile_definitions(params_dump PRIVATE PARAMS_DUMP)
        target_include_directories(
            params_dump
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        )

        set(PARAMS_DUMP "${CMAKE_CURRENT_BINARY_DIR}/params_dump${CMAKE_EXECUTABLE_SUFFIX}")
        string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

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
