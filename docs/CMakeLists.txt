set(WOOF_DOCS
    boom.txt
    boomdeh.txt
    boomref.txt
    mbf.txt
    mbfedit.txt
    mbffaq.txt
    options.txt
    woofhud.lmp
    woofhud.md)

if(WIN32)
    install(FILES ${WOOF_DOCS} DESTINATION docs)
else()
    install(FILES ${WOOF_DOCS} DESTINATION "share/doc/${PROJECT_SHORTNAME}")
endif()
