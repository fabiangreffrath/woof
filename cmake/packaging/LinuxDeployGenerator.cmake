find_program(
    LINUXDEPLOY_EXECUTABLE
    NAMES linuxdeploy linuxdeploy-${CPACK_SYSTEM_PROCESSOR}.AppImage
)

if(NOT LINUXDEPLOY_EXECUTABLE)
    message(FATAL_ERROR "linuxdeploy could not be found")
endif()

execute_process(
    COMMAND
        ${CMAKE_COMMAND} -E env LDAI_OUTPUT=${CPACK_PACKAGE_FILE_NAME}.AppImage
        LINUXDEPLOY_OUTPUT_VERSION=${CPACK_PACKAGE_VERSION} NO_STRIP=1
        LDAI_UPDATE_INFORMATION=gh-releases-zsync|fabiangreffrath|woof|latest|Woof-*-${CPACK_SYSTEM_NAME}.AppImage.zsync
        ${LINUXDEPLOY_EXECUTABLE} --appimage-extract-and-run
        --appdir=${CPACK_TEMPORARY_DIRECTORY}
        --executable=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/${CPACK_BIN_DIR}/woof
        --desktop-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/applications/io.github.fabiangreffrath.woof.desktop
        --icon-file=${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}/share/icons/hicolor/128x128/apps/woof.png
        --output=appimage
    RESULT_VARIABLE linuxdeploy_result
)

if(NOT linuxdeploy_result EQUAL 0)
    message(FATAL_ERROR  "linuxdeploy failed with exit code ${linuxdeploy_result}")
endif()
