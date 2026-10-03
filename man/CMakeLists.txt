include(GNUInstallDirs)

configure_file(ProjectInstall.cmake.in ProjectInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/ProjectInstall.cmake")

if(WIN32)
    install(FILES CMDLINE.txt DESTINATION .)
elseif(UNIX AND NOT APPLE)
    install(FILES CMDLINE.txt DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}")
    install(FILES "${PROJECT_SHORTNAME}.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "${PROJECT_SHORTNAME}-setup.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "bash-completion/${PROJECT_SHORTNAME}"
        DESTINATION "share/bash-completion/completions")
endif()
