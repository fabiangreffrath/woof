# Cross-compilation toolchain for 64-bit ARM Linux on a Debian/Ubuntu host.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Without this, pkg-config resolves the x86_64 copies of the system libraries
# instead of the :arm64 multiarch packages.
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
