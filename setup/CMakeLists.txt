include(WoofSettings)

add_library(setup STATIC
            mainmenu.c
            multiplayer.c       multiplayer.h
            execute.c           execute.h)

target_woof_settings(setup)

target_include_directories(setup
                           INTERFACE "."
                           PRIVATE "../src/" "${CMAKE_CURRENT_BINARY_DIR}/../")

target_link_libraries(setup textscreen SDL2::SDL2)
