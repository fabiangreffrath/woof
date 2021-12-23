include(WoofSettings)

add_library(textscreen STATIC
            textscreen.h
                                txt_main.h
            txt_button.c        txt_button.h
            txt_desktop.c       txt_desktop.h
            txt_gui.c           txt_gui.h
            txt_io.c            txt_io.h
            txt_label.c         txt_label.h
            txt_separator.c     txt_separator.h
            txt_strut.c         txt_strut.h
            txt_table.c         txt_table.h
            txt_widget.c        txt_widget.h
            txt_window_action.c txt_window_action.h
            txt_window.c        txt_window.h
            txt_sdl.c           txt_sdl.h
            txt_utf8.c          txt_utf8.h)

target_woof_settings(textscreen)

target_include_directories(textscreen
                           INTERFACE "."
                           PRIVATE "../Source/")
target_link_libraries(textscreen SDL2::SDL2)
