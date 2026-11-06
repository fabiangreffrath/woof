include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    find_program(PANDOC_EXECUTABLE pandoc)
    if(PANDOC_EXECUTABLE AND Python3_EXECUTABLE)
        string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

        set(WOOF_INSTALL_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/WoofInstall.cmake.in")
        set(WOOF_INSTALL_CONFIGURED "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake.configured")
        set(WOOF_INSTALL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

        # Step 1: Expand CMake variables at configure time (leaves $<TARGET_FILE:woof> intact)
        configure_file(
            "${WOOF_INSTALL_TEMPLATE}"
            "${WOOF_INSTALL_CONFIGURED}"
            @ONLY
        )

        # Step 2: Evaluate generator expressions at build time
        file(GENERATE
            OUTPUT "${WOOF_INSTALL_SCRIPT}"
            INPUT "${WOOF_INSTALL_CONFIGURED}"
        )

        # Run generated install script at install time
        install(SCRIPT "${WOOF_INSTALL_SCRIPT}")

        # Install generated man page from build directory
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_SHORTNAME}.6"
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man6
        )

        # Install generated bash completion from build directory
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bash-completion/${PROJECT_SHORTNAME}"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions"
        )
    endif()

    # Install iwad_paths.man
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/iwad_paths.man"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}"
        COMPONENT docs
    )
endif()
