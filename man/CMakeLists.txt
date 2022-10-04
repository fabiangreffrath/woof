include(GNUInstallDirs)

if(Python3_EXECUTABLE)
    add_custom_target(paramsgen
            COMMAND "${Python3_EXECUTABLE}" docgen -a
            "${PROJECT_SOURCE_DIR}/src" > "${PROJECT_SOURCE_DIR}/src/params.h"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES CMDLINE.txt DESTINATION .)
elseif(UNIX AND NOT APPLE)
    install(FILES CMDLINE.txt DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_SHORTNAME}")
    install(FILES "${PROJECT_SHORTNAME}.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "${PROJECT_SHORTNAME}-setup.6" DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    install(FILES "bash-completion/${PROJECT_SHORTNAME}"
        DESTINATION "share/bash-completion/completions")
endif()
