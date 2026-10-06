# toolchain-mingw.cmake
# Usage:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake
# Requires:
#   sudo apt install mingw-w64 mingw-w64-tools
#   sudo apt install nsis
#   sudo apt install gcc-mingw-w64 g++-mingw-w64
# Description:
#   Cross compile for Windows (x86_64) on Linux using MinGW-w64

# Target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compiler
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

# Optional: For static linking of libgcc/libstdc++
set(CMAKE_CXX_FLAGS_INIT "-static-libgcc -static-libstdc++")
set(CMAKE_C_FLAGS_INIT   "-static-libgcc")

# Optional: default to Release if not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Where to look for target environment libraries/includes
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Adjust search order: prefer target, avoid host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
