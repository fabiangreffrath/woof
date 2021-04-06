# Files that need no conversion.
set(WOOF_DOCS_FILES
    mbf-bugs.html)

# Plain text files that should be converted on Windows.
set(WOOF_DOCS_TEXTS
    boomdeh.txt
    boomref.txt
    boom.txt
    dckboom.txt
    mapcolors_vanilla.cfg
    mbf-bugs.html
    mbfedit.txt
    mbffaq.txt
    mbf.txt
    options.txt
    umapinfo.txt
    winmbf02s.txt
    winmbf02.txt)

configure_file(WoofInstall.cmake.in WoofInstall.cmake ESCAPE_QUOTES @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WoofInstall.cmake")

if(WIN32)
    install(FILES ${WOOF_DOCS_FILES} DESTINATION docs)
    foreach(WOOF_DOCS_TEXT ${WOOF_DOCS_TEXTS})
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${WOOF_DOCS_TEXT}"
            DESTINATION docs)
    endforeach()
else()
    install(DIRECTORY . DESTINATION "share/doc/${PROJECT_TARNAME}"
        PATTERN CMakeLists.txt EXCLUDE PATTERN *.cmake.in EXCLUDE)
endif()
