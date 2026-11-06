include(GNUInstallDirs)

# Find pandoc
find_program(PANDOC_EXECUTABLE pandoc)
if(NOT PANDOC_EXECUTABLE)
    message(WARNING "pandoc not found, man page will not be generated")
endif()

# Build date
string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

if(UNIX AND NOT APPLE AND PANDOC_EXECUTABLE)
    install(CODE [[
        execute_process(
            COMMAND "$<TARGET_FILE:woof>" -doc "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}/cmdline.md"
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
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
                "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}/cmdline.md"
                -o "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man6/${PROJECT_SHORTNAME}.6"
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(WARNING "Failed to generate man page")
        else()
            message(STATUS "Generated man page")
        endif()
    ]])

    if(Python3_EXECUTABLE)
        install(CODE [[
            execute_process(
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/bash_completion.py"
                    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}/cmdline.md"
                    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions/${PROJECT_SHORTNAME}"
                    "${PROJECT_SHORTNAME}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                RESULT_VARIABLE result
            )
            if(NOT result EQUAL 0)
                message(WARNING "Failed to generate bash completion")
            else()
                message(STATUS "Generated bash completion")
            endif()
        ]])
    endif()
endif()

# Install iwad_paths.man
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/iwad_paths.man"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}"
        COMPONENT docs
    )
endif()
