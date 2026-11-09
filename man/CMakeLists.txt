if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    find_program(PANDOC_EXECUTABLE pandoc)

    if(PANDOC_EXECUTABLE AND Python3_EXECUTABLE)
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