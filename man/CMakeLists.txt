include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    find_program(pandoc_EXECUTABLE pandoc)
    if(NOT pandoc_EXECUTABLE)
        message(WARNING "pandoc not found, man page will not be generated")
    endif()

    # Build date
    string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

    # Generated files in build directory
    set(CMDLINE_MD "${CMAKE_CURRENT_BINARY_DIR}/cmdline.md")
    set(MAN_PAGE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_SHORTNAME}.6")
    set(BASH_COMPLETION "${CMAKE_CURRENT_BINARY_DIR}/bash-completion/${PROJECT_SHORTNAME}")

    if(pandoc_EXECUTABLE)
        # Generate man page at build time (POST_BUILD on woof target)
        add_custom_command(TARGET woof POST_BUILD
            COMMAND "$<TARGET_FILE:woof>" -doc "${CMDLINE_MD}"
            COMMAND "${pandoc_EXECUTABLE}" -s -t man
                --metadata title="${PROJECT_SHORTNAME}"
                --metadata section=6
                --metadata date="${BUILD_DATE}"
                "${CMDLINE_MD}" -o "${MAN_PAGE}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating man page"
            VERBATIM
        )

        # Install generated man page
        install(FILES "${MAN_PAGE}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
    endif()

    if(Python3_EXECUTABLE)
        # Generate bash completion at build time
        add_custom_command(TARGET woof POST_BUILD
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/bash_completion.py"
                "${CMDLINE_MD}" "${BASH_COMPLETION}" "${PROJECT_SHORTNAME}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating bash completion"
            VERBATIM
        )

        # Install generated bash completion
        install(FILES "${BASH_COMPLETION}" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions")
    endif()

    # Install iwad_paths.man
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/iwad_paths.man"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}"
        COMPONENT docs
    )
endif()
