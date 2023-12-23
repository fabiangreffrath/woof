string(TIMESTAMP PROJECT_DATE "%Y-%m-%d")

configure_file(io.github.fabiangreffrath.woof.metainfo.in
               io.github.fabiangreffrath.woof.metainfo.xml
               ESCAPE_QUOTES @ONLY)

if(UNIX AND NOT APPLE)
    install(FILES io.github.fabiangreffrath.woof.desktop DESTINATION share/applications)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/io.github.fabiangreffrath.woof.metainfo.xml"
            DESTINATION share/metainfo)
    install(FILES io.github.fabiangreffrath.woof-setup.desktop DESTINATION share/applications)
    install(FILES woof.png DESTINATION share/icons/hicolor/128x128/apps)
    install(FILES woof-setup.png DESTINATION share/icons/hicolor/128x128/apps)
endif()
