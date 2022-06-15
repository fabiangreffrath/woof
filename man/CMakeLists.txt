
configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES CMDLINE.txt DESTINATION .)
else()
    install(FILES CMDLINE.txt DESTINATION "share/doc/${PROJECT_TARNAME}")
endif()
