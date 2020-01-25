# Files that need no conversion.
set(WOOF_EXAMPLES_FILES
    battle.wad
    dogfly.deh
    donut.wad
    fireplas.deh
    fly.deh
    friend.deh
    grenade.deh
    hockey.wad
    mbfedit.wad
    mine.deh
    mushroom.deh
    playbud.deh
    possbud.deh
    sky.wad
    touchy.deh)

# Plain text files that should be converted on Windows.
set(WOOF_EXAMPLES_TEXTS
    donut.txt
    mbfedit.txt)

configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES ${WOOF_EXAMPLES_FILES} DESTINATION docs/examples)
    foreach(WOOF_EXAMPLES_TEXT ${WOOF_EXAMPLES_TEXTS})
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${WOOF_EXAMPLES_TEXT}"
            DESTINATION docs/examples)
    endforeach()
else()
    install(DIRECTORY . DESTINATION "share/doc/${PROJECT_TARNAME}/examples"
        PATTERN CMakeLists.txt EXCLUDE PATTERN *.cmake.in EXCLUDE)
endif()
