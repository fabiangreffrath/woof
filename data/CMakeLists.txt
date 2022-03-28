if(Python3_FOUND)
    add_custom_target(convert-icon
        COMMAND "${Python3_EXECUTABLE}" "convert-icon"
            "woof.png" "../Source/icon.c"
        VERBATIM WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(UNIX AND NOT APPLE)
    install(FILES woof.6 DESTINATION share/man/man6)
    install(FILES woof.desktop DESTINATION share/applications)
    install(FILES woof.png DESTINATION share/icons/hicolor/128x128/apps)
endif()
