string(TIMESTAMP PROJECT_DATE "%Y-%m-%d" UTC)

configure_file("project.metainfo.in"
               "${PROJECT_APPID}.metainfo.xml"
               ESCAPE_QUOTES @ONLY)

configure_file("project.desktop.in"
               "${PROJECT_APPID}.desktop"
               @ONLY)

configure_file("project-setup.desktop.in"
               "${PROJECT_APPID_SETUP}.desktop"
               @ONLY)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_APPID}.desktop"
            DESTINATION share/applications)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_APPID}.metainfo.xml"
            DESTINATION share/metainfo)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_APPID_SETUP}.desktop"
            DESTINATION share/applications)
    install(FILES woof.png DESTINATION share/icons/hicolor/128x128/apps)
    install(FILES woof-setup.png DESTINATION share/icons/hicolor/128x128/apps)
endif()
