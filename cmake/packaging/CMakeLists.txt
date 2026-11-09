set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "Fabian Greffrath and contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Continuation of the Boom/MBF bloodline of Doom source ports.")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/COPYING)

if(WIN32)
    set(CPACK_GENERATOR ZIP)
elseif(LINUX AND CMAKE_VERSION VERSION_GREATER_EQUAL 4.2)
    # Built-in AppImage generator requires CMake 4.2+
    set(CPACK_GENERATOR AppImage)
    
    set(CPACK_PACKAGE_ICON "woof")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/WoofAppImageInstall.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/WoofAppImageInstall.cmake @ONLY)
    install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/WoofAppImageInstall.cmake)

    # Let generator auto-detect desktop file from install manifest
    set(CPACK_APPIMAGE_EXECUTABLES "woof")
    set(CPACK_APPIMAGE_APP_NAME "Woof")
    
    # zsync update information
    set(CPACK_APPIMAGE_UPDATE_INFORMATION
        "gh-releases-zsync|fabiangreffrath|woof|latest|Woof-*-${CPACK_SYSTEM_NAME}.AppImage.zsync")
    
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        # Keep the arm64 AppImage from colliding with the x86_64 one.
        set(CPACK_SYSTEM_NAME "Linux-aarch64")
    endif()
elseif(APPLE)
    # Keep existing External generator for macOS (no built-in DMG generator)
    set(CPACK_GENERATOR External)
    set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/DarwinGenerator.cmake")
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_BIN_DIR "${WOOF_INSTALL_BIN_DIR}")
    set(CPACK_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
    set(CPACK_SYSTEM_NAME "MacOS-universal")
endif()

include(CPack)
