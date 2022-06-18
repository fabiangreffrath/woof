include(WoofSettings)

add_library(opl STATIC
            opl_internal.h
            opl.c           opl.h
            opl_queue.c     opl_queue.h
            opl_sdl.c
            opl3.c          opl3.h)

target_woof_settings(opl)

target_include_directories(opl
                           INTERFACE "."
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries(opl SDL2::mixer)
