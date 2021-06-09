set (CMAKE_SYSTEM_NAME "Linux")
set (CMAKE_SYSTEM_PROCESSOR "armv5e-poky")
set (ARCH "arm-poky-linux-gnueabi")

set (SYSROOTS "/opt/poky/armv5e/sysroots")

if (NOT TOOLCHAIN)
  set(TOOLCHAIN ${SYSROOTS}/x86_64-pokysdk-linux/usr/bin/${ARCH})
endif ()

set (CMAKE_C_COMPILER "${TOOLCHAIN}/${ARCH}-gcc")
set (CMAKE_CXX_COMPILER "${TOOLCHAIN}/${ARCH}-g++")

set (CMAKE_SYSROOT ${SYSROOTS}/armv5e-poky-linux-gnueabi)
#set (CMAKE_PREFIX_PATH ${CMAKE_SYSROOT}/usr/lib/pkgconfig)

set (ENV{PKG_CONFIG_PATH} ${CMAKE_SYSROOT}/usr/lib/pkgconfig)

find_program (CMAKE_OBJCOPY NAMES "${TOOLCHAIN}/${ARCH}-objcopy" PATH "${TOOLCHAIN}" NO_DEFAULT_PATH)
find_program (CMAKE_STRIP NAMES "${TOOLCHAIN}/${ARCH}-strip" PATH "${TOOLCHAIN}" NO_DEFAULT_PATH)

set (CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set (STDCPP_PATH_SUFFIX lib)

set (COMPILER_FLAGS " -O2 -pipe -g -feliminate-unused-debug-types ")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILER_FLAGS}" CACHE STRING "" FORCE)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS}" CACHE STRING "" FORCE)

set (LINKER_FLAGS " -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed ")
set (CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} ${LINKER_FLAGS}" CACHE STRING "" FORCE)

