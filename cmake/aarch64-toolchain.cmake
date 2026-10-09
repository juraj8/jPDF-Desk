set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

# Debian multiarch installs target libraries under /usr/lib/aarch64-linux-gnu
# and shared headers under /usr/include. CMake derives the library architecture
# from the cross-compiler; no separate sysroot is needed.
list(APPEND CMAKE_IGNORE_PATH /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu)

# These settings apply only to this CMake process and its children, leaving the
# preceding native Qt host-tools build unaffected.
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR}
    "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
