include(WoofSettings)

add_library(textscreen STATIC
            textscreen.h
                                txt_main.h
            txt_sdl.c           txt_sdl.h
            txt_utf8.c          txt_utf8.h)

target_woof_settings(textscreen)

target_include_directories(textscreen
                           INTERFACE "."
                           PRIVATE "../Source/")
target_link_libraries(textscreen SDL2::SDL2)
