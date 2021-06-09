set (CMAKE_SYSTEM_NAME "Linux")
set (CMAKE_SYSTEM_PROCESSOR "x86_64")
set (ARCH "i686-mcx-linux-gnu")

if (NOT TOOLCHAIN)
  set(TOOLCHAIN "/opt/i686-mcx-linux-gnu")
endif ()

set (CMAKE_C_COMPILER "${TOOLCHAIN}/bin/${ARCH}-gcc")
# Not g++ here because of using static libstdc++
set (CMAKE_CXX_COMPILER "${TOOLCHAIN}/bin/${ARCH}-gcc")

find_program (CMAKE_OBJCOPY NAMES "${TOOLCHAIN}/bin/${ARCH}-objcopy" PATH "${TOOLCHAIN}/bin" NO_DEFAULT_PATH)
find_program (CMAKE_STRIP NAMES "${TOOLCHAIN}/bin/${ARCH}-strip" PATH "${TOOLCHAIN}/bin" NO_DEFAULT_PATH)

set (CMAKE_FIND_ROOT_PATH ${TOOLCHAIN}/${ARCH} ${TOOLCHAIN}/${ARCH}/libc/usr ${TOOLCHAIN}/${ARCH}/lib ${TOOLCHAIN}/sysroot ${TOOLCHAIN}/${ARCH}/multi-libs)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set (STDCPP_PATH_SUFFIX lib)
