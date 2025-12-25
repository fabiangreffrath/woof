vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO libsdl-org/SDL
    REF a2d19ec7f325bf5925c0ea1f7065259dff36d3fb
    SHA512 8e567562dba6631530b1aa415740785d1e3ba56bdad78e500f35d8937cb9e69870fc61ace69b703e3b5cc01d5759540f743fe6fc0a1e8f75e12da6280e190675
    HEAD_REF main
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" SDL_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" SDL_SHARED)
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" FORCE_STATIC_VCRT)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        alsa     SDL_ALSA
        ibus     SDL_IBUS
        vulkan   SDL_VULKAN
        wayland  SDL_WAYLAND
        x11      SDL_X11
)

if ("x11" IN_LIST FEATURES)
    message(WARNING "You will need to install Xorg dependencies to use feature x11:\nsudo apt install libx11-dev libxft-dev libxext-dev\n")
endif()
if ("wayland" IN_LIST FEATURES)
    message(WARNING "You will need to install Wayland dependencies to use feature wayland:\nsudo apt install libwayland-dev libxkbcommon-dev libegl1-mesa-dev\n")
endif()
if ("ibus" IN_LIST FEATURES)
    message(WARNING "You will need to install ibus dependencies to use feature ibus:\nsudo apt install libibus-1.0-dev\n")
endif()
# option for not need to show windows
list(APPEND FEATURE_OPTIONS -DSDL_UNIX_CONSOLE_BUILD=ON)
if (VCPKG_TARGET_IS_LINUX AND NOT "x11" IN_LIST FEATURES AND NOT "wayland" IN_LIST FEATURES)
    message(WARNING "The selected features don't allow sdl3 to create windows, which is usually unintentional. You can get windowing support by installing the x11 and/or wayland features.")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DSDL_STATIC=${SDL_STATIC}
        -DSDL_SHARED=${SDL_SHARED}
        -DSDL_FORCE_STATIC_VCRT=${FORCE_STATIC_VCRT}
        -DSDL_LIBC=ON
        -DSDL_TEST_LIBRARY=OFF
        -DSDL_TESTS=OFF
        -DSDL_INSTALL_CMAKEDIR_ROOT=share/${PORT}
        # Specifying the revision skips the need to use git to determine a version
        -DSDL_REVISION=vcpkg
        -DCMAKE_DISABLE_FIND_PACKAGE_LibUSB=1
    MAYBE_UNUSED_VARIABLES
        SDL_FORCE_STATIC_VCRT
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt"
    COMMENT "Some configurations may use code licensed under the MIT and Apache-2.0 licenses."
)
