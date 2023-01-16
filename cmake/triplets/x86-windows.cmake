set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)

if(${PORT} MATCHES "libsamplerate")
    set(VCPKG_CXX_FLAGS "/fp:fast")
    set(VCPKG_C_FLAGS "/fp:fast")
endif()
