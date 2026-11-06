include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    find_program(PANDOC_EXECUTABLE pandoc)
    if(PANDOC_EXECUTABLE)
        string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

        set(CMDLINE_MD "${CMAKE_CURRENT_BINARY_DIR}/cmdline.md")
        set(MAN_PAGE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_SHORTNAME}.6")

        # Generate man page in build directory at install time
        install(CODE [[
            execute_process(
                COMMAND "$<TARGET_FILE:woof>" -doc "${CMDLINE_MD}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src"
                RESULT_VARIABLE result
            )
            if(NOT result EQUAL 0)
                message(WARNING "Failed to generate cmdline.md")
            endif()
        ]])

        install(CODE [[
            execute_process(
                COMMAND "${PANDOC_EXECUTABLE}" -s -t man
                    --metadata title="${PROJECT_SHORTNAME}"
                    --metadata section=6
                    --metadata date="${BUILD_DATE}"
                    "${CMDLINE_MD}"
                    -o "${MAN_PAGE}"
                RESULT_VARIABLE result
            )
            if(NOT result EQUAL 0)
                message(WARNING "Failed to generate man page")
            else()
                message(STATUS "Generated man page")
            endif()
        ]])

        # Install generated man page from build directory
        install(FILES "${MAN_PAGE}" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    endif()

    if(Python3_EXECUTABLE)
        set(CMDLINE_MD "${CMAKE_CURRENT_BINARY_DIR}/cmdline.md")
        set(BASH_COMPLETION "${CMAKE_CURRENT_BINARY_DIR}/bash-completion/${PROJECT_SHORTNAME}")

        # Generate bash completion in build directory at install time
        install(CODE [[
            execute_process(
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/bash_completion.py"
                    "${CMDLINE_MD}"
                    "${BASH_COMPLETION}"
                    "${PROJECT_SHORTNAME}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src"
                RESULT_VARIABLE result
            )
            if(NOT result EQUAL 0)
                message(WARNING "Failed to generate bash completion")
            else()
                message(STATUS "Generated bash completion")
            endif()
        ]])

        # Install generated bash completion from build directory
        install(FILES "${BASH_COMPLETION}"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions"
        )
    endif()

    # Install iwad_paths.man
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/iwad_paths.man"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}"
        COMPONENT docs
    )
endif()
