
configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES CMDLINE.txt DESTINATION .)
elseif(UNIX AND NOT APPLE)
    install(FILES CMDLINE.txt DESTINATION "share/doc/${PROJECT_SHORTNAME}")
    install(FILES "${PROJECT_SHORTNAME}.6" DESTINATION share/man/man6)
    install(FILES "${PROJECT_SHORTNAME}-setup.6" DESTINATION share/man/man6)
    install(FILES "bash-completion/${PROJECT_SHORTNAME}"
        DESTINATION "share/bash-completion/completions")
endif()
