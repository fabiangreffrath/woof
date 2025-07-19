include(GNUInstallDirs)

configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES CMDLINE.txt DESTINATION docs)
elseif(UNIX AND NOT APPLE)
    install(FILES CMDLINE.txt DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}")
    install(FILES "${PROJECT_SHORTNAME}.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "${PROJECT_SHORTNAME}-setup.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "bash-completion/${PROJECT_SHORTNAME}"
        DESTINATION "share/bash-completion/completions")
endif()
