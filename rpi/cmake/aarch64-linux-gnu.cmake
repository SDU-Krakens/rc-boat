# Cross compile for the Zero 2 W (64-bit Raspberry Pi OS)
# Arch: pacman -S aarch64-linux-gnu-gcc
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
# Search path only, no CMAKE_SYSROOT: Arch's cross gcc has this sysroot built
# in, and on Ubuntu a sysroot breaks the absolute paths in libc.so
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
