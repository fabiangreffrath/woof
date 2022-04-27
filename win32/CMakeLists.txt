if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    add_executable(win-textfile
        win-textfile.c
        win_fopen.c
        win_fopen.h
    )
endif()
