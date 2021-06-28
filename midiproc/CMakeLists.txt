if(WIN32)
    include(WoofSettings)

    add_executable(woof-midiproc WIN32 buffer.c buffer.h main.c proto.h)

    target_woof_settings(woof-midiproc)
    target_include_directories(woof-midiproc
                               PRIVATE "../Source/" "${CMAKE_CURRENT_BINARY_DIR}/../")
    target_link_libraries(woof-midiproc SDL2::SDL2main SDL2::mixer)

    install(TARGETS woof-midiproc RUNTIME DESTINATION .)
endif()
